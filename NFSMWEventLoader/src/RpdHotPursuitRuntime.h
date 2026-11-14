#pragma once
#include "RpdHotPursuitLedger.h"

namespace rpd_hot_pursuit_runtime {
using namespace rpd_hot_pursuit;
using combat_health_runtime::Read;
Ledger ledger;
Run owner{};
Roster roster{};
std::array<Contact,NativeSlots> contacts{};
struct ArrestLock {
 unsigned car=0,bits=0;
 bool healthProtected=false,parked=false;
 NFSPluginSDK::MW05::UMath::Vector3 position{};
};
std::array<ArrestLock,NativeSlots> locks{};
std::array<bool,NativeSlots> retirement{};
unsigned focus=0,nearest=0,contactTarget=0,contactOfficer=0,lastTarget=0,settled=0,resultBounty=0;
float contactClock=-1.f;
bool wreck=false,result=false;
using Vehicle=NFSPluginSDK::MW05::IVehicle;
using Body=NFSPluginSDK::MW05::IRigidBody;
Body* ArrestBody(unsigned car){
 unsigned body=0,table=0;
 if(!gauntlet_runtime::Table(car,0x8AA828)||!Read(car-0x80+0x4C,body)||!body||!Read(body,table))return nullptr;
 // The same raw position/quaternion getters and setters are available on
 // both native body implementations. Revalidate after a physics mode swap.
 switch(table){
 case 0x8A9818:case 0x8AC390:case 0x8AC548:case 0x8AC880:
 case 0x8ACAF0:case 0x8ACD28:case 0x8ACF50:case 0x8AD178:
  return reinterpret_cast<Body*>(body);
 default:return nullptr;
 }
}
bool Finite(const NFSPluginSDK::MW05::UMath::Vector3& p){
 return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
void Hold(ArrestLock& lock,Body& body){
 const auto vehicle=reinterpret_cast<Vehicle*>(lock.car);
 const auto missing=3u&~unsigned(vehicle->GetForceStop());
 lock.bits|=missing;
 if(missing)vehicle->ForceStopOn(static_cast<Vehicle::ForceStopType>(missing));
 // This is a parked gameplay car. NIS animating mode rebuilds its vehicle
 // mechanics and expects a live scene transform. Keep the normal renderer
 // and orientation; only correct translation if an impact pushes it away.
 const auto p=body.GetPosition();
 if(!Finite(p))return;
 if(std::hypot(p.x-lock.position.x,p.z-lock.position.z)>.18f||std::abs(p.y-lock.position.y)>.45f)
  body.SetPosition(lock.position);
 const NFSPluginSDK::MW05::UMath::Vector3 zero{};
 body.SetLinearVelocity(zero);body.SetAngularVelocity(zero);
}
void ReleaseLocks(){
 for(unsigned slot=1;slot<NativeSlots;++slot){
  auto& lock=locks[slot];
  auto& health=combat_health_runtime::ledger.racers[slot];
  if(lock.healthProtected&&health.handle==roster[slot].handle)health.invincible=false;
  if(lock.car&&gauntlet_runtime::Vehicle(roster[slot].handle)==lock.car){
   if(lock.bits){
    reinterpret_cast<Vehicle*>(lock.car)->ForceStopOff(static_cast<Vehicle::ForceStopType>(lock.bits));
    Log("RPD_HOT_PURSUIT_ARREST_UNLOCK handle=%u owned=%u",roster[slot].handle,lock.bits);
   }
  }
  lock={};
 }
}
void ClearContact(){contacts={};nearest=contactTarget=contactOfficer=0;contactClock=-1.f;}
void Reset(){rpd_sprint_capture_camera::Reset();ReleaseLocks();ledger.Reset();owner={};roster={};ClearContact();retirement={};focus=lastTarget=settled=resultBounty=0;wreck=result=false;}
bool Current(){
 const auto package=event_race_policies::Current();
 return package&&mwel_rpd::PoliceSprint(package->authoring);
}
constexpr unsigned FirstRacer=0x18,RacerStride=0x194;
bool Context(Run& run,unsigned& count){
 unsigned race=0;
 if(!Current()||GetCurrentThreadId()!=combat_health_runtime::gameThread||
    !combat_health_runtime::Racing()||!Read(0x91E000,race)||!race||!Read(race+0x1958,count)||
    count<3||count>NativeSlots)return false;
 run={race,MWELNeoCurrentKey(),combat_health_runtime::generation};
 return bool(run);
}
bool Bind(Run run,unsigned count){
 if(owner==run)return true;
 const auto package=event_race_policies::Current();
 if(!package||package->authoring.at("racerCount")!=count)return false;
 Roster found{};
 for(unsigned slot=0;slot<count;++slot){
  unsigned handle=0,driver=~0u;
  if(!Read(unsigned(run.race)+FirstRacer+slot*RacerStride,handle)||!handle)return false;
  const auto body=gauntlet_runtime::Vehicle(handle);
  if(!body||!Read(body+0x94,driver)||driver!=(slot?3u:0u))return false;
  found[slot]={handle,slot?Role::Suspect:Role::PlayerOfficer};
 }
 Reset();if(!ledger.Begin(run,count,found))return false;
 owner=run;roster=found;Log("RPD_HOT_PURSUIT_ROSTER police=1 suspects=%u after_go=1",count-1);return true;
}
bool Owns(){unsigned race=0;return owner&&Current()&&owner.eventKey==MWELNeoCurrentKey()&&owner.generation==combat_health_runtime::generation&&Read(0x91E000,race)&&race==owner.race;}
Status State(unsigned handle){return handle&&Owns()?ledger.State(owner,handle):Status::NotSuspect;}
bool Retired(unsigned handle){const auto state=State(handle);return state==Status::Arrested||state==Status::Escaped;}
bool NativeSceneOwns(unsigned handle){
 return handle==lastTarget&&!wreck&&(rpd_native_arrest_nis::Owns()||rpd_native_arrest_nis::setupSuspect==handle);
}
bool Locked(unsigned handle){
 if(!handle||!Owns())return false;
 if(NativeSceneOwns(handle))return false;
 return ledger.State(owner,handle)==Status::Arrested;
}
const char* Standing(unsigned handle){
 const auto state=State(handle);
 return state==Status::Arrested?"ARREST":state==Status::Escaped?"ESCAPE":nullptr;
}
struct StandingBinding {StandingBinding(){expanded_leaderboard::policeStatus=Standing;}} standingBinding;
bool LockArrest(unsigned slot,unsigned handle){
 const auto car=gauntlet_runtime::Vehicle(handle);const auto body=ArrestBody(car);if(!body)return false;
 const auto vehicle=reinterpret_cast<Vehicle*>(car);
 const auto position=body->GetPosition();
 if(vehicle->IsAnimating()||!Finite(position))return false;
 auto& lock=locks[slot];
 lock={car};lock.position=position;lock.parked=true;
 Hold(lock,*body);
 auto& health=combat_health_runtime::ledger.racers[slot];
 if(health.occupied&&health.handle==handle&&!health.invincible){health.invincible=true;lock.healthProtected=true;}
 if(worldLogEnabled){
  float p[3]{};memcpy(p,&lock.position,sizeof(p));
  Log("RPD_HOT_PURSUIT_ARREST_LOCK handle=%u owned=%u position=%.3f,%.3f,%.3f native_render=1",handle,lock.bits,p[0],p[1],p[2]);
 }
 return true;
}
void HoldLocks(){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 static ULONGLONG next=0;
 const auto now=worldLogEnabled?GetTickCount64():0;
 const bool audit=now&&now>=next;if(audit)next=now+1000;
#endif
 for(unsigned slot=1;slot<NativeSlots;++slot){
  auto& lock=locks[slot];const auto handle=roster[slot].handle;
  if(ledger.State(owner,slot,handle)!=Status::Arrested)continue;
  if(NativeSceneOwns(handle))continue;
  const auto current=gauntlet_runtime::Vehicle(handle);if(!current)continue;
  if(lock.car!=current){lock.car=current;lock.bits=0;}
  const auto vehicle=reinterpret_cast<Vehicle*>(lock.car);
  if(vehicle->IsAnimating())continue;
  // A wreck keeps its native crash motion/effects, but is never allowed to
  // resume propulsion. Previously only contact arrests had any drive lock.
  if(!lock.parked){
   const auto missing=1u&~unsigned(vehicle->GetForceStop());lock.bits|=missing;
   if(missing)vehicle->ForceStopOn(static_cast<Vehicle::ForceStopType>(missing));
   continue;
  }
  if(vehicle->GetPhysicsMode()==NFSPluginSDK::PhysicsMode::Emulated)vehicle->SetPhysicsMode(NFSPluginSDK::PhysicsMode::Simulated);
  if(auto body=ArrestBody(lock.car)){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
   if(audit){
    combat_impact::Vector p{},anchor{},velocity{};
    memcpy(&p,&body->GetPosition(),sizeof(p));memcpy(&anchor,&lock.position,sizeof(anchor));
    memcpy(&velocity,&body->GetLinearVelocity(),sizeof(velocity));
    const auto delta=p-anchor;
    NFSPluginSDK::MW05::UMath::Matrix4 matrix{};body->GetMatrix4(matrix);
    float m[16]{};memcpy(m,&matrix,sizeof(m));
    const auto length=[&](unsigned i){return std::sqrt(m[i]*m[i]+m[i+1]*m[i+1]+m[i+2]*m[i+2]);};
    Log("EXPERIMENT_RPD_SPRINT_PARKED handle=%u displacement=%.4f speed=%.4f active=%u physics=%u animated=%u basis=%.4f,%.4f,%.4f",
        handle,std::sqrt(combat_impact::Dot(delta,delta)),std::sqrt(combat_impact::Dot(velocity,velocity)),
        unsigned(vehicle->IsActive()),unsigned(vehicle->GetPhysicsMode()),unsigned(vehicle->IsAnimating()),length(0),length(4),length(8));
   }
#endif
   Hold(lock,*body);
  }
 }
}
bool Sample(unsigned handle,combat_impact::Motion& motion){const auto car=gauntlet_runtime::Vehicle(handle);return car&&combat_health_runtime::NativeMotion(car-0x80,motion)&&motion.handle==handle;}
struct LeadProgress {unsigned handle=0;int percent=0;};
LeadProgress Lead(){
 if(!Owns()||result)return {};
 unsigned bestRank=~0u;
 LeadProgress lead{};
 for(unsigned slot=1;slot<NativeSlots;++slot){
  const auto handle=roster[slot].handle;
  if(ledger.State(owner,slot,handle)!=Status::Active)continue;
  const auto info=unsigned(owner.race)+FirstRacer+slot*RacerStride;
  unsigned actual=0,rank=0,outcome=0;BYTE finished=0;float percent=0;
  if(!Read(info,actual)||actual!=handle||!Read(info+0x10,rank)||
     !Read(info+0x1C,outcome)||outcome||!Read(info+0x21,finished)||finished||
     !Read(info+0x18,percent)||!std::isfinite(percent))continue;
  if(!rank||rank>NativeSlots)rank=slot+1;
  if(rank>=bestRank)continue;
  bestRank=rank;
  lead={handle,int(std::clamp(percent,0.f,99.f))}; // 100% belongs to a native finish
 }
 return lead;
}
unsigned Target(unsigned officer){
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!Owns()||!officer)return 0;
 combat_impact::Motion local{};if(!Sample(officer,local))return 0;
 unsigned best=0;float distance=1e12f,current=1e12f;
 for(unsigned slot=1;slot<NativeSlots;++slot){const auto h=roster[slot].handle;combat_impact::Motion m{};
  if(ledger.State(owner,slot,h)!=Status::Active||!Sample(h,m))continue;
  const auto d=m.position-local.position;const auto dist=std::sqrt(combat_impact::Dot(d,d));
  if(h==focus)current=dist;if(dist<distance){distance=dist;best=h;}
 }
 if(current<1e12f&&current<=distance+12.f)return focus;
 return focus=best;
}
float FootprintGap(const combat_impact::Motion& a,const combat_impact::Motion& b){
 using namespace combat_impact;
 auto forward=[](const Motion& m){const auto len=std::hypot(m.forward.x,m.forward.z);return len>.05f?Vector{m.forward.x/len,0,m.forward.z/len}:Vector{};};
 const auto af=forward(a),bf=forward(b);if(Dot(af,af)<.5f||Dot(bf,bf)<.5f)return 100.f;
 const Vector ar{af.z,0,-af.x},br{bf.z,0,-bf.x};const auto delta=a.position-b.position;
 float gap=-100.f;
 for(auto axis:{af,ar,bf,br})gap=std::max(gap,std::abs(Dot(delta,axis))-a.halfLength*std::abs(Dot(af,axis))-a.halfWidth*std::abs(Dot(ar,axis))-b.halfLength*std::abs(Dot(bf,axis))-b.halfWidth*std::abs(Dot(br,axis)));
 return gap;
}
void Observe(Run run,unsigned slot,unsigned handle,bool physicalFinish,bool physicalArrest=false,float required=Contact::Required){
 if(!slot||slot>=NativeSlots||ledger.State(run,slot,handle)!=Status::Active)return;
 const auto info=unsigned(run.race)+FirstRacer+slot*RacerStride;
 unsigned actual=0;BYTE totalled=0;
 if(!Read(info,actual)||actual!=handle||!Read(info+0x1D,totalled))return;
 if(physicalArrest&&!totalled&&!LockArrest(slot,handle))return;
 if(!ledger.Observe(run,slot,handle,{physicalArrest,totalled!=0,physicalFinish}))return;
 if(physicalArrest||totalled)combat_health_runtime::Takedown(totalled?mwcombat::Takedown::RacerWreck:mwcombat::Takedown::RacerArrest,roster[0].handle,handle);
 const auto score=ledger.Result(run);
 if(score.Complete()){
  rpd_sprint_capture_camera::Reset();
  if(physicalArrest||totalled){lastTarget=handle;wreck=totalled!=0;}
  else retirement[slot]=true;
 }else{
  retirement[slot]=true;
  if(physicalArrest||totalled)rpd_sprint_capture_camera::Enqueue(run,roster[0].handle,handle,totalled!=0);
 }
 contacts[slot]={};if(nearest==slot)nearest=0;if(contactTarget==slot)contactTarget=0;
 Log("RPD_HOT_PURSUIT_SUSPECT slot=%u handle=%u status=%u arrested=%u escaped=%u active=%u contact_arrest=%u required_seconds=%.0f wreck=%u final_scene=%u",
     slot,handle,unsigned(ledger.State(run,slot,handle)),score.arrested,score.escaped,score.Active(),unsigned(physicalArrest),required,unsigned(totalled!=0),unsigned(lastTarget==handle));
}
void Retire(unsigned slot){
 const auto handle=roster[slot].handle;const auto car=gauntlet_runtime::Vehicle(handle);
 const auto group=hot_pursuit_runtime::FindPursuit(handle);
 if(group){if(!hot_pursuit_runtime::ReleaseGroundSupport(group))return;
  reinterpret_cast<void(__thiscall*)(unsigned)>(0x4198E0)(group);
  if(hot_pursuit_runtime::FindPursuit(handle))return;
 }
 if(ledger.State(owner,slot,handle)==Status::Escaped&&car&&gauntlet_runtime::Vehicle(handle)==car){
  auto v=reinterpret_cast<Vehicle*>(car);if(v->IsActive())v->Deactivate();
 }
 retirement[slot]=false;
}
bool Result(){return result&&Owns();}
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
void AmbientFaultFixture(){
 static bool attempted=false;if(attempted)return;
 const auto marker=game/L"MWEventLoader.rpd-ambient-test";
 if(!mwel::fs::is_regular_file(marker)||mwel::Read(marker,128)!="MW_EVENT_LOADER_AMBIENT_STALE_DEFINITION_V1\n")return;
 attempted=true;
 reinterpret_cast<void(__cdecl*)()>(0x831029)();
 unsigned prior=0,state=0;Read(0x9147D4,prior);Read(0x9147D8,state);
 auto read=[](unsigned a,void* out,size_t n){return Copy(reinterpret_cast<void*>(a),out,n);};
 if(mwel_csis_handle::ClassDefinition(prior,read)!=mwel_csis_handle::Membership::Live){
  reinterpret_cast<void(__cdecl*)()>(0x83103A)();Log("EXPERIMENT_AMBIENT_FIXTURE_REFUSED live_bank_not_proved=1");return;
 }
 // Exact pointer/generation condition from the reported Main native fault.
 *reinterpret_cast<unsigned*>(0x9147D4)=0xEA777C06;
 std::array<unsigned,5> weather{};
 const auto instantiate=reinterpret_cast<int(__cdecl*)(unsigned,void*,unsigned*)>(0x831A8A);
 const auto first=instantiate(0x9147D4,weather.data()+1,weather.data());
 const auto lookup=first<0?reinterpret_cast<int(__thiscall*)(unsigned,unsigned)>(0x83198B)(0x9147D4,0x8F02A0):0;
 const auto retry=first<0?instantiate(0x9147D4,weather.data()+1,weather.data()):first;
 if(weather[0])reinterpret_cast<void(__thiscall*)(unsigned)>(0x831782)(weather[0]);
 unsigned recovered=0;Read(0x9147D4,recovered);
 reinterpret_cast<void(__cdecl*)()>(0x83103A)();
 Log("EXPERIMENT_AMBIENT_NATIVE_REBIND injected=EA777C06 generation=%08X original=%08X recovered=%08X first=%d lookup=%d instantiate=%d native_instance_released=1",state,prior,recovered,first,lookup,retry);
}
void ContactFixture(unsigned officer){
 static const unsigned mode=[](){const auto marker=game/L"MWEventLoader.rpd-sprint-contact-test";
  if(!mwel::fs::is_regular_file(marker))return 0u;const auto text=mwel::Read(marker,128);
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_CONTACT_V1\n")return 1u;
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_WRECK_V1\n")return 2u;
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_OFFICER_WRECK_V1\n")return 4u;
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_ZONE_V1\n")return 5u;
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_RETIRED_AUDIT_V1\n")return 6u;
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_COMPLETE_RETRY_V1\n")return 7u;
  if(text=="MW_EVENT_LOADER_RPD_SPRINT_MIXED_RETRY_V1\n")return 8u;
  return text=="MW_EVENT_LOADER_RPD_SPRINT_PARKED_V1\n"?3u:0u;}();
 if(!mode)return;
 // Private diagnostic marker only: exercise the real career return path.
 // Other fixtures remain restricted to the synthetic manual race harness.
 if(mode!=4&&mode!=6&&mode!=7&&mode!=8&&(productionRuntime||!race_start_experiment::enabled||!race_start_experiment::manualPlayer))return;
 if((mode==3||mode==5||mode==6)&&ledger.Result(owner).arrested)return;
 static Run epoch{};static float start=0,positioned=0;static unsigned active=0,phase=0;
 float now=0;if(!Read(0x9885D8,now))return;
 if(!(epoch==owner)){epoch=owner;start=now;active=phase=0;}
 if(now-start<6.f)return;
 if(mode==4){
  AmbientFaultFixture();
  auto& hp=combat_health_runtime::ledger.racers[0];const auto& binding=combat_health_runtime::bindings[0];
  if(hp.occupied&&!hp.retired&&hp.handle==officer&&binding.handle==officer&&combat_health_runtime::Native(binding.damage,officer)){
   hp.remaining=0;hp.retired=true;combat_health_runtime::Write(0);
   if(combat_health_runtime::Retire(0))Log("EXPERIMENT_RPD_SPRINT_OFFICER_WRECK handle=%u synthetic_hp=1 native_destruction=1",officer);
  }
  return;
 }
 unsigned target=0,targetSlot=0;for(unsigned slot=1;slot<NativeSlots;++slot){const auto h=roster[slot].handle;if(ledger.State(owner,slot,h)==Status::Active){target=h;targetSlot=slot;break;}}
 combat_impact::Motion police{};if(!target||!Sample(officer,police))return;
 const auto car=gauntlet_runtime::Vehicle(target),cop=gauntlet_runtime::Vehicle(officer);
 if(!car||!cop)return;
 auto body=ArrestBody(car);
 auto ownBody=ArrestBody(cop);
 if(!body||!ownBody)return;
 float separation=2.1f,speed=mode==1?2.f:0.f;
 if(mode==7||mode==8){
  combat_impact::Motion suspect{};
  if(!Sample(target,suspect))return;
  separation=police.halfWidth+suspect.halfWidth+.25f;
  // Lifecycle-only fixture: do not stack successive parked suspects at the
  // same contact point. That overlap launched the officer hundreds of metres
  // above the road and looked like missing scenery. Production parking and
  // recovery logic are unchanged; only this private fixture moves its old cars.
  if(active&&active!=target)for(unsigned slot=1;slot<NativeSlots;++slot){
   auto& lock=locks[slot];
   if(!lock.parked||ledger.State(owner,slot,roster[slot].handle)!=Status::Arrested)continue;
   auto oldBody=ArrestBody(lock.car);if(!oldBody)continue;
   const auto proposed=police.position+police.forward*(25.f+12.f*slot);
   mwel_grid::Layout fitted;
   const float rotation=std::atan2(-police.forward.x,police.forward.z)*57.295779513f;
   if(!MWELFitGrid(owner.eventKey,1,{proposed.x,proposed.y,proposed.z},rotation,fitted))continue;
   const auto& p=fitted.positions[0];
     memcpy(&lock.position,p.data(),12);
   oldBody->SetPosition(lock.position);oldBody->SetLinearVelocity({});oldBody->SetAngularVelocity({});
   Log("EXPERIMENT_RPD_RETRY_FIXTURE_PARK handle=%u checked_road=1 production_parking_changed=0",roster[slot].handle);
  }
 }
 if(mode==5){
  const float t=active==target?now-positioned:0.f;
  const unsigned step=t<.60f?1u:t<1.20f?2u:t<1.80f?3u:4u;
  if(step==2){separation=13.f;speed=11.f;}
  if(step==4){separation=4.4f;speed=5.f;}
  if(step!=phase){
   phase=step;Log("EXPERIMENT_RPD_SPRINT_ZONE phase=%u separation=%.1f speed=%.1f",phase,separation,speed);
   if(step==2||step==3){
    const auto otherHandle=roster[targetSlot==1?2:1].handle;
    const auto other=gauntlet_runtime::Vehicle(otherHandle);unsigned ai=0,otherAI=0;
    if(Read(car+0x54,ai)&&other&&Read(other+0x54,otherAI))
     Log("EXPERIMENT_RPD_SPRINT_RECOVERY_SCOPE phase=%u target=%u target_AI_placement=%u target_vehicle_placement=%u other=%u other_AI_placement=%u other_vehicle_placement=%u",
      phase,target,unsigned(MWELRpdNativePlacementAllowed(reinterpret_cast<void*>(ai))),unsigned(MWELRpdVehiclePlacementAllowed(reinterpret_cast<void*>(car))),
      otherHandle,unsigned(MWELRpdNativePlacementAllowed(reinterpret_cast<void*>(otherAI))),unsigned(MWELRpdVehiclePlacementAllowed(reinterpret_cast<void*>(other))));
   }
  }
 }
 const auto point=police.position+combat_impact::Vector{police.forward.z,0,-police.forward.x}*separation;
 // IRigidBody reads/writes raw physics XYZ. SDK UMath::Vector3 stores
 // y,z,x, so its named constructor permutes that triplet. Copy raw bytes.
 NFSPluginSDK::MW05::UMath::Vector3 position{},zero{};
 memcpy(&position,&point,12);
 if(active!=target){
  NFSPluginSDK::MW05::UMath::Matrix4 matrix{};
  if(mode==7||mode==8)ownBody->GetMatrix4(matrix);else body->GetMatrix4(matrix);
  unsigned table=0,place=0;
  // The EN1.3 IRigidBody vtable has an extra slot before PlaceObject.
  if(Read(unsigned(body),table)&&Read(table+38*4,place)&&(place==0x6A6FB0||place==0x68BAF0))
   reinterpret_cast<void(__thiscall*)(Body*,const void*,const void*)>(place)(body,&matrix,&position);
 }
 const combat_impact::Vector velocity{police.forward.x*speed,0,police.forward.z*speed};
 NFSPluginSDK::MW05::UMath::Vector3 rolling{};memcpy(&rolling,&velocity,12);
 body->SetPosition(position);body->SetLinearVelocity(rolling);body->SetAngularVelocity(zero);
 ownBody->SetLinearVelocity(mode==5?zero:rolling);ownBody->SetAngularVelocity(zero);focus=target;
 if(active!=target){active=target;positioned=now;Log("EXPERIMENT_RPD_SPRINT_CONTACT handle=%u physical_pose_stimulus=1 arrest_flags_written=0 stimulus_mps=%.1f",target,speed);}
 const auto score=ledger.Result(owner);
 if((mode==2&&(!score.arrested||score.Active()==1)||mode==8&&targetSlot%2==0)&&now-positioned>=1.f){
  auto& hp=combat_health_runtime::ledger.racers[targetSlot];const auto& binding=combat_health_runtime::bindings[targetSlot];
  if(hp.occupied&&!hp.retired&&hp.handle==target&&binding.handle==target&&combat_health_runtime::Native(binding.damage,target)){
   hp.remaining=0;hp.retired=true;combat_health_runtime::Write(targetSlot);
   if(combat_health_runtime::Retire(targetSlot))Log("EXPERIMENT_RPD_SPRINT_WRECK handle=%u synthetic_hp=1 native_destruction=1",target);
  }
 }
}
#endif
void Ranking(unsigned race){if(Result()&&race==owner.race)*reinterpret_cast<unsigned*>(race+FirstRacer+0x10)=1;}
void Present(unsigned race,unsigned officer){
 // Delay one tick: a finish or destruction message may still own the car.
 if(!settled){settled=1;return;}
 if(settled==1){
  settled=2;
  if(lastTarget&&!wreck)rpd_native_arrest_nis::Sprint(race,lastTarget);
 }
 if(lastTarget&&rpd_finish_camera::Hold(officer,lastTarget,wreck))return;
 if(!MWELRpdPrepareResult())return;
 auto actor=runtime_lifecycle::FinishActor(nullptr);
 auto finish=race_start_experiment::enabled?race_start_experiment::originalFinish:runtime_lifecycle::originalFinish;
 auto placement=MWELArmsNativePlacement();
 if(!actor||MWELArmsNativeSlot(actor)!=1||!finish||!placement)return;
 result=true;Ranking(race);MWELRpdLabEndAutodrive();
 if(lastTarget&&!wreck&&rpd_native_arrest_nis::Owns())rpd_native_arrest_nis::Victory();
 const auto score=ledger.Result(owner);
 const auto package=event_race_policies::Current();
 const auto bounty=package?std::min<std::uint64_t>(
  std::uint64_t(package->authoring.at("cashValue").get<unsigned>())*score.BountyPercent()/100,0x7FFFFFFF):0;
 resultBounty=unsigned(bounty);
 Log("RPD_HOT_PURSUIT_RESULT arrested=%u suspects=%u escaped=%u medal=%u bounty_percent=%u bounty=%u neutral=1 career_win_written=0",
     score.arrested,score.suspects,score.escaped,unsigned(score.Rank()),score.BountyPercent(),unsigned(bounty));
 finish(actor);
 unsigned current=0;BYTE done=0;
 if(Read(0x91E000,current)&&current==race&&Read(race+FirstRacer+0x21,done)&&done){
  ReleaseLocks();
  placement(nullptr,actor,1);
  const auto identity=profile_progress::CurrentIdentity();
  if(package&&score.Complete()&&!identity.empty())try{
   mwel::ProfileSidecar sidecar(mwel_paths::Profiles(game),identity);
   const bool saved=sidecar.RecordRpdArrests(*package,score.arrested,score.suspects);
   Log("RPD_HOT_PURSUIT_RECORD arrested=%u suspects=%u saved=%u native_result_confirmed=1",score.arrested,score.suspects,unsigned(saved));
  }catch(const std::exception& error){Log("RPD_HOT_PURSUIT_RECORD_FAILED reason=%s",error.what());}
  if(bounty&&!profile_progress::CurrentIdentity().empty()){
   reinterpret_cast<void(__cdecl*)(int)>(0x612220)(int(bounty));
   Log("RPD_HOT_PURSUIT_BOUNTY amount=%u percent=%u once_per_result=1",unsigned(bounty),score.BountyPercent());
  }
 }
}
void Frame(){
 if(Result()||GetCurrentThreadId()!=combat_health_runtime::gameThread)return;
 const bool owns=Owns();if(owns){
  const auto& hp=combat_health_runtime::ledger.racers[0];BYTE totalled=0;
  if((hp.occupied&&hp.handle==roster[0].handle&&hp.retired)||
     (Read(unsigned(owner.race)+FirstRacer+0x1D,totalled)&&totalled)){
   rpd_sprint_capture_camera::Reset();ClearContact();ReleaseLocks();return; // Native crash owns the retry and defeats a simultaneous result.
  }
  HoldLocks();
 }
 if(owns&&ledger.Result(owner).Complete()){
  for(unsigned slot=1;slot<NativeSlots;++slot)if(retirement[slot])Retire(slot);
  unsigned officer=0;if(Read(unsigned(owner.race)+FirstRacer,officer))Present(unsigned(owner.race),officer);return;
 }
 Run run{};unsigned count=0;if(!Context(run,count)||!Bind(run,count))return;
 unsigned officer=0;if(!Read(unsigned(run.race)+FirstRacer,officer))return;
 for(unsigned slot=1;slot<count;++slot){
  if(retirement[slot])Retire(slot);
  Observe(run,slot,roster[slot].handle,false);
 }
 if(ledger.Result(run).Complete())return;
 if(reinterpret_cast<bool(__cdecl*)()>(0x468390)())return;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 ContactFixture(officer);
#endif
 combat_impact::Motion police{};float clock=0;
 if(!Sample(officer,police)||!Read(0x9885D8,clock)||!std::isfinite(clock)){ClearContact();return;}
 float dt=clock-contactClock;
 if(contactOfficer!=officer||dt<0||dt>.35f){ClearContact();dt=0;}
 contactOfficer=officer;contactClock=clock;
 const auto own=gauntlet_runtime::Vehicle(officer);
 const auto& playerHealth=combat_health_runtime::ledger.racers[0];
 const bool canArrest=own&&!MWELRpdNoAllyDefeat()&&!reinterpret_cast<Vehicle*>(own)->IsDestroyed()&&
    (!playerHealth.occupied||playerHealth.handle!=officer||playerHealth.remaining>0);
 using combat_impact::Dot;
 const auto pursuitTarget=Target(officer);
 if(pursuitTarget)rpd_finish_camera::Remember(unsigned(run.race),run.eventKey,run.generation,pursuitTarget);
 const auto policeSpeed=std::sqrt(Dot(police.velocity,police.velocity));
 unsigned candidate=0;float bestDistance=1e12f;
 struct ContactSample {bool enter=false,within=false,sameRoad=false;};
 std::array<ContactSample,NativeSlots> samples{};
 for(unsigned slot=1;slot<count;++slot){const auto h=roster[slot].handle;combat_impact::Motion racer{};
  if(!canArrest||ledger.State(run,slot,h)!=Status::Active||!Sample(h,racer))continue;
  const auto delta=racer.position-police.position;const float distance=Dot(delta,delta);
  if(distance>24.f*24.f)continue;
  const auto gap=FootprintGap(police,racer),height=std::abs(delta.y),speed=std::sqrt(Dot(racer.velocity,racer.velocity));
  if(!std::isfinite(speed)||!std::isfinite(gap)||height>1.75f)continue;
  auto& sample=samples[slot];sample={Contact::Eligible(gap,height,policeSpeed,speed),Contact::Within(gap,speed),true};
  if(sample.enter&&distance<bestDistance){candidate=slot;bestDistance=distance;}
 }
 if(!contactTarget&&candidate)contactTarget=candidate;
 for(unsigned slot=1;slot<count;++slot){
  auto& contact=contacts[slot];const auto& sample=samples[slot];const bool tracked=slot==contactTarget;
  const bool wasLatched=contact.latched;
  if(contact.Advance(dt,tracked&&sample.enter,tracked&&sample.within,tracked&&sample.sameRoad)){
   Observe(run,slot,roster[slot].handle,false,true);
  }
  if(tracked){
   if(contact.latched||contact.alpha>0)nearest=slot;
   if(!contact.latched)contactTarget=0;
   if(wasLatched!=contact.latched)Log("RPD_HOT_PURSUIT_CONTACT slot=%u handle=%u active=%u elapsed=%.2f",
     slot,roster[slot].handle,unsigned(contact.latched),contact.elapsed);
  }
 }
 if(!contactTarget&&nearest&&!contacts[nearest].alpha)nearest=0;
}
unsigned Gauge(unsigned handle,float* fill,float* alpha){
 if(!handle||!fill||!alpha||!nearest||roster[nearest].handle!=handle||!Owns())return 0;
 const auto& gauge=contacts[nearest];if(gauge.alpha<=0)return 0;
 *fill=gauge.elapsed/Contact::Required;*alpha=gauge.alpha;return 1;
}
void Finish(unsigned race,unsigned key,unsigned slot,unsigned handle){
 Run run{};unsigned count=0;
 if(Context(run,count)&&run.race==race&&run.eventKey==key&&slot&&slot<count&&Bind(run,count))
  Observe(run,slot,handle,true);
}
}

bool MWELRpdHotPursuitScope(){return rpd_hot_pursuit_runtime::Current();}
unsigned MWELRpdActiveSuspects(){
 using namespace rpd_hot_pursuit_runtime;
 return Owns()?ledger.Result(owner).Active():~0u;
}
bool MWELRpdArrestProgress(unsigned* arrested,unsigned* initial){
 using namespace rpd_hot_pursuit_runtime;
 if(arrested)*arrested=0;if(initial)*initial=0;
 if(!Owns())return false;
 const auto score=ledger.Result(owner);
 if(arrested)*arrested=score.arrested;if(initial)*initial=score.suspects;return true;
}
bool MWELRpdSprintRetired(unsigned handle){return rpd_hot_pursuit_runtime::Retired(handle);}
bool MWELRpdSprintArresting(unsigned handle){
 using namespace rpd_hot_pursuit_runtime;
 return handle&&contactTarget&&Owns()&&roster[contactTarget].handle==handle&&contacts[contactTarget].latched;
}
#pragma comment(linker,"/EXPORT:MWEL_RpdArrestGaugeV1=_MWEL_RpdArrestGaugeV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdArrestGaugeV1(unsigned handle,float* fill,float* alpha){
 return rpd_hot_pursuit_runtime::Gauge(handle,fill,alpha);
}
unsigned MWELRpdSprintTarget(unsigned officer){return rpd_hot_pursuit_runtime::Target(officer);}
bool MWELRpdSprintResult(){return rpd_hot_pursuit_runtime::Result();}
void MWELRpdHotPursuitGoal(unsigned race,unsigned key,unsigned slot,unsigned handle){
 rpd_hot_pursuit_runtime::Finish(race,key,slot,handle);
}
