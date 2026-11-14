#pragma once
#include "HotPursuitArrestPolicy.h"
namespace hot_pursuit_arrest {
using namespace gauntlet_runtime;
using rpd_hot_pursuit::Contact;
using rpd_hot_pursuit::Run;
Run owner{};
struct Entry {unsigned handle=0,officer=0;Countdown timer;bool retired=false,releaseGroup=false;rpd_hot_pursuit_runtime::ArrestLock lock;};
std::array<Entry,16> entries{};
float clock=-1.f,next=0;
bool Scope(){
 const auto p=event_race_policies::Current();
 return hot_pursuit_runtime::Active()&&p&&(!mwel_rpd::CaseFile(p->authoring)||mwel_rpd::PoliceSprint(p->authoring));
}
bool Owns(){unsigned race=0;return owner&&owner.eventKey==MWELNeoCurrentKey()&&owner.generation==combat_health_runtime::generation&&
 Read(0x91E000,race)&&race==owner.race&&Scope();}
void Reset(){
 unsigned race=0;
 const bool same=owner&&Read(0x91E000,race)&&race==owner.race&&owner.eventKey==MWELNeoCurrentKey()&&owner.generation==combat_health_runtime::generation;
 for(unsigned slot=1;slot<entries.size();++slot){auto& e=entries[slot];if(!same||!e.handle)continue;
  auto& hp=combat_health_runtime::ledger.racers[slot];
  if(e.lock.healthProtected&&hp.handle==e.handle)hp.invincible=false;
  if(e.lock.car&&Vehicle(e.handle)==e.lock.car&&e.lock.bits)
   reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(e.lock.car)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(e.lock.bits));
 }
 entries={};owner={};clock=-1;next=0;
}
bool Arresting(unsigned handle){if(!handle||!Owns())return false;for(const auto& e:entries)if(e.handle==handle)return !e.retired&&e.timer.RecoveryHeld();return false;}
bool Retired(unsigned handle){if(!handle||!Owns())return false;for(const auto& e:entries)if(e.handle==handle)return e.retired;return false;}
bool Sample(unsigned handle,combat_impact::Motion& m){const auto car=Vehicle(handle);return car&&combat_health_runtime::NativeMotion(car-0x80,m)&&m.handle==handle;}
bool NormalArrest(unsigned slot,Entry& e){
 const auto car=Vehicle(e.handle);auto body=rpd_hot_pursuit_runtime::ArrestBody(car);
 const auto v=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car);
 if(!body||v->IsAnimating()||!rpd_hot_pursuit_runtime::Finite(body->GetPosition()))return false;
 // GRacerInfo::OnBusted stops its own timers and reranks the native field.
 // This is an AI racer only: no player arrest script, scene or result menu.
 const auto info=unsigned(owner.race)+0x18+slot*0x194;unsigned actual=0,flags=0;BYTE finished=0,busted=0;
 if(!slot||!Read(info,actual)||actual!=e.handle||!Read(info+0x1C,flags)||!Read(info+0x1F,busted)||!Read(info+0x21,finished)||finished)return false;
 // Native pursuit may commit OnBusted before our eight/five-second observer.
 // Adopt that genuine result as well, otherwise NeoAI keeps driving it.
 if(!busted){
  const BYTE signature[]={0x56,0x8B,0xF1,0x8A,0x46,0x21,0x84,0xC0};
  if(flags||memcmp(reinterpret_cast<void*>(0x600370),signature,sizeof(signature)))return false;
 }
 e.lock={car};e.lock.position=body->GetPosition();e.lock.parked=true;
 const bool nativeAlreadyBusted=busted!=0;
 if(!busted)reinterpret_cast<void(__thiscall*)(unsigned)>(0x600370)(info);
 if(!Read(info+0x1F,busted)||!busted){e.lock={};return false;}
 e.retired=true;e.releaseGroup=true;
 rpd_hot_pursuit_runtime::Hold(e.lock,*body);
 auto& hp=combat_health_runtime::ledger.racers[slot];
 if(hp.occupied&&hp.handle==e.handle&&!hp.invincible){hp.invincible=true;e.lock.healthProtected=true;}
 combat_health_runtime::Takedown(mwcombat::Takedown::RacerArrest,e.officer,e.handle);
 if(nativeAlreadyBusted)Log("HOT_PURSUIT_NATIVE_ARREST_ADOPTED target=%u slot=%u parked=1 human_excluded=1",e.handle,slot);
 return true;
}
void HoldRetired(){
 for(auto& e:entries)if(e.retired){
  if(e.releaseGroup){const auto group=hot_pursuit_runtime::FindPursuit(e.handle);
   if(!group)e.releaseGroup=false;
   else if(hot_pursuit_runtime::ReleaseGroundSupport(group)){reinterpret_cast<void(__thiscall*)(unsigned)>(0x4198E0)(group);e.releaseGroup=false;}
  }
  if(e.lock.car&&Vehicle(e.handle)==e.lock.car){
   auto v=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(e.lock.car);
   if(!v->IsAnimating())if(auto body=rpd_hot_pursuit_runtime::ArrestBody(e.lock.car))rpd_hot_pursuit_runtime::Hold(e.lock,*body);
  }
 }
}
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
template<class Cops>void Fixture(const Cops& cops,unsigned count){
 static const bool enabled=[](){const auto p=game/L"MWEventLoader.ai-arrest-test";
  return mwel::fs::is_regular_file(p)&&mwel::Read(p,128)=="MW_EVENT_LOADER_AI_ARREST_V1\n";}();
 if(!enabled)return;
 static Run run{};static unsigned target=0,cop=0;static float since=0,nextAudit=0;static bool done=false,nativeDone=false;
 static combat_impact::Motion anchor{};
 if(!(run==owner)){run=owner;target=cop=0;done=nativeDone=false;}
 if(done){
  // Separate callback integration proof: a genuine native OnBusted result
  // must be adopted even without completing our auxiliary countdown. Do not
  // write result flags, HP, poses or race-end state for this second actor.
  if(!nativeDone&&!MWELRpdHotPursuitScope())for(unsigned slot=1;slot<count;++slot){
   const auto info=unsigned(owner.race)+0x18+slot*0x194;unsigned h=0,flags=0,driver=0;BYTE finished=0;
   if(!Read(info,h)||h==target||!Read(info+0x1C,flags)||flags||!Read(info+0x21,finished)||finished)continue;
   const auto car=Vehicle(h);if(!car||!Read(car+0x94,driver)||driver!=3)continue;
   const BYTE signature[]={0x56,0x8B,0xF1,0x8A,0x46,0x21,0x84,0xC0};
   if(memcmp(reinterpret_cast<void*>(0x600370),signature,sizeof(signature)))return;
   reinterpret_cast<void(__thiscall*)(unsigned)>(0x600370)(info);nativeDone=true;
   Log("EXPERIMENT_NATIVE_ONBUSTED target=%u native_callback=1 result_flag_writes=0 physical_pose_unchanged=1",h);break;
  }
  return;
 }
 unsigned local=0;Read(unsigned(owner.race)+0x18,local);combat_impact::Motion player;
 if(!Sample(local,player))return;
 if(!target){
  for(const auto& c:cops){unsigned ai=0,pursuit=0;const auto car=Vehicle(c.handle);
   if(!car||!Read(car+0x54,ai)||!Read(ai+0x70,pursuit))continue;
   const auto h=hot_pursuit_runtime::TargetHandle(pursuit);const auto delta=c.motion.position-player.position;
   if(!h||h==local||std::hypot(delta.x,delta.z)<80||MWELRpdSprintRetired(h))continue;
   unsigned slot=count;for(unsigned i=1;i<count;++i){unsigned entry=0;Read(unsigned(owner.race)+0x18+i*0x194,entry);if(entry==h)slot=i;}
   if(slot==count||entries[slot].retired)continue;
   target=h;cop=c.handle;anchor=c.motion;Read(0x9885D8,since);nextAudit=since+1;break;
  }
 }
 if(!target)return;
 if(MWELRpdSprintRetired(target)||Retired(target)){done=true;Log("EXPERIMENT_AI_ARREST_COMPLETE target=%u cop=%u physical_contact_only=1",target,cop);return;}
 const auto targetCar=Vehicle(target),copCar=Vehicle(cop);
 auto suspect=rpd_hot_pursuit_runtime::ArrestBody(targetCar),officer=rpd_hot_pursuit_runtime::ArrestBody(copCar);
 if(!suspect||!officer){done=true;return;}
 const auto p=anchor.position+combat_impact::Vector{anchor.forward.z,0,-anchor.forward.x}*2.2f;
 const NFSPluginSDK::MW05::UMath::Vector3 a{anchor.position.z,anchor.position.x,anchor.position.y},b{p.z,p.x,p.y},zero{};
 officer->SetPosition(a);officer->SetLinearVelocity(zero);officer->SetAngularVelocity(zero);
 suspect->SetPosition(b);suspect->SetLinearVelocity(zero);suspect->SetAngularVelocity(zero);
 float now=0;Read(0x9885D8,now);
 if(now>=nextAudit){nextAudit=now+1;unsigned ai=0;Read(targetCar+0x54,ai);
  Log("EXPERIMENT_AI_ARREST_CONTACT target=%u cop=%u age=%.2f AI_placement=%u vehicle_placement=%u recovery_held=%u physical_only=1",target,cop,now-since,
      unsigned(MWELRpdNativePlacementAllowed(reinterpret_cast<void*>(ai))),unsigned(MWELRpdVehiclePlacementAllowed(reinterpret_cast<void*>(targetCar))),unsigned(Arresting(target)));
 }
}
#endif
void Frame(){
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread)return;
 if(!Scope()){if(owner)Reset();return;}
 unsigned race=0,count=0;Read(0x91E000,race);Read(race+0x1958,count);if(!race||count<2||count>entries.size())return;
 const Run run{race,MWELNeoCurrentKey(),combat_health_runtime::generation};
 if(!(run==owner)){Reset();owner=run;}
 if(!combat_health_runtime::Racing())return;
 HoldRetired();
 if(reinterpret_cast<bool(__cdecl*)()>(0x468390)())return;
 float now=0;if(!Read(0x9885D8,now)||!std::isfinite(now))return;
 if(clock>=0&&now<next)return;
 float dt=clock>=0?now-clock:0;if(dt<0||dt>.35f)dt=0;clock=now;next=now+.1f;
 const bool police=MWELRpdHotPursuitScope();
 unsigned player=0;Read(race+0x18,player);combat_impact::Motion local{};
 const bool localReady=police&&Sample(player,local);
 struct Cop {unsigned handle=0;combat_impact::Motion motion;};
 std::vector<unsigned> cars;std::vector<Cop> cops;if(!List(cars))return;
 for(const auto car:cars){unsigned driver=0,ai=0;
  if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->IsDestroyed())continue;
  Cop c;c.handle=Handle(car);if(combat_health_runtime::NativeMotion(car-0x80,c.motion))cops.push_back(c);
 }
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 Fixture(cops,count);
#endif
 for(unsigned slot=1;slot<count;++slot){unsigned handle=0,flags=0,driver=0;BYTE finished=0,busted=0;
  Read(race+0x18+slot*0x194,handle);auto& e=entries[slot];if(e.handle!=handle)e={handle};
  const auto car=Vehicle(handle);combat_impact::Motion target;
  if(!police&&!e.retired&&car&&Read(car+0x94,driver)&&driver==3&&Read(race+0x18+slot*0x194+0x1F,busted)&&busted){
   if(NormalArrest(slot,e))e.timer={};
  }
  if(e.retired||!car||!Read(car+0x94,driver)||driver!=3||!Read(race+0x18+slot*0x194+0x1C,flags)||flags||
     !Read(race+0x18+slot*0x194+0x21,finished)||finished||MWELRpdSprintRetired(handle)||!Sample(handle,target)){e.timer={};continue;}
  bool enter=false,within=false,sameRoad=false;unsigned officer=0;float best=1e12f;
  const float speed=std::sqrt(combat_impact::Dot(target.velocity,target.velocity));
  for(const auto& c:cops){const auto delta=target.position-c.motion.position;const auto distance=combat_impact::Dot(delta,delta);
   if(distance>24.f*24.f||std::abs(delta.y)>1.75f)continue;
   const auto gap=rpd_hot_pursuit_runtime::FootprintGap(c.motion,target);
   const bool starts=Contact::Eligible(gap,std::abs(delta.y),std::sqrt(combat_impact::Dot(c.motion.velocity,c.motion.velocity)),speed);
   const bool holds=Contact::Within(gap,speed);
   if((starts&&!enter)||(starts==enter&&holds&&distance<best)){enter=starts;within=holds;sameRoad=true;officer=c.handle;best=distance;}
  }
  const auto delta=target.position-local.position;
  const bool priority=localReady&&PlayerPriority(std::hypot(delta.x,delta.z),delta.y);
  const bool before=e.timer.RecoveryHeld();
  const bool complete=e.timer.Advance(dt,enter,within,sameRoad,priority,police);e.officer=officer;
  if(before!=e.timer.RecoveryHeld())Log("HOT_PURSUIT_AI_ARREST_CONTACT target=%u cop=%u active=%u required=%.0f player_priority=%u elapsed=%.2f recovery_suppressed=%u",handle,officer,unsigned(e.timer.RecoveryHeld()),Required(police),unsigned(priority),e.timer.contact.elapsed,unsigned(e.timer.RecoveryHeld()));
  if(!complete)continue;
  if(police)rpd_hot_pursuit_runtime::Observe(rpd_hot_pursuit_runtime::owner,slot,handle,false,true,Required(true));
  else NormalArrest(slot,e);
  const bool accepted=police?MWELRpdSprintRetired(handle):e.retired;
  if(accepted){Log("HOT_PURSUIT_AI_ARREST target=%u cop=%u required=%.0f elapsed=%.2f police_player=%u human_excluded=1",handle,officer,Required(police),e.timer.contact.elapsed,unsigned(police));e.timer={};}
 }
}
}
bool MWELHotPursuitAIArresting(unsigned handle){return hot_pursuit_arrest::Arresting(handle);}
bool MWELHotPursuitNormalRetired(unsigned handle){return hot_pursuit_arrest::Retired(handle);}
void MWELHotPursuitArrestReset(){hot_pursuit_arrest::Reset();}
