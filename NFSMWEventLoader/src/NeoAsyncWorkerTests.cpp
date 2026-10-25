#include "NeoAsyncWorker.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace std::chrono_literals;
using TestWorker=mwel_async::Worker<int,int,2>;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static mwel_async::Identity Key(){return {10,20,30,40,1,2};}
static bool Await(TestWorker& worker,std::size_t slot,mwel_async::Identity key,std::uint64_t now,std::uint64_t age,int& value){
 const auto limit=std::chrono::steady_clock::now()+2s;
 do{if(worker.Take(slot,key,now,age,value))return true;std::this_thread::sleep_for(1ms);}while(std::chrono::steady_clock::now()<limit);return false;
}
struct Gate {
 std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
 void Hold(){std::unique_lock<std::mutex> lock(mutex);entered=true;changed.notify_all();Check(changed.wait_for(lock,2s,[&]{return released;}),"blocked test callback timed out");}
 void Entered(){std::unique_lock<std::mutex> lock(mutex);Check(changed.wait_for(lock,2s,[&]{return entered;}),"worker did not enter callback");}
 void Release(){std::lock_guard<std::mutex> lock(mutex);released=true;changed.notify_all();}
};
int main(){try{
 TestWorker worker;int value=0;
 // Two slots must compute concurrently, while output ownership is unchanged.

 for(auto item:std::array<std::pair<unsigned,unsigned>,10>{{{0,1},{1,1},{2,1},{3,1},{4,2},{8,2},{12,3},{16,4},{32,8},{64,8}}})
  Check(mwel_async::PlanningWorkers(item.first)==item.second,"CPU-affinity budget changed");
 for(unsigned width:{4u,8u}){
  mwel_async::Worker<int,int,16> pool;std::mutex guard;std::condition_variable changed;
  unsigned entered=0;bool release=false;std::atomic<unsigned> retired{0};
  Check(!pool.Start([](int v,const mwel_async::Cancel&){return std::optional<int>(v);},9),"pool ceiling exceeded");
  Check(pool.Start([&](int v,const mwel_async::Cancel& cancel)->std::optional<int>{
   std::unique_lock<std::mutex> lock(guard);++entered;changed.notify_all();
   changed.wait_for(lock,2s,[&]{return release;});
   if(cancel.Requested()){++retired;return {};}return v;
  },width),"multi-core pool start failed");
  for(unsigned slot=0;slot<width;++slot)Check(pool.Submit(slot,{Key(),100,int(slot)}),"pool request lost");
  bool concurrent=false;{std::unique_lock<std::mutex> lock(guard);concurrent=changed.wait_for(lock,2s,[&]{return entered==width;});}
  pool.Invalidate();{std::lock_guard<std::mutex> lock(guard);release=true;}changed.notify_all();pool.Stop();
  Check(concurrent&&retired==width,"multi-core retirement failed");
  for(unsigned slot=0;slot<width;++slot){int out=-1;Check(!pool.Take(slot,Key(),110,20,out),"retired multi-core plan escaped");}
 }
 Gate left,right;
 Check(!worker.Start([](int v,const mwel_async::Cancel&){return std::optional<int>(v);},3),"unbounded worker count accepted");
 Check(worker.Start([&](int v,const mwel_async::Cancel&){(v==1?left:right).Hold();return std::optional<int>(v);},2),"two-worker start failed");
 worker.Submit(0,{Key(),100,1});worker.Submit(1,{Key(),100,2});left.Entered();right.Entered();left.Release();right.Release();
 Check(Await(worker,0,Key(),110,20,value)&&value==1,"parallel left failed");
 Check(Await(worker,1,Key(),110,20,value)&&value==2,"parallel right failed");worker.Stop();
 Gate retiringLeft,retiringRight;std::atomic<unsigned> cancelled{0};
 Check(worker.Start([&](int v,const mwel_async::Cancel& cancel){(v==1?retiringLeft:retiringRight).Hold();if(cancel.Requested()){++cancelled;return std::optional<int>{};}return std::optional<int>(v);},2),"parallel retirement start failed");
 worker.Submit(0,{Key(),100,1});worker.Submit(1,{Key(),100,2});retiringLeft.Entered();retiringRight.Entered();
 worker.Invalidate();retiringLeft.Release();retiringRight.Release();worker.Stop();
 Check(cancelled==2,"race retirement failed to cancel both CPU workers");
 Check(!worker.Take(0,Key(),110,20,value)&&!worker.Take(1,Key(),110,20,value),"retired parallel result escaped");
 Check(!worker.Submit(0,{Key(),100,2}),"disabled worker accepted work");
 Check(worker.Start([](int input,const mwel_async::Cancel&){return std::optional<int>(input*2);}),"worker start failed");
 Check(!worker.Start({}),"double start accepted");Check(!worker.Submit(2,{Key(),100,2}),"out of range slot accepted");
 Check(worker.Submit(0,{Key(),100,2}),"request rejected");Check(Await(worker,0,Key(),110,20,value)&&value==4,"owned result failed");
 Check(!worker.Take(0,Key(),110,20,value),"result consumed twice");worker.Stop();
 // Same-slot updates coalesce while an older plan is still computing.
 Gate latest;Check(worker.Start([&](int input,const mwel_async::Cancel&){if(input==1)latest.Hold();return std::optional<int>(input);}),"restart failed");
 worker.Submit(0,{Key(),100,1});latest.Entered();worker.Submit(0,{Key(),101,2});worker.Submit(0,{Key(),102,3});latest.Release();
 Check(Await(worker,0,Key(),110,20,value)&&value==3,"older/superseded plan escaped");worker.Stop();
 // Race/world retirement invalidates work in flight without waiting.
 Gate retire;Check(worker.Start([&](int input,const mwel_async::Cancel& cancel){retire.Hold();return cancel.Requested()?std::nullopt:std::optional<int>(input);}),"start failed");
 worker.Submit(0,{Key(),100,8});retire.Entered();const auto before=std::chrono::steady_clock::now();worker.Invalidate();
 Check(std::chrono::steady_clock::now()-before<100ms,"invalidate waited for planning");retire.Release();worker.Stop();Check(!worker.Take(0,Key(),110,20,value),"retired race output escaped");
 // Emergency synchronous planning discards just this slot. Its in-flight
 // advisory result must not overwrite the emergency plan after completion,
 // while unrelated vehicles continue and the slot can accept fresh work.
 Gate discarded;Check(worker.Start([&](int input,const mwel_async::Cancel&){if(input==1)discarded.Hold();return std::optional<int>(input);}),"discard test start failed");
 worker.Submit(0,{Key(),100,1});discarded.Entered();worker.Discard(0);
 worker.Submit(1,{Key(),100,2});discarded.Release();
 Check(Await(worker,1,Key(),110,20,value)&&value==2,"discard affected unrelated vehicle");
 Check(!worker.Take(0,Key(),110,20,value),"discarded in-flight advisory escaped");
 worker.Submit(0,{Key(),100,3});Check(Await(worker,0,Key(),110,20,value)&&value==3,"discard permanently disabled slot");
 worker.Discard(2);worker.Stop();
 // Cancellation reaches already-running obsolete jobs, not only publication.
 for(bool resubmit:{false,true}){
  Gate obsolete;std::atomic<bool> cancelled{false};
  Check(worker.Start([&](int& input,const mwel_async::Cancel& cancel)->std::optional<int>{
   if(input==1){obsolete.Hold();cancelled=cancel.Requested();}return input;
  }),"obsolete test start failed");
  worker.Submit(0,{Key(),100,1});obsolete.Entered();
  if(resubmit)worker.Submit(0,{Key(),101,3});else worker.Discard(0);
  worker.Submit(1,{Key(),101,7});obsolete.Release();
  Check(Await(worker,1,Key(),110,20,value)&&value==7,"unrelated work lost");
  Check(cancelled.load(),"obsolete in-flight job did not observe cancellation");
  if(resubmit)Check(Await(worker,0,Key(),110,20,value)&&value==3,"new request cancelled");
  worker.Stop();
 }
 // Each ownership discriminator matters independently. Results may never be
 // applied after handle reuse, CP/lap changes, world swaps, or clock reversal.
 for(unsigned field=0;field<8;++field){
  Gate publication;Check(worker.Start([&](int input,const mwel_async::Cancel&){publication.Hold();return std::optional<int>(input);}),"start failed");
  worker.Submit(0,{Key(),100,9});publication.Entered();publication.Release();
  // Wait until worker has published by inserting a second-slot marker.
  worker.Submit(1,{Key(),100,11});Check(Await(worker,1,Key(),110,20,value),"publication marker failed");
  auto wrong=Key();if(field==0)++wrong.raceGeneration;if(field==1)++wrong.worldGeneration;if(field==2)++wrong.course;if(field==3)++wrong.vehicle;if(field==4)++wrong.lap;if(field==5)++wrong.leg;
  Check(!worker.Take(0,wrong,field==6?200:field==7?99:110,20,value),"stale ownership/age accepted");worker.Stop();
 }
 // Exceptions are rejected without terminating the worker or producing logs.
 Check(worker.Start([](int input,const mwel_async::Cancel&)->std::optional<int>{if(input==0)throw std::runtime_error("planner failure");return input;}),"start failed");
 worker.Submit(0,{Key(),100,0});worker.Submit(1,{Key(),100,12});Check(Await(worker,1,Key(),110,20,value)&&value==12,"exception stopped worker");Check(!worker.Take(0,Key(),110,20,value),"exception published result");worker.Stop();
 Check(!worker.Submit(0,{Key(),100,13}),"stopped worker accepted request");
 std::cout<<"PASS disabled lifecycle, coalescing, in-flight invalidation/discard with unrelated-slot progress and reuse, race/world/course/vehicle/lap/leg ownership, age, clock reversal, exception recovery and stop\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
