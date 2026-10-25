#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace mwel_async {
// Values only: a pointer recycled by the game is not a lifecycle identity.
struct Identity {
 std::uint64_t raceGeneration=0,worldGeneration=0;
 std::uint32_t course=0,vehicle=0,lap=0;
 std::int32_t leg=-1;
 bool operator==(const Identity& b)const{return raceGeneration==b.raceGeneration&&worldGeneration==b.worldGeneration&&course==b.course&&vehicle==b.vehicle&&lap==b.lap&&leg==b.leg;}
};
struct Cancel {
 const std::atomic<std::uint64_t>* epoch=nullptr;
 std::uint64_t expected=0;
 const std::atomic<std::uint64_t>* slotEpoch=nullptr;std::uint64_t expectedSlot=0;
 bool Requested()const{return epoch->load(std::memory_order_acquire)!=expected||(slotEpoch&&slotEpoch->load(std::memory_order_acquire)!=expectedSlot);}
};
// The CPU plan pool respects process affinity, with one worker per four
// available logical processors and a ceiling of eight independent plans.
// Small CPUs retain the existing one/two-worker fallback. The native game,
// rendering, resource and audio threads retain scheduling headroom.
inline unsigned PlanningWorkers(unsigned available){
 if(available<4)return 1;
 if(available<8)return 2;
 return available/4<8?available/4:8;
}
// At most eight CPU workers and one latest request/result per race slot. No logger,
// game callback, geometry API, D3D call or native object is owned here.
// Input must own all data (including immutable local geometry); Compute must
// be CPU-only, bounded, and check Cancel between planning stages.
template<class Input,class Output,std::size_t Slots=16> class Worker {
public:
 using Compute=std::function<std::optional<Output>(Input&,const Cancel&)>;
 struct Request {Identity identity;std::uint64_t capturedMs=0;Input input;};
private:
 struct Job {Request request;std::uint64_t sequence=0,epoch=0;};
 struct Result {Identity identity;std::uint64_t capturedMs=0,sequence=0;Output output;};
 std::array<std::optional<Job>,Slots> pending{};
 std::array<std::optional<Result>,Slots> completed{};
 std::array<std::atomic<std::uint64_t>,Slots> sequences{};
 std::mutex mutex;std::condition_variable ready;std::array<std::thread,(Slots<8?Slots:8)> threads;
 std::atomic<std::uint64_t> epoch{1};bool running=false,stopping=false;
 std::size_t cursor=0;Compute compute;
 bool HasPending()const{for(const auto& item:pending)if(item)return true;return false;}
 void Run(){
  for(;;){
   std::optional<Job> job;std::size_t slot=0;
   {std::unique_lock<std::mutex> lock(mutex);ready.wait(lock,[&]{return stopping||HasPending();});if(stopping)return;
    for(std::size_t i=0;i<Slots;++i){slot=(cursor+i)%Slots;if(pending[slot]){job=std::move(pending[slot]);pending[slot].reset();cursor=(slot+1)%Slots;break;}}}
   std::optional<Output> output;
   try{Cancel cancel{&epoch,job->epoch,&sequences[slot],job->sequence};if(!cancel.Requested())output=compute(job->request.input,cancel);}catch(...){/* Failed computation never replaces a valid live plan. */}
   {std::lock_guard<std::mutex> lock(mutex);
    if(output&&!stopping&&job->epoch==epoch.load(std::memory_order_acquire)&&job->sequence==sequences[slot])
     completed[slot]=Result{job->request.identity,job->request.capturedMs,job->sequence,std::move(*output)};}
  }
 }
public:
 Worker()=default;Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
 ~Worker(){Stop();}
 // Explicit opt-in lifecycle, independent of diagnostics ON/OFF.
 bool Start(Compute operation,unsigned workers=1){std::unique_lock<std::mutex> lock(mutex);if(running||!operation||workers<1||workers>threads.size())return false;compute=std::move(operation);stopping=false;running=true;
  try{for(unsigned i=0;i<workers;++i)threads[i]=std::thread([this]{Run();});}
  catch(...){stopping=true;epoch.fetch_add(1,std::memory_order_release);ready.notify_all();lock.unlock();for(auto& thread:threads)if(thread.joinable())thread.join();lock.lock();running=false;compute={};return false;}return true;}
 bool Submit(std::size_t slot,Request request){if(slot>=Slots)return false;std::lock_guard<std::mutex> lock(mutex);if(!running||stopping)return false;
  const auto sequence=++sequences[slot];completed[slot].reset();pending[slot]=Job{std::move(request),sequence,epoch.load(std::memory_order_relaxed)};ready.notify_one();return true;}
 bool Take(std::size_t slot,const Identity& current,std::uint64_t nowMs,std::uint64_t maximumAgeMs,Output& output){
  if(slot>=Slots)return false;std::lock_guard<std::mutex> lock(mutex);auto& value=completed[slot];if(!value)return false;
  const bool valid=value->identity==current&&value->sequence==sequences[slot]&&nowMs>=value->capturedMs&&nowMs-value->capturedMs<=maximumAgeMs;
  if(valid)output=std::move(value->output);value.reset();return valid;
 }
 // Race/leg/world retirement rejects an in-flight result without waiting for
 // it on the game thread. Native data must never be captured by Compute.
 void Invalidate(){std::lock_guard<std::mutex> lock(mutex);epoch.fetch_add(1,std::memory_order_release);for(auto& p:pending)p.reset();for(auto& r:completed)r.reset();}
 void Discard(std::size_t slot){if(slot>=Slots)return;std::lock_guard<std::mutex> lock(mutex);++sequences[slot];pending[slot].reset();completed[slot].reset();}
 // Run on explicit host shutdown outside DllMain/loader lock. Does not detach.
 void Stop(){
  {std::lock_guard<std::mutex> lock(mutex);if(!running)return;stopping=true;epoch.fetch_add(1,std::memory_order_release);for(auto& p:pending)p.reset();for(auto& r:completed)r.reset();ready.notify_all();}
  for(auto& thread:threads)if(thread.joinable())thread.join();
  {std::lock_guard<std::mutex> lock(mutex);running=false;compute={};}
 }
};
}
