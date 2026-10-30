// Local-lab native start proof. Authoring geometry and save integration remain
// separate acceptance gates; this launches only the inherited template route.
#include "../../World2MW/src/NativeLoader/NeoBody.h"
namespace race_start_experiment {
void(__cdecl* originalStart)();
void(__thiscall* originalDriving)(void*,float);
bool playerSetup=false;
bool nativeBaseline=false;
bool neoObserver=false;
bool manualPlayer=false;
bool playerEngineTest=false,playerPadEngineTest=false;
bool observerRegistered=false;
bool radarRoamingTest=false,radarRoamingEntered=false;
bool customTransitionTest=false,customTransitionInvoked=false;
bool completedTransitionTest=false,completedGoalObserved=false;
bool careerChainTest=false;
unsigned careerObservedKey=0;
unsigned completedGoalPlace=0;
ULONGLONG completedDelayAt=0;
unsigned transitionLimit=1,transitionCount=0;
std::string transitionPrimary,transitionSecondary;
ULONGLONG roamingEnteredAt=0;
BYTE* unloadPlayerVehicle=nullptr;
float unloadPlayerAnchor[3]{};
bool unloadPlayerAnchored=false;
bool unloadPlayerReleased=false;
bool retainUnloadHold=false;
ULONGLONG unloadPlayerLogged=0;
bool ConfigureCustomTransition(){
 auto marker=game/L"MWEventLoader.custom-transition-test";if(!mwel::fs::exists(marker))return false;
 auto text=mwel::Read(marker,256);
 careerChainTest=text.rfind("MW_EVENT_LOADER_CAREER_CHAIN_V1\n",0)==0;
 completedTransitionTest=text.rfind("MW_EVENT_LOADER_COMPLETED_TRANSITION_V1\n",0)==0;
 const std::string prefix=careerChainTest?"MW_EVENT_LOADER_CAREER_CHAIN_V1\n":completedTransitionTest?"MW_EVENT_LOADER_COMPLETED_TRANSITION_V1\n":"MW_EVENT_LOADER_CUSTOM_TRANSITION_V1\n";
 if(text.rfind(prefix,0)!=0||text.back()!='\n')throw std::runtime_error("Invalid custom transition marker");
 auto split=text.find('\n',prefix.size());if(split==std::string::npos)throw std::runtime_error("Missing transition IDs");
 transitionPrimary=text.substr(prefix.size(),split-prefix.size());transitionSecondary=text.substr(split+1,text.size()-split-2);
 if(packages.size()!=2||!mwel::EventId(transitionPrimary)||!mwel::EventId(transitionSecondary)||transitionPrimary==transitionSecondary)throw std::runtime_error("Invalid transition package pair");
 for(auto id:{transitionPrimary,transitionSecondary})if(std::none_of(packages.begin(),packages.end(),[&](const auto& p){return p.id==id;}))throw std::runtime_error("Transition package missing");
 auto repeat=game/L"MWEventLoader.custom-transition-repeat-test";
 if(mwel::fs::exists(repeat)){
  auto value=mwel::Read(repeat,64);const std::string header="MW_EVENT_LOADER_CUSTOM_TRANSITION_REPEAT_V1\n";
  if(value.rfind(header,0)!=0||value.back()!='\n')throw std::runtime_error("Invalid transition repeat marker");
  auto count=value.substr(header.size(),value.size()-header.size()-1);
  if(count.empty()||count.size()>2||count.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid transition repeat count");
  transitionLimit=std::stoul(count);if(!transitionLimit||transitionLimit>16)throw std::runtime_error("Transition repeat count exceeds lab bound");
 }
 customTransitionTest=true;return true;
}
const mwel::EventPackage& BootstrapPackage(){
 if(customTransitionTest)for(const auto& p:packages)if(p.id==transitionPrimary)return p;
 return packages.front();
}
void __cdecl ObserveFrame();
std::string selection;
std::string playerCar="bmwm3gtre46";
ULONGLONG firstDriving=0;
unsigned faults=0;
bool aborted=false;
void* checkpointAuditRace=nullptr;
bool checkpointLayoutLogged=false;
struct Motion {unsigned handle=0,routeRevision=0;float initial[3]{};float last[3]{};float progressAnchor[3]{};float routeStation=0;ULONGLONG moved=0,sampled=0,progress=0,pendingGateAt=0,routeProgressAt=0;int checkpoint=-1;bool eliminationLogged=false,recoveryVerificationUsed=false;};
Motion motions[16]{};
template<class T>T Read(void* ptr,unsigned offset=0){T result{};if(ptr)Copy(static_cast<BYTE*>(ptr)+offset,&result,sizeof(result));return result;}

#include "NativePerformanceFixture.h"
#include "NativeGoalEvidence.h"
void ResetObservation(){
 RestoreObservedFinishers();
 completedDelayAt=0;
 // Native retry may reuse the same race allocation and vehicle handles. The
 // real countdown boundary also reaches this reset, not just debug bootstrap.
 playerSetup=false;firstDriving=0;faults=0;aborted=false;unloadPlayerVehicle=nullptr;unloadPlayerAnchored=false;unloadPlayerReleased=false;unloadPlayerLogged=0;for(auto& motion:motions)motion={};
 genuineGoals=0;forcedEstimationObserved=false;deferredPlayer=nullptr;deferredRace=nullptr;deferredPlayerSeconds=0;
 deferredEnd=deferredResults=deferredStop=deferredShow=deferredShowArgument=false;
 deferredTriggers.clear();
 checkpointAuditRace=nullptr;checkpointLayoutLogged=false;
 for(auto& handle:performanceFixtureHandles)handle=0;
 for(auto& handle:performanceAuditHandles)handle=0;
}
void __cdecl Start(){
 ResetObservation();
 MWELResetNeoDriverRace();MWELResetNeoProviderRace();
 Log("EXPERIMENT_RACE_STATE_RESET observations=1 owned_controllers=1 pointer_reuse_safe=1");
 if(neoObserver&&!observerRegistered){
  auto module=GetModuleHandleW(L"NFSMWNativeFreeRoamRacers.asi");
  using Register=int(__cdecl*)(void(__cdecl*)());
  auto registerObserver=module?reinterpret_cast<Register>(GetProcAddress(module,"NFRR_RegisterFrameObserverV1")):nullptr;
  if(!registerObserver||!registerObserver(ObserveFrame)){Log("EXPERIMENT_RACE_ABORT NeoAI_frame_observer_unavailable");return;}
  observerRegistered=true;Log("EXPERIMENT_NEOAI_FRAME_OBSERVER active=1 driving_hook_owner=NFRR");
 }
 *reinterpret_cast<unsigned*>(0x926064)=1;*reinterpret_cast<char*>(0x926068)='1';
 *reinterpret_cast<unsigned*>(0x926094)=0;
 // Native debug startup treats 8F86C0 as force-cops-OFF, not enable cops.
 // Preserve the authored policy so a police mode is actually tested as one.
 const bool wantPolice=registration_experiment::authoredGeometry&&!nativeBaseline&&BootstrapPackage().authoring.value("cops",false);
 *reinterpret_cast<unsigned*>(0x8F86C0)=wantPolice?0u:1u;
 *reinterpret_cast<float*>(0x926090)=0;
 // Native debug start passes this lap option to GRaceParameters::SetNumLaps
 // at 006EA0BD..006EA0CC. Its default 2 otherwise overrides the package.
 // Configure the fixture before native setup; never write live CP/finish state.
 if(registration_experiment::authoredGeometry&&!nativeBaseline)*reinterpret_cast<unsigned*>(0x8F86BC)=mwel::RaceLaps(BootstrapPackage().authoring);
 auto& clone=*registration_experiment::clones.front();
 auto db=*reinterpret_cast<void**>(0x91E004);
 unsigned key=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)(selection.c_str());
 auto descriptor=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5FB710)(db,key);
 Log("EXPERIMENT_NATIVE_START key=%08X descriptor=%08X event=%s native_baseline=%u geometry=%s save_load=skipped",key,unsigned(descriptor),selection.c_str(),unsigned(nativeBaseline),registration_experiment::authoredGeometry?"authored_owned":"inherited_template");
 if(!descriptor){Log("EXPERIMENT_RACE_ABORT event_not_registered");return;}
 if(registration_experiment::authoredGeometry){
  unsigned selectedOwner=0;
  if(careerChainTest)for(const auto& c:registration_experiment::clones)if(c->race&&c->eventId==selection)selectedOwner=c->collection.key;
  if(careerChainTest&&!selectedOwner){Log("EXPERIMENT_RACE_ABORT career_primary_owner_missing");return;}
  for(auto& gate:registration_experiment::clones)if(!gate->race&&!gate->bin&&!gate->placement){
   if(careerChainTest&&gate->ownerKey!=selectedOwner)continue;
   if(!registration_experiment::GeometryObject(gate->collection.key,gate->templateKey==0xAAF8BB9C)){Log("EXPERIMENT_RACE_ABORT geometry_object_unavailable");return;}
  }
 }
 originalStart();
}
void SampleUnsafe(void* primary){
 if(aborted)return;
 auto owner=Read<unsigned>(primary);if(owner!=0x892720&&owner!=0x892AD0)return;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);
 if(!race||Read<unsigned>(race,0x1960)!=1)return;
 if(!Read<BYTE>(race,0x198A)||!Read<BYTE>(race,0x1978))return;
 if(!checkpointLayoutLogged){
  for(const auto& clone:registration_experiment::clones)if(clone->race&&!clone->correction){auto key=registration_experiment::Hash(clone->eventId.c_str());auto object=registration_experiment::EventLookup(key);Log("EVENT_NATIVE_OBJECT_AUDIT event_id=%08X object=%08X",key,unsigned(object));}
 }
 if(checkpointAuditRace!=race){
  auto parameters=Read<BYTE*>(race,0x1968),index=Read<BYTE*>(parameters,4);
  Log("EXPERIMENT_NATIVE_LAP_CONFIGURATION authored=%u runtime=%u index=%08X index_laps=%u",unsigned(mwel::RaceLaps(packages.front().authoring)),reinterpret_cast<unsigned(__thiscall*)(void*)>(0x5FBA00)(parameters),unsigned(index),unsigned(Read<BYTE>(index,0x28)));
  auto instance=Read<BYTE*>(parameters,8);auto collection=Read<registration_experiment::Collection*>(instance,4);
  for(unsigned depth=0;collection&&depth<8;++depth){
   for(unsigned n=0;n<collection->capacity&&n<4096;++n){const auto& field=collection->nodes[n];if(field.key==0x0EBDC165)Log("EXPERIMENT_NATIVE_LAP_LAYER depth=%u key=%08X value=%u flags=%08X own_collection=%u",depth,collection->key,field.value,field.flags,unsigned(registration_experiment::Find(collection->key)!=nullptr));}
   collection=collection->parent;
  }
  using NativeRace=NFSPluginSDK::MW05::GRaceStatus;
  static_assert(offsetof(NativeRace,mRaceParms)==0x1968);
  // Executable evidence places this vector at 0x19AC. The SDK layout omits
  // a preceding word and must not be used to locate this native container.
  auto begin=Read<BYTE**>(race,0x19AC);
  auto end=Read<BYTE**>(race,0x19B0);
  auto countBytes=uintptr_t(end)-uintptr_t(begin);
  if(!checkpointLayoutLogged){checkpointLayoutLogged=true;Log("EXPERIMENT_NATIVE_CHECKPOINT_LAYOUT offset=%X begin=%08X end=%08X",unsigned(offsetof(NativeRace,mCheckpoints)),unsigned(begin),unsigned(end));for(unsigned offset=0x1990;offset<0x19E0;offset+=4)Log("EXPERIMENT_NATIVE_CHECKPOINT_WORD offset=%X value=%08X",offset,Read<unsigned>(race,offset));}
  if(begin&&end&&countBytes<=64*sizeof(void*)&&countBytes%sizeof(void*)==0){
   for(unsigned i=0;i<countBytes/sizeof(void*);++i){
    auto trigger=Read<BYTE*>(begin,i*4);float position[3]{},direction[3]{},bounds[16]{};
    Copy(trigger+0x58,position,12);Copy(trigger+0x68,direction,12);Copy(trigger+0x28,bounds,64);
    auto key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(trigger+4);
    Log("EXPERIMENT_NATIVE_CHECKPOINT index=%u key=%08X object=%08X position=%.3f,%.3f,%.3f direction=%.4f,%.4f,%.4f enabled=%u active=%u bounds=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f",i,key,unsigned(trigger),position[0],position[1],position[2],direction[0],direction[1],direction[2],Read<unsigned>(trigger,0x74),unsigned(Read<BYTE>(trigger,0xC4)),bounds[0],bounds[1],bounds[2],bounds[3],bounds[4],bounds[5],bounds[6],bounds[7],bounds[8],bounds[9],bounds[10],bounds[11],bounds[12],bounds[13],bounds[14],bounds[15]);
   }
   checkpointAuditRace=race;
  }
 }
 bool delayedStart=completedTransitionTest&&!transitionCount;
#ifdef MWEL_SUPPORT_FLIGHT_PROOF
 // Private integration build: let a real AI racer lead before flight is
 // observed. No position, ranking, checkpoint or finish state is fabricated.
 delayedStart=true;
#endif
 if(delayedStart&&!completedDelayAt){
  completedDelayAt=GetTickCount64();Log("EXPERIMENT_LOWER_FINISH_START_DELAY milliseconds=8000 rank_write=0 fixture_only=1");
 }
 if(!manualPlayer&&!playerSetup&&Read<BYTE>(race,0x198A)&&Read<BYTE>(race,0x1978)&&
    (!delayedStart||GetTickCount64()-completedDelayAt>=8000)){
  const BYTE guard[]={0x8B,0x44,0x24,0x04,0x85,0xC0,0x75,0x12};
  if(memcmp(reinterpret_cast<void*>(0x6123B0),guard,sizeof(guard))){Log("EXPERIMENT_RACE_ABORT native_player_controller_guard");return;}
  reinterpret_cast<void(__cdecl*)(bool)>(0x6123B0)(true);playerSetup=true;
  Log("EXPERIMENT_NATIVE_PLAYER_AI active=1 input_override=0");
 }
 auto vehicle=Read<BYTE*>(primary,0x48),sim=Read<BYTE*>(primary,0x34);
 unsigned handle=Read<unsigned>(sim,8),count=Read<unsigned>(race,0x1958),slot=16;
 if(!vehicle||!handle||count>16)return;
 for(unsigned i=0;i<count;++i)if(Read<unsigned>(race,0x18+i*0x194)==handle){slot=i;break;}
 if(slot==16)return;
 if(!ApplyPerformanceFixture(vehicle,handle,slot)){aborted=true;Log("EXPERIMENT_RACE_ABORT native_performance_fixture_failed handle=%u",handle);return;}
 if(slot==0)unloadPlayerVehicle=vehicle;
 // Combat removal requires both the authoritative health ledger and the
 // native totalled flag. Never treat an immobile living car as a retirement.
 if(Read<BYTE>(race,0x18+slot*0x194+0x1D)&&MWELCombatRetired(slot,handle)){
  auto& motion=motions[slot];if(!motion.eliminationLogged){motion.eliminationLogged=true;
   Log("EXPERIMENT_COMBAT_RETIREMENT handle=%u slot=%u health_zero=1 native_totalled=1 genuine_goal_verified=0 recovery_failure=0",handle,slot);
  }return;
 }
 // Native lap knockout removes the last-place car before its next finish
 // crossing. This is neither a genuine goal nor a failed AI recovery.
 static_assert(offsetof(NFSPluginSDK::MW05::GRacerInfo,mKnockedOut)==0x1C);
 if(!packages.empty()&&packages.front().authoring.at("mode")=="lapknockout"&&Read<BYTE>(race,0x18+slot*0x194+0x1C)){
  auto& motion=motions[slot];if(!motion.eliminationLogged){motion.eliminationLogged=true;
   Log("EXPERIMENT_NATIVE_KNOCKOUT handle=%u slot=%u native_knocked_out=1 genuine_goal_verified=0 recovery_failure=0",handle,slot);
  }return;
 }
 // Hot Pursuit arrests AI racers too. The native mBusted flag retires a
 // participant without setting mFinished; it is not failed driving recovery.
 static_assert(offsetof(NFSPluginSDK::MW05::GRacerInfo,mBusted)==0x1F);
 if(!packages.empty()&&packages.front().authoring.value("combatMode",std::string("none"))=="hotpursuit"&&Read<BYTE>(race,0x18+slot*0x194+0x1F)){
  auto& motion=motions[slot];if(!motion.eliminationLogged){motion.eliminationLogged=true;
   Log("EXPERIMENT_HOT_PURSUIT_NATIVE_ARREST handle=%u slot=%u native_busted=1 genuine_goal_verified=0 recovery_failure=0",handle,slot);
  }return;
 }
 // A native arrest intentionally stops the player. Record the loss rather
 // than misclassifying the arrest animation as failed driving recovery.
 if(slot==0&&!packages.empty()&&(packages.front().authoring.value("combatMode",std::string("none"))=="gauntlet"||packages.front().authoring.value("combatMode",std::string("none"))=="hotpursuit")){
  if(motions[slot].handle==handle&&motions[slot].eliminationLogged)return;
  auto manager=Read<BYTE*>(reinterpret_cast<void*>(0x993CC8));auto pursuit=Read<BYTE*>(manager,0x130);
  if(Read<unsigned>(pursuit)==0x892770&&Read<BYTE>(pursuit,0xE8)){
   auto& motion=motions[slot];if(!motion.eliminationLogged){motion.eliminationLogged=true;
    if(packages.front().authoring.value("combatMode",std::string("none"))=="gauntlet")Log("EXPERIMENT_GAUNTLET_NATIVE_ARREST handle=%u native_busted=1 genuine_goal_verified=0 recovery_failure=0",handle);
    else Log("EXPERIMENT_HOT_PURSUIT_PLAYER_ARREST handle=%u native_busted=1 genuine_goal_verified=0 recovery_failure=0",handle);
   }return;
  }
 }
 // The explicit visual-only fixture deliberately holds the player still.
 // Do not accept it as a completed race or mistake that hold for AI failure.
 if(MWELCombatVisualHeld(slot)){
  static unsigned heldHandle=0;if(heldHandle!=handle){heldHandle=handle;Log("EXPERIMENT_VISUAL_STATIONARY_PLAYER handle=%u slot=%u intentional_hold=1 race_time_acceptance=0",handle,slot);}return;
 }
 NeoBodyMotion body;if(!NeoReadBody(vehicle,body))return;
 auto now=GetTickCount64();if(!firstDriving)firstDriving=now;
 auto& motion=motions[slot];if(motion.handle!=handle){motion.handle=handle;memcpy(motion.initial,body.position,12);memcpy(motion.last,body.position,12);memcpy(motion.progressAnchor,body.position,12);motion.moved=motion.progress=now;}
 float distance=std::hypot(body.position[0]-motion.last[0],body.position[2]-motion.last[2]);
 if(distance>.25f){memcpy(motion.last,body.position,12);motion.moved=now;}
 float progressDistance=std::hypot(body.position[0]-motion.progressAnchor[0],body.position[2]-motion.progressAnchor[2]);
 if(progressDistance>5.f){memcpy(motion.progressAnchor,body.position,12);motion.progress=now;}
 bool finished=Read<BYTE>(race,0x18+slot*0x194+0x21)!=0;
 bool goalVerified=(genuineGoals&(1u<<slot))!=0;
 int checkpoint=Read<int>(race,0x18+slot*0x194+0x2C);
 if(checkpoint!=motion.checkpoint){motion.checkpoint=checkpoint;motion.pendingGateAt=0;motion.routeProgressAt=0;motion.recoveryVerificationUsed=false;}
 float station=0;unsigned routeRevision=0;
 if(playerSetup&&!finished&&!goalVerified&&checkpoint>=0&&MWELNeoRouteProgress(handle,unsigned(checkpoint),&station,&routeRevision)){
  if(motion.routeRevision!=routeRevision){motion.routeRevision=routeRevision;motion.routeStation=station;motion.routeProgressAt=now;motion.recoveryVerificationUsed=false;Log("EXPERIMENT_CHECKED_ROAD_REBASE handle=%u CP=%d revision=%u station=%.3f native_progress_written=0 body_watchdog_unchanged=1",handle,checkpoint,routeRevision,station);}
  if(!motion.routeProgressAt){motion.routeStation=station;motion.routeProgressAt=now;}
  // Repeated reverse/forward movement must not masquerade as recovery.
  // Require advancement along the current native CP's committed road and
  // observed body movement. CP changes reset the separate route cursor.
  if(station>motion.routeStation+5.f&&now-motion.moved<1000){motion.routeStation=station;motion.routeProgressAt=now;motion.recoveryVerificationUsed=false;}
  if(now-firstDriving>25000&&now-motion.routeProgressAt>20000&&mwel_recovery_observation::VerifyOnce(motion.recoveryVerificationUsed,MWELNeoRecoveryResumed(handle,unsigned(checkpoint)))){
   motion.routeProgressAt=now-16000;Log("EXPERIMENT_RECOVERY_RESUMPTION_WINDOW handle=%u CP=%d seconds=4 verified_placement=1 checked_forward_body_metres=2 once_until_real_progress=1 body_watchdog_unchanged=1",handle,checkpoint);
  }
  if(now-firstDriving>25000&&now-motion.routeProgressAt>20000){
   aborted=true;Log("EXPERIMENT_RACE_ABORT body_cannot_resume_route handle=%u slot=%u CP=%d route_station=%.3f anchor=%.3f age_ms=%llu body_motion_alone_not_accepted=1",handle,slot,checkpoint,station,motion.routeStation,now-motion.routeProgressAt);return;
  }
 }
 if(playerSetup&&!finished&&!goalVerified&&checkpoint>=0&&MWELNeoPendingGateEnd(handle,unsigned(checkpoint))){
  if(!motion.pendingGateAt)motion.pendingGateAt=now;
  if(now-motion.pendingGateAt>20000){
   auto begin=Read<BYTE**>(race,0x19AC),end=Read<BYTE**>(race,0x19B0);
   auto bytes=uintptr_t(end)-uintptr_t(begin);
   if(begin&&end&&bytes<=64*sizeof(void*)&&checkpoint<int(bytes/sizeof(void*))){
    auto trigger=Read<BYTE*>(begin,unsigned(checkpoint)*4);
    auto insideBegin=Read<BYTE**>(trigger,0x78),insideEnd=Read<BYTE**>(trigger,0x7C);
    Log("EXPERIMENT_PENDING_GATE_STATE CP=%d object=%08X enabled=%u active=%u activation_references=%d inside_begin=%08X inside_end=%08X player_goal_seen=%u",checkpoint,unsigned(trigger),Read<unsigned>(trigger,0x74),unsigned(Read<BYTE>(trigger,0xC4)),Read<int>(trigger,0xC8),unsigned(insideBegin),unsigned(insideEnd),unsigned((genuineGoals&1)!=0));
   }
   aborted=true;Log("EXPERIMENT_RACE_ABORT pending_native_gate_not_cleared handle=%u slot=%u CP=%d age_ms=%llu physical_nudges_not_accepted=1",handle,slot,checkpoint,now-motion.pendingGateAt);return;
  }
 }
 if(playerSetup&&!finished&&!goalVerified&&now-firstDriving>25000&&now-motion.progress>20000){aborted=true;Log("EXPERIMENT_RACE_ABORT body_cannot_resume handle=%u slot=%u progress_age_ms=%llu displacement=%.3f",handle,slot,now-motion.progress,progressDistance);return;}
 if(now-motion.sampled>=1000){
  auto goal=Read<void*>(primary,0xB8),action=Read<void*>(goal,4);
  float total=std::hypot(body.position[0]-motion.initial[0],body.position[2]-motion.initial[2]);
  Log("EXPERIMENT_RACE_BODY handle=%u slot=%u position=%.3f,%.3f,%.3f moved_metres=%.3f owner=%08X action=%08X checkpoint=%d lap=%d finished_flag=%u genuine_goal_verified=%u",handle,slot,body.position[0],body.position[1],body.position[2],total,Read<unsigned>(primary),Read<unsigned>(action),Read<int>(race,0x18+slot*0x194+0x2C),Read<int>(race,0x18+slot*0x194+0x28),unsigned(finished),unsigned((genuineGoals&(1u<<slot))!=0));
  auto info=race+0x18+slot*0x194;
  static_assert(offsetof(NFSPluginSDK::MW05::GRacerInfo,mRanking)==0x10);
  float nativeSeconds=Read<float>(info,0x150);if(Read<BYTE>(info,0x154))nativeSeconds+=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-Read<float>(info,0x14C);
  Log("EXPERIMENT_NATIVE_LAP_CLOCK slot=%u handle=%u lap=%d CP=%d native_seconds=%.6f timer_written=0",slot,handle,Read<int>(info,0x28),checkpoint,nativeSeconds);
  Log("EXPERIMENT_NATIVE_RANK slot=%u handle=%u rank=%d ai_rank=%d CP=%d lap=%d percent=%.6f lap_percent=%.6f distance_to_next=%.3f route_station=%.3f native_rank_unmodified=1",slot,handle,Read<int>(info,0x10),Read<int>(info,0x14),checkpoint,Read<int>(info,0x28),Read<float>(info,0x18),Read<float>(info,0x24),Read<float>(info,0xF8),station);
  if(!packages.empty()&&packages.front().authoring.at("mode")=="speedtraprace"){
   using Info=NFSPluginSDK::MW05::GRacerInfo;
   auto crossed=Read<int>(info,unsigned(offsetof(Info,mSpeedTrapsCrossed)));float sum=0;
   for(unsigned i=0;i<16;++i)sum+=Read<float>(info,unsigned(offsetof(Info,mSpeedTrapSpeed))+i*4);
   Log("EXPERIMENT_NATIVE_SPEEDTRAP slot=%u handle=%u crossed=%d speed_sum=%.3f native_scoring_unmodified=1",slot,handle,crossed,sum);
  }
  motion.sampled=now;
 }
}
void Sample(void* primary){__try{SampleUnsafe(primary);}__except(EXCEPTION_EXECUTE_HANDLER){if(faults++<3)Log("EXPERIMENT_RACE_ABORT body_read_fault code=%08X",GetExceptionCode());}}
void HoldRoamingPlayer(){
 // Explicit lab radar/exit marker only. Keep the former player stationary
 // while other cars leave streaming range; never run in production gameplay.
 if(!unloadPlayerVehicle||customTransitionTest)return;
 auto vehicles=Read<BYTE**>(reinterpret_cast<void*>(0x92CD1C));auto count=Read<unsigned>(reinterpret_cast<void*>(0x92CD24));
 if(!vehicles||count>128)return;bool live=false;
 for(unsigned i=0;i<count;++i)if(Read<BYTE*>(vehicles,i*4)==unloadPlayerVehicle){live=true;break;}
 NeoBodyMotion motion;if(!live||!NeoReadBody(unloadPlayerVehicle,motion))return;
 if(!unloadPlayerAnchored){memcpy(unloadPlayerAnchor,motion.position,12);unloadPlayerAnchored=true;}
 const auto now=GetTickCount64();
 if(!retainUnloadHold&&now-roamingEnteredAt>=5000){
  if(!unloadPlayerReleased){
   reinterpret_cast<void(__cdecl*)(bool)>(0x6123B0)(false);unloadPlayerReleased=true;
   Log("EXPERIMENT_UNLOAD_PLAYER_RELEASE hold_seconds=%.3f manual_input_restored=1 further_rigid_writes=0",double(now-roamingEnteredAt)/1000);
  }
  if(now-unloadPlayerLogged>1000){unloadPlayerLogged=now;float squared=0;for(unsigned i=0;i<3;++i){float d=motion.position[i]-unloadPlayerAnchor[i];squared+=d*d;}
   Log("EXPERIMENT_UNLOAD_PLAYER_RELEASED_BODY distance_from_anchor=%.3f body_position=%.3f,%.3f,%.3f",sqrtf(squared),motion.position[0],motion.position[1],motion.position[2]);}
  return;
 }
 auto entity=unloadPlayerVehicle-0x80;auto vt=Read<void**>(entity);
 auto body=reinterpret_cast<void*(__thiscall*)(void*)>(vt[21])(entity);if(!body)return;
 auto bt=Read<void**>(body);const float zero[3]{};
 reinterpret_cast<void(__thiscall*)(void*,const float*)>(bt[23])(body,unloadPlayerAnchor);
 reinterpret_cast<void(__thiscall*)(void*,const float*)>(bt[24])(body,zero);
 reinterpret_cast<void(__thiscall*)(void*,const float*)>(bt[25])(body,zero);
 if(now-unloadPlayerLogged>1000){unloadPlayerLogged=now;
  Log("EXPERIMENT_UNLOAD_PLAYER_HOLD seconds=%.3f body_position=%.3f,%.3f,%.3f anchor=%.3f,%.3f,%.3f race_finish_claimed=0",double(now-roamingEnteredAt)/1000,motion.position[0],motion.position[1],motion.position[2],unloadPlayerAnchor[0],unloadPlayerAnchor[1],unloadPlayerAnchor[2]);}
}
void ObserveFrameUnsafe(){
 RetireObservedFinishers();
 ReleaseField();
 auto race=*reinterpret_cast<BYTE**>(0x91E000);
 if(careerChainTest&&race&&Read<unsigned>(race,0x1960)==1){
  const auto key=MWELNeoCurrentKey();
  if(key&&key!=careerObservedKey){
   // Observe the native PostRaceActivity transition. Never activate its next
   // event, change a result, or synthesize checkpoint/finish progress here.
   ResetObservation();careerObservedKey=key;
   Log("EXPERIMENT_CAREER_CHAIN_EPOCH key=%08X native_post_race_activity=1 forced_activation=0",key);
  }
 }
 if(completedTransitionTest&&!transitionCount&&!aborted){
  // Observe a physical lower-place goal and the normal Continue-to-roaming
  // UI path. Unlike the abort fixture, never force EndRace, rank or roaming.
  if(!completedGoalObserved&&(genuineGoals&1)&&!forcedEstimationObserved){
   const auto place=Read<unsigned>(race,0x28);
   if(place>1&&place<=8){completedGoalObserved=true;completedGoalPlace=place;
    // Only the explicit completed-transition lab fixture owns this autopilot.
    // Game_ForceAIControl(false) still enables AI; it selects the first player.
    // Restore human input via the separately verified Game_ClearAIControl.
    const BYTE guard[]={0x8B,0x44,0x24,0x04,0x85,0xC0,0x75,0x12};
    if(memcmp(reinterpret_cast<void*>(0x612420),guard,sizeof(guard))){aborted=true;Log("EXPERIMENT_RACE_ABORT native_player_clear_controller_guard");return;}
    reinterpret_cast<void(__cdecl*)(bool)>(0x612420)(false);
    Log("EXPERIMENT_COMPLETED_PLAYER_AI_CLEARED genuine_goal=1 input_restored=1 result_and_rank_unmodified=1");
    Log("EXPERIMENT_COMPLETED_TRANSITION_GOAL actual_place=%u genuine_goal=1 native_finish_unmodified=1",place);}
  }
  if(completedGoalObserved&&!radarRoamingEntered&&Read<unsigned>(race,0x1960)==0&&Read<unsigned>(reinterpret_cast<void*>(0x925E90))==6){
   radarRoamingEntered=true;roamingEnteredAt=GetTickCount64();
   Log("EXPERIMENT_COMPLETED_TRANSITION_ROAMING mode=0 actual_place=%u forced_roaming=0",completedGoalPlace);
  }
 }
 event_maps::Frame(OwnedRace(race),MWELNeoCurrentKey());
 checkpoint_display::Tick();
 event_speedtraps::Frame(MWELNeoCurrentKey(),Read<unsigned>(reinterpret_cast<void*>(0x925E90))==6&&Read<unsigned>(race,0x1960)==1);
 if(radarRoamingTest&&!radarRoamingEntered&&!aborted&&firstDriving&&(!customTransitionTest||transitionCount<transitionLimit)&&GetTickCount64()-firstDriving>20000){
  radarRoamingEntered=true;roamingEnteredAt=GetTickCount64();event_maps::RestoreCalibrations();auto key=MWELNeoCurrentKey();
  if(!registration_experiment::ReleaseGeometry(key)){aborted=true;Log("EXPERIMENT_RACE_ABORT radar_roaming_geometry_release");return;}
  MWELResetNeoDriverRace();MWELResetNeoProviderRace();
  reinterpret_cast<void(__thiscall*)(void*)>(0x5FE3E0)(race);
  if(!customTransitionTest){
   reinterpret_cast<void(__cdecl*)(bool)>(0x6123B0)(false);
   Log("EXPERIMENT_NATIVE_PLAYER_AI active=0 boundary=roaming hold_does_not_compete_with_race_recovery=1");
  }
  package_resources::RestoreRoamingRadar();Log("EXPERIMENT_RADAR_ROAMING_NATIVE_ENTER mode=%u race_finish_claimed=0",Read<unsigned>(race,0x1960));return;
 }
 if(radarRoamingEntered&&!customTransitionInvoked){
  package_resources::RestoreRoamingRadar();
  if(!customTransitionTest)HoldRoamingPlayer();
  if(!customTransitionTest||GetTickCount64()-roamingEnteredAt<2000||Read<unsigned>(reinterpret_cast<void*>(0x925E90))!=6||Read<unsigned>(race,0x1960)!=0)return;
  const auto& nextId=transitionCount%2?transitionPrimary:transitionSecondary;
  const mwel::EventPackage* next=nullptr;for(const auto& p:packages)if(p.id==nextId)next=&p;
  if(!next){aborted=true;return;}customTransitionInvoked=true;
  for(auto& motion:motions)motion={};firstDriving=0;
  // A native-baseline sequence must stay on original events after roaming,
  // rather than silently switching to its unrelated package descriptor.
  const auto nextEvent=nativeBaseline?next->authoring.at("eventId").get<std::string>():next->nativeEventId;
  auto eventId=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)(nextEvent.c_str());
  ++transitionCount;
  Log("EXPERIMENT_CUSTOM_TRANSITION_ACTIVATE event=%08X id=%s native_roaming_confirmed=1 cycle=%u limit=%u",eventId,next->id.c_str(),transitionCount,transitionLimit);
  reinterpret_cast<void(__thiscall*)(void*,unsigned)>(0x5FDCB0)(race,eventId);
  if(transitionCount<transitionLimit){radarRoamingEntered=false;customTransitionInvoked=false;roamingEnteredAt=0;}
  return;
 }
 const unsigned count=Read<unsigned>(reinterpret_cast<void*>(0x92CD24));
 auto vehicles=Read<BYTE**>(reinterpret_cast<void*>(0x92CD1C));
 const unsigned capacity=Read<unsigned>(reinterpret_cast<void*>(0x92CD20));
 if(!vehicles||count>128||count>capacity)return;
 for(unsigned i=0;i<count;++i){
  auto vehicle=Read<BYTE*>(vehicles,i*sizeof(void*));if(!vehicle)continue;
  auto vtable=Read<void**>(vehicle);if(!vtable)continue;
  auto getter=Read<void*>(vtable,43*sizeof(void*));
  if(getter!=reinterpret_cast<void*>(0x688230))continue;
  auto ai=reinterpret_cast<BYTE*(__thiscall*)(void*)>(getter)(vehicle);
  if(ai){auto primary=ai-0x4C;if(Read<BYTE*>(primary,0x48)==vehicle)Sample(primary);}
 }
}
void __cdecl ObserveFrame(){__try{ObserveFrameUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){if(faults++<3)Log("EXPERIMENT_RACE_ABORT frame_read_fault code=%08X",GetExceptionCode());}}
void __fastcall Driving(void* self,void*,float dt){originalDriving(self,dt);Sample(self);}
bool Install(){
 auto radarMarker=game/L"MWEventLoader.radar-roaming-test";
 if(mwel::fs::exists(radarMarker)){if(mwel::Read(radarMarker,128)!="MW_EVENT_LOADER_RADAR_ROAMING_V1\n")return false;radarRoamingTest=true;}
 // Only explicit long-range residency trials retain a stationary player.
 // Normal exit/radar checks release all position/velocity writes after 5 s.
 retainUnloadHold=mwel::fs::exists(game/L"MWEventLoader.unload-residency-test");
 const BYTE startGuard[]={0xA1,0x64,0x60,0x92,0x00,0x85,0xC0};
 const BYTE drivingGuard[]={0x56,0x57,0x8B,0x7C,0x24,0x0C};
 const BYTE selectionGuard[]={0x68,0x68,0x60,0x92,0x00};
 const BYTE lapOptionGuard[]={0xA1,0xBC,0x86,0x8F,0,0x75,0x05,0xB8,1,0,0,0,0x50,0x8B,0xCE,0xE8,0x9F,0xF9,0xEF,0xFF};
 if(memcmp(reinterpret_cast<void*>(0x6EA040),startGuard,sizeof(startGuard))||(!neoObserver&&memcmp(reinterpret_cast<void*>(0x408690),drivingGuard,sizeof(drivingGuard)))||memcmp(reinterpret_cast<void*>(0x6EA075),selectionGuard,sizeof(selectionGuard))||memcmp(reinterpret_cast<void*>(0x6EA0BD),lapOptionGuard,sizeof(lapOptionGuard)))return false;
 const BYTE finishGuard[]={0x64,0xA1,0,0,0,0,0x6A,0xFF};
 const BYTE endGuard[]={0xA1,0,0xE0,0x91,0,0x53,0x8B,0xD9};
 if(memcmp(reinterpret_cast<void*>(0x6119F0),finishGuard,sizeof(finishGuard))||memcmp(reinterpret_cast<void*>(0x6021B0),endGuard,sizeof(endGuard)))return false;
 const BYTE checkpointGuard[]={0x51,0x8B,0x44,0x24,0x08};
 if(memcmp(reinterpret_cast<void*>(0x6117F0),checkpointGuard,sizeof(checkpointGuard)))return false;
 std::vector<void*> installed;
 auto rollback=[&](){for(auto address:installed){MH_DisableHook(address);MH_RemoveHook(address);}return false;};
 auto add=[&](unsigned address,void* detour,void** original){auto site=reinterpret_cast<void*>(address);if(MH_CreateHook(site,detour,original)!=MH_OK)return false;installed.push_back(site);return true;};
 if(!add(0x6EA040,Start,reinterpret_cast<void**>(&originalStart))||(!neoObserver&&!add(0x408690,Driving,reinterpret_cast<void**>(&originalDriving)))||!add(0x6119F0,Finish,reinterpret_cast<void**>(&originalFinish))||!add(0x6021B0,End,reinterpret_cast<void**>(&originalEnd))||!add(0x6117F0,Checkpoint,reinterpret_cast<void**>(&originalCheckpoint))||!InstallFieldHooks(installed))return rollback();
 for(auto address:installed)if(MH_QueueEnableHook(address)!=MH_OK)return rollback();
 if(MH_ApplyQueued()!=MH_OK)return rollback();return true;
}
void BootstrapSelection(const std::string& event){
 DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(0x6EA075),5,PAGE_EXECUTE_READWRITE,&old))throw std::runtime_error("race selection patch unavailable");
 selection=event;
 *reinterpret_cast<const char**>(0x6EA076)=selection.c_str();
 DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(0x6EA075),5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(0x6EA075),5);
 *reinterpret_cast<const char**>(0x8F86A8)=playerCar.c_str();
 // Gauntlet's authored count includes its transient police role. Native
 // debug startup must retain the Tollbooth's one actual racer, like menu starts.
 *reinterpret_cast<unsigned*>(0x926080)=registration_experiment::authoredGeometry&&!nativeBaseline?
  (BootstrapPackage().authoring.value("combatMode",std::string("none"))=="gauntlet"?0u:BootstrapPackage().authoring.at("racerCount").get<unsigned>()-1):3;
 *reinterpret_cast<unsigned*>(0x926064)=1;*reinterpret_cast<char*>(0x926068)='1';
}
std::string BaselineSelection(){
 const auto& authoring=BootstrapPackage().authoring;
 return authoring.at("eventId").get<std::string>();
}
void PrimeBootstrap(){
 if(mwel::fs::exists(game/L"MWEventLoader.player-engine-test")){
  const auto marker=mwel::Read(game/L"MWEventLoader.player-engine-test",128);
  playerPadEngineTest=marker=="MW_EVENT_LOADER_PLAYER_PAD_ENGINE_TEST_V1\n";
  if(productionRuntime||!neoObserver||!mwel::fs::exists(game/L"MWEventLoader.local-test")||(!playerPadEngineTest&&marker!="MW_EVENT_LOADER_PLAYER_ENGINE_TEST_V1\n"))throw std::runtime_error("Player engine test requires the marked isolated no-save lab");
  playerEngineTest=true;Log("EXPERIMENT_PLAYER_ENGINE_TEST native_input_autopilot=1 AI_body_correction=0 player_special_engine=1");
 }
 if(mwel::fs::exists(game/L"MWEventLoader.manual-player-test")){
  if(productionRuntime||!neoObserver||!mwel::fs::exists(game/L"MWEventLoader.local-test")||mwel::Read(game/L"MWEventLoader.manual-player-test",128)!="MW_EVENT_LOADER_MANUAL_PLAYER_TEST_V1\n")throw std::runtime_error("Manual player test requires the marked isolated no-save lab");
  manualPlayer=true;Log("EXPERIMENT_MANUAL_PLAYER native_input=1 player_autopilot=0");
 }
 if(packages.size()!=1&&!customTransitionTest)throw std::runtime_error("exactly one package required for early bootstrap");
 const auto performanceFixtureMode=GetPrivateProfileIntW(L"Diagnostics",L"NativePerformanceFixture",0,mwel_paths::Settings(game).c_str());
 performanceFixture=performanceFixtureMode==1||performanceFixtureMode==2;
 performanceFixtureJunkman=performanceFixtureMode==1;
 if(performanceFixture&&(productionRuntime||!neoObserver||!mwel::fs::exists(game/L"MWEventLoader.local-test")))throw std::runtime_error("Performance fixture requires isolated no-save driving");
 auto carMarker=game/L"MWEventLoader.player-car-test";
 if(mwel::fs::exists(carMarker)){
  auto raw=mwel::Read(carMarker,128);const std::string header="MW_EVENT_LOADER_PLAYER_CAR_EXPERIMENT_V1\n";
  if(raw.rfind(header,0)!=0||raw.back()!='\n')throw std::runtime_error("player car marker invalid");
  playerCar=raw.substr(header.size(),raw.size()-header.size()-1);
  if(!std::regex_match(playerCar,std::regex("[a-z0-9_]{1,31}"))||!mwel::fs::is_directory(game/L"CARS"/mwel::fs::u8path(playerCar)))throw std::runtime_error("player car unavailable");
 }
 BootstrapSelection(nativeBaseline?BaselineSelection():BootstrapPackage().nativeEventId);
 Log("EXPERIMENT_RACE_BOOTSTRAP_PRIMED ASI_initialization=1 database_not_constructed=1 car=%s",playerCar.c_str());
}
void SetSelection(){
 if(registration_experiment::clones.empty()||(!customTransitionTest&&std::count_if(registration_experiment::clones.begin(),registration_experiment::clones.end(),[](auto& c){return c->race;})!=1))throw std::runtime_error("exactly one registered race required for start experiment");
 BootstrapSelection(nativeBaseline?BaselineSelection():BootstrapPackage().nativeEventId);
 Log("EXPERIMENT_RACE_SELECTION configured=1 custom_key=%08X bootstrap=native_debug_start",registration_experiment::clones.front()->collection.key);
}
bool enabled=false;
}
