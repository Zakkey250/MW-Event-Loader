#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>

namespace rpd_hot_pursuit {
// One police player occupies native race slot 0. Police reinforcements are
// outside the race roster; only verified racer bodies may be bound here.
constexpr unsigned NativeSlots=9;
struct Contact {
 static constexpr float Required=3.f;
 static constexpr float MaximumSpeed=10.f/3.6f;
 static constexpr float EscapeSpeed=30.f/3.6f;
 static constexpr float CloseGap=4.f, MaximumGap=8.f, EscapeGrace=.25f;
 float elapsed=0,alpha=0,outside=0;
 bool latched=false;
 static bool Eligible(float gap,float height,float copSpeed,float suspectSpeed){return gap<=.65f&&height<=1.5f&&copSpeed<=MaximumSpeed&&suspectSpeed<=MaximumSpeed;}
 static bool Within(float gap,float suspectSpeed){return gap<=MaximumGap&&(gap<=CloseGap||suspectSpeed<EscapeSpeed);}
 bool Advance(float dt,bool enter,bool within,bool sameRoad,float required=Required){
  if(!std::isfinite(dt)||dt<0||!std::isfinite(required)||required<=0)return false;
  if(enter&&sameRoad)latched=true;
  const bool holding=latched&&sameRoad&&within;
  float decay=dt;
  if(holding){outside=0;elapsed=std::min(elapsed+dt,required);}
  else if(latched&&sameRoad){
   decay=std::max(0.f,dt-(EscapeGrace-outside));
   outside+=dt;
   if(outside>=EscapeGrace){latched=false;outside=0;}
  }else{latched=false;outside=0;}
  if(!holding)elapsed=std::max(0.f,elapsed-2.f*decay);
  alpha=std::clamp(alpha+(elapsed>0?5.f*dt:-5.f*dt),0.f,1.f);
  return holding&&elapsed>=required;
 }
};
struct Run {
 std::uintptr_t race=0;
 unsigned eventKey=0,generation=0;
 explicit operator bool()const{return race&&eventKey&&generation;}
 bool operator==(const Run& b)const{return race==b.race&&eventKey==b.eventKey&&generation==b.generation;}
};
enum class Role {Missing,PlayerOfficer,Suspect};
struct Entrant {unsigned handle=0;Role role=Role::Missing;};
using Roster=std::array<Entrant,NativeSlots>;
enum class Status {NotSuspect,Active,Arrested,Escaped};
enum class Medal {None,Bronze,Silver,Gold};
struct Evidence {
 bool nativeArrest=false;
 bool nativeTotalled=false;
 // Set only from the genuine native sprint finish observer, with the
 // physical body at the authored gate. Cooldown and DNF are not finishes.
 bool physicalFinish=false;
};
struct Evaluation {
 unsigned suspects=0,arrested=0,escaped=0;
 unsigned Active()const{return suspects-arrested-escaped;}
 bool Complete()const{return suspects>=2&&!Active();}
 Medal Rank()const{
  if(!suspects||!arrested)return Medal::None;
  if(8*arrested>=7*suspects)return Medal::Gold;
  if(8*arrested>=6*suspects)return Medal::Silver;
  return 2*arrested>=suspects?Medal::Bronze:Medal::None;
 }
 unsigned BountyPercent()const{
  if(suspects&&arrested==suspects)return 120;
  constexpr unsigned payout[]={0,40,80,100};
  return payout[unsigned(Rank())];
 }
};

class Ledger {
 Run run_{};
 unsigned count_=0;
 std::array<unsigned,NativeSlots> handles_{};
 std::array<Status,NativeSlots> status_{};
public:
 void Reset(){*this=Ledger{};}
 bool Begin(Run run,unsigned count,const Roster& roster){
  if(!run||count<3||count>NativeSlots)return false;
  for(unsigned slot=0;slot<count;++slot){
   if(!roster[slot].handle||roster[slot].role!=(slot?Role::Suspect:Role::PlayerOfficer))return false;
   for(unsigned prior=0;prior<slot;++prior)if(roster[prior].handle==roster[slot].handle)return false;
  }
  if(run==run_){
   if(count!=count_)return false;
   for(unsigned slot=0;slot<count;++slot)if(handles_[slot]!=roster[slot].handle)return false;
   return true;
  }
  Reset();run_=run;count_=count;
  for(unsigned slot=0;slot<count;++slot){
   handles_[slot]=roster[slot].handle;
   if(slot)status_[slot]=Status::Active;
  }
  return true;
 }
 Status State(Run run,unsigned slot,unsigned handle)const{
  return run==run_&&slot&&slot<count_&&handle&&handles_[slot]==handle?status_[slot]:Status::NotSuspect;
 }
 Status State(Run run,unsigned handle)const{
  for(unsigned slot=1;slot<count_;++slot)if(handles_[slot]==handle)return State(run,slot,handle);
  return Status::NotSuspect;
 }
 bool Observe(Run run,unsigned slot,unsigned handle,Evidence evidence){
  if(State(run,slot,handle)!=Status::Active)return false;
  // If native arrest/destruction already committed when the finish callback
  // arrives, count the arrest. Subsequent evidence cannot count the car again.
  if(evidence.nativeArrest||evidence.nativeTotalled)status_[slot]=Status::Arrested;
  else if(evidence.physicalFinish)status_[slot]=Status::Escaped;
  else return false;
  return true;
 }
 Evaluation Result(Run run)const{
  if(!(run==run_)||!count_)return {};
  Evaluation result{count_-1};
  for(unsigned slot=1;slot<count_;++slot){
   if(status_[slot]==Status::Arrested)++result.arrested;
   if(status_[slot]==Status::Escaped)++result.escaped;
  }
  return result;
 }
};
}
