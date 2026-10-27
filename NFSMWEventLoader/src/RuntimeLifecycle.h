#pragma once
// Production observers never bootstrap a race or change player ownership.
// Explicit isolated career-autodrive fixtures are gated separately below.
namespace runtime_lifecycle {
bool enabled=false,ending=false;
BYTE* currentRace=nullptr;
unsigned currentKey=0,recordedGoals=0;
bool wasRunning=false;
float previousSeconds=0;
struct ValidationMotion {unsigned handle=0,routeRevision=0;int checkpoint=-1;float station=0,gateBest=std::numeric_limits<float>::infinity();float position[3]{};ULONGLONG progressed=0,pending=0,moved=0,logged=0;bool recoveryVerificationUsed=false;};
ValidationMotion validation[16]{};
bool validationEnabled=false;
bool careerAutodrive=false;
void(__cdecl* originalFinish)(void*)=nullptr;
void(__thiscall* originalEnd)(void*)=nullptr;
void(__cdecl* originalStop)()=nullptr;
void(__thiscall* originalRoaming)(void*)=nullptr;
void(__thiscall* originalWorldUnload)(void*)=nullptr;
void(__cdecl* originalExitWorld)()=nullptr;
template<class T>T Read(void* pointer,unsigned offset=0){T value{};if(pointer)Copy(static_cast<BYTE*>(pointer)+offset,&value,sizeof(value));return value;}
#include "EventResourceDiagnostics.h"
#include "PlayerResourceDiagnostics.h"
#include "PlayerTextureRepair.h"
void AuditCheckpoints(BYTE* race){
 auto first=Read<BYTE**>(race,0x19AC),end=Read<BYTE**>(race,0x19B0);auto bytes=uintptr_t(end)-uintptr_t(first);if(!first||!end||bytes>64*4||bytes%4)return;
 for(unsigned i=0;i<bytes/4;++i){auto trigger=Read<BYTE*>(first,i*4);Log("EVENT_RUNTIME_NATIVE_GATE index=%u key=%08X object=%08X enabled=%u active=%u references=%d",i,trigger?reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(trigger+4):0,unsigned(trigger),Read<unsigned>(trigger,0x74),unsigned(Read<BYTE>(trigger,0xC4)),Read<int>(trigger,0xC8));}
}
bool Body(unsigned handle,NeoBodyMotion& output){
 auto count=Read<unsigned>(reinterpret_cast<void*>(0x92CD24)),capacity=Read<unsigned>(reinterpret_cast<void*>(0x92CD20));
 auto vehicles=Read<BYTE**>(reinterpret_cast<void*>(0x92CD1C));if(!vehicles||count>128||count>capacity)return false;
 for(unsigned i=0;i<count;++i){auto vehicle=Read<BYTE*>(vehicles,i*4);auto table=Read<void**>(vehicle);auto getter=Read<void*>(table,43*4);if(getter!=reinterpret_cast<void*>(0x688230))continue;
  auto ai=reinterpret_cast<BYTE*(__thiscall*)(void*)>(getter)(vehicle);if(!ai)continue;auto primary=ai-0x4C;
  auto owner=Read<unsigned>(primary);if(owner!=0x892720&&owner!=0x892AD0)continue;
  auto sim=Read<BYTE*>(primary,0x34);if(Read<unsigned>(sim,8)==handle&&Read<BYTE*>(primary,0x48)==vehicle)return NeoReadBody(vehicle,output);
 }return false;
}
void Validate(BYTE* race,float seconds){
 if(!validationEnabled||!std::isfinite(seconds)||seconds<0)return;auto count=Read<unsigned>(race,0x1958);if(count>16)return;
 // Native race time freezes while the user pauses. Wall-clock time would
 // incorrectly classify a menu pause as an unrecovered controlled vehicle.
 auto now=ULONGLONG(double(seconds)*1000)+1;
 // Include the local body only in the explicit isolated autodrive fixture.
 // Never substitute speedometer values or recovery flags for actual motion.
 for(unsigned slot=careerAutodrive?0:1;slot<count;++slot){auto info=race+0x18+slot*0x194;unsigned handle=Read<unsigned>(info);if(!handle||(recordedGoals&(1u<<slot)))continue;
  NeoBodyMotion body{};if(!Body(handle,body))continue;int cp=Read<int>(info,0x2C);auto& m=validation[slot];
  if(m.handle!=handle){m={};m.handle=handle;m.checkpoint=-1;memcpy(m.position,body.position,12);m.moved=now;}
  if(std::hypot(body.position[0]-m.position[0],body.position[2]-m.position[2])>.25f){memcpy(m.position,body.position,12);m.moved=now;}
  if(cp!=m.checkpoint){m.checkpoint=cp;m.progressed=m.pending=0;m.gateBest=std::numeric_limits<float>::infinity();m.recoveryVerificationUsed=false;}
  float station=0;unsigned revision=0;bool progress=cp>=0&&MWELNeoRouteProgress(handle,unsigned(cp),&station,&revision);
  if(progress&&m.routeRevision!=revision){m.routeRevision=revision;m.station=station;m.progressed=now;m.recoveryVerificationUsed=false;Log("EVENT_RUNTIME_CHECKED_ROAD_REBASE handle=%u CP=%d revision=%u station=%.3f native_progress_written=0 body_watchdog_unchanged=1",handle,cp,revision,station);}
  if(progress){if(!m.progressed||station>m.station+5.f&&now-m.moved<1000){m.station=station;m.progressed=now;m.recoveryVerificationUsed=false;}}
  // Police can push a racer off the projected line. A stale line cursor is
  // not proof that its physically moving body failed to recover. Permit only
  // a new best approach to the real pending gate to extend this observation;
  // circling/recovery flags alone cannot keep it alive. Never write race CPs.
  float gate[3]{},dimensions[3]{},rotation=0;
  if(cp>=0&&MWELNeoGate(currentKey,unsigned(cp),gate,dimensions,&rotation)){
   const float distance=std::hypot(std::hypot(body.position[0]-gate[0],body.position[2]-gate[2]),body.position[1]-gate[1]);
   if(std::isfinite(distance)&&distance+5.f<m.gateBest&&now-m.moved<1000){m.gateBest=distance;m.progressed=now;m.recoveryVerificationUsed=false;}
  }
  if(seconds>25&&m.progressed&&now-m.progressed>20000&&mwel_recovery_observation::VerifyOnce(m.recoveryVerificationUsed,MWELNeoRecoveryResumed(handle,unsigned(cp)))){m.progressed=now-16000;Log("EVENT_RUNTIME_RECOVERY_RESUMPTION_WINDOW handle=%u CP=%d seconds=4 checked_forward_body_metres=2 once_until_real_progress=1 body_watchdog_unchanged=1",handle,cp);}
  bool pending=cp>=0&&MWELNeoPendingGateEnd(handle,unsigned(cp));if(!pending)m.pending=0;else if(!m.pending)m.pending=now;
  if(now-m.logged>=1000){m.logged=now;Log("EVENT_RUNTIME_RACE_BODY handle=%u slot=%u CP=%d position=%.3f,%.3f,%.3f station=%.3f pending=%u movement_age_ms=%llu",handle,slot,cp,body.position[0],body.position[1],body.position[2],station,unsigned(pending),now-m.moved);}
  if((m.pending&&now-m.pending>8000)||(seconds>25&&now-m.moved>20000)||(seconds>25&&m.progressed&&now-m.progressed>20000)){
   AuditCheckpoints(race);validationEnabled=false;Log("EXPERIMENT_RACE_ABORT production_opponent_cannot_resume handle=%u slot=%u CP=%d station=%.3f pending=%u body_observed=1",handle,slot,cp,station,unsigned(pending));return;
  }
 }
}
void FrameUnsafe(){
 if(!enabled)return;
 native_runtime_diagnostics::Hud();
 profile_progress::RestoreNativeProgress();
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto key=MWELNeoCurrentKey();
 bool running=key&&race&&Read<unsigned>(reinterpret_cast<void*>(0x925E90))==6&&Read<unsigned>(race,0x1960)==1&&Read<BYTE>(race,0x198A)&&Read<BYTE>(race,0x1978);
 float seconds=running?Read<float>(race,0x18+0x150):0;
 if(running&&Read<BYTE>(race,0x18+0x154))seconds+=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-Read<float>(race,0x18+0x14C);
 bool restart=running&&(!wasRunning||race!=currentRace||key!=currentKey||(std::isfinite(seconds)&&seconds+.25f<previousSeconds));
 if(restart){MWELResetNeoDriverRace();MWELResetNeoProviderRace();recordedGoals=0;ending=false;for(auto& motion:validation)motion={};Log("EVENT_RUNTIME_RACE_EPOCH key=%08X race=%08X player_controller_changed=%u retry_state_reset=1",key,unsigned(race),unsigned(careerAutodrive&&validationEnabled));AuditCheckpoints(race);}
 if(key!=currentKey&&currentKey){player_texture_repair::Queue(currentKey);MWELResetNeoDriverRace();MWELResetNeoProviderRace();recordedGoals=0;Log("EVENT_RUNTIME_RACE_EXIT old_key=%08X",currentKey);
  // Geometry is released at native Stop, before simulation teardown. This
  // later display callback observes the exit but must not unregister CARP
  // triggers after their physical world has already been dismantled.
 }
 // Explicit local frontend autopilot must relinquish input after native end.
 if(careerAutodrive&&validationEnabled&&wasRunning&&!running){
  const BYTE guard[]={0x8B,0x44,0x24,0x04,0x85,0xC0,0x75,0x12};
  if(!memcmp(reinterpret_cast<void*>(0x612420),guard,sizeof(guard))){
   reinterpret_cast<void(__cdecl*)(unsigned)>(0x612420)(0);
   Log("EXPERIMENT_CAREER_INPUT_RESTORED native_end_observed=1 result_state_written=0");
  }
 }
 currentRace=race;currentKey=key;wasRunning=running;previousSeconds=seconds;
 if(running){
  // Explicit isolated frontend playtest: use native player autopilot and the
  // same checked NeoAI trail as opponents. No debug race start or CP writes.
  if(careerAutodrive&&validationEnabled)reinterpret_cast<void(__cdecl*)(unsigned)>(0x6123B0)(0);
  Validate(race,seconds);Resources(race,key);
 }
 event_speedtraps::Frame(key,running);
 auto mapKey=Read<unsigned>(race,0x1960)==1?key:0;
 event_maps::Frame(mapKey!=0,mapKey);
 package_resources::RestoreRoamingRadar();
 PlayerResourceFrame(running);
}
void __cdecl Frame(){__try{FrameUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EVENT_RUNTIME_FRAME_REFUSED fault=%08X",unsigned(GetExceptionCode()));}}
void* FinishActor(void* actor){
 if(actor)return actor;
 // 006119F0 accepts NULL as the current local player. Resolve the same
 // IPlayer::GetSimable view before inspecting slot/body/IsPlayer; passing
 // NULL through our observer used to skip all Hot Pursuit finish rules.
 auto count=Read<unsigned>(reinterpret_cast<void*>(0x92D884));
 auto list=Read<BYTE**>(reinterpret_cast<void*>(0x92D87C));
 if(!count||count>8||!list)return nullptr;
 auto local=Read<BYTE*>(list);auto table=Read<void**>(local);
 auto getter=Read<void*>(table,4);
 if(uintptr_t(getter)<0x401000||uintptr_t(getter)>=0x889000)return nullptr;
 return reinterpret_cast<void*(__thiscall*)(void*)>(getter)(local);
}
void __cdecl Finish(void* argument){
 auto actor=FinishActor(argument);
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto key=MWELNeoCurrentKey();
 int slot=actor?reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor)-1:-1;
 const mwel::EventPackage* package=nullptr;auto clone=registration_experiment::Find(key);
 if(clone&&clone->race)for(const auto& p:packages)if(p.nativeNamespace==clone->name){package=&p;break;}
 bool genuine=false,player=false;
 if(package&&slot>=0&&slot<16&&!ending&&!(recordedGoals&(1u<<slot))){
  auto info=race+0x18+slot*0x194;NeoBodyMotion body{};auto table=Read<void**>(actor);
  if(table)player=reinterpret_cast<bool(__thiscall*)(void*)>(table[9])(actor);
  if(Body(Read<unsigned>(info),body)&&!Read<BYTE>(info,0x190)&&Read<int>(info,0x28)>=int(mwel::RaceLaps(package->authoring))){
   for(const auto& gate:package->authoring.at("gates"))if(gate.at("kind")=="finish"){
    const auto& p=gate.at("position");const auto& dimensions=gate.at("dimensions");
    float dx=body.position[0]+p.at(1).get<float>(),dy=body.position[1]-p.at(2).get<float>(),dz=body.position[2]-p.at(0).get<float>();
    float radius=0;for(const auto& component:dimensions){auto size=component.get<float>();radius+=size*size;}
    genuine=std::sqrt(dx*dx+dy*dy+dz*dz)<=std::sqrt(radius)*.5f+3.f;break;
   }
  }
 }
 Log("EVENT_RUNTIME_FINISH_OBSERVED key=%08X slot=%d null_argument=%u player=%u genuine=%u ending=%u",key,slot,unsigned(argument==nullptr),unsigned(player),unsigned(genuine),unsigned(ending));
 MWELHotPursuitFinish(actor,genuine,player,slot,originalFinish);
 if(genuine){recordedGoals|=1u<<slot;Log("EVENT_RUNTIME_GENUINE_GOAL key=%08X slot=%d player=%u forced=0",key,slot,unsigned(player));}
 if(genuine&&player&&race==Read<BYTE*>(reinterpret_cast<void*>(0x91E000)))profile_progress::Record(*package,race+0x18+slot*0x194,true,false);
}
void __fastcall End(void* race,void*){bool before=ending;ending=true;checkpoint_display::Reset();MWELCombatRaceExit();originalEnd(race);ending=before;}
unsigned StopUnsafe(bool resultPending){
 checkpoint_display::Reset();MWELCombatRaceExit();
 PlayerResourceBoundary("before_owned_stop");
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=stop objects=%u",removed);
 auto key=MWELNeoCurrentKey();Log("EVENT_RUNTIME_NATIVE_STOP key=%08X before_simulation_teardown=1",key);
 player_texture_repair::Queue(key);
 event_maps::RestoreCalibrations();
 // Native Stop also runs while the result menu still owns a retry-capable
 // activity. That activity reuses its marker references rather than InitRacers
 // on retry. Retain this completed combat race's objects until native roaming
 // or world teardown, which already release the exact owner before unmount.
 if(resultPending)Log("EVENT_RUNTIME_RESULT_GEOMETRY_RETAINED key=%08X native_retry_references_live=1 release_boundary=roaming_or_world",key);
 else if(key&&!registration_experiment::ReleaseGeometry(key)){enabled=false;Log("REFUSED owned_geometry_teardown owner=%08X",key);}
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();return key;
}
void ReleaseCharactersUnsafe(unsigned key){
 if(!key)return;PlayerResourceBoundary("before_owned_characters");
 if(!registration_experiment::ReleaseCharacters(key))Log("EVENT_EXTRA_CHARACTER_RELEASE_DEFERRED owner=%08X",key);
 PlayerResourceBoundary("after_owned_characters");
}
void __cdecl Stop(){
 const bool resultPending=MWELCombatResultPending();
 unsigned key=0;__try{key=StopUnsafe(resultPending);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_stop_teardown_fault=%08X native_stop_retained=1",unsigned(GetExceptionCode()));}
 // An owned cleanup fault must never skip the game's simulation teardown.
 // Native exceptions remain native exceptions, rather than being swallowed.
 originalStop();
 PlayerResourceBoundary("after_native_stop");
 __try{if(!resultPending)ReleaseCharactersUnsafe(key);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_character_release_fault=%08X",unsigned(GetExceptionCode()));}
}
unsigned RoamingUnsafe(){
 checkpoint_display::Reset();MWELCombatRaceExit();
 PlayerResourceBoundary("before_owned_roaming");
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=roaming objects=%u",removed);
 // Career abort enters roaming directly and bypasses the QuickRace Stop
 // callback. Release while the old race parameters and CARP world are live.
 auto key=MWELNeoCurrentKey();Log("EVENT_RUNTIME_ENTER_ROAMING key=%08X before_native_mode_reset=1",key);
 player_texture_repair::Queue(key);
 event_maps::RestoreCalibrations();
 if(key&&!registration_experiment::ReleaseGeometry(key)){enabled=false;Log("REFUSED owned_geometry_teardown owner=%08X",key);}
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();return key;
}
void AfterRoamingUnsafe(unsigned key){
 ReleaseCharactersUnsafe(key);
 package_resources::RestoreRoamingRadar();
}
void __fastcall Roaming(void* self,void*){
 unsigned key=0;__try{key=RoamingUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_roaming_teardown_fault=%08X native_roaming_retained=1",unsigned(GetExceptionCode()));}
 PlayerResourceBoundary("after_owned_roaming_before_native");
 originalRoaming(self);
 PlayerResourceBoundary("after_native_roaming");
 __try{AfterRoamingUnsafe(key);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_after_roaming_fault=%08X",unsigned(GetExceptionCode()));}
}
void WorldUnloadUnsafe(void* self){
 checkpoint_display::Reset();MWELCombatRaceExit();
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=world_unload objects=%u",removed);
 auto key=MWELNeoCurrentKey();Log("EVENT_RUNTIME_WORLD_UNLOAD key=%08X observed_key=%08X native_state=%u before_streaming_teardown=1",key,currentKey,Read<unsigned>(reinterpret_cast<void*>(0x925E90)));
 event_maps::RestoreCalibrations();
 if(key&&!registration_experiment::ReleaseGeometry(key)){enabled=false;Log("REFUSED owned_geometry_teardown owner=%08X",key);}
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();
}
void __fastcall WorldUnload(void* self,void*){__try{WorldUnloadUnsafe(self);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_world_unload_teardown_fault=%08X native_world_unload_retained=1",unsigned(GetExceptionCode()));}originalWorldUnload(self);}
void ExitWorldUnsafe(){
 checkpoint_display::Reset();MWELCombatRaceExit();
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=exit_world objects=%u",removed);
 auto key=MWELNeoCurrentKey();Log("EVENT_RUNTIME_EXIT_WORLD key=%08X observed_key=%08X native_state=%u before_state7=1",key,currentKey,Read<unsigned>(reinterpret_cast<void*>(0x925E90)));
 event_maps::RestoreCalibrations();
 if(key&&!registration_experiment::ReleaseGeometry(key)){enabled=false;Log("REFUSED owned_geometry_teardown owner=%08X",key);}
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();
}
void __cdecl ExitWorld(){__try{ExitWorldUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_exit_world_teardown_fault=%08X native_exit_world_retained=1",unsigned(GetExceptionCode()));}originalExitWorld();}
bool Install(bool teardownOnly=false){
 const BYTE finishGuard[]={0x64,0xA1,0,0,0,0,0x6A,0xFF},endGuard[]={0xA1,0,0xE0,0x91,0,0x53,0x8B,0xD9};
 const BYTE stopGuard[]={0x8B,0x0D,0,0xE0,0x91,0,0xE8,0xE5,0xAC,0xFD,0xFF};
 const BYTE roamingGuard[]={0x6A,0xFF,0x68,0xBE,0x60,0x87,0};
 const BYTE unloadGuard[]={0x53,0x56,0x57,0x8B,0xF9,0xB9,0xD0,0xA9,0x9A,0};
 const BYTE exitWorldGuard[]={0x53,0x56,0x6A,0x08,0x33,0xDB,0x53};
 auto finish=reinterpret_cast<void*>(0x6119F0),end=reinterpret_cast<void*>(0x6021B0),stop=reinterpret_cast<void*>(0x60DEB0);
 auto roaming=reinterpret_cast<void*>(0x5FE3E0);
 auto unload=reinterpret_cast<void*>(0x664250);
 auto exitWorld=reinterpret_cast<void*>(0x6617E0);
 const BYTE skinUnloadGuard[]={0x81,0xEC,0x5C,0x01,0,0,0x56};
 const BYTE connectionUnloadGuard[]={0x53,0x56,0x8B,0xF1,0x8B,0x46,0x28,0x33,0xDB};
 if(memcmp(reinterpret_cast<void*>(0x759BA0),skinUnloadGuard,sizeof(skinUnloadGuard))||memcmp(reinterpret_cast<void*>(0x75C4D0),connectionUnloadGuard,sizeof(connectionUnloadGuard)))return false;
 if((!teardownOnly&&(memcmp(finish,finishGuard,sizeof(finishGuard))||memcmp(end,endGuard,sizeof(endGuard))))||memcmp(stop,stopGuard,sizeof(stopGuard))||memcmp(roaming,roamingGuard,sizeof(roamingGuard))||memcmp(unload,unloadGuard,sizeof(unloadGuard))||memcmp(exitWorld,exitWorldGuard,sizeof(exitWorldGuard)))return false;
 struct Hook{void* site;void* detour;void** original;};const Hook hooks[]={{finish,Finish,reinterpret_cast<void**>(&originalFinish)},{end,End,reinterpret_cast<void**>(&originalEnd)},{stop,Stop,reinterpret_cast<void**>(&originalStop)},{roaming,Roaming,reinterpret_cast<void**>(&originalRoaming)},{unload,WorldUnload,reinterpret_cast<void**>(&originalWorldUnload)},{exitWorld,ExitWorld,reinterpret_cast<void**>(&originalExitWorld)},{reinterpret_cast<void*>(0x759BA0),player_texture_repair::UnloadPerms,reinterpret_cast<void**>(&player_texture_repair::originalUnloadPerms)},{reinterpret_cast<void*>(0x75C4D0),player_texture_repair::UnloadConnection,reinterpret_cast<void**>(&player_texture_repair::originalUnloadConnection)}};
 std::vector<void*> installed;auto rollback=[&](){for(auto site:installed){MH_DisableHook(site);MH_RemoveHook(site);}return false;};
 for(unsigned i=teardownOnly?2:0;i<std::size(hooks);++i){auto hook=hooks[i];if(MH_CreateHook(hook.site,hook.detour,hook.original)!=MH_OK)return rollback();installed.push_back(hook.site);}
 for(auto site:installed)if(MH_QueueEnableHook(site)!=MH_OK)return rollback();if(MH_ApplyQueued()!=MH_OK)return rollback();
 auto marker=game/L"MWEventLoader.runtime-validation-test";
 if(mwel::fs::exists(marker))validationEnabled=mwel::Read(marker,128)=="MW_EVENT_LOADER_RUNTIME_VALIDATION_V1\n";
 auto autopilot=game/L"MWEventLoader.career-autodrive-test";
 const BYTE autopilotGuard[]={0x8B,0x44,0x24,0x04,0x85,0xC0,0x75,0x12};
 careerAutodrive=validationEnabled&&mwel::fs::exists(autopilot)&&mwel::Read(autopilot,128)=="MW_EVENT_LOADER_CAREER_AUTODRIVE_V1\n"&&!memcmp(reinterpret_cast<void*>(0x6123B0),autopilotGuard,sizeof(autopilotGuard));
 enabled=true;Log("EVENT_RUNTIME_LIFECYCLE_ACTIVE debug_bootstrap=0 player_control_override=0 lab_validation=%u",unsigned(validationEnabled));return true;
}
}
