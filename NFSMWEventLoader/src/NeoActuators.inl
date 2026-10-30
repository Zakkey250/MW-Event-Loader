#include "../../World2MW/src/NeoAI/Driver.h"
#include "../../World2MW/src/NeoAI/JamWatch.h"
#include "../../World2MW/src/NeoAI/Nitrous.h"
#include "../../World2MW/src/NativeLoader/NeoBody.h"
#include "NeoSimulationStep.h"
#include "NeoDriftIntent.h"
#include "NeoDriftPhysics.h"
#include "NeoPostGoalCoast.h"
#include "NeoNativePowerTrim.h"
#include "NeoNativeSlipLead.h"
#include "../../MW-Arcade-Drift-public/src/NativePadSteering.h"
using NeoDrivingFn=void(__thiscall*)(void*,float);
using NeoInputFn=void(__thiscall*)(void*,float);
static NeoDrivingFn neoOriginalDriving=nullptr;
static NeoInputFn neoOriginalInput[4]{};
using NeoNOSFn=void(__thiscall*)(void*,bool);
static NeoNOSFn neoOriginalNOS=nullptr;
static bool neoNitrousInstalled=false;
static bool neoInstalled=false;static DWORD neoWriter=0;
static thread_local void* neoUpdatingPrimary=nullptr;
static thread_local bool neoUpdateDrove=false;
static void NeoResumeAfterNativeAction(void* primary,float dt);
using NeoWrongWayFn=bool(__thiscall*)(void*);
static NeoWrongWayFn neoOriginalWrongWay=nullptr;
static mwel_difficulty::Settings neoDifficulty;
static thread_local std::uintptr_t neoDrivingCaller=0;
struct NeoRuntimeState {
 void* ai=nullptr;void* input=nullptr;void* race=nullptr;unsigned handle=0;bool owned=false;
 mwneo::Driver driver;mwneo::Guidance guidance;mwneo::Controls controls;ULONGLONG sampled=0,logged=0,activeAt=0;
 unsigned ticks=0,rejectedNative=0,guidanceFailures=0,fallbacks=0;
 unsigned junkmanAttempts=0;ULONGLONG junkmanNext=0;bool junkmanApplied=false;
 unsigned powerCalls=0;float lastPowerNative=0,lastPowerBoosted=0,powerScale=1.75f;ULONGLONG powerLogged=0,difficultySampled=0;
 mwel_difficulty::RaceSample difficultyRace{};
 mwneo::JamWatch jam{};unsigned respawns=0,respawnRefused=0;
 mwneo::Nitrous nitrous{};bool nosEquipped=false,nosEngaged=false,nosRead=false;float nosTank=0,nosBoost=0,nosRequestedSeconds=0,nosEngagedSeconds=0;
 float requested[3]{},postwrite[3]{};ULONGLONG diagnosticAt=0;unsigned diagnosticPhase=0;
 std::array<mwneo::Point,65> testPoints{};unsigned testPointCount=0;bool testPointsReady=false;float testIntegral=0,testSteering=0;
 bool brakeProbeStarted=false,brakeProbeFinished=false,brakeProbeValid=true;float brakeProbeInitial=0,brakeProbeTime=0,brakeProbeGrade=0;
 bool genuineGoalHeld=false;
 NeoPostGoalCoast genuineGoalCoast;
 NeoNativePowerTrim nativePowerTrim;
 mcd::NativePadSteering nativeTurnDemand;
 mwneo::Point groundPosition{};float groundExtent=0;
 NeoDriftIntent driftIntent;bool driftIntentActive=false;
 mwneo::Point driftMotionHeading{},driftMotionTravel{};bool driftMotionReady=false;
 float driftBodyRate=0,driftTravelRate=0,driftClearSeconds=-1;ULONGLONG driftClearAt=0;
 bool driftSelected=false;float driftLearningHold=0,driftSteeringLock=0;ULONGLONG driftLogged=0;
 NeoSimulationStep simulationStep;
};
static NeoRuntimeState neoRuntime[16];
// Leases cannot live inside the copied/reset row. Release before every row reset.
static std::array<mcdai::EventLoaderClient,16> neoDriftClients;
static std::array<mwel_drift::SuspensionLease,16> neoDriftPhysics;
static std::uint64_t neoDriftGeneration=1;
static mcdai::EventLoaderClient& NeoDriftClient(NeoRuntimeState& row){return neoDriftClients[&row-neoRuntime];}
static void NeoDriftRelease(NeoRuntimeState& row){
 NeoDriftClient(row).Reset();auto& lease=neoDriftPhysics[size_t(&row-&neoRuntime[0])];const auto handle=row.handle?row.handle:lease.handle;
 auto restored=lease.Restore();
 if(restored==3){lease.failed=true;Log(LogLevel::Warning,"EVENT_DRIFT_SUSPENSION_REFUSED handle=%u restoration_unverified=1",handle);}
 else if(restored)Log(LogLevel::Info,"EVENT_DRIFT_SUSPENSION_RESTORED handle=%u result=%u",handle,restored);
 row.driftLearningHold=0;row.driftIntent.Reset();row.driftIntentActive=false;
 row.nativePowerTrim={};
 row.nativeTurnDemand.reset();
 row.driftMotionReady=false;row.driftClearSeconds=-1;row.driftClearAt=0;row.driftBodyRate=row.driftTravelRate=0;
}

static bool NeoLocalPlayer();
#include "NeoOwnership.inl"
static unsigned NeoSelectedController(){
 static ULONGLONG refreshAt=0;static unsigned selected=0;auto now=GetTickCount64();
 if(now<refreshAt)return selected;
 char file[MAX_PATH]{};if(!MWELNeoSettingsFile(file,sizeof(file)))return 0;
 selected=GetPrivateProfileIntA("NeoAI","Enabled",1,file)?2u:0u;refreshAt=now+1000;return selected;
}
static HMODULE NeoProviderModule(bool& owned){
 auto events=GetModuleHandleW(L"MWEventLoader.asi");using Owned=unsigned(__cdecl*)();
 static Owned owns=nullptr;if(!owns&&events)owns=reinterpret_cast<Owned>(GetProcAddress(events,"MWEL_NeoCourseOwnedV1"));
 owned=owns&&owns();return owned?events:nullptr;
}
static bool NeoLocalPlayer(){
 auto events=GetModuleHandleW(L"MWEventLoader.asi");using LocalTest=unsigned(__cdecl*)();
 static LocalTest local=nullptr;if(!local&&events)local=reinterpret_cast<LocalTest>(GetProcAddress(events,"MWEL_LocalRaceTestV1"));
 return local&&local();
}
static NeoRuntimeState* NeoOwned(void* ai){for(auto& s:neoRuntime)if(s.ai==ai&&s.owned&&GetTickCount64()-s.activeAt<250)return &s;return nullptr;}
using NeoPlaceFn=bool(__thiscall*)(void*,float*,float*);
static NeoPlaceFn neoOriginalPlacement=nullptr;
static thread_local const float* neoPlacementPoint=nullptr;
bool MWELNeoPlaceVehicle(void* ai,float* position,float* heading){
 if(!neoOriginalPlacement)return false;
 const auto prior=neoPlacementPoint;bool result=false;neoPlacementPoint=position;
 __try{result=neoOriginalPlacement(ai,position,heading);}__finally{neoPlacementPoint=prior;}
 return result;
}
static bool __fastcall NeoNativePlacement(void* ai,void*,float* position,float* heading){
 if(neoInstalled&&MWELNeoCourseEnabled())if(auto* row=NeoOwned(ai)){
  // WRoadNav can refer to an earlier or opposite-direction road. The owned
  // planner validates its own grounded placement without changing CP/laps.
  Log(LogLevel::Info,"EVENT_NEOAI_NATIVE_PLACEMENT_RETAINED handle=%u caller=%08X owned_recovery_only=1 CP_lap_writes=0",row->handle,unsigned(_ReturnAddress()));
  return true;
 }
 return neoOriginalPlacement(ai,position,heading);
}
using NeoPhysicsModeFn=void(__thiscall*)(void*,int);
static NeoPhysicsModeFn neoOriginalPhysicsMode=nullptr;
// Native simple physics follows WRoadNav, not the event-owned directed CP
// road. On a shared out-and-back road it can rotate/propel an owned racer in
// the opposite direction between driving ticks. Preserve native full physics
// only for the controller we currently own; staging, retirement, police,
// traffic and native/NFRR drivers keep every original mode transition.
static void __fastcall NeoPhysicsMode(void* vehicle,void*,int mode){
 if(mode==2&&neoInstalled&&MWELNeoCourseEnabled()&&BattleVtable(vehicle,0x8AA828)){
  void* ai=nullptr;
  if(AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&ai)&&NeoOwned(ai)){
   Log(LogLevel::Info,"EVENT_NEOAI_FULL_PHYSICS_RETAINED handle=%u requested=2 retained=1 caller=%08X owned_controller_only=1",NeoOwned(ai)->handle,unsigned(_ReturnAddress()));
   mode=1;
  }
 }
 neoOriginalPhysicsMode(vehicle,mode);
}
static void NeoLogNativeUpgradeLevels(void* vehicle,unsigned handle){
 const unsigned char guard[]={0x6A,0xFF,0x68,0x38,0xA9,0x87,0x00};
 if(memcmp(reinterpret_cast<void*>(Address(0x672D30)),guard,sizeof(guard)))return;
 __try {
  const unsigned char nameGuard[]={0x8B,0x41,0x2C,0x8B,0x40,0x24,0xC3};
  if(!memcmp(reinterpret_cast<void*>(Address(0x688090)),nameGuard,sizeof(nameGuard))){
   const char* model=reinterpret_cast<const char*(__thiscall*)(void*)>(Address(0x688090))(vehicle);
   if(model&&strnlen_s(model,64)<64)Log(LogLevel::Info,"MW_NEOAI_NATIVE_MODEL handle=%u physical_model=%s",handle,model);
  }
  // IVehicle::GetVehicleAttributes (688070) returns this exact +24 surface.
  // Read effective attribute upgrade levels even when no customization
  // record exists (the no-save player uses native performance matching).
  auto& attributes=*reinterpret_cast<const NFSPluginSDK::MW05::Attrib::Gen::pvehicle*>(static_cast<unsigned char*>(vehicle)+0x24);
  unsigned levels[7]{};
  for(unsigned i=0;i<7;++i)levels[i]=unsigned(NFSPluginSDK::MW05::Physics::Upgrades::Package::GetLevel(attributes,NFSPluginSDK::MW05::Physics::Upgrades::Type(i)));
  Log(LogLevel::Info,"MW_NEOAI_EFFECTIVE_UPGRADES handle=%u native_attribute_levels=1 tires=%u brakes=%u chassis=%u transmission=%u engine=%u induction=%u nos=%u",handle,levels[0],levels[1],levels[2],levels[3],levels[4],levels[5],levels[6]);
 }__except(EXCEPTION_EXECUTE_HANDLER){Log(LogLevel::Warning,"MW_NEOAI_EFFECTIVE_UPGRADES handle=%u read_failed=1",handle);}
}
static void (__cdecl* neoOriginalEngineOutput)()=nullptr;
static bool neoEngineOutputInstalled=false;
static void __cdecl NeoBoostEngineOutput(void* engine,float* output){
 // EngineRacer's primary object owns the vehicle at +48; its verified
 // IEngine interface is +54. Match that vehicle's exact acquired AI owner.
 void* vehicle=nullptr;void* ai=nullptr;
 if(!engine||!output||(!BattleVtable(static_cast<unsigned char*>(engine)+0x54,0x8AB6E0)&&!BattleVtable(static_cast<unsigned char*>(engine)+0x54,0x8ABF88))||
    !AudioRead(static_cast<unsigned char*>(engine)+0x48,&vehicle)||!vehicle||!AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&ai))return;
 if(MWELGauntletBoostOutput(vehicle,output))return;
 auto* row=NeoOwned(ai);if(!row)return;
 float native=*output,result=mwel_difficulty::Output(native,row->powerScale);if(result==native)return;
 *output=result;++row->powerCalls;row->lastPowerNative=native;row->lastPowerBoosted=result;
}
// 006B002A follows the stock positive-output catchup calculation. Preserve
// all registers/flags and replay the displaced mov ecx,[esi+84] via MinHook.
static void __declspec(naked) NeoEngineOutputHook(){
 __asm {
  pushfd
  pushad
  mov ebx,esp
  and esp,0FFFFFFF0h
  sub esp,512
  fxsave [esp]
  lea eax,[ebx+64]
  push eax
  push esi
  call NeoBoostEngineOutput
  add esp,8
  fxrstor [esp]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [neoOriginalEngineOutput]
 }
}
static bool InstallNeoEngineOutput(){
 if(neoEngineOutputInstalled)return true;
 const unsigned char guard[]={0x8B,0x8E,0x84,0x00,0x00,0x00,0x51,0x8B,0xCE,0xE8};
 auto target=reinterpret_cast<void*>(Address(0x6B002A));
 if(memcmp(target,guard,sizeof(guard))){Log(LogLevel::Warning,"MW_NEOAI_POWER_REFUSED engine_output_guard=006B002A");return false;}
 bool ok=MH_CreateHook(target,&NeoEngineOutputHook,reinterpret_cast<void**>(&neoOriginalEngineOutput))==MH_OK;
 if(ok&&MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);ok=false;}
 Log(LogLevel::Info,"MW_NEOAI_POWER installed=%u base_scale=%.3f catchup_enabled=%u catchup_max=%.3f start_metres=%.1f full_metres=%.1f positive_engine_output=1 owned_racers_and_local_player=1 blacklist_curve=%u BL15=%.3f BL1=%.3f far_enabled=%u far_metres=%.1f far_catchup=%.3f",unsigned(ok),neoDifficulty.enabled?neoDifficulty.engineScale:1.f,unsigned(neoDifficulty.enabled&&neoDifficulty.catchUp),neoDifficulty.catchUpMaxScale,neoDifficulty.startMetres,neoDifficulty.fullMetres,unsigned(neoDifficulty.blacklistScaling),neoDifficulty.blacklist15Scale,neoDifficulty.blacklist1Scale,unsigned(neoDifficulty.farCatchUp),neoDifficulty.farMetres,neoDifficulty.farScale);return ok;
}
static bool NeoRoadBlocked(void* nav){
 void* primary=nullptr;return nav&&AudioRead(static_cast<unsigned char*>(nav)+0x84,&primary)&&BattleVtable(primary,kRacecarPrimaryVtable)&&NeoOwned(static_cast<unsigned char*>(primary)+0x4C);
}
static void NeoInputDispatch(unsigned channel,void* input,float value,std::uintptr_t caller){
 NeoDiagnosticWriter(input,channel,value,caller);
 if(neoWriter!=GetCurrentThreadId())for(auto& row:neoRuntime)if(row.owned&&row.input==input&&GetTickCount64()-row.activeAt<250){
  ++row.rejectedNative;return;
 }
 neoOriginalInput[channel](input,value);
}
static void __fastcall NeoSteering(void* p,void*,float v){NeoInputDispatch(0,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoGas(void* p,void*,float v){NeoInputDispatch(1,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoBrake(void* p,void*,float v){NeoInputDispatch(2,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoHandBrake(void* p,void*,float v){NeoInputDispatch(3,p,v,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
static void __fastcall NeoNOS(void* input,void*,bool pressed){
 if(neoWriter!=GetCurrentThreadId())for(auto& row:neoRuntime)if(row.owned&&row.input==input&&GetTickCount64()-row.activeAt<250)return;
 neoOriginalNOS(input,pressed);
}
static bool InstallNeoNitrous(){
 const unsigned char setter[]={0x8A,0x44,0x24,0x04,0x88,0x41,0x29,0xC2,0x04,0x00};
 auto address=reinterpret_cast<void*>(Address(0x69CC40));
 if(memcmp(address,setter,sizeof(setter)))return false;
 bool ok=MH_CreateHook(address,&NeoNOS,reinterpret_cast<void**>(&neoOriginalNOS))==MH_OK;
 if(ok&&MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);ok=false;neoOriginalNOS=nullptr;}
 Log(LogLevel::Info,"MW_NEOAI_NITROUS installed=%u native_finite_tank=1 automatic_refill=0 owned_race_only=1",unsigned(ok));return ok;
}
static bool NeoReadNitrous(void* primary,bool& equipped,float& tank,bool& engaged,float& boost){
 static_assert(offsetof(NFSPluginSDK::MW05::AIVehicle,mIEngine)==0xDC);
 void* engine=nullptr;
 if(!AudioRead(static_cast<BYTE*>(primary)+0xDC,&engine)||
    (!BattleVtable(engine,0x8AB6E0)&&!BattleVtable(engine,0x8ABF88)))return false;
 auto table=*reinterpret_cast<void***>(engine);
 if(table[7]!=reinterpret_cast<void*>(Address(0x6A03F0))||table[8]!=reinterpret_cast<void*>(Address(0x6A0410))||table[10]!=reinterpret_cast<void*>(Address(0x6A0400))||table[11]!=reinterpret_cast<void*>(Address(0x6A0430)))return false;
 equipped=reinterpret_cast<bool(__thiscall*)(void*)>(table[11])(engine);
 tank=reinterpret_cast<float(__thiscall*)(void*)>(table[7])(engine);
 engaged=reinterpret_cast<bool(__thiscall*)(void*)>(table[8])(engine);
 boost=reinterpret_cast<float(__thiscall*)(void*)>(table[10])(engine);
 return std::isfinite(tank)&&tank>=0&&tank<=1.01f&&std::isfinite(boost);
}
static bool NeoDrive(void* primary,float dt){
 if(!neoInstalled||NeoSelectedController()!=2||!MWELNeoCourseEnabled())return false;
 auto* ai=static_cast<unsigned char*>(primary)+0x4C;VehicleSnapshot racer{},player{};
 if(MWELNeoDetailedLoggingEnabled()) {static ULONGLONG next=0;auto t=GetTickCount64();if(t>=next){next=t+1000;Log(LogLevel::Info,"DRIFT_ENTRY primary=%08X local=%u mode=%u",unsigned(primary),unsigned(NeoLocalPlayer()),MWELNeoDriftMode());}}

 // Native stuck/avoidance actions can temporarily replace AIActionRace.
 // Once acquired, retain ownership while exact vehicle/race membership is
 // valid; changing a native sub-action is not permission to resume native AI.
 unsigned scopeFailure=0;
 // Exact live membership already excludes police, traffic, finished and
 // eliminated racers. Reacquisition cannot require AIActionRace: an off-road
 // racer may remain in a native stuck/avoidance action after losing ownership.
 bool raceScope=W2CRaceVehicle(ai,racer,player,NeoLocalPlayer(),false,&scopeFailure);
 // The isolated all-goal observer defers the player result, but native AI
 // finishers brake inside the trigger. Retain only a previously verified
 // racer in this SAME race, and only after its genuine goal. The NFRR scope
 // must have rejected it specifically as finished (8); no other rejection
 // can enter this bounded zero-power coast. FieldPending is always false
 // in normal gameplay. Native finish/checkpoint flags remain untouched.
 if(!raceScope&&scopeFailure==8&&BattleVtable(primary,kRacecarPrimaryVtable)){
  void* currentRace=nullptr;void* vehicle=nullptr;void* back=nullptr;
  if(AudioRead(reinterpret_cast<void*>(Address(0x91E000)),&currentRace)&&currentRace&&
     AudioRead(ai-4,&vehicle)&&BattleVtable(vehicle,0x8AA828)&&
     AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&back)&&back==ai){
   for(const auto& prior:neoRuntime)if(prior.ai==ai&&prior.race==currentRace&&MWELNeoHoldingAtGenuineGoal(prior.handle)){
    racer.pointer=vehicle;racer.handle=prior.handle;racer.driverClass=1;raceScope=true;break;
   }
  }
 }
 if(!raceScope){if(MWELNeoDetailedLoggingEnabled()){static ULONGLONG next=0;auto t=GetTickCount64();if(t>=next){next=t+1000;Log(LogLevel::Info,"DRIFT_SCOPE_FAILURE primary=%08X failure=%u",unsigned(primary),scopeFailure);}}for(auto& s:neoRuntime)if(s.ai==ai&&s.owned){if(neoNitrousInstalled&&s.input)neoOriginalNOS(s.input,false);Log(LogLevel::Info,"MW_NEOAI_RELEASE handle=%u reason=out_of_race_scope ticks=%u scope_failure=%u driver_class=%u",s.handle,s.ticks,scopeFailure,racer.driverClass);NeoDriftRelease(s);s.owned=false;}return false;}
 void* input=nullptr;void* transmission=nullptr;void* race=nullptr;
 if(!AudioRead(static_cast<unsigned char*>(primary)+0xE0,&input)||!BattleVtable(input,racer.driverClass==kDriverHuman?0x8AC670:0x8AB5C8)||
    !AudioRead(static_cast<unsigned char*>(primary)+0xD4,&transmission)||!transmission||
    !AudioRead(reinterpret_cast<void*>(Address(0x91E000)),&race)){if(MWELNeoDetailedLoggingEnabled()){static ULONGLONG next=0;auto t=GetTickCount64();if(t>=next){next=t+1000;unsigned vt=0;AudioRead(input,&vt);Log(LogLevel::Info,"DRIFT_INPUT_FAILURE primary=%08X input=%08X vt=%08X driver=%u",unsigned(primary),unsigned(input),vt,racer.driverClass);}}return false;}

 // A vehicle may enter this controller while already simplified. Acquire
 // full physics through its guarded native API before reading its body.
 if(neoOriginalPhysicsMode&&BattleVtable(racer.pointer,0x8AA828)){
  const BYTE getter[]={0x8B,0x81,0xAC,0,0,0,0xC3};
  if(!memcmp(reinterpret_cast<void*>(Address(0x688210)),getter,sizeof(getter))&&
     reinterpret_cast<int(__thiscall*)(void*)>(Address(0x688210))(racer.pointer)==2)
   neoOriginalPhysicsMode(racer.pointer,1);
 }
 unsigned handle=W2CRaceHandle(racer);NeoBodyMotion body;if(!NeoReadBody(static_cast<BYTE*>(racer.pointer),body)){Log(LogLevel::Info,"DRIFT_BODY_FAILURE handle=%u vehicle=%08X",handle,unsigned(racer.pointer));return false;}
 mwneo::Point pos{body.position[0],body.position[1],body.position[2]},heading{body.heading[0],body.heading[1],body.heading[2]};
 mwneo::Vehicle v;v.handle=handle;v.position=pos;v.heading=heading;v.velocity={body.velocity[0],body.velocity[1],body.velocity[2]};v.speed=mwneo::LengthXZ(v.velocity);v.dt=dt;v.halfWidth=body.halfWidth;v.halfLength=body.halfLength;
 NeoRuntimeState* selected=nullptr;
 for(auto& s:neoRuntime)if(s.ai==ai){selected=&s;break;}
 if(!selected)for(auto& s:neoRuntime)if(!s.owned){selected=&s;break;}if(!selected)return false;
 auto& row=*selected;
 if(row.ai!=ai||row.input!=input||row.handle!=handle||row.race!=race){NeoDriftRelease(row);row={};row.ai=ai;row.input=input;row.handle=handle;row.race=race;}
 auto now=GetTickCount64();bool wasOwned=row.owned;
 row.groundPosition=pos;row.groundExtent=v.halfLength+5.f;
 // The same simulation step may arrive through Driving and the mechanic
 // recovery hook. Reuse the already applied inputs rather than integrate
 // steering, jam timers or recovery twice.
 float simulation=reinterpret_cast<float(__cdecl*)()>(Address(0x6E8DE0))();
 const float previousSimulation=row.simulationStep.previous;
 if(!row.simulationStep.Begin(simulation))return row.owned;
 if(previousSimulation>=0&&simulation>previousSimulation)v.dt=std::min(.1f,simulation-previousSimulation);
 if(MWELNeoHoldingAtGenuineGoal(handle)){
  const bool motionReady=row.driftMotionReady;
  const auto priorHeading=row.driftMotionHeading,priorTravel=row.driftMotionTravel;
  const float priorBodyRate=row.driftBodyRate,priorTravelRate=row.driftTravelRate;
  if(!row.genuineGoalHeld)NeoDriftRelease(row);
  // A genuine finish can be deferred solely for full-field evidence.
  // Do not park inside its goal trigger. Coast without power only while the
  // real grounded chassis has a fresh complete wall sweep; stop within30m/4s.
  // This observer path cannot run in production and never follows reset CPs.
  const auto h=mwneo::UnitXZ(v.heading),t=mwneo::UnitXZ(v.velocity);
  bool coastSafe=false;
  if(!row.genuineGoalCoast.done&&motionReady&&v.dt>0&&v.dt<=.1f){
   const float bodyRate=std::atan2(mwneo::CrossXZ(priorHeading,h),mwneo::DotXZ(priorHeading,h))/v.dt;
   const float travelRate=std::atan2(mwneo::CrossXZ(priorTravel,t),mwneo::DotXZ(priorTravel,t))/v.dt;
   row.driftBodyRate=priorBodyRate+(bodyRate-priorBodyRate)*v.dt/(.12f+v.dt);
   row.driftTravelRate=priorTravelRate+(travelRate-priorTravelRate)*v.dt/(.12f+v.dt);
   unsigned wheels=0;
   coastSafe=mwel_drift::SuspensionLease::Read(unsigned(racer.pointer)+0x84,wheels)&&wheels>=3&&wheels<=4&&
    MWELNeoDriftClearSeconds(body.position,body.heading,body.velocity,v.halfWidth,v.halfLength,row.driftBodyRate,row.driftTravelRate)==.8f;
  }
  row.driftMotionReady=motionReady;row.driftMotionHeading=h;row.driftMotionTravel=t;
  const bool wasDone=row.genuineGoalCoast.done;
  const bool coast=row.genuineGoalCoast.Step(pos,v.speed,now,coastSafe);
  if(!wasDone&&row.genuineGoalCoast.done)Log(LogLevel::Info,"EVENT_NEOAI_GENUINE_GOAL_COAST_END handle=%u position=%.3f,%.3f,%.3f travelled=%.3f throttle=0 native_pose_writes=0",handle,pos.x,pos.y,pos.z,mwneo::Distance(pos,row.genuineGoalCoast.origin));
  row.owned=true;row.activeAt=now;row.controls={};row.controls.brake=coast?0.f:1.f;row.controls.valid=true;
  neoWriter=GetCurrentThreadId();neoOriginalInput[0](input,0);neoOriginalInput[1](input,0);neoOriginalInput[2](input,row.controls.brake);neoOriginalInput[3](input,0);
  if(neoNitrousInstalled)neoOriginalNOS(input,false);row.nitrous.pressed=false;neoWriter=0;++row.ticks;
  if(!row.genuineGoalHeld){row.genuineGoalHeld=true;Log(LogLevel::Info,"EVENT_NEOAI_GENUINE_GOAL_HOLD handle=%u native_finished_flag_written=0 native_timer_written=0 throttle=0 brake=%.1f bounded_native_coast=%u",handle,row.controls.brake,unsigned(coast));}
  return true;
 }
 const auto driftMode=MWELNeoDriftMode();
 static auto engineTestModule=GetModuleHandleW(L"MWEventLoader.asi");using PlayerEngineTest=unsigned(__cdecl*)();
 static auto engineTest=engineTestModule?reinterpret_cast<PlayerEngineTest>(GetProcAddress(engineTestModule,"MWEL_PlayerEngineTestV1")):nullptr;
 const bool playerEngineTest=racer.driverClass==kDriverHuman&&engineTest&&engineTest();
 const bool playerPadEngineTest=playerEngineTest&&engineTest()==2;
 if(playerEngineTest&&driftMode){
  // Release a prior AI physics lease once. Releasing on every input frame
  // erased the corner filter/entry timer and invalidated the player test.
  if(row.driftSelected)NeoDriftRelease(row);
  row.driftSelected=false;
 }
 else if(!driftMode||(racer.driverClass==kDriverHuman&&!NeoLocalPlayer())){NeoDriftRelease(row);row.driftSelected=false;}
 else if(racer.driverClass==kDriverHuman&&NeoLocalPlayer())row.driftSelected=true;
 else if(!row.driftSelected){
  unsigned selectedCount=0;for(const auto& other:neoRuntime)if(other.driftSelected&&other.input&&BattleVtable(other.input,0x8AB5C8))++selectedCount;
  row.driftSelected=selectedCount<MWELNeoDriftLimit();
 }
  if(row.driftSelected&&!NeoDriftClient(row).Available()){NeoDriftClient(row).Reset();row.driftSelected=false;}
  if(row.driftSelected){
  auto& physics=neoDriftPhysics[size_t(&row-&neoRuntime[0])];
  const bool previouslyFailed=physics.failed;
   // Special now adjusts native tyre attributes. Preserve the car's original
   // suspension: replacing Player with Racer changes the actual tyre model.
   // Arcade retains its previously validated suspension lease.
   unsigned result=0;
   if(driftMode==2){
    if(physics.active){const auto restored=physics.Restore();result=restored==1?1:restored==3?2:0;}
    const auto type=mwel_drift::SuspensionLease::Vtable(unsigned(racer.pointer));
    if(type!=0x8abc30&&type!=0x8abac8)result=2;
   }else result=physics.Ensure(unsigned(racer.pointer),handle);
  if(result==2){if(!previouslyFailed)Log(LogLevel::Warning,"EVENT_DRIFT_SUSPENSION_REFUSED handle=%u",handle);row.driftSelected=false;}
  else if(result==1){
   Log(LogLevel::Info,"EVENT_DRIFT_SUSPENSION_CHANGED handle=%u mode=%u native_type=%08X original_override=%u original_value=%08X body_AI_unchanged=1 resample_next_tick=1",handle,driftMode,mwel_drift::SuspensionLease::Vtable(unsigned(racer.pointer)),unsigned(physics.previousPresent),physics.previousValue);
    row.owned=true;row.activeAt=now;row.sampled=0;row.driver.Reset();row.driftIntent.Reset();row.driftLearningHold=0;
   neoWriter=GetCurrentThreadId();neoOriginalInput[0](input,0);neoOriginalInput[1](input,0);neoOriginalInput[2](input,.1f);neoOriginalInput[3](input,0);
   if(neoNitrousInstalled)neoOriginalNOS(input,false);neoWriter=0;++row.ticks;return true;
  }
 }
 mcdai::StateV1 driftFeedback;
 const bool driftFeedbackValid=row.driftSelected&&NeoDriftClient(row).Read(driftFeedback)==mcdai::Ok;
 if(row.driftIntentActive||(driftFeedbackValid&&driftFeedback.phase==2&&driftFeedback.eligible))row.driftLearningHold=.5f;
 else row.driftLearningHold=std::max(0.f,row.driftLearningHold-v.dt);
 row.driver.gripGuard=true;
#ifdef MWEL_NEO_OBSERVE_ONLY
 const auto observedPriorControls=row.controls;
#endif
 v.steering=row.controls.steering;
 v.steeringGain=row.driver.response.gain;
 v.recovery=row.driver.recovery;
 const float measuredBeta=v.speed>1?std::atan2(mwneo::CrossXZ(mwneo::UnitXZ(v.heading),mwneo::UnitXZ(v.velocity)),mwneo::DotXZ(mwneo::UnitXZ(v.velocity),mwneo::UnitXZ(v.heading))):0;
 const bool driftPlanning=(playerEngineTest||(row.driftSelected&&driftFeedbackValid&&driftFeedback.calibrated))&&std::abs(measuredBeta)<.8f;
 auto planningVehicle=NeoDriftPlanningVehicle(v,driftPlanning,true);
 if(driftPlanning)planningVehicle.steering=row.driver.steering;
 bool diagnostic=NeoDiagnosticTarget(row);
 if(diagnostic&&!row.diagnosticAt)row.diagnosticAt=now;
 if(diagnostic)NeoDiagnosticOrder(row,"NEO_UPDATE_BEGIN",neoDrivingCaller);
 if((!diagnostic||neoDiagnosticMode==3||neoDiagnosticMode==6)&&(!row.sampled||now-row.sampled>=50)){
  using Query=unsigned(__cdecl*)(const mwneo::Vehicle*,mwneo::Guidance*);
  bool ownedProvider=false;auto module=NeoProviderModule(ownedProvider);static Query query=nullptr;if(!query&&module)query=reinterpret_cast<Query>(GetProcAddress(module,"MWEL_NeoGuidanceV1"));
  mwneo::Guidance next;unsigned status=module&&query?query(&planningVehicle,&next):0;
  if(!status){if(!wasOwned){static ULONGLONG next=0;auto t=GetTickCount64();if(MWELNeoDetailedLoggingEnabled()&&t>=next){next=t+1000;Log(LogLevel::Info,"DRIFT_GUIDANCE_FAILURE handle=%u module=%08X query=%08X status=%u",handle,unsigned(module),unsigned(query),status);}return false;}++row.guidanceFailures;row.guidance.status=2;}
  else row.guidance=next;row.sampled=now;
  if(!wasOwned&&status)Log(LogLevel::Info,"EVENT_NEOAI_PROVIDER_BOUND handle=%u owner=%s fallback_to_world_on_failure=0",handle,ownedProvider?"NFSMWEventLoader":"World2MWNativeLoader");
 }
 row.owned=true;row.activeAt=now;
 if(!row.difficultySampled||now-row.difficultySampled>=50){
  row.difficultyRace={};MWELNeoDifficultySample(handle,row.difficultyRace);
  row.powerScale=mwel_difficulty::Multiplier(neoDifficulty,row.difficultyRace);row.difficultySampled=now;
 }
 mwneo::Profile profile;using GetProfile=unsigned(__cdecl*)(mwneo::Profile*,unsigned);bool ownedProvider=false;auto module=NeoProviderModule(ownedProvider);
 static GetProfile getProfile=nullptr;if(!getProfile&&module)getProfile=reinterpret_cast<GetProfile>(GetProcAddress(module,"MWEL_NeoProfileV1"));if(module&&getProfile)getProfile(&profile,sizeof(profile));
  if(row.driftSelected||playerEngineTest){
   row.powerScale=1.f;profile.lateralAccel=driftMode==2?mwel_drift_speed::LateralAcceleration:30.f;profile.maxSpeed=mwel_drift_speed::Maximum;
   if(row.driftSteeringLock==0&&mwel_drift::SuspensionLease::SteeringLock(unsigned(racer.pointer),row.driftSteeringLock))
    Log(LogLevel::Info,"DRIFT_STEERING_GEOMETRY handle=%u assumed_radians=%.6f measured_radians=%.6f native_handling_writes=0",handle,profile.steeringLock,row.driftSteeringLock);
   if(row.driftSteeringLock>0)profile.steeringLock=row.driftSteeringLock;
  }
 using NativeHandlingVersion=unsigned(__cdecl*)();static NativeHandlingVersion nativeHandlingVersion=nullptr;
 if(!nativeHandlingVersion){if(auto drift=GetModuleHandleW(L"MWArcadeDrift.asi"))nativeHandlingVersion=reinterpret_cast<NativeHandlingVersion>(GetProcAddress(drift,"MWArcadeDriftNativeHandlingVersionV1"));}
 const bool nativeTurnInput=nativeHandlingVersion&&nativeHandlingVersion()==1&&(playerPadEngineTest||(driftMode==2&&row.driftSelected&&mwel_drift::SuspensionLease::Vtable(unsigned(racer.pointer))==0x8abc30));
 // The stock response learner expects wheel lock, not a requested yaw rate.
 // Feeding turn requests into that learner falsely reduces grip to its floor.
 if(diagnostic&&neoDiagnosticMode!=3)NeoDiagnosticControls(row,v,now);else row.controls=row.driver.Update(planningVehicle,row.guidance,profile,nativeTurnInput||row.driftLearningHold>0);
 float driftHandbrake=0;
  float driftRoadCurve=std::numeric_limits<float>::quiet_NaN(),driftRoadPeak=0,driftRoadSpeed=std::numeric_limits<float>::quiet_NaN();bool driftEntryPrepared=false;const float driftPosition[]={pos.x,pos.y,pos.z};
  if(row.driftSelected||playerEngineTest)MWELNeoDriftRoadCurve(handle,row.guidance.edge,driftPosition,std::max(60.f,v.speed*3.f),&driftRoadCurve,&driftRoadPeak,&driftEntryPrepared,v.speed,profile.brakeDecel,&driftRoadSpeed);
  // The marked lab drives the ORIGINAL human suspension through normal
  // inputs. Do not turn it into SuspensionRacer merely to automate a test.
  unsigned playerWheels=0;
  const bool playerPhysicalSafe=playerEngineTest&&mwel_drift::SuspensionLease::Read(unsigned(racer.pointer)+0x84,playerWheels)&&playerWheels>=3&&playerWheels<=4;
  // Special uses native tyre forces: the old correction controller's phase
  // cannot decide whether the real chassis is already sliding.
  const bool measuredSlide=driftMode==2||playerEngineTest;
  // The native steering path is active above 5m/s. Its unwind state also
  // needs real angular samples below the 12m/s drift-entry threshold.
  if(driftMode==2&&(playerEngineTest||row.driftSelected)&&v.speed>5&&v.recovery==mwneo::Recovery::Normal&&row.controls.recovery==mwneo::Recovery::Normal){
   const auto h=mwneo::UnitXZ(v.heading),t=mwneo::UnitXZ(v.velocity);
   if(row.driftMotionReady&&v.dt>0&&v.dt<=.1f){
    const float bodyRate=std::atan2(mwneo::CrossXZ(row.driftMotionHeading,h),mwneo::DotXZ(row.driftMotionHeading,h))/v.dt;
    const float travelRate=std::atan2(mwneo::CrossXZ(row.driftMotionTravel,t),mwneo::DotXZ(row.driftMotionTravel,t))/v.dt;
    row.driftBodyRate+=(bodyRate-row.driftBodyRate)*v.dt/(.12f+v.dt);
    row.driftTravelRate+=(travelRate-row.driftTravelRate)*v.dt/(.12f+v.dt);
    if(v.speed>12){
     if(now-row.driftClearAt>=50){row.driftClearAt=now;row.driftClearSeconds=MWELNeoDriftClearSeconds(body.position,body.heading,body.velocity,v.halfWidth,v.halfLength,row.driftBodyRate,row.driftTravelRate);}
    }else {row.driftClearSeconds=-1;row.driftClearAt=0;}
   }
   row.driftMotionHeading=h;row.driftMotionTravel=t;row.driftMotionReady=true;
  }else {row.driftMotionReady=false;row.driftClearSeconds=-1;row.driftClearAt=0;row.driftBodyRate=row.driftTravelRate=0;}
  row.driftIntentActive=(playerEngineTest||(row.driftSelected&&driftFeedbackValid))&&row.driftIntent.Apply(driftMode,v,row.guidance,profile,playerEngineTest||driftFeedback.calibrated!=0,measuredSlide?std::abs(measuredBeta)>.15f:driftFeedback.phase==2,measuredBeta,row.controls,driftHandbrake,driftRoadCurve,playerEngineTest?playerPhysicalSafe:driftFeedback.eligible!=0,driftRoadPeak,driftEntryPrepared,row.driftClearSeconds,row.driftMotionReady?-row.driftBodyRate:std::numeric_limits<float>::quiet_NaN(),driftRoadSpeed);
  const float travelSteering=row.controls.steering;
  if(row.driftIntent.nativeRetryResumed)Log(LogLevel::Info,"EVENT_NEOAI_NATIVE_CONTINUATION handle=%u beta=%.5f speed=%.3f line_error=%.3f room=%.3f clear_seconds=%.3f attempts=%u handbrake=%.3f native_pose_writes=0",handle,measuredBeta,v.speed,row.driftIntent.retryLineError,row.driftIntent.retryRoom,row.driftClearSeconds,row.driftIntent.attempts,driftHandbrake);
  // Keep the grip planner's internal actuator state in its travel frame
  // while the drift controller owns the real steering. A stale grip input
  // must not become the first command after a controlled exit.
  if(!nativeTurnInput&&driftMode==2&&driftPlanning&&(row.driftIntentActive||row.driftIntent.rejection==18)&&profile.steeringLock>.1f)
   row.driver.steering=std::clamp(row.controls.steering-measuredBeta/profile.steeringLock,-1.f,1.f);
  if(NeoDriftGripHandoff(driftPlanning,row.driftIntentActive,playerEngineTest||NeoDriftSteeringFeedback(driftFeedbackValid,driftFeedback.reason),row.driftIntent.rejection,v,measuredBeta,profile,row.controls)&&MWELNeoDetailedLoggingEnabled()){
   static ULONGLONG next[16]{};auto index=size_t(&row-&neoRuntime[0]);
   if(now>=next[index]&&std::abs(measuredBeta)>.15f){next[index]=now+500;Log(LogLevel::Info,"EVENT_NEOAI_DRIFT_HANDOFF handle=%u beta=%.5f travel_steering=%.4f chassis_steering=%.4f brake=%.3f throttle=%.3f reason=%u feedback_reason=%u body_correction=0",handle,measuredBeta,travelSteering,row.controls.steering,row.controls.brake,row.controls.throttle,row.driftIntent.rejection,driftFeedback.reason);}
  }

 const bool wasNativeUnwind=row.driftIntent.nativeRecovery.active;
 const bool nativeUnwind=row.driftIntent.nativeRecovery.Update(nativeTurnInput&&driftMode==2&&row.controls.valid&&!row.controls.reverse&&row.controls.recovery==mwneo::Recovery::Normal&&v.speed>5,
  measuredBeta,row.driftMotionReady?-row.driftBodyRate:std::numeric_limits<float>::quiet_NaN(),v.dt);
 if(nativeUnwind){
  if(!wasNativeUnwind)row.driftIntent.StopAttempt();
  // Zero requested yaw retains the player's existing native countersteer
  // feedback. Never pass an emergency wheel-lock value as a yaw request.
  row.controls.steering=0;driftHandbrake=0;row.controls.throttle=std::min(row.controls.throttle,.55f);
  row.driftIntentActive=false;row.driftIntent.rejection=19;
 }else if(nativeTurnInput&&row.controls.valid&&!row.controls.reverse&&row.controls.recovery==mwneo::Recovery::Normal&&v.speed>5&&driftHandbrake<=.2f){
  // Native Player tyre model: request a turn through the same steering
  // feedback as a human. Never stack two countersteering controllers.
  // Human automatic inputs remain restricted to the marked no-save lab.
  const auto travel=mwneo::UnitXZ(v.velocity),aim=mwneo::Sub(row.guidance.aim,v.position);
  const float distance2=mwneo::DotXZ(aim,aim);
  if(distance2>4&&mwneo::Finite(aim)){
   const float arc=2*mwneo::CrossXZ(travel,aim)/distance2;
   const float requestedYaw=std::clamp(-arc*v.speed,-mcd::NativeDriftTurnLimit,mcd::NativeDriftTurnLimit);
   const float ledYaw=NeoNativeSlipLead(driftMode==2&&row.driftMotionReady,requestedYaw,
       measuredBeta,row.driftTravelRate-row.driftBodyRate);
   row.controls.steering=-ledYaw/mcd::NativeDriftTurnLimit;
   // Do not request an arc that the bounded native steering cannot execute.
   // Ordinary pedals shed excess entry speed; no velocity/pose mutation.
   if(std::abs(arc)>.001f){
    const float cap=std::max(8.f,mcd::NativeDriftTurnLimit/std::abs(arc));
    if(v.speed>cap+1){row.controls.throttle=0;row.controls.brake=std::max(row.controls.brake,std::clamp((v.speed-cap-1)*.08f,0.f,.6f));}
   }
  }
 }
 const float beforePowerTrim=row.controls.throttle;
 row.controls.throttle=row.nativePowerTrim.Step(nativeTurnInput&&driftMode==2&&
  (racer.driverClass!=kDriverHuman||playerEngineTest)&&row.controls.valid&&!row.controls.reverse&&
  row.controls.recovery==mwneo::Recovery::Normal&&v.recovery==mwneo::Recovery::Normal&&
  row.guidance.status==1&&row.driftMotionReady&&v.speed>5&&driftHandbrake<=.2f,
  v.dt,row.controls.throttle,measuredBeta,row.driftTravelRate-row.driftBodyRate);
 const bool nativePowerTrimmed=row.controls.throttle<beforePowerTrim;
 if(!row.controls.valid){row.controls={};row.controls.brake=.4f;++row.guidanceFailures;}
 if(!diagnostic&&row.jam.Update(v,row.guidance,row.driver.recovery,nativeTurnInput&&driftMode==2)){
  using Recover=unsigned(__cdecl*)(const mwneo::Vehicle*,void*);
  auto recover=module?reinterpret_cast<Recover>(GetProcAddress(module,"MWEL_NeoRecoverV1")):nullptr;
  unsigned result=recover?recover(&v,ai):0;float age=row.jam.age;row.jam.Attempted();
  // The bridge returns 2 when native placement was attempted but the
  // measured pose/CP verification failed. Do not clear the unresolved jam
  // or count such an attempt as a successful recovery.
  if(result==1){
   ++row.respawns;auto response=row.driver.response;response.ClearWindow();response.lastSteering=0;
   auto brakes=row.driver.brakes;brakes.ClearWindow();brakes.ready=false;
   row.driver.Reset();row.driver.response=response;row.driver.brakes=brakes;row.driver.recovery=mwneo::Recovery::Rejoin;row.jam.Reset();row.sampled=0;
   row.controls={};row.controls.brake=.4f;row.controls.valid=true;
  }else {++row.respawnRefused;if(result==2)row.sampled=0;}
  Log(LogLevel::Info,"MW_NEOAI_RECOVERY_RESULT handle=%u stalled_physics_seconds=%.3f native_respawn_result=%u respawns=%u refused=%u native_driving_handoff=0",handle,age,result,row.respawns,row.respawnRefused);
  if(result){
   // A placement does not retire this vehicle. Keep the suspension stable.
   NeoDriftClient(row).Pause();row.driftLearningHold=0;row.driftIntent.Reset();row.driftIntentActive=false;
   row.driftMotionReady=false;row.driftClearSeconds=-1;row.driftClearAt=0;row.driftBodyRate=row.driftTravelRate=0;
   // The old pose/guidance belong to the jam location. Stop this tick and
   // query the measured post-respawn pose before applying any throttle.
   neoOriginalInput[0](input,0);neoOriginalInput[1](input,0);neoOriginalInput[2](input,.4f);neoOriginalInput[3](input,0);
   if(neoNitrousInstalled)neoOriginalNOS(input,false);row.nitrous.pressed=false;
   ++row.ticks;return true;
  }
 }
 // Native reverse override timer has no authority over an owned driver's gear.
 *reinterpret_cast<float*>(static_cast<unsigned char*>(primary)+0x94)=0;
 auto table=*reinterpret_cast<void***>(transmission);
 bool reverse=reinterpret_cast<bool(__thiscall*)(void*)>(table[5])(transmission);
 if(reverse!=row.controls.reverse){
  // First brake to a stop. Applying the forward controller's throttle while
  // still in R makes the car accelerate backwards and prevents the shift.
  if(std::abs(v.speed)<1){
   reinterpret_cast<bool(__thiscall*)(void*,int)>(table[3])(transmission,row.controls.reverse?0:2);
   reverse=reinterpret_cast<bool(__thiscall*)(void*)>(table[5])(transmission);
   row.driver.integral=0;row.driver.effort=0;
  }
  row.controls.throttle=0;row.controls.brake=reverse!=row.controls.reverse?.65f:0;
 }
#ifdef MWEL_NEO_AIR_COAST_EXPERIMENT
 // Private input ablation only. Preserve the independent NOS decision's
 // original inputs so this trial changes the throttle command alone.
 const auto nitrousControlsBeforeCoast=row.controls;
 if(MWELNeoDetailedLoggingEnabled()&&v.speed>10&&!row.controls.reverse){
  MWELNeoFrameState coastState{v.handle,{v.position.x,v.position.y,v.position.z},{v.velocity.x,v.velocity.y,v.velocity.z},{v.heading.x,v.heading.y,v.heading.z}};
  if(MWELNeoDiagnosticWheelCount(racer.pointer,coastState)==0)row.controls.throttle=0;
 }
#endif
 // Same requested-turn slew as human Pad input. The physical yaw/slip
 // feedback runs afterwards, unfiltered, in the native tyre call. Keep an
 // independent state per racer and bypass immediate unwind/recovery/HB.
 const float nativeTurnRequested=row.controls.steering;
 row.controls.steering=row.nativeTurnDemand.step(handle,now,v.dt,row.controls.steering,
  nativeTurnInput&&driftMode==2&&row.driftSelected&&racer.driverClass!=kDriverHuman&&
  row.controls.valid&&!row.controls.reverse&&row.controls.recovery==mwneo::Recovery::Normal&&
  !nativeUnwind&&v.speed>5&&driftHandbrake<=.2f);
 #ifdef MWEL_NATIVE_FULL_THROTTLE_PROBE
 // Isolated laboratory pedal stimulus. Requires the existing no-save Pad
 // engine-test marker; never enabled in the ordinary/release build. Retain
 // braking, handbrake and recovery vetoes, but replace coasting with full gas.
 if(playerPadEngineTest&&driftMode==2&&row.controls.valid&&!reverse&&
    row.controls.recovery==mwneo::Recovery::Normal&&v.recovery==mwneo::Recovery::Normal&&
    v.speed>5&&row.controls.brake<.01f&&driftHandbrake<.01f)row.controls.throttle=1;
 #endif
 neoWriter=GetCurrentThreadId();
 // PlayerEngineTest uses the same input-only drift planner above while
 // retaining SuspensionPlayer, real native controls and normal safety gates.
 // MW's IInput steering sign is opposite to the raw X/Z bicycle model.
 // Verified from live heading delta for both Human and Racecar inputs.
 neoOriginalInput[0](input,-row.controls.steering);
 neoOriginalInput[1](input,row.controls.throttle);
 neoOriginalInput[2](input,row.controls.brake);
 neoOriginalInput[3](input,driftHandbrake);
 if(neoNitrousInstalled){
  row.nosRead=NeoReadNitrous(primary,row.nosEquipped,row.nosTank,row.nosEngaged,row.nosBoost);
#ifdef MWEL_NEO_AIR_COAST_EXPERIMENT
  bool pressed=neoDifficulty.nitrous&&!row.driftSelected&&!playerEngineTest&&!diagnostic&&row.nosRead&&row.nitrous.Update(v,row.guidance,profile,nitrousControlsBeforeCoast,row.nosEquipped,row.nosTank);
#else
  bool pressed=neoDifficulty.nitrous&&!row.driftSelected&&!playerEngineTest&&!diagnostic&&row.nosRead&&row.nitrous.Update(v,row.guidance,profile,row.controls,row.nosEquipped,row.nosTank);
#endif
  row.nitrous.pressed=pressed;neoOriginalNOS(input,pressed);
  if(pressed)row.nosRequestedSeconds+=std::clamp(v.dt,0.f,.1f);
  if(row.nosRead&&row.nosEngaged)row.nosEngagedSeconds+=std::clamp(v.dt,0.f,.1f);
 }
 const bool driftAllowed=row.owned&&row.controls.valid&&!row.controls.reverse&&row.controls.recovery==mwneo::Recovery::Normal&&row.guidance.status==1;
 mcdai::IntentV1 driftRequest;driftRequest.active=row.driftIntentActive?1u:0u;
  driftRequest.pathYaw=row.driftIntent.pathYaw;
  driftRequest.targetSlip=row.driftIntent.targetSlip;
 auto driftResult=NeoDriftClient(row).Frame(unsigned(reinterpret_cast<std::uintptr_t>(racer.pointer)),handle,
   row.driftSelected?driftMode:0,unsigned(reinterpret_cast<std::uintptr_t>(race)),neoDriftGeneration,row.owned&&!row.genuineGoalHeld,driftAllowed,racer.driverClass==kDriverHuman,&driftRequest,row.driftIntent.pathStrength);
 if(MWELNeoDetailedLoggingEnabled()&&(row.driftSelected||playerEngineTest)&&now>=row.driftLogged){
  row.driftLogged=now+100;
  Log(LogLevel::Info,"EVENT_NEOAI_DRIFT handle=%u mode=%u result=%u correction_allowed=%u feedback=%u phase=%u applied=%u beta=%.5f speed=%.3f learning_hold=%.3f intent=%u steering=%.4f planned_curve=%.5f drift_road_curve=%.5f lateral=%.3f widths=%.3f,%.3f obstacle=%.3f rejection=%u path_yaw=%.5f travel_rate=%.5f steer_trim=%.5f wall_clear_seconds=%.4f body_rate=%.5f motion_rate=%.5f attempts=%u retry_settle=%.4f retry_line=%.4f retry_room=%.4f retry_aligned=%u power_entry=%u exit_resumed=%u power_entries=%u native_power_trimmed=%u native_turn_requested=%.5f",handle,driftMode,unsigned(driftResult),unsigned(driftAllowed),unsigned(driftFeedbackValid),driftFeedback.phase,driftFeedback.applied,measuredBeta,v.speed,row.driftLearningHold,unsigned(row.driftIntentActive),row.controls.steering,row.controls.curvature,row.driftIntent.roadCurve,row.guidance.lateralError,row.guidance.samples[0].widthLeft,row.guidance.samples[0].widthRight,row.guidance.obstacleDistance,row.driftIntent.rejection,row.driftIntent.pathYaw,row.driftIntent.measuredTravelRate,row.driftIntent.physicalSteerTrim,row.driftClearSeconds,row.driftBodyRate,row.driftTravelRate,row.driftIntent.attempts,row.driftIntent.settle,row.driftIntent.retryLineError,row.driftIntent.retryRoom,unsigned(row.driftIntent.retryAligned),unsigned(row.driftIntent.powerEntry),unsigned(row.driftIntent.nativeExitResumed),row.driftIntent.powerEntries,unsigned(nativePowerTrimmed),nativeTurnRequested);
 }
 neoWriter=0;++row.ticks;
#ifdef MWEL_NEO_OBSERVE_ONLY
 if(MWELNeoDetailedLoggingEnabled()){
  MWELNeoFrameState traceState{v.handle,{v.position.x,v.position.y,v.position.z},{v.velocity.x,v.velocity.y,v.velocity.z},{v.heading.x,v.heading.y,v.heading.z},{observedPriorControls.steering,observedPriorControls.throttle,observedPriorControls.brake}};
  MWELNeoObservePhysicsFrame(racer.pointer,traceState,row.controls.steering,row.controls.throttle,row.controls.brake);
 }
 if(neoDiagnosticMode==7&&MWELNeoDetailedLoggingEnabled()){
  if(!row.diagnosticAt)row.diagnosticAt=now;
  row.requested[0]=-row.controls.steering;row.requested[1]=row.controls.throttle;row.requested[2]=row.controls.brake;
  NeoDiagnosticRead(input,row.postwrite);
 }
#endif
 if(diagnostic){row.requested[0]=-row.controls.steering;row.requested[1]=row.controls.throttle;row.requested[2]=row.controls.brake;NeoDiagnosticRead(input,row.postwrite);NeoDiagnosticOrder(row,"NEO_POSTWRITE",neoDrivingCaller);}
 if(diagnostic&&now-row.diagnosticAt<NeoDiagnosticWindow())Log(LogLevel::Info,"NEO_RESPONSE seq=%llu tick=%u vehicle=%u phase=%u dt=%.6f speed=%.6f heading=%.6f,%.6f,%.6f position=%.6f,%.6f,%.6f velocity=%.6f,%.6f,%.6f gear=%u transmission_reverse=%u requested_reverse=%u steer=%.6f gas=%.6f brake=%.6f target=%.6f status=%u",++neoDiagnosticSequence,row.ticks,row.handle,row.diagnosticPhase,dt,v.speed,heading.x,heading.y,heading.z,pos.x,pos.y,pos.z,v.velocity.x,v.velocity.y,v.velocity.z,reinterpret_cast<unsigned(__thiscall*)(void*)>(table[1])(transmission),unsigned(reverse),unsigned(row.controls.reverse),row.requested[0],row.requested[1],row.requested[2],row.controls.targetSpeed,row.guidance.status);
 if(!wasOwned){
  W2CReleaseNativeQuery(ai);Log(LogLevel::Info,"MW_NEOAI_ACQUIRE handle=%u event=%08X route=NeoAI speed=NeoAI actuator=NeoAI recovery=NeoAI native_physics=1",handle,row.guidance.event);
  // Read the same native customization package returned by IVehicle slot 50
  // (688260: mov eax,[ecx+38]). Do not infer upgrades from the model name.
  void* customization=nullptr;std::array<unsigned,8> package{};
  const bool packageRead=AudioRead(static_cast<unsigned char*>(racer.pointer)+0x38,&customization)&&customization&&
   AudioRead(static_cast<unsigned char*>(customization)+offsetof(NFSPluginSDK::MW05::FECustomizationRecord,mInstalledPhysics),&package);
  Log(LogLevel::Info,"MW_NEOAI_VEHICLE_SETUP handle=%u customization_read=%u tires=%u brakes=%u chassis=%u transmission=%u engine=%u induction=%u nos=%u junkman=%u",handle,unsigned(packageRead),package[0],package[1],package[2],package[3],package[4],package[5],package[6],package[7]);
  NeoLogNativeUpgradeLevels(racer.pointer,handle);
 }
 // Race status GO becomes valid after ownership is acquired. Retry a bounded
 // number of times instead of treating a pre-GO scope rejection as final.
 if(racer.driverClass!=kDriverHuman&&!row.junkmanApplied&&row.junkmanAttempts<20&&now>=row.junkmanNext){
  using Mirror=unsigned(__cdecl*)(void*,unsigned,unsigned*,unsigned*);
  auto nfr=GetModuleHandleW(L"NFSMWNativeFreeRoamRacers.asi");
  auto mirror=nfr?reinterpret_cast<Mirror>(GetProcAddress(nfr,"NFRR_MirrorRaceJunkmanV1")):nullptr;
  unsigned playerParts=0,aiParts=0;
  const unsigned applied=mirror?mirror(ai,unsigned(NeoLocalPlayer()),&playerParts,&aiParts):0;
  row.junkmanApplied=applied!=0;++row.junkmanAttempts;row.junkmanNext=now+500;
  if(applied||row.junkmanAttempts==20)Log(applied?LogLevel::Info:LogLevel::Warning,
   "MW_NEOAI_JUNKMAN handle=%u player_parts=%u ai_parts=%u applied=%u attempts=%u nfr_bridge=%u",
   handle,playerParts,aiParts,applied,row.junkmanAttempts,unsigned(mirror!=nullptr));
 }
 if(MWELNeoDetailedLoggingEnabled()&&now>=row.logged){row.logged=now+250;auto planningProfile=profile;planningProfile.brakeDecel=row.driver.brakes.PlanningDecel(profile);
  // Report the envelope actually used by the driver, including learned grip.
  if(row.driver.gripGuard&&row.driver.gripCeiling>0)planningProfile.lateralAccel=std::min(planningProfile.lateralAccel,row.driver.gripCeiling);
  auto speedDiagnostic=mwneo::PlanSpeed(row.guidance,v,planningProfile);
  auto travel=mwneo::UnitXZ(v.velocity),facing=mwneo::UnitXZ(v.heading);
  float slip=v.speed>2?std::atan2(mwneo::CrossXZ(facing,travel),mwneo::DotXZ(facing,travel)):0;
  Log(LogLevel::Info,"MW_NEOAI_MOTION handle=%u CP=%u progress=%.2f velocity=%.4f,%.4f,%.4f slip=%.5f yaw_rate=%.5f",handle,row.guidance.edge,row.guidance.progress,v.velocity.x,v.velocity.y,v.velocity.z,slip,row.driver.yawRate);
  if(now>=row.powerLogged){row.powerLogged=now+1000;Log(LogLevel::Info,"MW_NEOAI_POWER_SAMPLE handle=%u scale=%.3f rank=%u player_gap_metres=%.3f catchup_eligible=%u applied_calls=%u native=%.3f boosted=%.3f blacklist=%u base_scale=%.3f",handle,row.powerScale,row.difficultyRace.rank,row.difficultyRace.playerGap,unsigned(row.difficultyRace.eligible),row.powerCalls,row.lastPowerNative,row.lastPowerBoosted,row.difficultyRace.blacklist,mwel_difficulty::BaseScale(neoDifficulty,row.difficultyRace));}
  Log(LogLevel::Info,"MW_NEOAI_AVOIDANCE handle=%u predictive=%u road_curve=%.5f steering_gain=%.3f steering_windows=%u brake_gain=%.3f brake_windows=%u preview_brake=%u planning_decel=%.3f grip_ceiling=%.1f steer_saturation=%.2f understeer_time=%.2f",handle,row.guidance.predictiveAvoidance,row.guidance.samples[0].roadCurvature,row.driver.response.gain,row.driver.response.windows,row.driver.brakes.gain,row.driver.brakes.windows,unsigned(row.driver.previewBraking),row.driver.brakes.PlanningDecel(profile),row.driver.gripCeiling,row.driver.steerSaturation,row.driver.understeerTime);
  if(neoNitrousInstalled)Log(LogLevel::Info,"MW_NEOAI_NITROUS_SAMPLE handle=%u read=%u equipped=%u requested=%u engaged=%u tank=%.4f boost=%.4f requested_seconds=%.3f engaged_seconds=%.3f",handle,unsigned(row.nosRead),unsigned(row.nosEquipped),unsigned(row.nitrous.pressed),unsigned(row.nosEngaged),row.nosTank,row.nosBoost,row.nosRequestedSeconds,row.nosEngagedSeconds);
  Log(LogLevel::Info,"MW_NEOAI_CONTROL handle=%u event=%08X CP=%u progress=%.2f speed=%.2f target=%.2f steering=%.3f gas=%.3f brake=%.3f state=%u status=%u curve=%.5f heading_error=%.3f position=%.2f,%.2f,%.2f heading=%.4f,%.4f,%.4f obstacle=%.2f lead=%.2f ticks=%u native_rejected=%u guidance_failures=%u fallbacks=%u gear=%u reverse_request=%u rear_clear=%u dt=%.5f recovery_time=%.3f travel_heading=%.4f,%.4f,%.4f native_speed=%.2f dimension=%.3f,%.3f,%.3f aim_distance=%.2f plan_curve=%.5f speed_constraint=%u constraint_distance=%.2f constraint_curve=%.5f traffic_push=%u",handle,row.guidance.event,row.guidance.edge,row.guidance.progress,v.speed,row.controls.targetSpeed,row.controls.steering,row.controls.throttle,row.controls.brake,unsigned(row.controls.recovery),row.guidance.status,row.controls.curvature,row.controls.headingError,pos.x,pos.y,pos.z,heading.x,heading.y,heading.z,row.guidance.obstacleDistance,row.guidance.leadSpeed,row.ticks,row.rejectedNative,row.guidanceFailures,row.fallbacks,reinterpret_cast<unsigned(__thiscall*)(void*)>(table[1])(transmission),unsigned(row.controls.reverse),row.guidance.rearClear,dt,row.driver.stateTime,racer.heading.y,racer.heading.z,racer.heading.x,racer.speed,body.dimension[0],body.dimension[1],body.dimension[2],mwneo::LengthXZ(mwneo::Sub(row.guidance.aim,v.position)),speedDiagnostic.maxCurvature,speedDiagnostic.constraint,speedDiagnostic.constraintDistance,speedDiagnostic.constraintCurve,unsigned(row.guidance.leadVehicle==mwneo::RaceTrafficPush));
 }
 return true;
}
static bool NeoOwnedFailure(void* primary){
 auto* ai=static_cast<unsigned char*>(primary)+0x4C;
 for(auto& s:neoRuntime)if(s.ai==ai&&s.owned){
  NeoDriftRelease(s);++s.guidanceFailures;s.activeAt=GetTickCount64();neoWriter=0;
  // An owned controller fault cannot silently hand steering back to native AI.
  if(s.guidanceFailures==1||s.guidanceFailures%120==0)Log(LogLevel::Error,"MW_NEOAI_SAFE_STOP handle=%u reason=controller_fault failures=%u",s.handle,s.guidanceFailures);
  __try{neoOriginalInput[1](s.input,0);neoOriginalInput[2](s.input,.5f);neoOriginalInput[3](s.input,0);if(neoNitrousInstalled)neoOriginalNOS(s.input,false);}__except(EXCEPTION_EXECUTE_HANDLER){}
  return true;
 }return false;
}
static bool NeoDriveGuarded(void* primary,float dt){__try{if(NeoDrive(primary,dt))return true;return NeoOwnedFailure(primary);}__except(EXCEPTION_EXECUTE_HANDLER){return NeoOwnedFailure(primary);}}
static void NeoResumeAfterNativeAction(void* primary,float dt){
 if(!neoInstalled||!std::isfinite(dt)||dt<=0||dt>.1f||NeoSelectedController()!=2||!MWELNeoCourseEnabled())return;
 // The verified native mechanic tick still runs when its goal no longer
 // calls Driving. Run once only in that missing-call case, on the simulation
 // thread; retain the full race/vehicle membership checks in NeoDrive.
 if(NeoDriveGuarded(primary,dt)){
  auto* row=NeoOwned(static_cast<BYTE*>(primary)+0x4C);
  if(row&&row->ticks%120==1)Log(LogLevel::Info,"EVENT_NEOAI_ACTION_RESUMED handle=%u simulation_tick=1 native_driving_call_missing=1 CP_lap_writes=0",row->handle);
 }
}
static void __fastcall NeoDriving(void* primary,void*,float dt){if(neoUpdatingPrimary==primary)neoUpdateDrove=true;neoDrivingCaller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());if(!NeoDriveGuarded(primary,dt))neoOriginalDriving(primary,dt);}
static unsigned __cdecl NeoWrongWayObserved(void* human,unsigned native){
 if(!neoInstalled||!MWELNeoCourseEnabled()||!BattleVtable(human,0x89293C))return native&255;
 // EN1.3 constructors/destructors install IHumanAI at +7CC (the bundled
 // SDK's full derived-class size differs and must not supply this offset).
 auto* primary=static_cast<BYTE*>(human)-0x7CC;void* vehicle=nullptr;void* sim=nullptr;unsigned handle=0;
 if(!BattleVtable(primary,0x892AD0)||!AudioRead(primary+0x48,&vehicle)||!AudioRead(primary+0x34,&sim)||!AudioRead(static_cast<BYTE*>(sim)+8,&handle))return native&255;
 __try{
  NeoBodyMotion body;if(!NeoReadBody(static_cast<BYTE*>(vehicle),body))return native&255;
  bool result=MWELNeoCourseFacing(handle,body.position,body.heading,(native&255)!=0);
  if(MWELNeoDetailedLoggingEnabled()){
   static ULONGLONG logged=0;auto now=GetTickCount64();if(now>=logged){logged=now+1000;Log(LogLevel::Info,"EVENT_NEOAI_COURSE_FACING handle=%u native_wrong=%u ordered_CP_wrong=%u changed=%u player_warning_only=1 native_CP_writes=0",handle,native&255,unsigned(result),unsigned(result!=((native&255)!=0)));}
  }return result;
 }__except(EXCEPTION_EXECUTE_HANDLER){return native&255;}
}
// Native callers can treat this getter as a leaf. Preserve flags, registers,
// floating-point state and upper EAX; replace only its returned AL byte.
static void __declspec(naked) NeoWrongWay(){
 __asm {
  call dword ptr [neoOriginalWrongWay]
  pushfd
  pushad
  mov ebx,esp
  and esp,0FFFFFFF0h
  sub esp,512
  fxsave [esp]
  push dword ptr [ebx+28]
  push dword ptr [ebx+24]
  call NeoWrongWayObserved
  add esp,8
  mov byte ptr [ebx+28],al
  fxrstor [esp]
  mov esp,ebx
  popad
  popfd
  ret
 }
}
static void InstallNeoAI(){
 char settingsFile[MAX_PATH]{};if(MWELNeoSettingsFile(settingsFile,sizeof(settingsFile))){neoDifficulty=mwel_difficulty::Load(settingsFile);Log(LogLevel::Info,"MW_NEOAI_DIFFICULTY_SETTINGS file=%s scripts_ini_ignored=1",settingsFile);}
 const unsigned addresses[]={0x69CB80,0x69CB90,0x69CBA0,0x69CBC0};
 const unsigned char offsets[]={0x0C,0x1C,0x20,0x24};
 for(unsigned i=0;i<4;++i){unsigned char expected[]={0x8B,0x44,0x24,0x04,0x89,0x41,offsets[i],0xC2,0x04,0x00};if(memcmp(reinterpret_cast<void*>(Address(addresses[i])),expected,10)){Log(LogLevel::Warning,"MW_NEOAI_REFUSED input_abi=%08X",addresses[i]);return;}}
 const unsigned char driving[]={0x56,0x57,0x8B,0x7C,0x24,0x0C,0x8B,0xF1,0x8B,0x06,0x57};
 if(memcmp(reinterpret_cast<void*>(Address(0x408690)),driving,sizeof(driving))||*reinterpret_cast<unsigned*>(Address(0x892720+17*4))!=Address(0x408690)){Log(LogLevel::Warning,"MW_NEOAI_REFUSED driver_abi=1");return;}
 const BYTE physics[]={0x53,0x8B,0x5C,0x24,0x08,0x56,0x8B,0xF1};
 if(memcmp(reinterpret_cast<void*>(Address(0x688390)),physics,sizeof(physics))||*reinterpret_cast<unsigned*>(Address(0x8AA828+13*4))!=Address(0x688390)){Log(LogLevel::Warning,"MW_NEOAI_REFUSED physics_mode_abi=1");return;}
 const BYTE mechanic[]={0x53,0x56,0x8B,0xF1,0x8B,0x46,0x4C};
 if(memcmp(reinterpret_cast<void*>(Address(0x42AA80)),mechanic,sizeof(mechanic))){Log(LogLevel::Warning,"MW_NEOAI_REFUSED mechanic_tick_abi=1");return;}
 const BYTE placement[]={0x53,0x8B,0x5C,0x24,0x08,0x56,0x57};
 if(memcmp(reinterpret_cast<void*>(Address(0x422730)),placement,sizeof(placement))){Log(LogLevel::Warning,"MW_NEOAI_REFUSED native_placement_abi=1");return;}
 const BYTE clock[]={0xD9,0x05,0xD8,0x85,0x98,0,0xC3};
 if(memcmp(reinterpret_cast<void*>(Address(0x6E8DE0)),clock,sizeof(clock))){Log(LogLevel::Warning,"MW_NEOAI_REFUSED simulation_clock_abi=1");return;}
 const BYTE facing[]={0x8A,0x41,0x1C,0xC3,0xCC,0xCC,0xCC,0xCC};
 if(memcmp(reinterpret_cast<void*>(Address(0x4351A0)),facing,sizeof(facing))||*reinterpret_cast<unsigned*>(Address(0x892960))!=Address(0x4351A0)){Log(LogLevel::Warning,"MW_NEOAI_REFUSED human_facing_abi=1");return;}
 void* detours[]={&NeoSteering,&NeoGas,&NeoBrake,&NeoHandBrake};unsigned created=0;bool ok=true;
 for(unsigned i=0;i<4;++i){if(MH_CreateHook(reinterpret_cast<void*>(Address(addresses[i])),detours[i],reinterpret_cast<void**>(&neoOriginalInput[i]))!=MH_OK){ok=false;break;}++created;}
 if(ok)ok=MH_CreateHook(reinterpret_cast<void*>(Address(0x408690)),&NeoDriving,reinterpret_cast<void**>(&neoOriginalDriving))==MH_OK;
 if(ok)ok=MH_CreateHook(reinterpret_cast<void*>(Address(0x688390)),&NeoPhysicsMode,reinterpret_cast<void**>(&neoOriginalPhysicsMode))==MH_OK;
 if(ok)ok=MH_CreateHook(reinterpret_cast<void*>(Address(0x42AA80)),&NeoRacerUpdate,reinterpret_cast<void**>(&neoOriginalRacerUpdate))==MH_OK;
 if(ok)ok=MH_CreateHook(reinterpret_cast<void*>(Address(0x4351A0)),&NeoWrongWay,reinterpret_cast<void**>(&neoOriginalWrongWay))==MH_OK;
 if(ok)ok=MH_CreateHook(reinterpret_cast<void*>(Address(0x422730)),&NeoNativePlacement,reinterpret_cast<void**>(&neoOriginalPlacement))==MH_OK;
 if(ok){for(unsigned i=0;i<4;++i)ok=MH_EnableHook(reinterpret_cast<void*>(Address(addresses[i])))==MH_OK&&ok;ok=MH_EnableHook(reinterpret_cast<void*>(Address(0x408690)))==MH_OK&&ok;}
 if(ok)ok=MH_EnableHook(reinterpret_cast<void*>(Address(0x688390)))==MH_OK;
 if(ok)ok=MH_EnableHook(reinterpret_cast<void*>(Address(0x42AA80)))==MH_OK;
 if(ok)ok=MH_EnableHook(reinterpret_cast<void*>(Address(0x4351A0)))==MH_OK;
 if(ok)ok=MH_EnableHook(reinterpret_cast<void*>(Address(0x422730)))==MH_OK;
 if(!ok){for(unsigned i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(Address(addresses[i])));MH_RemoveHook(reinterpret_cast<void*>(Address(addresses[i])));}if(neoOriginalDriving){MH_DisableHook(reinterpret_cast<void*>(Address(0x408690)));MH_RemoveHook(reinterpret_cast<void*>(Address(0x408690)));}}
 if(!ok&&neoOriginalPhysicsMode){MH_DisableHook(reinterpret_cast<void*>(Address(0x688390)));MH_RemoveHook(reinterpret_cast<void*>(Address(0x688390)));neoOriginalPhysicsMode=nullptr;}
 if(!ok&&neoOriginalRacerUpdate){MH_DisableHook(reinterpret_cast<void*>(Address(0x42AA80)));MH_RemoveHook(reinterpret_cast<void*>(Address(0x42AA80)));neoOriginalRacerUpdate=nullptr;}
 if(!ok&&neoOriginalWrongWay){MH_DisableHook(reinterpret_cast<void*>(Address(0x4351A0)));MH_RemoveHook(reinterpret_cast<void*>(Address(0x4351A0)));neoOriginalWrongWay=nullptr;}
 if(!ok&&neoOriginalPlacement){MH_DisableHook(reinterpret_cast<void*>(Address(0x422730)));MH_RemoveHook(reinterpret_cast<void*>(Address(0x422730)));neoOriginalPlacement=nullptr;}
 neoInstalled=ok;Log(LogLevel::Info,"MW_NEOAI_INSTALLED=%u setter_guards=1 final_consumer_verified=0 custom_event_opponents_only=1",unsigned(ok));
 if(ok){InstallNeoOwnershipDiagnostic();if(NeoSelectedController()==2){neoEngineOutputInstalled=InstallNeoEngineOutput();neoNitrousInstalled=InstallNeoNitrous();}}
}
