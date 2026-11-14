#pragma once
#include "HotPursuitRoadblock.h"
#include "PoliceSupportRuntime.h"
#include "PoliceHelicopterNative.h"
#include "HotPursuitRadar.h"
namespace hot_pursuit_runtime {
using namespace gauntlet_runtime;
bool UnsupportedSUV(unsigned car){return SUV(car)&&!MWELRpdFleetAllowsVehicle(reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->GetVehicleKey());}
// Verified constructor 0043CA00 stores the native patrol limit at +90.
// The SDK's CopMgr layout is four bytes larger here; never use its offsetof.
constexpr unsigned PatrolOffset=0x90;
unsigned Perpetrator(unsigned car){
 unsigned com=0,table=0,get=0,set=0;if(!Read(car-0x80+4,com)||!com)return 0;
 const auto perp=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x4037E0);
 return Read(perp,table)&&Read(table+4,get)&&get==0x409400&&Read(table+8,set)&&set==0x409060?perp:0;
}
void Reset(){
 if(draining)return;
 draining=true;MWELHotPursuitArrestReset();ResetSupport();ReleaseGroups();
 gauntlet_runtime::RestoreDeployment(police_deployment::Owner::HotPursuit);replacements={};
 const bool same=MWELNeoCurrentKey()==0||MWELNeoCurrentKey()==owner;
 for(const auto& p:perps)if(same&&p.component){const auto car=Vehicle(p.handle);float heat=0;unsigned driver=0;
  if(car&&Read(car+0x94,driver)&&driver==3&&Perpetrator(car)==p.component&&Read(p.component+0x1C,heat)&&std::abs(heat-p.written)<.001f)
   reinterpret_cast<void(__thiscall*)(unsigned,float)>(0x409060)(p.component,p.original);
 }
 intercept={};detections={};lost={};assignments={};spawnActivations={};createdPursuits={};createdTargets={};nextPursuitCreate={};eligibleHandles={};playerPursuitStarted=false;patrolBootstrapDone=false;ownershipRedirects=steeringRedirects=0;nextRetainLog=0;perps={};owner=0;next=nextRequest=nextLog=0;requested=roadReady=false;model.clear();draining=false;
}
unsigned TargetHandle(unsigned pursuit){
 unsigned target=0,sim=0,handle=0;
 if(!Table(pursuit,0x892770))return 0;
 target=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(0x40AE70)(pursuit);
 return Read(target+0x1C,sim)&&Read(sim+8,handle)?handle:0;
}
unsigned TargetQuota(unsigned rank,unsigned count,unsigned handle){
 const auto policy=event_race_policies::Current();
 const bool policeSprint=policy&&mwel_rpd::PoliceSprint(policy->authoring);
 if(policy&&mwel_rpd::Armed(policy->authoring)&&!policeSprint)return mwel_rpd::Allies(policy->authoring);
 unsigned manager=0,capacity=0;
 if(!Read(0x92C67C,manager)||!Table(manager,0x891900)||!Read(manager-0x54+0x98,capacity)||!capacity||capacity>64)return 0;
 unsigned race=0,survivors=0,survivorRank=1;
 if(!Read(0x91E000,race)||!rank||rank>count||count>16)return 0;
 unsigned targetSlot=count;
 if(policeSprint)for(unsigned slot=1;slot<count;++slot){unsigned other=0;if(Read(race+0x18+slot*0x194,other)&&other==handle){targetSlot=slot;break;}}
 if(policeSprint&&targetSlot==count)return 0;
 for(unsigned slot=policeSprint?1u:0u;slot<count;++slot){unsigned flags=0,otherRank=0,other=0;BYTE finished=0;
  if(!Read(race+0x18+slot*0x194+0x1C,flags)||!Read(race+0x18+slot*0x194+0x21,finished))return 0;
  if(flags||(policeSprint&&(!Read(race+0x18+slot*0x194,other)||MWELRpdSprintRetired(other)||finished)))continue;
  Read(race+0x18+slot*0x194+0x10,otherRank);if(!otherRank||otherRank>count)otherRank=slot+1;
  ++survivors;if(otherRank<rank||(policeSprint&&otherRank==rank&&slot<targetSlot))++survivorRank;
 }
 // Retirement redistributes the existing budget; an unfinished racer below
 // the 25-percent gate keeps its reservation and never triggers early pursuit.
 return hot_pursuit::Quota(std::min(survivorRank,survivors),survivors,policeSprint?std::min(capacity,mwel_rpd::Allies(policy->authoring)):capacity,policeSprint?count-1:count);
}
unsigned Slot(unsigned handle,unsigned& race,unsigned& count){
 if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count>16)return 16;
 for(unsigned i=0;i<count;++i){unsigned h=0;if(Read(race+0x18+i*0x194,h)&&h==handle)return i;}return 16;
}
bool RunningTarget(unsigned handle){
 if(MWELRpdHotPursuitScope()&&MWELRpdSprintRetired(handle))return false;
 unsigned race=0,count=0,flags=0;BYTE finished=0;const auto slot=Slot(handle,race,count);
 return handle&&slot<count&&Read(race+0x18+slot*0x194+0x1C,flags)&&!flags&&Read(race+0x18+slot*0x194+0x21,finished)&&(!finished||MWELHotPursuitEscaping(handle))&&Vehicle(handle);
}
bool EndedTarget(unsigned handle){
 if(MWELRpdHotPursuitScope()&&MWELRpdSprintRetired(handle))return true;
 unsigned race=0,count=0,flags=0;BYTE finished=0;const auto slot=Slot(handle,race,count);
 return handle&&slot<count&&Read(race+0x18+slot*0x194+0x1C,flags)&&Read(race+0x18+slot*0x194+0x21,finished)&&(flags||(finished&&!MWELHotPursuitEscaping(handle)));
}
bool ReadyTarget(unsigned handle){
 if(!RunningTarget(handle))return false;
 return std::find(eligibleHandles.begin(),eligibleHandles.end(),handle)!=eligibleHandles.end();
}
void UpdateEligibility(unsigned race,unsigned count){
 const auto policy=event_race_policies::Current();const bool officer=policy&&mwel_rpd::Armed(policy->authoring);
 for(unsigned slot=0;slot<count;++slot){unsigned handle=0;float percent=0;
  if(officer&&slot==0){eligibleHandles[slot]=0;continue;}
  if(!Read(race+0x18+slot*0x194,handle)||!RunningTarget(handle)||eligibleHandles[slot]==handle)continue;
  if(officer||(Read(race+0x18+slot*0x194+0x18,percent)&&hot_pursuit::StartEligible(percent))){
   eligibleHandles[slot]=handle;Log("HOT_PURSUIT_RACER_ELIGIBLE slot=%u handle=%u race_percent=%.3f threshold=25 strict_greater=1",slot,handle,percent);
  }
 }
}
bool RpdDispatchOwned(){
 const auto p=event_race_policies::Current();
 // Every police mission now uses target-relative reinforcement requests,
 // including a vanilla interceptor without LB/fleet expansion. Its first
 // model must come from the heat roster, not from an ambient bootstrap cop.
 return p&&mwel_rpd::Armed(p->authoring);
}
const rpd_vehicle_catalog::Entry* RpdDispatch(){
 unsigned race=0,handle=0;
 if(!RpdDispatchOwned()||!Read(0x91E000,race)||!Read(race+0x18,handle))return nullptr;
 const auto officer=Vehicle(handle);
 if(!officer)return nullptr;const auto heat=Heat(officer);
 if(rpd_vehicle_catalog::FleetEnabled(heat))return rpd_vehicle_catalog::fleets[heat].models.front();
 return rpd_vehicle_catalog::DispatchVehicle(heat);
}
#include "HotPursuitOwnership.inl"
unsigned SpeechTargets(bool additional){
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!Active()||!combat_health_runtime::Racing())return ~0u;
 unsigned race=0,count=0;
 if(!Read(0x91E000,race)||!race||!Read(race+0x1958,count)||!count||count>16)return ~0u;
 unsigned tracked=0;
 const bool officer=MWELRpdHotPursuitScope();
 for(unsigned slot=officer||additional?1u:0u;slot<count;++slot){
  unsigned handle=0;BYTE busted=0,bailed=0;
  if(!Read(race+0x18+slot*0x194,handle)||!RunningTarget(handle))continue;
  const auto group=FindPursuit(handle);
  if(group&&Read(group+0xE8,busted)&&!busted&&Read(group+0xE9,bailed)&&!bailed)++tracked;
 }
 return officer&&additional&&tracked?tracked-1:tracked;
}
unsigned CopTarget(unsigned handle){
 const auto car=Vehicle(handle);unsigned target=0,ai=0,pursuit=0;
 if(auto a=AssignmentFor(car))target=a->target;
 if(!target&&car&&Read(car+0x54,ai)&&Read(ai+0x70,pursuit))target=TargetHandle(pursuit);
 return target;
}
unsigned OfficerPursuit(unsigned officer){
 const auto car=Vehicle(officer);unsigned driver=~0u,ai=0,pursuit=0;
 if(!car||!Read(car+0x94,driver)||driver!=0||!Read(car+0x54,ai)||!Table(ai,0x892480)||
    !Read(ai+0x70,pursuit))return 0;
 const auto target=TargetHandle(pursuit);
 return target&&RunningTarget(target)&&FindPursuit(target)==pursuit?pursuit:0;
}
unsigned SightWitness(unsigned pursuit){
 const auto target=TargetHandle(pursuit);
 if(!target||!RunningTarget(target)||FindPursuit(target)!=pursuit)return 0;
 const auto targetCar=Vehicle(target);
 combat_impact::Motion suspect{};if(!targetCar||!combat_health_runtime::NativeMotion(targetCar-0x80,suspect))return 0;
 unsigned data=0,count=0;
 if(!Read(0x92CD1C,data)||!Read(0x92CD24,count)||!data||count>512)return 0;
 for(unsigned n=0;n<count;++n){
  unsigned car=0,driver=~0u,ai=0,group=0,steering=0,sim=0;BYTE valid=0;float age=0;
  if(!Read(data+n*4,car)||!Table(car,0x8AA828)||!Read(car+0x94,driver)||(driver!=0&&driver!=2)||
     !reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car)||
     reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car)||
     !Read(car+0x54,ai)||!Table(ai,0x892480)||!Read(ai+0x70,group)||group!=pursuit||
     !Read(ai+0x54,steering)||!Table(steering,0x891100)||!Read(steering+0x2C,valid)||!valid||
     !Read(steering+0x1C,sim)||sim!=targetCar-0x80||!Table(ai-0x4C+0x758,0x8923E8)||
     !Read(ai-0x4C+0x758+0x40,age)||!std::isfinite(age)||age<-.30f||age>.50f)continue;
  combat_impact::Motion officer{};
  if(!combat_health_runtime::NativeMotion(car-0x80,officer))continue;
  const auto gap=officer.position-suspect.position;
  if(std::abs(gap.y)<=8.f&&std::hypot(gap.x,gap.z)<=150.f)return Handle(car);
 }
 return 0;
}
void CopDestroyed(unsigned handle,unsigned target=0){
 if(!Active()||owner!=MWELNeoCurrentKey()||!combat_health_runtime::Racing())return;
 if(!target)target=CopTarget(handle);
 if(!ReadyTarget(target))return;
 if(replacements.Destroyed(handle,target,GetTickCount64()))
  Log("HOT_PURSUIT_REPLACEMENT_DELAY cop=%u racer=%u delay_ms=3000 native_destroyed=1",handle,target);
}
struct DestructionBinding {DestructionBinding(){combat_health_runtime::policeTarget=CopTarget;combat_health_runtime::policeDestroyed=CopDestroyed;}};
inline DestructionBinding destructionBinding;
bool NewInterceptor(unsigned car){
 if(!intercept.target||GetTickCount64()>=intercept.until||!RunningTarget(intercept.target))return false;
 const auto h=Handle(car);if(std::find(intercept.before.begin(),intercept.before.end(),h)!=intercept.before.end())return false;
 combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(car-0x80,motion))return false;
 const auto delta=motion.position-intercept.position;
 return std::abs(delta.y)<12&&std::hypot(delta.x,delta.z)<100;
}
bool Retain(unsigned car){
 if(!Active()||!owner||owner!=MWELNeoCurrentKey()||!combat_health_runtime::Racing())return false;
 unsigned driver=0,ai=0,pursuit=0;BYTE busted=0;
 if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||(!Table(ai,0x892480)&&!Helicopter(ai))||(Table(ai,0x892480)&&(Roadblock(car,ai)||UnsupportedSUV(car)))||
    !reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car)||reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car)||!Read(ai+0x70,pursuit))return false;
 const auto target=TargetHandle(pursuit);bool retain=false;
 if(target&&RunningTarget(target)&&Read(pursuit+0xE8,busted)&&!busted){
  combat_impact::Motion copMotion,targetMotion;const auto racer=Vehicle(target);
  if(racer&&combat_health_runtime::NativeMotion(car-0x80,copMotion)&&combat_health_runtime::NativeMotion(racer-0x80,targetMotion)){
   const auto delta=copMotion.position-targetMotion.position;
   retain=hot_pursuit::Retain(true,true,true,std::hypot(delta.x,delta.z),delta.y);
  }
 }else if(!pursuit)retain=NewInterceptor(car); // bounded asynchronous native spawn/detection grace
 if(retain&&worldLogEnabled&&GetTickCount64()>=nextRetainLog){nextRetainLog=GetTickCount64()+2000;
  Log("HOT_PURSUIT_RECYCLE_DEFERRED cop=%u target=%u pending=%u assigned_racer_distance=1",Handle(car),target,unsigned(!pursuit));}
 return retain;
}
unsigned Assigned(const std::vector<unsigned>& cars,unsigned handle,bool includePending=true);
bool __fastcall Detect(unsigned self,void*,unsigned candidate){
 if(const auto p=event_race_policies::Current();p&&mwel_rpd::CaseFile(p->authoring)){unsigned driver=~0u;if(Read(candidate+0x94,driver)&&driver==0)return false;}
 unsigned car=0;const bool owned=Active()&&owner==MWELNeoCurrentKey()&&combat_health_runtime::Racing()&&Read(self+0x48,car)&&Table(car,0x8AA828);
 unsigned incoming=0;
 if(owned){
  unsigned pursuit=0,race=0,count=0,rank=0;Read(self+0x4C+0x70,pursuit);
  const auto actual=TargetHandle(pursuit);incoming=Handle(candidate);
  if(!ReadyTarget(incoming))return false;
  if(auto a=AssignmentFor(car);a&&LockValid(*a)&&incoming!=a->target)return false;
  if(incoming!=actual){
   const auto slot=Slot(incoming,race,count);if(slot>=count||!ReadyTarget(incoming))return false;
   Read(race+0x18+slot*0x194+0x10,rank);if(!rank||rank>count)rank=slot+1;
   std::vector<unsigned> cars;if(!List(cars))return false;
   if(Assigned(cars,incoming)>=TargetQuota(rank,count,incoming))return false;
   // Keep a valid single chaser on its assigned racer. Surplus cars may
   // change to an underserved racer through the ordinary native detection.
   if(actual&&Retain(car)&&Assigned(cars,actual)<=1)return false;
   if(!pursuit&&NewInterceptor(car)&&incoming!=intercept.target)return false;
  }
 }
 const bool accepted=originalDetect(self,candidate);
 if(accepted&&owned){const auto h=Handle(car);const auto now=GetTickCount64();
  auto record=std::find_if(detections.begin(),detections.end(),[&](const Detection& d){return d.cop==h;});
  if(record==detections.end())record=std::find_if(detections.begin(),detections.end(),[&](const Detection& d){return !d.cop||d.until<=now;});
  if(record!=detections.end())*record={h,incoming,now+2500};
 }
 return accepted;
}
bool Install(){
 if(originalDetect&&originalTarget&&originalActivity&&originalRelease&&originalPursuitSpawn&&originalRoadblock&&roadblockCommitTrampoline&&requestSiteTrampoline&&originalHelicopter&&candidateTrampoline&&airBudgetTrampoline&&copRequestTrampoline&&heliThinkTrampoline&&heliFlightTrampoline&&heliActionEnterTrampoline&&heliActionUpdateTrampoline&&heliActionSideTrampoline&&heliFuelTrampoline&&heliDiveTrampoline&&hot_pursuit_radar::originalUpdate)return true;if(!Surface()||!HelicopterNativeSurface()||!hot_pursuit_radar::Surface())return false;
 void* sites[]={reinterpret_cast<void*>(0x429EF0),reinterpret_cast<void*>(0x423860),reinterpret_cast<void*>(0x6FBF80),reinterpret_cast<void*>(0x406450),reinterpret_cast<void*>(0x430E60),reinterpret_cast<void*>(0x43DD30),reinterpret_cast<void*>(0x43E7DF),reinterpret_cast<void*>(0x43EBEF),reinterpret_cast<void*>(0x4269A0),reinterpret_cast<void*>(0x43DF9A),reinterpret_cast<void*>(0x43EB84),reinterpret_cast<void*>(0x43EB9E),reinterpret_cast<void*>(0x42ADC2),reinterpret_cast<void*>(0x42AEF4),reinterpret_cast<void*>(0x579E00),reinterpret_cast<void*>(0x42778A),reinterpret_cast<void*>(0x42781C),reinterpret_cast<void*>(0x42791D),reinterpret_cast<void*>(0x42AD73),reinterpret_cast<void*>(0x427A17)};
 void* hooks[]={reinterpret_cast<void*>(Detect),reinterpret_cast<void*>(AcquireTarget),reinterpret_cast<void*>(AddActivity),reinterpret_cast<void*>(ReleaseCop),reinterpret_cast<void*>(SpawnForPursuit),reinterpret_cast<void*>(PlaceRoadblock),reinterpret_cast<void*>(CommitRoadblock),reinterpret_cast<void*>(RoadblockRequestSite),reinterpret_cast<void*>(RequestHelicopter),reinterpret_cast<void*>(RoadblockCandidate),reinterpret_cast<void*>(AirBudgetSite),reinterpret_cast<void*>(CopRequestSite),reinterpret_cast<void*>(HelicopterThinkPlayer),reinterpret_cast<void*>(HelicopterFlightPlayer),reinterpret_cast<void*>(hot_pursuit_radar::Update),reinterpret_cast<void*>(HelicopterActionEnterPlayer),reinterpret_cast<void*>(HelicopterActionUpdatePlayer),reinterpret_cast<void*>(HelicopterActionSidePlayer),reinterpret_cast<void*>(HelicopterFuelPlayer),reinterpret_cast<void*>(HelicopterDivePlayer)};
 void** outputs[]={reinterpret_cast<void**>(&originalDetect),reinterpret_cast<void**>(&originalTarget),reinterpret_cast<void**>(&originalActivity),reinterpret_cast<void**>(&originalRelease),reinterpret_cast<void**>(&originalPursuitSpawn),reinterpret_cast<void**>(&originalRoadblock),&roadblockCommitTrampoline,&requestSiteTrampoline,reinterpret_cast<void**>(&originalHelicopter),&candidateTrampoline,&airBudgetTrampoline,&copRequestTrampoline,&heliThinkTrampoline,&heliFlightTrampoline,reinterpret_cast<void**>(&hot_pursuit_radar::originalUpdate),&heliActionEnterTrampoline,&heliActionUpdateTrampoline,&heliActionSideTrampoline,&heliFuelTrampoline,&heliDiveTrampoline};
 unsigned made=0;bool ok=true;
 for(unsigned i=0;i<std::size(sites);++i){const auto status=MH_CreateHook(sites[i],hooks[i],outputs[i]);if(status!=MH_OK){Log("COMBAT_SUPPORT_HOOK_FAILED site=%08X stage=create status=%u",unsigned(sites[i]),unsigned(status));ok=false;break;}++made;}
 if(ok)for(unsigned i=0;i<std::size(sites);++i)if(const auto status=MH_EnableHook(sites[i]);status!=MH_OK){Log("COMBAT_SUPPORT_HOOK_FAILED site=%08X stage=enable status=%u",unsigned(sites[i]),unsigned(status));ok=false;break;}
 if(!ok){for(unsigned i=0;i<made;++i){MH_DisableHook(sites[i]);MH_RemoveHook(sites[i]);}originalDetect=nullptr;originalTarget=originalRelease=nullptr;originalActivity=nullptr;originalPursuitSpawn=nullptr;originalRoadblock=nullptr;roadblockCommitTrampoline=nullptr;requestSiteTrampoline=nullptr;originalHelicopter=nullptr;candidateTrampoline=airBudgetTrampoline=copRequestTrampoline=heliThinkTrampoline=heliFlightTrampoline=nullptr;hot_pursuit_radar::originalUpdate=nullptr;heliActionEnterTrampoline=heliActionUpdateTrampoline=heliActionSideTrampoline=heliFuelTrampoline=heliDiveTrampoline=nullptr;}
 return ok;
}
unsigned Assigned(const std::vector<unsigned>& cars,unsigned handle,bool includePending){
 unsigned n=includePending?replacements.Held(handle,GetTickCount64()):0;for(auto car:cars){unsigned ai=0,driver=0,pursuit=0;
  if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||Roadblock(car,ai)||!Read(ai+0x70,pursuit)||reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car))continue;
  const auto actual=TargetHandle(pursuit);if(actual==handle){++n;continue;}
  if(actual||!includePending)continue;
  const auto h=Handle(car);for(const auto& d:detections)if(d.cop==h&&d.target==handle&&GetTickCount64()<d.until){++n;break;}
 }return n;
}
bool NativeSpawnAllowed(bool pursuit){
 if(!Active())return true;
 if(!replacements.RefillReady(GetTickCount64()))return false;
 // Assigned interceptors call the original factory directly. Hold ambient
 // patrol requests so the shared pool remains available for racer targets.
 const auto policy=event_race_policies::Current();const bool officer=policy&&mwel::RpdStartOnly(policy->authoring);
 // RPD reinforcement requests below already carry a checked suspect-relative
 // road position. Native requests are centered on the human officer and can
 // consume the same pool, even when their later activity targets a far racer.
 // They must not compete with that explicit factory (which calls originalSpawn).
 if(policy&&mwel_rpd::Armed(policy->authoring))return false;
 if(!pursuit&&!officer)return false;
 unsigned race=0,count=0,handle=0,rank=0;
 if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count>16)return false;
 const auto p=event_race_policies::Current();const unsigned slot=p&&mwel::RpdStartOnly(p->authoring)?1u:0u;
 if(slot>=count||!Read(race+0x18+slot*0x194,handle)||!ReadyTarget(handle))return false;
 Read(race+0x18+0x10,rank);if(!rank||rank>count)rank=1;
 std::vector<unsigned> cars;if(!List(cars))return false;
 return Assigned(cars,handle)<TargetQuota(rank,count,handle);
}
bool RecycleLost(const std::vector<unsigned>& cars,unsigned car,unsigned ai,unsigned race,unsigned count){
 if(NewInterceptor(car))return false;
 const float offscreen=reinterpret_cast<float(__thiscall*)(unsigned)>(0x688120)(car);
 if(!std::isfinite(offscreen)||offscreen<3)return false;
 combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(car-0x80,motion))return false;
 unsigned pursuit=0;Read(ai+0x70,pursuit);const auto target=TargetHandle(pursuit),h=Handle(car);
 float nearest=1e9f,targetDistance=1e9f;bool running=false;
 for(unsigned slot=0;slot<count;++slot){unsigned handle=0;Read(race+0x18+slot*0x194,handle);if(!RunningTarget(handle))continue;
  combat_impact::Motion racer;const auto v=Vehicle(handle);if(!v||!combat_health_runtime::NativeMotion(v-0x80,racer))continue;
  const auto d=racer.position-motion.position;const float distance=std::hypot(d.x,d.z);nearest=std::min(nearest,distance);
  if(handle==target){running=true;targetDistance=distance;}
 }
 // Off-screen grace prevents popping. A running target is kept until the
 // native controller has fallen behind for several seconds; idle pooled cars
 // only yield when no live racer is nearby. Blocks/helicopters never enter.
 const bool due=target?(!running||targetDistance>650):nearest>300;
 auto record=std::find_if(lost.begin(),lost.end(),[&](const Lost& r){return r.cop==h;});
 if(!due||nearest<160){if(record!=lost.end())*record={};return false;}
 if(record==lost.end())record=std::find_if(lost.begin(),lost.end(),[&](const Lost& r){return !r.cop||!Vehicle(r.cop);});
 if(record==lost.end())return false;
 const auto now=GetTickCount64();if(record->cop!=h||record->target!=target){*record={h,target,now};return false;}
 if(now-record->since<4000)return false;
 if(!ReleaseChaser(car))return false;
 Log("HOT_PURSUIT_LOST_HANDOFF cop=%u target=%u target_gap=%.1f nearest_racer=%.1f offscreen_seconds=%.1f native_pool_release=1",h,target,targetDistance,nearest,offscreen);
 *record={};return true;
}
bool AllowAcquire(unsigned pursuit){
 if(!Active())return true;
 if(!replacements.RefillReady(GetTickCount64()))return false;
 unsigned race=0,count=0,rank=0;const auto handle=TargetHandle(pursuit),slot=Slot(handle,race,count);
 if(slot>=count||!ReadyTarget(handle))return false; // Only eligible authored racers get this budget.
 Read(race+0x18+slot*0x194+0x10,rank);if(!rank||rank>count)rank=slot+1;
 std::vector<unsigned> cars;if(!List(cars))return false;
 return Assigned(cars,handle)<TargetQuota(rank,count,handle);
}
void Frame(){
 if(!Active()){if(owner)Reset();return;}
 if(combat_health_runtime::Racing())FinalizeSpawnActivations();
 if(!combat_health_runtime::Racing()||GetTickCount64()<next)return;next=GetTickCount64()+200;
 const auto key=MWELNeoCurrentKey();if(owner!=key){Reset();owner=key;roadReady=MWELPrepareGauntletRoad(key);Log("HOT_PURSUIT_ROAD_CACHE ready=%u loaded_once=1",unsigned(roadReady));}
 unsigned race=0,count=0,play=0;Read(0x91E000,race);Read(race+0x1958,count);Read(race+0x1960,play);
 unsigned outcome=0;BYTE finished=0;Read(race+0x18+0x1C,outcome);Read(race+0x18+0x21,finished);
 if(outcome||finished)return;
 unsigned manager=0;if(!Read(0x92C67C,manager)||!Table(manager,0x891900))return;
 const auto player=Vehicle(*reinterpret_cast<unsigned*>(race+0x18));if(!player)return;
 ApplyDeployment(manager,Budget());
 UpdateEligibility(race,count);
 const bool policeSprint=MWELRpdHotPursuitScope(),ownedDispatch=RpdDispatchOwned();
 const int heat=Heat(player);
 unsigned rosterHeat=0;
 // A fresh ordinary race can have eligible AI suspects before the human
 // reaches 25 percent. Do not wait for a human-centred ambient cop merely
 // to learn a model name; use the installed heat catalogue for that first
 // checked request too. Existing ordinary native dispatch remains allowed.
 auto dispatch=(ownedDispatch||model.empty())?rpd_vehicle_catalog::DispatchVehicle(heat,&rosterHeat):nullptr;
 if(ownedDispatch&&rpd_vehicle_catalog::FleetEnabled(heat)){dispatch=rpd_vehicle_catalog::fleets[heat].models.front();rosterHeat=heat;}
 // Start-only pursuit may set 'requested' in its own bootstrap before
 // this controller runs. Report the effective model once per race either way.
 if(worldLogEnabled&&ownedDispatch){
  static unsigned lastRace=0,lastEpoch=0;
  if(lastRace!=race||lastEpoch!=combat_health_runtime::generation){
   lastRace=race;lastEpoch=combat_health_runtime::generation;
   Log("RPD_DISPATCH_ROSTER requested_heat=%d roster_heat=%u fallback=%u model=%s sprint=%u",heat,rosterHeat,
       unsigned(dispatch&&rosterHeat!=unsigned(heat)),dispatch?dispatch->name.c_str():"unavailable",unsigned(policeSprint));
   const bool varied=rpd_vehicle_catalog::FleetEnabled(heat);
   if(varied){const auto& fleet=rpd_vehicle_catalog::fleets[heat];
    Log("RPD_FLEET_VARIETY heat=%d models=%u patrol=%u suv=%u lb_ready=1 existing_budget=%u",heat,unsigned(fleet.models.size()),unsigned(fleet.patrol.size()),unsigned(fleet.suv.size()),Budget());}
  }
 }
 if(!requested){reinterpret_cast<void(__thiscall*)(unsigned,bool)>(0x411410)(manager,false);
  requested=true;
  if(policeSprint)Log("RPD_HOT_PURSUIT_DEPLOYMENT racers=%u officer=1 ally_limit=%u police_total=%u heat=%d model=%s",count-1,Budget(),Budget()+1,heat,dispatch?dispatch->name.c_str():"unavailable");
  Log("HOT_PURSUIT_BEGIN racers=%u budget=%u heat=%d start_after_race_percent=25 native_cop_controllers=1 prototype=1",count,Budget(),heat);return;}
 const auto playerHandle=Handle(player);
 if(!playerPursuitStarted&&ReadyTarget(playerHandle)){
  reinterpret_cast<void(__thiscall*)(unsigned,int)>(0x42EBF0)(manager,heat);playerPursuitStarted=true;
 }
 std::vector<unsigned> cars;if(!List(cars))return;
 const auto replacementNow=GetTickCount64();
 for(auto& ticket:replacements.tickets)if(ticket.cop&&replacementNow>=ticket.until){
  const auto current=std::find_if(cars.begin(),cars.end(),[&](unsigned car){return Handle(car)==ticket.cop;});
  // Native pooling can reuse the same vehicle/handle. A confirmed living
  // activation begins a fresh destruction lifetime; an old wreck does not.
  if(current==cars.end()||!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(*current))ticket={};
 }
 auto chasers=ChaseCount(0);
 for(auto car:cars){unsigned ai=0,driver=0;if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||Roadblock(car,ai))continue;
  if(reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car))CopDestroyed(Handle(car));
  const auto carKey=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->GetVehicleKey();
  const bool varied=rpd_vehicle_catalog::FleetEnabled(heat);
  if(ownedDispatch&&(varied?!rpd_vehicle_catalog::FleetAccepts(heat,carKey):(dispatch&&carKey!=dispatch->source))){
   if(ReleaseChaser(car)&&chasers)--chasers;
   continue;
  }
  if(SUV(car)&&!MWELRpdFleetAllowsVehicle(carKey)){if(ReleaseChaser(car)&&chasers)--chasers;continue;}
  if(RecycleLost(cars,car,ai,race,count)){if(chasers)--chasers;continue;}
  if(model.empty()){const char* text=reinterpret_cast<const char*(__thiscall*)(unsigned)>(0x688090)(car);char buffer[64]{};
   if(Copy(text,buffer,sizeof(buffer))&&memchr(buffer,0,sizeof(buffer)))model=buffer;}
 }
 cars.clear();if(!List(cars))return;
 RefreshAssignments(cars);
 RefreshHelicopters(cars);
 if(intercept.target){
  if(GetTickCount64()>=intercept.until||!RunningTarget(intercept.target))intercept={};
  else {const auto targetCar=Vehicle(intercept.target);
   for(const auto cop:cars){unsigned ai=0,driver=0,pursuit=0;
    if(!Read(cop+0x94,driver)||driver!=2||!Read(cop+0x54,ai)||!Table(ai,0x892480)||Roadblock(cop,ai)||!Read(ai+0x70,pursuit)||!NewInterceptor(cop))continue;
    if(intercept.rollingCop&&intercept.rollingCop!=Handle(cop))continue; // one queued request initializes one car
    if(intercept.rollingCop!=Handle(cop)){
     // The queued patrol factory activates at rest and chooses a native road
     // direction. Resolve both exactly once, while this is still the new car
     // belonging to our checked placement. Never restart a car already fighting.
     combat_impact::Motion before,aligned,targetMotion;
     if(!combat_health_runtime::NativeMotion(cop-0x80,before)||!combat_health_runtime::NativeMotion(targetCar-0x80,targetMotion))continue;
     const auto gap=before.position-intercept.position;
     if(std::hypot(gap.x,gap.z)>35||std::abs(gap.y)>8)continue;
     const bool placed=reinterpret_cast<bool(__thiscall*)(unsigned,const combat_impact::Vector*,const combat_impact::Vector*)>(0x422730)(ai,&intercept.position,&intercept.forward);
     const bool read=placed&&combat_health_runtime::NativeMotion(cop-0x80,aligned);
     const auto offset=aligned.position-intercept.position;
     const float heading=read?combat_impact::Dot(aligned.forward,intercept.forward):0;
     if(!read||std::hypot(offset.x,offset.z)>3||std::abs(offset.y)>3||heading<.7f){
      Log("HOT_PURSUIT_ROLLING_REFUSED cop=%u heading=%.3f native_road_alignment=0",Handle(cop),heading);intercept.rollingCop=Handle(cop);continue;
     }
     const auto targetDelta=aligned.position-targetMotion.position;
     const float rolling=hot_pursuit::RollingSpeed(std::hypot(targetMotion.velocity.x,targetMotion.velocity.z),std::hypot(targetDelta.x,targetDelta.z));
     reinterpret_cast<void(__thiscall*)(unsigned,float)>(0x670210)(cop,rolling);
     intercept.rollingCop=Handle(cop);
     Log("HOT_PURSUIT_ROLLING_SPAWN cop=%u racer=%u before_mps=%.3f rolling_mps=%.3f heading=%.3f native_road_and_speed=1",Handle(cop),intercept.target,std::hypot(before.velocity.x,before.velocity.z),rolling,heading);
    }
    if(TargetHandle(pursuit)==intercept.target){Log("HOT_PURSUIT_INTERCEPT_CONFIRMED racer=%u cop=%u",intercept.target,Handle(cop));intercept={};break;}
    if(pursuit||GetTickCount64()<intercept.detectAfter)continue;
    Remember(cop,intercept.target);
    if(reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x429EF0)(ai-0x4C,targetCar)){
     Log("HOT_PURSUIT_INTERCEPT_DETECTED racer=%u cop=%u awaiting_native_pursuit_owner=1",intercept.target,Handle(cop));
     intercept.detectAfter=GetTickCount64()+1000;break;
    }
   }
  }
 }

 unsigned tracked=0;for(unsigned slot=0;slot<count;++slot){
  unsigned handle=0,flags=0;Read(race+0x18+slot*0x194,handle);Read(race+0x18+slot*0x194+0x1C,flags);
  if(!handle||flags||!ReadyTarget(handle))continue;const auto car=Vehicle(handle);if(!car)continue;
  if(slot){const auto perp=Perpetrator(car);float value=0;if(perp&&Read(perp+0x1C,value)){
   auto& record=perps[slot];if(record.handle!=handle||record.component!=perp)record={handle,perp,value,float(heat)};
   if(value<heat){reinterpret_cast<void(__thiscall*)(unsigned,float)>(0x409060)(perp,float(heat));float actual=0;Read(perp+0x1C,actual);
    Log("HOT_PURSUIT_RACER_HEAT handle=%u requested=%d actual=%.2f native_perpetrator=1",handle,heat,actual);}
  }}
  if(slot)EnsurePursuit(slot,handle);
  if(Assigned(cars,handle,false))++tracked;
 }
 if(GetTickCount64()>=nextLog){nextLog=GetTickCount64()+3000;Log("HOT_PURSUIT_STATE racers=%u chased=%u active_chasers=%u limit=%u native_pursuit_owner=1",count,tracked,ChaseCount(0),Budget());
  if(worldLogEnabled){
   unsigned nativeActive=0,capacity=0,poolPending=0,requests=0,disabled=0;float delay=0;BYTE locked=0;
   Read(manager-0x54+0x94,nativeActive);Read(manager-0x54+0x98,capacity);Read(manager-0x54+0xC0,poolPending);
   Read(manager+0x1C,requests);Read(manager+0x58,delay);Read(manager+0x60,locked);Read(0x90D5EC,disabled);
   Log("HOT_PURSUIT_DISPATCH_AUDIT epoch=%u manager=%08X native_active=%u capacity=%u pending_pool=%u queued=%u reserved=%u road_ready=%u model_ready=%u lockout=%.3f locked=%u disabled=%u refill_ready=%u",combat_health_runtime::generation,manager,nativeActive,capacity,poolPending,requests,intercept.target,unsigned(roadReady),unsigned(dispatch!=nullptr),delay,unsigned(locked),disabled,unsigned(replacements.RefillReady(GetTickCount64())));
  }
 }
 if(!replacements.RefillReady(GetTickCount64())||GetTickCount64()<nextRequest)return;nextRequest=GetTickCount64()+1000;
 unsigned pending=0;if(!Read(manager-0x54+0xC0,pending)||pending||intercept.target)return;
 std::array<unsigned,16> order{};for(unsigned i=0;i<count;++i)order[i]=i;
 auto rank=[&](unsigned slot){unsigned r=0;Read(race+0x18+slot*0x194+0x10,r);return r&&r<=count?r:slot+1;};
 auto assigned=[&](unsigned slot){unsigned handle=0;Read(race+0x18+slot*0x194,handle);return Assigned(cars,handle);};
 std::sort(order.begin(),order.begin()+count,[&](unsigned a,unsigned b){const bool waitingA=assigned(a)==0,waitingB=assigned(b)==0;return waitingA!=waitingB?waitingA:rank(a)<rank(b);});
 for(unsigned index=0;index<count;++index){const auto slot=order[index];
  unsigned handle=0,flags=0;Read(race+0x18+slot*0x194,handle);Read(race+0x18+slot*0x194+0x1C,flags);
  if(!handle||flags||!ReadyTarget(handle)||Assigned(cars,handle)>=TargetQuota(rank(slot),count,handle))continue;
  const auto racer=Vehicle(handle);combat_impact::Motion target;if(!racer||!combat_health_runtime::NativeMotion(racer-0x80,target))continue;
  for(auto cop:cars){unsigned ai=0,pursuit=0,driver=0;BYTE engaged=0;
   if(!Read(cop+0x94,driver)||driver!=2||UnsupportedSUV(cop)||!Read(cop+0x54,ai)||!Table(ai,0x892480)||Roadblock(cop,ai)||
      !Read(ai+0x70,pursuit)||!Read(ai-0x4C+0x760,engaged))continue;
   const auto actual=TargetHandle(pursuit);if(actual&&(actual==handle||(ReadyTarget(actual)&&Assigned(cars,actual)<=1)))continue;
   if(!pursuit&&engaged)continue;
   combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(cop-0x80,motion))continue;
   auto d=target.position-motion.position;if(std::abs(d.y)>8||std::hypot(d.x,d.z)>160)continue;
   Remember(cop,handle);
   const bool detected=reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x429EF0)(ai-0x4C,racer);
   Log("HOT_PURSUIT_NATIVE_DETECT racer=%u cop=%u accepted=%u",handle,Handle(cop),unsigned(detected));if(detected)return;
  }
  const auto policy=event_race_policies::Current();const bool freeRun=policy&&mwel::RpdStartOnly(policy->authoring);
  const char* dispatchModel=(ownedDispatch||model.empty())?(dispatch?dispatch->name.c_str():nullptr):model.c_str();
  if(!police_deployment::Room(Budget(),ChaseCount(0),PendingDeployment(manager))||!dispatchModel||(!roadReady&&!freeRun))continue;
  unsigned nativeActive=0,nativeCapacity=0;
  if(!Read(manager-0x54+0x94,nativeActive)||!Read(manager-0x54+0x98,nativeCapacity)||!nativeCapacity||nativeCapacity>64||nativeActive>=nativeCapacity)continue; // never queue impossible spawns into a full native pool

  float point[3]{},tangent[3]{};unsigned leg=0;Read(race+0x18+slot*0x194+0x2C,leg);
  if(freeRun){if(!MWELRpdReinforcementAhead(handle,&target.position.x,&target.forward.x,point,tangent))continue;}
  else if(!MWELCourseAhead(owner,&target.position.x,leg,100,point,tangent))continue;
  mwel_grid::Layout layout;const float rotation=std::atan2(tangent[0],-tangent[2])*57.295779513f;
  if(!MWELFitGrid(owner,1,{point[0],point[1],point[2]},rotation,layout)||!layout.valid)continue;
  // This is a same-direction pursuit reinforcement, not Gauntlet's head-on
  // interception. Do not spawn facing backwards into other racers' traffic.
  combat_impact::Vector position{layout.positions[0][0],layout.positions[0][1],layout.positions[0][2]},forward{tangent[0],tangent[1],tangent[2]};
  bool clear=true;for(auto car:cars){combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(car-0x80,motion))continue;auto d=motion.position-position;if(std::abs(d.y)<5&&std::hypot(d.x,d.z)<14)clear=false;}
  if(!clear)continue;
  intercept={handle,GetTickCount64()+8000,position,{}};intercept.forward=forward;for(const auto car:cars)intercept.before.push_back(Handle(car));
  if(ownedDispatch)if(const auto varied=rpd_vehicle_catalog::FleetVehicle(heat))dispatchModel=varied->name.c_str();
  originalSpawn(manager,&position,&forward,dispatchModel,false,false);
  nextRequest=GetTickCount64()+1800;
  Log("HOT_PURSUIT_INTERCEPT_REQUEST racer=%u rank=%u offset=100 heading_dot=1 native_patrol_then_detection=1 model=%s",handle,rank(slot),dispatchModel);return;
 }
}
}
bool MWELHotPursuitSpawnAllowed(bool pursuit){return hot_pursuit_runtime::NativeSpawnAllowed(pursuit);}
bool MWELHotPursuitAmbientPatrolAllowed(){
 using namespace hot_pursuit_runtime;
 if(!Active())return true;
 const auto policy=event_race_policies::Current();
 if(policy&&mwel_rpd::Armed(policy->authoring))return false;
 // CopMgr's ambient patrol factory places relative to the human officer, not
 // the fleeing racer. Repeated replenishment selected the same unassigned
 // active car every ~200 ms and moved its radar/body across unrelated roads.
 // Permit one native bootstrap after GO; subsequent reinforcements already
 // use this event's checked target-relative queued factory below.
 return policy&&mwel::RpdStartOnly(policy->authoring)&&hot_pursuit_runtime::owner==MWELNeoCurrentKey()&&
        combat_health_runtime::Racing()&&!patrolBootstrapDone;
}
void MWELHotPursuitPatrolSpawned(){
 if(hot_pursuit_runtime::Active()){
  hot_pursuit_runtime::patrolBootstrapDone=true;
  Log("HOT_PURSUIT_PATROL_BOOTSTRAP native_spawn=1 ambient_replenishment_closed=1 target_relative_reinforcements_retained=1");
 }
}
bool MWELHotPursuitRetain(unsigned car){return hot_pursuit_runtime::Retain(car);}
bool MWELHotPursuitAcquire(unsigned pursuit){return hot_pursuit_runtime::AllowAcquire(pursuit);}
bool MWELHotPursuitHealth(unsigned slot,unsigned handle,float& remaining,float& maximum){
 mwcombat::HealthV1 value;if(!hot_pursuit_runtime::Active()||!combat_health_runtime::Query(slot,handle,&value,sizeof(value)))return false;
 remaining=value.remaining;maximum=value.maximum;return true;
}
struct MWELHotPursuitHudBindings {MWELHotPursuitHudBindings(){expanded_leaderboard::combatActive=MWELHotPursuitActive;expanded_leaderboard::combatHealth=MWELHotPursuitHealth;}};
inline MWELHotPursuitHudBindings mwelHotPursuitHudBindings;
