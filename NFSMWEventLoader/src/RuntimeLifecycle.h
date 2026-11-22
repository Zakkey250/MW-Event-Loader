#pragma once
#include "TougeSpeechLifetime.h"
namespace gauntlet_runtime {unsigned Vehicle(unsigned);}
namespace hot_pursuit_runtime {
unsigned HandOffPlayerSupport(unsigned eventKey);
void ReleasePlayerSupport(unsigned handle);
}
// Production observers never bootstrap a race or change player ownership.
// Explicit isolated career-autodrive fixtures are gated separately below.
namespace runtime_lifecycle {
bool enabled=false,ending=false;
BYTE* currentRace=nullptr;
unsigned currentKey=0,recordedGoals=0;
unsigned resultsOwner=0;
unsigned roamingPursuitPlayer=0;
bool wasRunning=false;
bool tougeReturnPending=false;
bool tougeExitAwaiting=false;
bool tougeReturnInFlight=false;
bool ReturnAfterEvent(){
 const auto package=event_race_policies::Current();
 return package&&(mwel::Feature(package->authoring,"returnToSafehouse")||touge_grid::Active(*package)||mwel_rpd::CaseFile(package->authoring));
}
BYTE* rpdIntroFailureRace=nullptr;
ULONGLONG rpdIntroFailureAt=0;
const char* rpdIntroFailureReason=nullptr;
bool rpdIntroFailureShown=false,rpdIntroExitQueued=false;
void __cdecl Stop();
void RpdIntroFailed(const char* reason){
 auto race=*reinterpret_cast<BYTE**>(0x91E000);
 if(!race||rpdIntroFailureRace==race)return;
 rpdIntroFailureRace=race;rpdIntroFailureAt=GetTickCount64();rpdIntroFailureReason=reason;rpdIntroFailureShown=false;rpdIntroExitQueued=false;
 Log("RPD_REQUIRED_INTRO_REFUSED reason=%s native_fallback=0 event_aborted=1",reason);
}
void(__thiscall* originalSpeechDefinition)(void*,void*,unsigned)=nullptr;
void RetireTougeSpeech(void* self,bool detached){
 if(!tougeReturnPending&&!tougeExitAwaiting&&!tougeReturnInFlight)return;
 unsigned owner=0;Copy(reinterpret_cast<void*>(0x992230),&owner,4);
 auto read=[](std::uintptr_t address)->std::uintptr_t{unsigned value=0;Copy(reinterpret_cast<void*>(address),&value,4);return value;};
 auto write=[](std::uintptr_t address,std::uintptr_t value){*reinterpret_cast<unsigned*>(address)=unsigned(value);};
 auto destroy=[](std::uintptr_t handle){reinterpret_cast<void(__thiscall*)(void*,unsigned)>(0x4CE3A0)(reinterpret_cast<void*>(handle),1);};
 if(touge_speech_lifetime::Retire(tougeReturnPending||tougeExitAwaiting||tougeReturnInFlight,
       reinterpret_cast<std::uintptr_t>(self),owner,detached,read,write,destroy))
  Log("TOUGE_RETURN_SPEECH_RETIRED native_deleting_destructor=1 definition_detach_boundary=1");
}
void __fastcall SpeechDefinition(void* self,void*,void* definition,unsigned kind){
 if(!definition&&kind==1)RetireTougeSpeech(self,true);
 originalSpeechDefinition(self,definition,kind);
}
void RetireDetachedTougeSpeech(){
 if(!tougeReturnPending&&!tougeExitAwaiting&&!tougeReturnInFlight)return;
 unsigned speech=0,definition=0;Copy(reinterpret_cast<void*>(0x992230),&speech,4);
 if(speech&&Copy(reinterpret_cast<void*>(speech+0x44),&definition,4)&&!definition)
  RetireTougeSpeech(reinterpret_cast<void*>(speech),true);
}
void QueueTougeSafehouse(){
 // Native Roaming updates RaceStatus before the asynchronous HUD/map switch.
 // HUD destruction (005A0910 -> 0057C530 -> 006CB2A0) pumps pending resource
 // callbacks. Stage 2 (005A07ED -> 0058C8B0) reads RaceStatus, which native
 // ExitWorld has already released. Wait for stage 3's completion signal.
 // Actual roaming keeps HUD map mode 1; it is not RaceStatus play mode 0.
 unsigned flow=0,mode=~0u,mapMode=~0u,stage=0,ready=0;BYTE* race=nullptr;
 Copy(reinterpret_cast<void*>(0x925E90),&flow,4);Copy(reinterpret_cast<void*>(0x91E000),&race,4);
 if(race)Copy(race+0x1960,&mode,4);
 Copy(reinterpret_cast<void*>(0x91CF20),&mapMode,4);Copy(reinterpret_cast<void*>(0x91CF14),&stage,4);Copy(reinterpret_cast<void*>(0x8F511C),&ready,4);
 if(flow!=6||mode!=0||stage!=3||ready!=2){
  static unsigned previous=~0u;unsigned state=(flow<<24)|(mode<<16)|(mapMode<<8)|(stage<<4)|ready;
  if(state!=previous){previous=state;Log("TOUGE_SAFEHOUSE_WAIT flow=%u play_mode=%u radar_mode=%u load_stage=%u radar_ready=%u",flow,mode,mapMode,stage,ready);}return;
 }
 const BYTE guard[]={0x6A,0xFF,0x68,0x5F,0x65,0x87,0x00,0x64,0xA1};
 tougeReturnPending=tougeExitAwaiting=false;
 tougeReturnInFlight=true;
 if(!memcmp(reinterpret_cast<void*>(0x6052B0),guard,sizeof(guard))){reinterpret_cast<void(__cdecl*)()>(0x6052B0)();Log("TOUGE_NATIVE_SAFEHOUSE_RETURN_QUEUED original_game_command=1");}
 else Log("EXPERIMENT_RACE_ABORT touge_safehouse_command_surface");
}
float previousSeconds=0;
struct ValidationMotion {unsigned handle=0,routeRevision=0;int checkpoint=-1;float station=0,gateBest=std::numeric_limits<float>::infinity();float position[3]{};ULONGLONG progressed=0,pending=0,moved=0,logged=0;bool stationObserved=false,recoveryVerificationUsed=false;};
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
void AuditCheckpoints(BYTE* race){
 if(!worldLogEnabled)return;
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
 const auto package=event_race_policies::Current();
 const bool routeRequired=!(package&&mwel_rpd::Armed(package->authoring)&&mwel_rpd::FreeRun(package->authoring));
 // Native race time freezes while the user pauses. Wall-clock time would
 // incorrectly classify a menu pause as an unrecovered controlled vehicle.
 auto now=ULONGLONG(double(seconds)*1000)+1;
 // Include the local body only in the explicit isolated autodrive fixture.
 // Never substitute speedometer values or recovery flags for actual motion.
 for(unsigned slot=careerAutodrive?0:1;slot<count;++slot){auto info=race+0x18+slot*0x194;unsigned handle=Read<unsigned>(info);if(!handle||(recordedGoals&(1u<<slot))||MWELCombatRetired(slot,handle))continue;
  NeoBodyMotion body{};if(!Body(handle,body))continue;int cp=Read<int>(info,0x2C);auto& m=validation[slot];
  if(m.handle!=handle){m={};m.handle=handle;m.checkpoint=-1;memcpy(m.position,body.position,12);m.moved=now;}
  if(std::hypot(body.position[0]-m.position[0],body.position[2]-m.position[2])>.25f){memcpy(m.position,body.position,12);m.moved=now;}
  if(cp!=m.checkpoint){m.checkpoint=cp;m.progressed=m.pending=0;m.gateBest=std::numeric_limits<float>::infinity();m.stationObserved=m.recoveryVerificationUsed=false;}
  float station=0;unsigned revision=0;bool progress=routeRequired&&cp>=0&&MWELNeoRouteProgress(handle,unsigned(cp),&station,&revision);
  if(progress){
   // Gate distance may update the watchdog before NeoAI reports the new leg.
   // Establish its own projection baseline even when that watchdog was set.
   const bool rebase=m.stationObserved&&m.routeRevision!=revision;
   if(!m.stationObserved||rebase||(station>m.station+5.f&&now-m.moved<1000)){
    m.stationObserved=true;m.routeRevision=revision;m.station=station;m.progressed=now;m.recoveryVerificationUsed=false;
    if(rebase)Log("EVENT_RUNTIME_CHECKED_ROAD_REBASE handle=%u CP=%d revision=%u station=%.3f native_progress_written=0 body_watchdog_unchanged=1",handle,cp,revision,station);
   }
  }
  // Police can push a racer off the projected line. A stale line cursor is
  // not proof that its physically moving body failed to recover. Permit only
  // a new best approach to the real pending gate to extend this observation;
  // circling/recovery flags alone cannot keep it alive. Never write race CPs.
  float gate[3]{},dimensions[3]{},rotation=0;
  if(routeRequired&&cp>=0&&MWELNeoGate(currentKey,unsigned(cp),gate,dimensions,&rotation)){
   const float distance=std::hypot(std::hypot(body.position[0]-gate[0],body.position[2]-gate[2]),body.position[1]-gate[1]);
   if(std::isfinite(distance)&&distance+5.f<m.gateBest&&now-m.moved<1000){m.gateBest=distance;m.progressed=now;m.recoveryVerificationUsed=false;}
  }
  if(seconds>25&&m.progressed&&now-m.progressed>20000&&mwel_recovery_observation::VerifyOnce(m.recoveryVerificationUsed,MWELNeoRecoveryResumed(handle,unsigned(cp)))){m.progressed=now-16000;Log("EVENT_RUNTIME_RECOVERY_RESUMPTION_WINDOW handle=%u CP=%d seconds=4 checked_forward_body_metres=2 once_until_real_progress=1 body_watchdog_unchanged=1",handle,cp);}
  bool pending=routeRequired&&cp>=0&&MWELNeoPendingGateEnd(handle,unsigned(cp));if(!pending)m.pending=0;else if(!m.pending)m.pending=now;
  if(now-m.logged>=1000){m.logged=now;Log("EVENT_RUNTIME_RACE_BODY handle=%u slot=%u CP=%d position=%.3f,%.3f,%.3f station=%.3f pending=%u movement_age_ms=%llu",handle,slot,cp,body.position[0],body.position[1],body.position[2],station,unsigned(pending),now-m.moved);}
  if((m.pending&&now-m.pending>8000)||(seconds>25&&now-m.moved>20000)||(routeRequired&&seconds>25&&m.progressed&&now-m.progressed>20000)){
   AuditCheckpoints(race);validationEnabled=false;Log("EXPERIMENT_RACE_ABORT production_opponent_cannot_resume handle=%u slot=%u CP=%d station=%.3f pending=%u body_observed=1",handle,slot,cp,station,unsigned(pending));return;
  }
 }
}
void FrameUnsafe(){
 if(rpdIntroFailureRace){
  auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
  if(race!=rpdIntroFailureRace){rpdIntroFailureRace=nullptr;rpdIntroFailureAt=0;}
  else{
   if(!rpdIntroFailureShown){
    const unsigned list=Read<unsigned>(reinterpret_cast<void*>(0x92D87C));
    const unsigned player=Read<unsigned>(reinterpret_cast<void*>(list));
    const auto hud=player?reinterpret_cast<unsigned(__thiscall*)(unsigned)>(0x6F8F10)(player):0;
    const unsigned com=hud?Read<unsigned>(reinterpret_cast<void*>(hud+4)):0;
    const auto generic=com?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x5650B0):0;
    if(generic){
     const unsigned title=event_presentation::Hash("MWEL_RPD_INTRO_UNAVAILABLE");
     std::wstring reason;for(const char* p=rpdIntroFailureReason;p&&*p;++p)reason+=wchar_t(*p);
     event_titles::titles[title]=event_titles::Title{L"INTRO UNAVAILABLE: "+reason,"MWEL_RPD_INTRO_UNAVAILABLE",{},L"\u958b\u59cb\u6f14\u51fa\u3092\u518d\u751f\u3067\u304d\u307e\u305b\u3093: "+reason};
     reinterpret_cast<bool(__thiscall*)(unsigned,const char*,bool,unsigned,unsigned,unsigned,unsigned)>(0x568030)(generic,event_titles::Lookup(nullptr,title),false,0x8AB83EDB,0,0,2);
     rpdIntroFailureShown=true;
    }
   }
   // A mandatory intro failure never releases a playable race. Retain the
   // native player, stop it through IVehicle, then use Game_AbandonRace's
   // normal Stop/roaming/safehouse teardown on this game-thread boundary.
   const auto car=gauntlet_runtime::Vehicle(Read<unsigned>(race,0x18));
   if(car)reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->ForceStopOn(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));
   if(!rpdIntroExitQueued&&GetTickCount64()-rpdIntroFailureAt>=1500){
    // Queue the native Game_JumpToSafehouse command. Game_AbandonRace alone
    // only stops racer activity; it does not own the world transition.
    const BYTE guard[]={0x6A,0xFF,0x68,0x5F,0x65,0x87,0x00,0x64,0xA1};
    if(!memcmp(reinterpret_cast<void*>(0x6052B0),guard,sizeof(guard))){
     rpdIntroExitQueued=true;reinterpret_cast<void(__cdecl*)()>(0x6052B0)();
     Log("RPD_INTRO_FAILURE_SAFEHOUSE_QUEUED native_command=1 reason=%s",rpdIntroFailureReason);
    }else Log("RPD_INTRO_ABORT_UNAVAILABLE native_command_surface_changed=1");
   }
   return;
  }
 }
 if((tougeReturnPending||tougeExitAwaiting)&&Read<unsigned>(reinterpret_cast<void*>(0x925E90))==6){
  auto status=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
  if(status&&Read<unsigned>(status,0x1960)==0){
   // Verified native Game_JumpToSafehouse queues the standard game command.
   // Wait until roaming owns the world; preserve the native results and retry.
   QueueTougeSafehouse();
  }
 }
 if(!enabled)return;
 native_runtime_diagnostics::Hud();
 profile_progress::RestoreNativeProgress();
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto key=MWELNeoCurrentKey();
 const bool nativeRunning=race&&Read<unsigned>(reinterpret_cast<void*>(0x925E90))==6&&Read<unsigned>(race,0x1960)==1&&Read<BYTE>(race,0x198A)&&Read<BYTE>(race,0x1978);
 const bool running=key&&nativeRunning;
 float seconds=running?Read<float>(race,0x18+0x150):0;
 if(running&&Read<BYTE>(race,0x18+0x154))seconds+=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-Read<float>(race,0x18+0x14C);
 bool restart=running&&(!wasRunning||race!=currentRace||key!=currentKey||(std::isfinite(seconds)&&seconds+.25f<previousSeconds));
 if(restart){resultsOwner=0;roamingPursuitPlayer=0;event_speedtraps::Reset();MWELResetNeoDriverRace();MWELResetNeoProviderRace();recordedGoals=0;ending=false;for(auto& motion:validation)motion={};Log("EVENT_RUNTIME_RACE_EPOCH key=%08X race=%08X player_controller_changed=%u retry_state_reset=1",key,unsigned(race),unsigned(careerAutodrive&&validationEnabled));AuditCheckpoints(race);}
 if(key!=currentKey&&currentKey){event_maps::RestoreCalibrations();MWELResetNeoDriverRace();MWELResetNeoProviderRace();recordedGoals=0;Log("EVENT_RUNTIME_RACE_EXIT old_key=%08X",currentKey);
  // Only native lifecycle boundaries own geometry teardown.
 }
 // Explicit local frontend autopilot must relinquish input after native end.
 static bool previousAutodrive=false;
 const bool autodrive=careerAutodrive&&validationEnabled&&nativeRunning;
 if(previousAutodrive&&!autodrive){
  const BYTE guard[]={0x8B,0x44,0x24,0x04,0x85,0xC0,0x75,0x12};
  if(!memcmp(reinterpret_cast<void*>(0x612420),guard,sizeof(guard))){
   reinterpret_cast<void(__cdecl*)(unsigned)>(0x612420)(0);
   Log("EXPERIMENT_CAREER_INPUT_RESTORED native_end_observed=1 result_state_written=0");
  }
 }
 previousAutodrive=autodrive;
 if(autodrive)reinterpret_cast<void(__cdecl*)(unsigned)>(0x6123B0)(0);
 currentRace=race;currentKey=key;wasRunning=running;previousSeconds=seconds;
 if(running){
  // Explicit isolated frontend playtest: use native player autopilot and the
  // same checked NeoAI trail as opponents. No debug race start or CP writes.
  Validate(race,seconds);Resources(race,key);
 }
 event_speedtraps::Frame(key,race,nativeRunning);
 // The result menu retains the race record and can retain its minimap until
 // the next rival stage creates a HUD. Keep the calibration restored after
 // Stop, including while the old record still reports Race play mode.
 auto mapKey=key!=resultsOwner&&Read<unsigned>(race,0x1960)==1?key:0;
 event_maps::Frame(mapKey!=0,mapKey);
 // A Touge exit immediately unloads the world. Starting the asynchronous
 // roaming radar load here leaves its callback reading a destroyed RaceStatus
 // (0058C760) during safehouse entry. Let native world unload own the radar.
 if(!tougeReturnPending&&!tougeExitAwaiting&&!tougeReturnInFlight)package_resources::RestoreRoamingRadar();
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
 const bool policeSprint=MWELRpdHotPursuitScope();
 if(!policeSprint&&!MWELRpdFinishAllowed()){Log("RPD_ROUTE_FINISH_IGNORED arrest_or_timeout_required=1");return;}
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
 if(policeSprint){
  if(genuine&&!player&&slot>0)MWELRpdHotPursuitGoal(unsigned(race),key,unsigned(slot),Read<unsigned>(race+0x18+slot*0x194));
  return; // A suspect crossing is not a player finish or a win/loss.
 }
 Log("EVENT_RUNTIME_FINISH_OBSERVED key=%08X slot=%d null_argument=%u player=%u genuine=%u ending=%u",key,slot,unsigned(argument==nullptr),unsigned(player),unsigned(genuine),unsigned(ending));
 if(slot>=0)MWELDriftFinish(unsigned(slot),genuine);
 MWELHotPursuitFinish(actor,genuine,player,slot,originalFinish);
 if(genuine){recordedGoals|=1u<<slot;Log("EVENT_RUNTIME_GENUINE_GOAL key=%08X slot=%d player=%u forced=0",key,slot,unsigned(player));}
 if(genuine&&player&&race==Read<BYTE*>(reinterpret_cast<void*>(0x91E000)))profile_progress::Record(*package,race+0x18+slot*0x194,true,false);
}
void __fastcall End(void* race,void*){
 bool before=ending;ending=true;
 auto key=MWELNeoCurrentKey();
 if(key==registration_experiment::mountedOwner)resultsOwner=key;
 event_speedtraps::Reset();
 checkpoint_display::Reset();MWELCombatRaceExit(true);originalEnd(race);ending=before;
}
unsigned GeometryOwner(){
 if(registration_experiment::mountedOwner)return registration_experiment::mountedOwner;
 return MWELNeoCurrentKey();
}
void ReleaseGates(unsigned key){
 if(key&&!registration_experiment::ReleaseGeometry(key)){enabled=false;Log("REFUSED owned_geometry_teardown owner=%08X",key);}
}
unsigned StopUnsafe(bool resultPending){
 if(ReturnAfterEvent())tougeExitAwaiting=true;
 // Cover a definition already detached before the return request. The
 // detachment hook handles subsequent native resource callbacks as well.
 RetireDetachedTougeSpeech();
 hot_pursuit_runtime::ReleasePlayerSupport(roamingPursuitPlayer);
 event_speedtraps::Reset();checkpoint_display::Reset();MWELCombatRaceExit(resultPending);
 PlayerResourceBoundary("before_owned_stop");
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=stop objects=%u",removed);
 auto key=GeometryOwner();Log("EVENT_RUNTIME_NATIVE_STOP key=%08X before_simulation_teardown=1",key);
 event_maps::RestoreCalibrations();
 // Native Stop also runs while the result menu still owns a retry-capable
 // activity. That activity reuses its marker references rather than InitRacers
 // on retry. Native roaming or world teardown retires these references.
 if(key&&(resultPending||resultsOwner==key))Log("EVENT_RUNTIME_RESULT_GEOMETRY_RETAINED key=%08X native_retry_references_live=1 release_boundary=roaming_or_world",key);
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
 const bool retain=key&&(resultPending||resultsOwner==key);
 // An owned cleanup fault must never skip the game's simulation teardown.
 // Native exceptions remain native exceptions, rather than being swallowed.
 originalStop();
 PlayerResourceBoundary("after_native_stop");
 __try{if(!retain){ReleaseGates(key);ReleaseCharactersUnsafe(key);}}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_after_stop_fault=%08X",unsigned(GetExceptionCode()));}
}
unsigned RoamingUnsafe(){
 if(ReturnAfterEvent())tougeReturnPending=true;
 event_speedtraps::Reset();checkpoint_display::Reset();MWELCombatRaceExit();
 PlayerResourceBoundary("before_owned_roaming");
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=roaming objects=%u",removed);
 // Capture the owner before roaming clears the native race parameters.
 auto key=GeometryOwner();Log("EVENT_RUNTIME_ENTER_ROAMING key=%08X before_native_mode_reset=1",key);
 if(auto player=hot_pursuit_runtime::HandOffPlayerSupport(key))roamingPursuitPlayer=player;
 event_maps::RestoreCalibrations();
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();return key;
}
void AfterRoamingUnsafe(unsigned key){
 resultsOwner=0;ReleaseGates(key);
 ReleaseCharactersUnsafe(key);
 if(!tougeReturnPending&&!tougeExitAwaiting&&!tougeReturnInFlight)package_resources::RestoreRoamingRadar();
}
void __fastcall Roaming(void* self,void*){
 unsigned key=0;__try{key=RoamingUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_roaming_teardown_fault=%08X native_roaming_retained=1",unsigned(GetExceptionCode()));}
 PlayerResourceBoundary("after_owned_roaming_before_native");
 originalRoaming(self);
 PlayerResourceBoundary("after_native_roaming");
 __try{AfterRoamingUnsafe(key);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_after_roaming_fault=%08X",unsigned(GetExceptionCode()));}
}
void WorldUnloadUnsafe(void* self){
 hot_pursuit_runtime::ReleasePlayerSupport(roamingPursuitPlayer);roamingPursuitPlayer=0;
 event_speedtraps::Reset();checkpoint_display::Reset();MWELCombatRaceExit();
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=world_unload objects=%u",removed);
 auto key=GeometryOwner();Log("EVENT_RUNTIME_WORLD_UNLOAD key=%08X observed_key=%08X native_state=%u before_streaming_teardown=1",key,currentKey,Read<unsigned>(reinterpret_cast<void*>(0x925E90)));
 event_maps::RestoreCalibrations();
 resultsOwner=0;ReleaseGates(key);
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();
}
void __fastcall WorldUnload(void* self,void*){__try{WorldUnloadUnsafe(self);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_world_unload_teardown_fault=%08X native_world_unload_retained=1",unsigned(GetExceptionCode()));}originalWorldUnload(self);if(tougeExitAwaiting)QueueTougeSafehouse();}
void ExitWorldUnsafe(){
 // Retain the event's safehouse return through the native result ExitWorld.
 if(ReturnAfterEvent())tougeExitAwaiting=true;
 RetireDetachedTougeSpeech();
 hot_pursuit_runtime::ReleasePlayerSupport(roamingPursuitPlayer);roamingPursuitPlayer=0;
 event_speedtraps::Reset();checkpoint_display::Reset();MWELCombatRaceExit();
 MWELCancelCarbonIntro();auto removed=expanded_leaderboard::Reset();Log("EVENT_HUD_RUNTIME_ROWS_RELEASED boundary=exit_world objects=%u",removed);
 auto key=GeometryOwner();Log("EVENT_RUNTIME_EXIT_WORLD key=%08X observed_key=%08X native_state=%u before_state7=1",key,currentKey,Read<unsigned>(reinterpret_cast<void*>(0x925E90)));
 event_maps::RestoreCalibrations();
 resultsOwner=0;ReleaseGates(key);
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();
}
void __cdecl ExitWorld(){__try{ExitWorldUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;Log("EXPERIMENT_RACE_ABORT owned_exit_world_teardown_fault=%08X native_exit_world_retained=1",unsigned(GetExceptionCode()));}originalExitWorld();}
#ifdef MWEL_PRIVATE_RPD_RESIDENCY_BASELINE
#include "PlayerTextureRepair.h"
#endif
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
 const BYTE speechDefinitionGuard[]={0x8B,0x44,0x24,0x04,0x33,0xD2,0x3B,0xC2,0xC6,0x41,0x04,0x01};
 const BYTE speechDestroyGuard[]={0x56,0x8B,0xF1,0xE8,0x78,0xF1,0xFF,0xFF};
 if(memcmp(reinterpret_cast<void*>(0x6FF740),speechDefinitionGuard,sizeof(speechDefinitionGuard))||memcmp(reinterpret_cast<void*>(0x4CE3A0),speechDestroyGuard,sizeof(speechDestroyGuard)))return false;
 if((!teardownOnly&&(memcmp(finish,finishGuard,sizeof(finishGuard))||memcmp(end,endGuard,sizeof(endGuard))))||memcmp(stop,stopGuard,sizeof(stopGuard))||memcmp(roaming,roamingGuard,sizeof(roamingGuard))||memcmp(unload,unloadGuard,sizeof(unloadGuard))||memcmp(exitWorld,exitWorldGuard,sizeof(exitWorldGuard)))return false;
 struct Hook{void* site;void* detour;void** original;};const Hook hooks[]={{finish,Finish,reinterpret_cast<void**>(&originalFinish)},{end,End,reinterpret_cast<void**>(&originalEnd)},{stop,Stop,reinterpret_cast<void**>(&originalStop)},{roaming,Roaming,reinterpret_cast<void**>(&originalRoaming)},{unload,WorldUnload,reinterpret_cast<void**>(&originalWorldUnload)},{exitWorld,ExitWorld,reinterpret_cast<void**>(&originalExitWorld)}
#ifdef MWEL_PRIVATE_RPD_RESIDENCY_BASELINE
 ,{reinterpret_cast<void*>(0x759BA0),player_texture_repair::UnloadPerms,reinterpret_cast<void**>(&player_texture_repair::originalUnloadPerms)},{reinterpret_cast<void*>(0x75C4D0),player_texture_repair::UnloadConnection,reinterpret_cast<void**>(&player_texture_repair::originalUnloadConnection)}
#endif
 };
 std::vector<void*> installed;auto rollback=[&](){for(auto site:installed){MH_DisableHook(site);MH_RemoveHook(site);}return false;};
 auto speechSite=reinterpret_cast<void*>(0x6FF740);
 if(MH_CreateHook(speechSite,SpeechDefinition,reinterpret_cast<void**>(&originalSpeechDefinition))!=MH_OK)return false;
 installed.push_back(speechSite);
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
