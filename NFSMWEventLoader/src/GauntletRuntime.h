#pragma once
#include "GauntletBackupPolicy.h"
#include "GauntletBootstrapPolicy.h"
#include "GauntletVehiclePolicy.h"
#include "PoliceDeploymentPolicy.h"
#include "PoliceDeploymentLease.h"
// One authored leader and up to three ordinary native chasers. CopMgr owns creation, pursuit activity,
// controller and disposal. The Tollbooth keeps its ordinary one-racer rules;
// the police is a transient combat participant, never a fabricated GRacerInfo.
bool MWELInstallGauntletPursuit();
void MWELGauntletRam(unsigned,const combat_impact::Motion&,const combat_impact::Motion&);
namespace gauntlet_runtime {
constexpr unsigned ChaseLimit=4;
unsigned owner=0,opponentHandle=0;bool requested=false,refused=false;ULONGLONG next=0;
bool bootstrapConfirmed=false;unsigned bootstrapAttempts=0;
combat_impact::Vector origin{};bool originValid=false;
unsigned diagnosticOutcome=0;bool diagnosticFinished=false;
bool diagnosticArrest=false;
ULONGLONG nextPursuit=0;
using AcquireFn=bool(__thiscall*)(unsigned,unsigned);
AcquireFn originalAcquire=nullptr;
void* patrolTrampoline=nullptr;ULONGLONG nextUnspawnLog=0;
using SpawnFn=void(__thiscall*)(unsigned,const combat_impact::Vector*,const combat_impact::Vector*,const char*,bool,bool);
using QueuedSpawnFn=bool(__thiscall*)(unsigned,unsigned);
QueuedSpawnFn originalQueuedSpawn=nullptr;
using PatrolSpawnFn=bool(__thiscall*)(unsigned);
PatrolSpawnFn originalPatrolSpawn=nullptr;
SpawnFn originalSpawn=nullptr;bool replacementSpawn=false;ULONGLONG spawnPendingUntil=0;
int requestedHeat=1;float powerScale=1.75f;
unsigned boostVehicle=0,powerCalls=0;float nativePower=0,boostedPower=0;bool modelLogged=false;
gauntlet_backup::Trail trail;
bool handoffPending=false;unsigned handoffOld=0;float handoffAt=0,backupRetryAt=0;
combat_impact::Vector handoffPosition{};std::vector<unsigned> handoffBefore;
combat_impact::Vector handoffForward{};
float handoffSpeed=0;
std::string policeModel;
static_assert(offsetof(NFSPluginSDK::MW05::AIVehicle,mPursuit)-0x4C==0x70);
static_assert(offsetof(NFSPluginSDK::MW05::AIVehicle,mRoadBlock)-0x4C==0x74);
bool diagnosticBackup=false,diagnosticBackupRequested=false;
template<class T>bool Read(unsigned p,T& out){return combat_health_runtime::Read(p,out);}
bool Table(unsigned p,unsigned expected){unsigned table=0;return Read(p,table)&&table==expected;}
// IVehicle::GetAI returns the IVehicleAI COM view (+4C), not the primary
// AIVehicleHelicopter address exposed by the native helicopter singleton.
bool Helicopter(unsigned ai){return ai>=0x1004C&&Table(ai,0x891FF8)&&Table(ai-0x4C,0x8920D8);}
bool Surface(){
 const BYTE patrolSpawn[]={0x83,0xEC,0x0C,0x53,0x55,0x56,0x57,0x6A};
 const BYTE queued[]={0x6A,0xFF,0x68,0x4B,0x83,0x86,0,0x64};
 const BYTE acquire[]={0x83,0xEC,0x1C,0x53,0x55,0x56,0x57,0x6A};BYTE code[8]{};unsigned fn=0;
 return (originalPatrolSpawn||(Copy(reinterpret_cast<void*>(0x430D90),code,8)&&!memcmp(code,patrolSpawn,8)))&&
  (originalQueuedSpawn||(Copy(reinterpret_cast<void*>(0x42E6C0),code,8)&&!memcmp(code,queued,8)))&&
  (originalAcquire||(Copy(reinterpret_cast<void*>(0x42EA90),code,8)&&!memcmp(code,acquire,8)))&&
  Read(0x892480+41*4,fn)&&fn==0x431D80&&Read(0x892480+35*4,fn)&&fn==0x415DB0&&Read(0x892480+23*4,fn)&&fn==0x422730&&
  Read(0x8AA828+37*4,fn)&&fn==0x670210&&Read(0x891900+7*4,fn)&&fn==0x439DD0&&Read(0x891900+13*4,fn)&&fn==0x42EBF0&&Read(0x891900+11*4,fn)&&fn==0x411410;
}
bool List(std::vector<unsigned>& cars){
 unsigned data=0,count=0,capacity=0;
 if(!Read(0x92CD1C,data)||!Read(0x92CD20,capacity)||!Read(0x92CD24,count)||count>capacity||count>512)return false;
 for(unsigned i=0;i<count;++i){unsigned car=0;if(!Read(data+i*4,car)||!Table(car,0x8AA828))continue;
  if(reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car))cars.push_back(car);
 }return true;
}
// Only a CURRENT roadblock assignment is exempt. Cars released from a block
// into the pursuit consume a chase slot. LastSpawnTime is elapsed time and
// cannot serve as an immutable origin identifier for pooled cars.
bool Roadblock(unsigned car,unsigned ai){
 (void)car;unsigned block=0;return !Read(ai+0x74,block)||block!=0;
}
bool SUV(unsigned car){
 const BYTE expected[]={0x8B,0x41,0x2C,0x8B,0x40,0x24,0xC3};BYTE code[7]{};
 if(!Table(car,0x8AA828)||!Copy(reinterpret_cast<void*>(0x688090),code,7)||memcmp(code,expected,7))return false;
 const auto name=reinterpret_cast<const char*(__thiscall*)(unsigned)>(0x688090)(car);char model[64]{};
 if(!Copy(name,model,sizeof(model))||!memchr(model,0,sizeof(model)))return false;
 return gauntlet_vehicle::SUV(model);
}
unsigned ChaseCount(unsigned pursuit){
 (void)pursuit;
 unsigned data=0,count=0,capacity=0;
 if(!Read(0x92CD1C,data)||!Read(0x92CD20,capacity)||!Read(0x92CD24,count)||count>capacity||count>512)return ChaseLimit;
 unsigned chasers=0;
 for(unsigned i=0;i<count;++i){unsigned car=0,driver=0,ai=0;
  if(Read(data+i*4,car)&&Table(car,0x8AA828)&&Read(car+0x94,driver)&&driver==2&&Read(car+0x54,ai)&&Table(ai,0x892480)&&
     !Roadblock(car,ai)&&reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car)&&
     (!MWELHotPursuitActive()||!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car)))++chasers;
 }return chasers;
}
unsigned Handle(unsigned car){unsigned value=0;return Read(car-0x80+8,value)?value:0;}
unsigned Vehicle(unsigned handle){
 unsigned data=0,count=0,capacity=0;
 if(!handle||!Read(0x92CD1C,data)||!Read(0x92CD20,capacity)||!Read(0x92CD24,count)||count>capacity||count>512)return 0;
 for(unsigned i=0;i<count;++i){unsigned car=0;if(Read(data+i*4,car)&&Table(car,0x8AA828)&&Handle(car)==handle)return car;}return 0;
}
police_deployment::Lease deploymentLease;
bool ReadDeployment(unsigned manager,unsigned offset,unsigned& value){return Read(manager-0x54+offset,value);}
void WriteDeployment(unsigned manager,unsigned offset,unsigned value){*reinterpret_cast<unsigned*>(manager-0x54+offset)=value;}
void RestoreDeployment(police_deployment::Owner owner=police_deployment::Owner::Any){
 unsigned manager=0;
 if(!Read(0x92C67C,manager)||!Table(manager,0x891900))manager=0;
 deploymentLease.Release(manager,owner,ReadDeployment,WriteDeployment);
}
void ApplyDeployment(unsigned manager,unsigned limit){
 if(!limit||limit>16||!Table(manager,0x891900))return;
 const auto owner=MWELHotPursuitActive()?police_deployment::Owner::HotPursuit:police_deployment::Owner::Gauntlet;
 deploymentLease.Acquire(manager,owner,limit,ReadDeployment,[](unsigned m,unsigned offset,unsigned cap){
  WriteDeployment(m,offset,cap);Log("COMBAT_NATIVE_DEPLOYMENT offset=%X cap=%u no_post_spawn_cull=1",offset,cap);
 });
}
// CopMgr::SpawnCop queues a 60-byte request in its native list, rather than
// activating immediately. Reserve every ordinary queued car before admission.
unsigned PendingDeployment(unsigned manager){
 unsigned head=0,node=0;if(!Read(manager+0x18,head)||!Read(head,node))return 64;
 unsigned count=0;
 for(unsigned n=0;node!=head;++n){BYTE block=0;if(!node||n>=128||!Read(node+8+57,block))return 64;
  if(!block)++count;if(!Read(node,node))return 64;
 }return count;
}
unsigned DeploymentLimit(){return MWELHotPursuitActive()?MWELHotPursuitBudget():4u;}
void Reset(){RestoreDeployment(police_deployment::Owner::Gauntlet);bootstrapConfirmed=false;bootstrapAttempts=0;nextUnspawnLog=0;spawnPendingUntil=0;replacementSpawn=false;owner=opponentHandle=diagnosticOutcome=boostVehicle=powerCalls=0;requested=refused=originValid=diagnosticFinished=diagnosticArrest=modelLogged=false;next=nextPursuit=0;requestedHeat=1;powerScale=1.75f;nativePower=boostedPower=0;trail={};handoffPending=false;handoffOld=0;handoffAt=backupRetryAt=0;handoffBefore.clear();policeModel.clear();}
bool Current(){auto p=event_race_policies::Current();return p&&p->authoring.value("combatMode",std::string("none"))=="gauntlet";}
bool Duel(){
 unsigned race=0,play=0,count=0,outcome=0;BYTE done=0;
 return Read(0x91E000,race)&&Read(race+0x1960,play)&&play==1&&Read(race+0x1958,count)&&count==1&&Current()&&
  Read(race+0x18+0x1C,outcome)&&Read(race+0x18+0x21,done)&&!outcome&&!done;
}
bool ReleaseChaser(unsigned car);
bool __fastcall Acquire(unsigned manager,void*,unsigned pursuit){
 // Restrict ordinary pursuit replenishment. Roadblock requests and the
 // helicopter factory retain their native paths and budgets.
 if(Duel()||MWELHotPursuitActive()){
  // Acquire receives an activity COM view, which need not equal the IPursuit
  // view kept by AICop. Compare against the player's canonical pursuit view.
  unsigned player=0,activity=0;Read(0x993CC8,player);Read(player+0x130,activity);
  const auto limit=MWELHotPursuitActive()?MWELHotPursuitBudget():ChaseLimit;
  ApplyDeployment(manager+0x54,limit);
  if((Duel()&&handoffPending)||!police_deployment::Room(limit,ChaseCount(activity),PendingDeployment(manager+0x54))||!MWELHotPursuitAcquire(pursuit))return false;
 }
 const bool acquired=originalAcquire(manager,pursuit);
 if(acquired&&(Duel()||MWELHotPursuitActive())){
  // AcquireCop activates pooled cars without using SpawnCop. Reject SUVs
  // after native ownership/count insertion, then release with full bookkeeping.
  std::vector<unsigned> cars;if(List(cars))for(auto car:cars){unsigned ai=0,driver=0;
   if(Read(car+0x94,driver)&&driver==2&&Read(car+0x54,ai)&&Table(ai,0x892480)&&!Roadblock(car,ai)&&SUV(car)){
    if(ReleaseChaser(car))Log("GAUNTLET_SUV_EXCLUDED handle=%u pooled_acquire=1 native_count_restored=1",Handle(car));
   }
  }
 }return acquired;
}
// Patrol recycling also removes pursuit membership and decrements pool counts.
// Skip that complete per-car branch for the live duel owner, before any side
// effects; never suppress only UnSpawn after native bookkeeping has happened.
bool __cdecl RetainPatrol(unsigned car){
 if(MWELHotPursuitRetain(car))return true;
 unsigned liveAi=0,driver=0,block=0,playerManager=0,pursuit=0,race=0;BYTE busted=0,active=0,timer=0;
 Read(0x993CC8,playerManager);Read(playerManager+0x130,pursuit);Read(pursuit+0xE8,busted);
 if(owner&&owner==MWELNeoCurrentKey()&&Duel()&&!busted&&car==Vehicle(opponentHandle)&&
    Read(0x91E000,race)&&Read(race+0x198A,active)&&active&&Read(race+0x1978,timer)&&timer&&
    Read(car+0x54,liveAi)&&Table(liveAi,0x892480)&&
    Read(car+0x94,driver)&&driver==2&&Read(liveAi+0x74,block)&&!block&&
    reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car)){
  if(worldLogEnabled&&GetTickCount64()>=nextUnspawnLog){nextUnspawnLog=GetTickCount64()+2000;
   Log("GAUNTLET_OWNER_RECYCLE_DEFERRED handle=%u native_patrol_branch=0043189E pool_bookkeeping_preserved=1",opponentHandle);
  }return true;
 }return false;
}
void __declspec(naked) PatrolRecycle(){
 __asm {
  pushfd
  pushad
  push esi
  call RetainPatrol
  add esp,4
  test al,al
  jz nativeRecycle
  popad
  popfd
  push 0x431905
  ret
 nativeRecycle:
  popad
  popfd
  push esi
  mov ecx,ebp
  mov eax,0x406450
  call eax
  push 0x4318A6
  ret
 }
}
void __fastcall Spawn(unsigned manager,void*,const combat_impact::Vector* position,const combat_impact::Vector* forward,const char* model,bool pursuit,bool roadblock){
 if(MWELHotPursuitActive()&&!roadblock&&!MWELHotPursuitSpawnAllowed(pursuit))return;
 if((Duel()||MWELHotPursuitActive())&&pursuit&&!roadblock&&model){char name[64]{};if(Copy(model,name,sizeof(name))&&memchr(name,0,sizeof(name))&&gauntlet_vehicle::SUV(name))return;}
 if((Duel()||MWELHotPursuitActive())&&!roadblock&&!replacementSpawn){
  unsigned player=0,activity=0;Read(0x993CC8,player);Read(player+0x130,activity);
  const auto limit=MWELHotPursuitActive()?MWELHotPursuitBudget():ChaseLimit;
  ApplyDeployment(manager,limit);
  if((Duel()&&handoffPending)||!police_deployment::Room(limit,ChaseCount(activity),PendingDeployment(manager))||GetTickCount64()<spawnPendingUntil)return;
  // Native factory requests can be queued before the first cop becomes active.
  spawnPendingUntil=GetTickCount64()+1500;
 }
 originalSpawn(manager,position,forward,model,pursuit,roadblock);
}
bool __fastcall QueuedSpawn(unsigned manager,void*,unsigned request){
 const bool scoped=Duel()||MWELHotPursuitActive();BYTE roadblock=0;
 if(scoped){
  if(!Read(request+57,roadblock))return false;
  ApplyDeployment(manager+0x54,DeploymentLimit());
  if(!roadblock&&ChaseCount(0)>=DeploymentLimit())return false;
 }
 bool result=false;
 __try {result=originalQueuedSpawn(manager,request);}
 __finally {if(scoped)ApplyDeployment(manager+0x54,DeploymentLimit());}
 return result;
}
bool __fastcall PatrolSpawn(unsigned manager,void*){
 if(Duel()||MWELHotPursuitActive()){
  ApplyDeployment(manager+0x54,DeploymentLimit());
  if(!police_deployment::Room(DeploymentLimit(),ChaseCount(0),PendingDeployment(manager+0x54)))return false;
 }
 return originalPatrolSpawn(manager);
}
bool Install(){
 if(originalAcquire&&originalQueuedSpawn&&originalPatrolSpawn)return true;if(!Surface()||!MWELInstallGauntletPower()||!MWELInstallGauntletPursuit())return false;
 auto address=reinterpret_cast<void*>(0x42EA90);
 if(MH_CreateHook(address,Acquire,reinterpret_cast<void**>(&originalAcquire))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);originalAcquire=nullptr;return false;}
 auto spawn=reinterpret_cast<void*>(0x439DD0);
 if(MH_CreateHook(spawn,Spawn,reinterpret_cast<void**>(&originalSpawn))!=MH_OK||MH_EnableHook(spawn)!=MH_OK){
  MH_DisableHook(address);MH_RemoveHook(address);originalAcquire=nullptr;
  if(originalSpawn){MH_RemoveHook(spawn);originalSpawn=nullptr;}return false;
 }
 auto patrol=reinterpret_cast<void*>(0x43189E);
 const BYTE expected[]={0x56,0x8B,0xCD,0xE8,0xAA,0x4B,0xFD,0xFF};BYTE bytes[8]{};
 if(!Copy(patrol,bytes,8)||memcmp(bytes,expected,8)||
    MH_CreateHook(patrol,PatrolRecycle,&patrolTrampoline)!=MH_OK||MH_EnableHook(patrol)!=MH_OK){
  if(patrolTrampoline){MH_RemoveHook(patrol);patrolTrampoline=nullptr;}
  MH_DisableHook(spawn);MH_RemoveHook(spawn);originalSpawn=nullptr;
  MH_DisableHook(address);MH_RemoveHook(address);originalAcquire=nullptr;return false;
 }
 auto queued=reinterpret_cast<void*>(0x42E6C0);
 if(MH_CreateHook(queued,QueuedSpawn,reinterpret_cast<void**>(&originalQueuedSpawn))!=MH_OK||MH_EnableHook(queued)!=MH_OK){
  if(originalQueuedSpawn){MH_RemoveHook(queued);originalQueuedSpawn=nullptr;}
  MH_DisableHook(patrol);MH_RemoveHook(patrol);patrolTrampoline=nullptr;
  MH_DisableHook(spawn);MH_RemoveHook(spawn);originalSpawn=nullptr;
  MH_DisableHook(address);MH_RemoveHook(address);originalAcquire=nullptr;return false;
 }
 auto nativePatrol=reinterpret_cast<void*>(0x430D90);
 if(MH_CreateHook(nativePatrol,PatrolSpawn,reinterpret_cast<void**>(&originalPatrolSpawn))!=MH_OK||MH_EnableHook(nativePatrol)!=MH_OK){
  if(originalPatrolSpawn){MH_RemoveHook(nativePatrol);originalPatrolSpawn=nullptr;}
  MH_DisableHook(queued);MH_RemoveHook(queued);originalQueuedSpawn=nullptr;
  MH_DisableHook(patrol);MH_RemoveHook(patrol);patrolTrampoline=nullptr;
  MH_DisableHook(spawn);MH_RemoveHook(spawn);originalSpawn=nullptr;
  MH_DisableHook(address);MH_RemoveHook(address);originalAcquire=nullptr;return false;
 }return true;
}

bool Participant(unsigned slot,unsigned handle){
 if(slot!=1||!handle||!owner||handle!=opponentHandle||!Duel()||owner!=MWELNeoCurrentKey())return false;
 const auto car=Vehicle(handle);unsigned driver=0,ai=0;
 return car&&reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car)&&Read(car+0x94,driver)&&driver==2&&Read(car+0x54,ai)&&Table(ai,0x892480);
}
bool ControlBlocked(){
 using Fn=unsigned(__cdecl*)(unsigned);static Fn query=nullptr;
 if(!query){auto module=GetModuleHandleW(L"MWArmsAssist.asi");if(module)query=reinterpret_cast<Fn>(GetProcAddress(module,"MWAA_CombatControlBlockedV1"));}
 return query&&query(opponentHandle)!=0;
}
bool Boost(unsigned vehicle,float* output){
 if(!vehicle||vehicle!=boostVehicle||!output||!std::isfinite(*output)||*output<=0||!owner||!Duel()||Handle(vehicle)!=opponentHandle)return false;
 unsigned driver=0;if(!Read(vehicle+0x94,driver)||driver!=2)return false;
 if(ControlBlocked())return false;
 const float result=*output*powerScale;if(!std::isfinite(result))return false;
 nativePower=*output;boostedPower=result;*output=result;++powerCalls;return true;
}
int Heat(unsigned player){
 const auto package=event_race_policies::Current();auto setting=package->authoring.find("startHeatLevel");float wanted=1;
 if(setting!=package->authoring.end()&&setting->is_number())wanted=setting->get<float>();
 else {unsigned com=0,perp=0,fn=0;
  if(Read(player-0x80+4,com)&&com){perp=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x4037E0);
   unsigned table=0;if(Read(perp,table)&&Read(table+4,fn)&&fn==0x409400)Read(perp+0x1C,wanted);
  }
 }
 return std::isfinite(wanted)?std::clamp(int(std::floor(wanted)),1,10):1;
}
bool ReleaseChaser(unsigned car){
 unsigned manager=0,ai=0,before=0,after=0;
 if(!Table(car,0x8AA828)||!Read(car+0x54,ai)||!Table(ai,0x892480)||Roadblock(car,ai)||
    !reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car)||!Read(0x92C67C,manager)||!Table(manager,0x891900))return false;
 // 92C67C is ICopMgr, at +54 in AICopManager. Native callers of ReleaseCop
 // (406450) pass the complete object, just as 42ED7C/42ED9B subtract 54
 // before AcquireCop. A raw UnSpawn skips pursuit/pool bookkeeping.
 const auto complete=manager-0x54;
 if(!Read(complete+0x94,before)||!before||before>512)return false;
 reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x406450)(complete,car);
 Read(complete+0x94,after);
 const bool inactive=!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car);
 Log("GAUNTLET_CHASER_RELEASE handle=%u active_before=%u active_after=%u inactive=%u complete_manager=1",Handle(car),before,after,unsigned(inactive));
 return inactive&&after+1==before;
}
void RestoreLeaderHealth(){
 auto& binding=combat_health_runtime::bindings[1];float raw=0;
 if(binding.handle&&combat_health_runtime::Native(binding.damage,binding.handle)&&Read(binding.damage+0x3C,raw)&&std::abs(raw-binding.written)<.00001f)
  *reinterpret_cast<float*>(binding.damage+0x3C)=binding.original;
 binding={};combat_health_runtime::ledger.racers[1]={};combat_health_runtime::impactContacts[1]={};combat_health_runtime::motions[1]={};
}
void Leader(unsigned car){
 bootstrapConfirmed=true;
 if(Handle(car)==opponentHandle)return;
 RestoreLeaderHealth();opponentHandle=Handle(car);boostVehicle=car;
 originValid=modelLogged=false;policeModel.clear();powerCalls=0;nativePower=boostedPower=0;
}
bool Ordinary(unsigned car,unsigned pursuit){
 unsigned ai=0,driver=0,activity=0,object=0;
 if(!car||!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||
    Roadblock(car,ai)||SUV(car)||!Read(ai+0x70,activity)||activity!=pursuit||!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car))return false;
 if(!Read(car-0x80+4,object))return false;
 auto damage=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(object,0x405000);
 return Table(damage,0x8AD40C)&&!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x6B89A0)(damage);
}
unsigned NearbyLeader(const std::vector<unsigned>& cars,const combat_impact::Motion& player,unsigned pursuit,float maximum){
 unsigned candidate=0;float best=maximum;
 for(auto car:cars){if(Handle(car)==opponentHandle||!Ordinary(car,pursuit))continue;
  combat_impact::Motion other;if(!combat_health_runtime::NativeMotion(car-0x80,other))continue;
  const auto d=other.position-player.position;float distance=std::hypot(d.x,d.z);
  if(std::abs(d.y)<10&&distance<best){candidate=car;best=distance;}
 }return candidate;
}
bool RetireOldPolice(unsigned handle){
 const auto old=Vehicle(handle);unsigned ai=0,roadblock=0;
 if(old&&reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(old)){
  if(!Read(old+0x54,ai)||!Table(ai,0x892480)||!Read(ai+0x74,roadblock)||roadblock)return false;
 }
 if(combat_health_runtime::bindings[1].handle==handle)RestoreLeaderHealth();
 // Release through the native manager, including pursuit membership and pool
 // counters. The patrol guard does not intercept this explicit handoff.
 if(ai&&!ReleaseChaser(old))return false;
 const auto fresh=Vehicle(handle);
 return !fresh||!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(fresh);
}
void Backup(unsigned manager,const combat_impact::Motion& player,const combat_impact::Motion& cop,const std::vector<unsigned>& cars){
 const float now=player.sampledAt;
 const float playerSpeed=std::hypot(player.velocity.x,player.velocity.z);
 trail.Observe(player.position,player.forward,player.velocity.y,now,playerSpeed);
 if(handoffPending){
  if(now-handoffAt>8){handoffPending=false;Log("GAUNTLET_BACKUP_REFUSED native_queue_timeout=1 original_released_by_backup=0");}
  return;
 }
 const float dx=player.position.x-cop.position.x,dz=player.position.z-cop.position.z;
 const float distance=std::sqrt(dx*dx+dz*dz);
 // Explicit isolated-lab stimulus exercises native replacement without
 // moving either car or changing production distance/hysteresis settings.
 unsigned playerManager=0,pursuit=0;Read(0x993CC8,playerManager);Read(playerManager+0x130,pursuit);
 const bool stimulus=diagnosticBackup&&!diagnosticBackupRequested&&trail.count>=16&&ChaseCount(pursuit)>=2;
 if(!trail.Due(stimulus?650.f:distance,now)||now<backupRetryAt||policeModel.empty())return;
 backupRetryAt=now+2;
 // Prefer an existing nearby ordinary cop; demoted leader resumes native
 // health/performance without despawning and weapon CT remains event-owned.
 if(auto nearby=NearbyLeader(cars,player,pursuit,stimulus?350.f:std::min(350.f,distance-100.f))){
  const auto old=opponentHandle;Leader(nearby);trail.Committed(now);
  if(stimulus)diagnosticBackupRequested=true;
  Log("GAUNTLET_LEADER_PROMOTED old=%u new=%u existing_nearby=1 old_native_health_restored=1 chase_limit=4 test_stimulus=%u",old,opponentHandle,unsigned(stimulus));return;
 }
 // A replacement must fit the four-car chase budget. Free only a distant
 // ordinary nonleader, never a block car, helicopter, or on-screen nearby cop.
 if(ChaseCount(pursuit)>=ChaseLimit){
  unsigned distant=0;float farthest=400;
  for(auto car:cars){if(Handle(car)==opponentHandle||!Ordinary(car,pursuit))continue;combat_impact::Motion other;
   if(!combat_health_runtime::NativeMotion(car-0x80,other))continue;auto d=other.position-player.position;float gap=std::hypot(d.x,d.z);
   const auto offscreen=reinterpret_cast<float(__thiscall*)(unsigned)>(0x688120)(car);
   if(gap>farthest&&std::isfinite(offscreen)&&offscreen>=3){distant=car;farthest=gap;}
  }
  if(!distant||!ReleaseChaser(distant))return;
 }
 gauntlet_backup::Anchor anchor;combat_impact::Vector position{},forward{};bool ahead=false;
 auto safe=[&](const gauntlet_backup::Anchor& candidate,bool front){
  const auto delta=candidate.position-player.position;float separation=std::hypot(delta.x,delta.z);
  if(separation<(front?180.f:65.f)||separation>(front?330.f:145.f))return false;
  float length=std::hypot(candidate.forward.x,candidate.forward.z);if(length<.5f)return false;
  // A forward intercept faces the arriving player. The rear fallback faces
  // the same direction as the player's verified recent road movement.
  float sign=front?-1.f:1.f;forward={sign*candidate.forward.x/length,0,sign*candidate.forward.z/length};
  mwel_grid::Layout layout;float rotation=std::atan2(-forward.x,forward.z)*57.295779513f;
  if(!MWELFitGrid(owner,1,{candidate.position.x,candidate.position.y,candidate.position.z},rotation,layout)||!layout.valid)return false;
  position={layout.positions[0][0],layout.positions[0][1],layout.positions[0][2]};
  for(auto car:cars){combat_impact::Motion other;if(Handle(car)==opponentHandle||!combat_health_runtime::NativeMotion(car-0x80,other))continue;
   const auto d=other.position-position;if(std::abs(d.y)<5&&std::hypot(d.x,d.z)<14)return false;
  }return true;
 };
 unsigned race=0,leg=0;Read(0x91E000,race);Read(race+0x18+0x2C,leg);
 float point[3]{},tangent[3]{};
 if(MWELCourseAhead(owner,&player.position.x,leg,300,point,tangent)){
  anchor.position={point[0],point[1],point[2]};anchor.forward={tangent[0],tangent[1],tangent[2]};anchor.speed=std::min(playerSpeed,30.f);ahead=safe(anchor,true);
 }
 if(!ahead&&(!trail.Select(anchor)||!safe(anchor,false)))return;
 Log("GAUNTLET_BACKUP_PLACEMENT ahead=%u route_offset_metres=%d native_ground_and_clearance=1",unsigned(ahead),ahead?300:-100);

 handoffBefore.clear();for(auto car:cars)handoffBefore.push_back(Handle(car));
 handoffOld=opponentHandle;handoffPosition=position;handoffForward=forward;handoffAt=now;handoffPending=true;trail.Committed(now);
 handoffSpeed=std::min(anchor.speed,playerSpeed);
 if(stimulus){diagnosticBackupRequested=true;Log("GAUNTLET_BACKUP_TEST threshold_stimulus=1 actual_gap=%.2f production_distance_acceptance=0",distance);}
 // Queue the replacement first. The original remains valid and invincible
 // until a new active vehicle is observed at the checked road placement.
 replacementSpawn=true;originalSpawn(manager,&position,&forward,policeModel.c_str(),true,false);replacementSpawn=false;
 Log("GAUNTLET_BACKUP_REQUEST old=%u gap=%.2f time=%.3f model=%s position=%.3f,%.3f,%.3f forward=%.3f,%.3f original_retained=1",handoffOld,distance,now,policeModel.c_str(),position.x,position.y,position.z,forward.x,forward.z);
}
void Frame(){
 if(!Current()){if(owner)Reset();return;}
 if(worldLogEnabled&&!diagnosticArrest){unsigned playerManager=0,pursuit=0;BYTE busted=0;
  if(Read(0x993CC8,playerManager)&&Read(playerManager+0x130,pursuit)&&Table(pursuit,0x892770)&&Read(pursuit+0xE8,busted)&&busted){
   diagnosticArrest=true;Log("GAUNTLET_NATIVE_ARREST pursuit=%08X native_busted=1 direct_loss_or_menu=0",pursuit);
  }
 }
 unsigned race=0,count=0,playerHandle=0;BYTE active=0,timer=0,finished=0;unsigned outcome=0;
 if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count!=1||!Read(race+0x198A,active)||!Read(race+0x1978,timer))return;
 if(!Read(race+0x18+0x1C,outcome)||!Read(race+0x18+0x21,finished))return;
 if(worldLogEnabled&&(outcome!=diagnosticOutcome||bool(finished)!=diagnosticFinished)){
  diagnosticOutcome=outcome;diagnosticFinished=finished!=0;
  Log("GAUNTLET_NATIVE_OUTCOME flags=%08X busted=%u finished=%u native_rules=1 forced_result=0",outcome,unsigned((outcome>>24)&255),unsigned(finished));
 }
 if(outcome||finished||!active||!timer){boostVehicle=0;return;}
 unsigned playerManager=0,playerPursuit=0;BYTE busted=0;
 if(Read(0x993CC8,playerManager)&&Read(playerManager+0x130,playerPursuit)&&Table(playerPursuit,0x892770)&&Read(playerPursuit+0xE8,busted)&&busted){boostVehicle=0;return;}
 const auto key=MWELNeoCurrentKey();if(owner!=key){Reset();owner=key;refused=!Surface();diagnosticBackupRequested=false;diagnosticBackup=race_start_experiment::enabled&&worldLogEnabled&&mwel::fs::exists(game/L"MWEventLoader.gauntlet-backup-test")&&mwel::Read(game/L"MWEventLoader.gauntlet-backup-test",128)=="MW_EVENT_LOADER_GAUNTLET_BACKUP_V1\n";Log("GAUNTLET_SURFACE owner=%08X verified=%u",owner,unsigned(!refused));}
 if(refused||GetTickCount64()<next)return;next=GetTickCount64()+100;
 if(!Read(race+0x18,playerHandle))return;const auto player=Vehicle(playerHandle);if(!player)return;
 unsigned manager=0;if(!Read(0x92C67C,manager)||!Table(manager,0x891900))return;
 ApplyDeployment(manager,ChaseLimit);
 const auto now=GetTickCount64();
 const bool retry=gauntlet_bootstrap::Retry(requested&&!bootstrapConfirmed&&bootstrapAttempts<8,opponentHandle!=0,handoffPending,
   PendingDeployment(manager),ChaseCount(playerPursuit),ChaseLimit,now,nextPursuit);
 if(!requested||retry){
  combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(player-0x80,motion))return;
  // Native one-racer Tollbooth setup has a 60 second cop lockout. Unlock this
  // explicit weapon mode, create its native pursuit at the authored heat.
  // The native factory selects from the receiving game's police roster.
  requestedHeat=Heat(player);
  reinterpret_cast<void(__thiscall*)(unsigned,bool)>(0x411410)(manager,false);
  reinterpret_cast<void(__thiscall*)(unsigned,int)>(0x42EBF0)(manager,requestedHeat);
  // PursueAtHeatLevel already acquires one patrol cop through the native
  // factory. Do not enqueue a duplicate alongside that native request.
  requested=true;++bootstrapAttempts;nextPursuit=now+2000;
  Log("GAUNTLET_POLICE_REQUEST owner=%08X heat=%d native_factory=1 native_roster=1 roadblock_and_heli_native=1 retry_empty=%u attempt=%u",owner,requestedHeat,unsigned(retry),bootstrapAttempts);
  if(!retry)return;
 }
 std::vector<unsigned> cars;if(!List(cars))return;unsigned cops=0;
 for(auto car:cars){unsigned driver=0,ai=0;
  if(Read(car+0x94,driver)&&driver==2&&Read(car+0x54,ai)&&Table(ai,0x892480)&&!Roadblock(car,ai)&&SUV(car)){
   if(Handle(car)==opponentHandle){RestoreLeaderHealth();opponentHandle=boostVehicle=0;}
   if(ReleaseChaser(car))Log("GAUNTLET_SUV_EXCLUDED handle=%u native_release=1 roadblock_untouched=1",Handle(car));
  }
 }
 // Refresh after rejecting a pooled SUV so it cannot be counted or promoted.
 cars.clear();if(!List(cars))return;
 for(auto car:cars){unsigned ai=0,driver=0;if(Read(car+0x94,driver)&&driver==2&&Read(car+0x54,ai)&&Table(ai,0x892480))Roadblock(car,ai);}
 if(handoffPending)for(auto car:cars){
  unsigned ai=0,roadblock=0,driver=0;const auto handle=Handle(car);
  if(!handle||std::find(handoffBefore.begin(),handoffBefore.end(),handle)!=handoffBefore.end()||!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||!Read(ai+0x74,roadblock)||roadblock)continue;
  combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(car-0x80,motion))continue;
  const auto d=motion.position-handoffPosition;if(std::hypot(d.x,d.z)>35||std::abs(d.y)>8)continue;
  // SpawnCop alone can select the opposite native road direction. Initialize
  // the road navigator at the checked pose before releasing the old car.
  const bool placed=reinterpret_cast<bool(__thiscall*)(unsigned,const combat_impact::Vector*,const combat_impact::Vector*)>(0x422730)(ai,&handoffPosition,&handoffForward);
  combat_impact::Motion aligned;const bool read=placed&&combat_health_runtime::NativeMotion(car-0x80,aligned);
  const auto actual=aligned.position-handoffPosition;
  const float heading=read?combat_impact::Dot(aligned.forward,handoffForward):0;
  if(!read||std::hypot(actual.x,actual.z)>3||std::abs(actual.y)>3||heading<.7f){
   Log("GAUNTLET_BACKUP_REFUSED road_alignment_failed=1 old_retained=1 new=%u heading=%.3f",handle,heading);
   ReleaseChaser(car);handoffPending=false;break;
  }
  // A replacement joins already rolling at a bounded speed observed on this
  // actual road. Starting at rest lets native distance culling remove it
  // before it can catch a fast player. Steering/braking remain native.
  reinterpret_cast<void(__thiscall*)(unsigned,float)>(0x670210)(car,handoffSpeed);
  if(!RetireOldPolice(handoffOld)){Log("GAUNTLET_BACKUP_REFUSED old_release_failed=1 old=%u new=%u",handoffOld,handle);break;}
  Leader(car);handoffPending=false;handoffBefore.clear();
  Log("GAUNTLET_BACKUP_COMMIT old=%u new=%u old_active=0 native_unspawn=1 weapon_ct_owner=event road_heading=%.3f rolling_mps=%.3f",handoffOld,handle,heading,handoffSpeed);break;
 }
  const auto previousCop=Vehicle(opponentHandle);
 if(opponentHandle&&(!previousCop||!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(previousCop))){RestoreLeaderHealth();opponentHandle=0;boostVehicle=0;originValid=false;}
 if(!opponentHandle&&!handoffPending&&playerPursuit){combat_impact::Motion motion;
  if(combat_health_runtime::NativeMotion(player-0x80,motion))if(auto nearby=NearbyLeader(cars,motion,playerPursuit,100000.f))Leader(nearby);
 }
 for(auto car:cars){unsigned driver=0,ai=0,roadblock=0,activity=0;if(!Read(car+0x94,driver)||driver!=2)continue;
  if(Read(car+0x54,ai)&&Table(ai,0x892480)&&!Roadblock(car,ai))++cops;
  // Ambient police pursuing another vehicle never become this duel's owner.
  const auto handle=Handle(car);if(!opponentHandle&&!handoffPending&&playerPursuit&&handle&&Ordinary(car,playerPursuit))Leader(car);
 }
 // Capacity is enforced at native admission. Never erase visible surplus cars.
 if(worldLogEnabled){static ULONGLONG supportAt=0;if(GetTickCount64()>=supportAt){supportAt=GetTickCount64()+5000;
  unsigned helicopters=0,blocks=0,ground=0,air=0;Read(manager-0x54+0x94,ground);Read(manager-0x54+0x9C,air);
  for(auto car:cars){unsigned ai=0;if(!Read(car+0x54,ai))continue;if(Helicopter(ai))++helicopters;else if(Table(ai,0x892480)&&Roadblock(car,ai))++blocks;}
  Log("GAUNTLET_SUPPORT_STATE chase=%u limit=4 roadblock_cars=%u heli_active=%u native_ground_pool=%u native_air_pool=%u leader=%u",ChaseCount(playerPursuit),blocks,helicopters,ground,air,opponentHandle);
 }}
 const auto cop=Vehicle(opponentHandle);unsigned ai=0,driver=0;if(!cop||!Read(cop+0x94,driver)||driver!=2||!Read(cop+0x54,ai)||!Table(ai,0x892480))return;
 boostVehicle=cop;
 combat_impact::Motion playerMotion,copMotion;
 if(combat_health_runtime::NativeMotion(player-0x80,playerMotion)&&combat_health_runtime::NativeMotion(cop-0x80,copMotion)){
  const float dx=playerMotion.position.x-copMotion.position.x,dz=playerMotion.position.z-copMotion.position.z;
  powerScale=1.75f*(1.f+.35f*std::clamp((std::sqrt(dx*dx+dz*dz)-100.f)/500.f,0.f,1.f));
  if(!ControlBlocked())MWELGauntletRam(ai,playerMotion,copMotion);
  Backup(manager,playerMotion,copMotion,cars);
 }
 if(!modelLogged){
  const BYTE expected[]={0x8B,0x41,0x2C,0x8B,0x40,0x24,0xC3};BYTE code[7]{};
  if(Copy(reinterpret_cast<void*>(0x688090),code,7)&&!memcmp(code,expected,7)){
   auto name=reinterpret_cast<const char*(__thiscall*)(unsigned)>(0x688090)(cop);
   if(name&&strnlen_s(name,64)<64){policeModel=name;Log("GAUNTLET_POLICE_MODEL heat=%d handle=%u model=%s",requestedHeat,opponentHandle,name);modelLogged=true;}
  }
 }
 unsigned pursuit=0,target=0,targetSim=0;BYTE inPursuit=0;
 Read(ai+0x70,pursuit);Read(ai-0x4C+0x760,inPursuit);Read(ai+0x54,target);if(target)Read(target+0x1C,targetSim);
 // A normal pursuit may end after line-of-sight loss. This authored duel
 // keeps its one opponent pursuing until the Tollbooth rules end the event.
 // Rejoin through the native activity API; never invent an activity pointer.
 if((!pursuit||!inPursuit)&&GetTickCount64()>=nextPursuit){
  nextPursuit=GetTickCount64()+2000;
  reinterpret_cast<void(__thiscall*)(unsigned,bool)>(0x411410)(manager,false);
  reinterpret_cast<void(__thiscall*)(unsigned,int)>(0x42EBF0)(manager,requestedHeat);
  unsigned playerManager=0,activity=0,activityTarget=0,actualTarget=0,add=0;
  if(Read(0x993CC8,playerManager)&&Read(playerManager+0x130,activity)&&Table(activity,0x892770)&&Read(0x892770+30*4,add)&&add==0x4338A0){
   activityTarget=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(0x40AE70)(activity);
   if(Read(activityTarget+0x1C,actualTarget)&&actualTarget==player-0x80)
    reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(add)(activity,cop);
  }
  Read(ai+0x70,pursuit);Read(ai-0x4C+0x760,inPursuit);
  Log("GAUNTLET_PURSUIT_REJOIN handle=%u native_activity=%08X active=%u",opponentHandle,pursuit,unsigned(inPursuit));
 }
 if(worldLogEnabled){float meter=0,evade=0;unsigned status=0;Read(playerPursuit+0x110,meter);Read(playerPursuit+0xFC,evade);Read(playerPursuit+0x218,status);
  Log("GAUNTLET_PURSUIT_METER value=%.5f evade_level=%.5f native_status=%u cap_escape=-0.95000",meter,evade,status);
 }
 Log("GAUNTLET_CONTROLLER owner=%08X handle=%08X ai=%08X driver=%u pursuit=%08X active=%u target=%08X cops=%u",owner,opponentHandle,ai,driver,pursuit,unsigned(inPursuit),targetSim,cops);
 Log("GAUNTLET_POWER handle=%u scale=%.3f calls=%u native=%.3f boosted=%.3f native_police_driving=1",opponentHandle,powerScale,powerCalls,nativePower,boostedPower);
 if(worldLogEnabled){combat_impact::Motion motion;
  if(combat_health_runtime::NativeMotion(cop-0x80,motion)){
   if(!originValid){origin=motion.position;originValid=true;}
   const float dx=motion.position.x-origin.x,dz=motion.position.z-origin.z;
   Log("GAUNTLET_POLICE_BODY handle=%u controller=%08X driver=2 moved_metres=%.3f position=%.3f,%.3f,%.3f",opponentHandle,ai,std::sqrt(dx*dx+dz*dz),motion.position.x,motion.position.y,motion.position.z);
  }
 }
}
}
