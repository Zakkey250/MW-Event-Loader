#include "EventCore.h"
bool MWELCarbonOwnsAudioSurface();
#include "RpdCaseFilesPolicy.h"
#include "RpdInterceptorPolicy.h"
#include "SceneRoadPath.h"
#include "RpdFreeRunRoute.h"
#include "RpdRoadCorridor.h"
#include "RpdRecoveryPose.h"
#include "GeometryQueries.h"
#include "../../integration/RpdAIContract.h"
#include "UpdateNotice.h"
#include "SceneGestureTiming.h"
#include "PackageSelection.h"
#include "WorldCompatibility.h"
#include "NativeCashValue.h"
#include "NativeFaultRecord.h"
#include "NativeCsisHandlePolicy.h"
#include "CareerTuningPolicy.h"
#include "EventBarriers.h"
#include "CarbonParticleFX.h"
#include "NeoDriverBridge.h"
#include "DriftLabels.h"
#include "NeoProviderBridge.h"
#include "NeoRecoveryObservation.h"
#include "NativeCheckpointExtension.h"
#include "ExpandedLeaderboard.h"
#include <MinHook.h>
#include <cstdio>
#include <cstdarg>
#include <share.h>
#include <intrin.h>
#include <psapi.h>
#include "../../NFSMWNativeFreeRoamRacers/include/LimitBreakerResourceApi.h"
#pragma comment(lib,"psapi.lib")
#include <memory>
#include <vector>
#include <deque>
#include <random>
#include <mutex>
#include <NFSPluginSDK/Game.MW05/Types/IPlayer.h>
#include <NFSPluginSDK/Game.MW05/Types/ISimable.h>
#include <NFSPluginSDK/Game.MW05/Types/IVehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/PVehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/GCharacter.h>
#include <NFSPluginSDK/Game.MW05/Types/IRigidBody.h>
#include <NFSPluginSDK/Game.MW05/Types/IDamageable.h>
#include <NFSPluginSDK/Game.MW05/Types/AIVehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/FEngHud.h>
#include <NFSPluginSDK/Game.MW05/Types/Minimap.h>
#include <NFSPluginSDK/Game.MW05/Types/TrackInfo.h>
#include <NFSPluginSDK/Game.MW05/Types/FEMultiImage.h>
#include <NFSPluginSDK/Game.MW05/Types/GRacerInfo.h>
#include <NFSPluginSDK/Game.MW05/Types/GRaceStatus.h>
#include <NFSPluginSDK/Game.MW05/Types/GTrigger.h>
#include <NFSPluginSDK/Game.MW05/Types/GIcon.h>
#include <NFSPluginSDK/Game.MW05/Types/Physics.h>
#include <NFSPluginSDK/Game.MW05/Types/FECustomizationRecord.h>
#include <NFSPluginSDK/Game.MW05/Types/RideInfo.h>
#include "../../integration/AppearanceSafety.h"
#include "../../integration/InstalledCarType.h"
#include <NFSPluginSDK/Game.MW05/Types/Attrib/Gen/pvehicle.h>
bool MWELNativeGeometryAudit();
bool MWELNativeGeometryWarmup();
void MWELStartCarbonIntro();
unsigned long long MWELCarbonFilterHud(void*,unsigned long long);
void MWELCarbonStageBefore(bool);
void MWELCarbonStageAfter(bool);
void MWELCarbonDoneBefore();
void MWELCarbonDoneAfter();
void MWELCancelCarbonIntro();

void MWELRpdFreeRecoveryFrame();
bool MWELRpdNativePlacementAllowed(void* ai);
bool MWELRpdVehiclePlacementAllowed(void* vehicle);
bool MWELRpdResumeTarget(unsigned action);
bool MWELRpdConstrainResultCamera(const float*,float*,float*);
bool MWELCarbonRoadGraph(mwel_scene_road::Graph&);
namespace {
mwel::fs::path game;
static FILE* logFile=nullptr;
#include "../../World2MW/src/NativeLoader/BufferedDebugLog.h"
PVOID crashObserver=nullptr;
std::vector<mwel::EventPackage> packages;
bool productionRuntime=false,runtimeReady=false,neoDiagnosticsEnabled=false;
void*(__thiscall* originalDatabaseConstructor)(void*);
bool Copy(const void* from,void* to,size_t count){__try{memcpy(to,from,count);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
LONG CALLBACK ObserveCrash(EXCEPTION_POINTERS* fault){
 if(!worldLogEnabled)return EXCEPTION_CONTINUE_SEARCH;
 static thread_local bool observing=false;
 if(observing)return EXCEPTION_CONTINUE_SEARCH;
 struct Scope {bool& active;Scope(bool& value):active(value){active=true;}~Scope(){active=false;}} scope(observing);
 const auto code=fault->ExceptionRecord->ExceptionCode;
 if(code==EXCEPTION_ACCESS_VIOLATION||code==EXCEPTION_BREAKPOINT||code==EXCEPTION_ILLEGAL_INSTRUCTION||code==EXCEPTION_IN_PAGE_ERROR||code==EXCEPTION_STACK_OVERFLOW){
  auto c=fault->ContextRecord;
  HMODULE module=nullptr;wchar_t modulePath[MAX_PATH]{};
  if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(fault->ExceptionRecord->ExceptionAddress),&module))GetModuleFileNameW(module,modulePath,MAX_PATH);
  // The 0.1.2 distribution has a guarded eight-byte SafeRead here. VEH
  // observes it before its own SEH handler returns false; it is not a CTD.
  // Identify the exact instruction and local handler bytes, not arbitrary
  // exceptions in NFRR. Do not consume the exception or change recovery.
  const unsigned char guardedRead[]={0x8B,0x01,0x8B,0x49,0x04,0x89,0x02,0x89,0x4A,0x04,0xC7,0x45,0xFC,0xFE,0xFF,0xFF,0xFF};
  const unsigned char guardedHandler[]={0xB8,0x01,0x00,0x00,0x00,0xC3,0x8B,0x65,0xE8};
  unsigned char observedRead[sizeof(guardedRead)]{},observedHandler[sizeof(guardedHandler)]{};
  const auto guardedOffset=unsigned(fault->ExceptionRecord->ExceptionAddress)-unsigned(module);
  if(code==EXCEPTION_ACCESS_VIOLATION&&fault->ExceptionRecord->ExceptionInformation[0]==0&&module&&
     (guardedOffset==0x2E8F2||guardedOffset==0x2EAE2)&&
     _wcsicmp(mwel::fs::path(modulePath).filename().c_str(),L"NFSMWNativeFreeRoamRacers.asi")==0&&
     Copy(reinterpret_cast<const BYTE*>(module)+guardedOffset,observedRead,sizeof(observedRead))&&
     Copy(reinterpret_cast<const BYTE*>(module)+guardedOffset+0x25,observedHandler,sizeof(observedHandler))&&
     memcmp(observedRead,guardedRead,sizeof(guardedRead))==0&&memcmp(observedHandler,guardedHandler,sizeof(guardedHandler))==0){
   Log("EXPERIMENT_GUARDED_READ_FAULT module=NFRR_0.1.2 offset=%08X target=%08X seh_handler_pending=1",guardedOffset,unsigned(fault->ExceptionRecord->ExceptionInformation[1]));
   return EXCEPTION_CONTINUE_SEARCH;
  }
  // Preserve the instruction/context even if a first-chance AV terminates
  // the process before the normal log worker flushes, or its rate cap drops
  // the crash stack. Never consume the exception or require logging to play.
  mwel_fault_record::Capture(fault,modulePath,Copy);
  Log("EXPERIMENT_EXCEPTION_MODULE path=%ls offset=%08X",modulePath,unsigned(fault->ExceptionRecord->ExceptionAddress)-unsigned(module));
  if(code==EXCEPTION_ACCESS_VIOLATION)Log("EXPERIMENT_ACCESS_VIOLATION address=%08X access=%u target=%08X",unsigned(fault->ExceptionRecord->ExceptionAddress),unsigned(fault->ExceptionRecord->ExceptionInformation[0]),unsigned(fault->ExceptionRecord->ExceptionInformation[1]));
  Log("EXPERIMENT_FATAL_EXCEPTION code=%08X address=%08X eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X esp=%08X ebp=%08X",unsigned(code),unsigned(fault->ExceptionRecord->ExceptionAddress),c->Eax,c->Ebx,c->Ecx,c->Edx,c->Esi,c->Edi,c->Esp,c->Ebp);
  // Native string hashing reserves 256 bytes before its return address.
  // 32 words captured only local data, losing the caller of 004549BF.
  unsigned stack[128]{};if(Copy(reinterpret_cast<void*>(c->Esp),stack,sizeof(stack)))for(unsigned i=0;i<128;++i)Log("EXPERIMENT_CRASH_STACK index=%u value=%08X",i,stack[i]);
 }
 return EXCEPTION_CONTINUE_SEARCH;
}
#include "NativeRuntimeDiagnostics.h"
#include "TougeNativeGrid.h"
#include "RpdPlayerVehicle.h"
#include "RegistrationExperiment.h"
#include "RpdPlayerVehicleRuntime.h"
#include "RpdVehicleCatalog.h"
#include "RpdControlsRuntime.h"
void MWELCombatNewGeneration();
void MWELCombatRaceExit(bool preserveResult=false);
void MWELTougeNewGeneration();
void MWELDriftFinish(unsigned,bool);
bool MWELHotPursuitEscaping(unsigned);
void MWELHotPursuitFinish(void*,bool,bool,int,void(__cdecl*)(void*));
bool MWELCombatResultPending();
bool MWELRpdFinishAllowed();
bool MWELRpdHotPursuitScope();
unsigned MWELRpdSprintTarget(unsigned);
bool MWELRpdSprintRetired(unsigned);
bool MWELRpdSprintArresting(unsigned);
bool MWELHotPursuitAIArresting(unsigned);
bool MWELHotPursuitNormalRetired(unsigned);
void MWELHotPursuitArrestReset();
bool MWELRpdSprintResult();
void MWELRpdSprintPhoto(void*);
void MWELRpdHotPursuitGoal(unsigned,unsigned,unsigned,unsigned);
bool MWELRpdPursuitCompleted(unsigned);
void MWELRpdLabEndAutodrive();
const char* MWELRpdLanguage(unsigned);
void MWELRpdResults(void*);
void MWELRpdInfo(void*);
bool MWELCombatRetired(unsigned,unsigned);
bool MWELRpdOfficerDisabled(unsigned);
bool MWELRpdPrepareResult();
bool MWELRpdNoAllyDefeat();
void MWELLabProductionFinish(void*,void(__cdecl*)(void*));
void MWELLabProductionEnd(void*,void(__thiscall*)(void*));
#include "EventRacePolicies.h"
bool MWELRpdControlsScope(){const auto p=event_race_policies::Current();return p&&mwel_rpd::Armed(p->authoring);}
#include "EventTitles.h"
#include "PackageResources.h"
#include "EventPresentation.h"
#include "RpdCaseFiles.h"
#include "EventMaps.h"
#include "CheckpointDisplay.h"
event_preview_stats::Finish MWELPreviewFinishMetrics(const mwel::EventPackage&,void*);
#include "ProfileProgress.h"
#include "EventPreviewStats.h"
#include "NativeSaveIsolation.h"
#include "EventSpeedTraps.h"
bool MWELCombatVisualHeld(unsigned);
#include "RaceStartExperiment.h"
#include "RuntimeLifecycle.h"
#include "DriftRaceRuntime.h"
#include "DriftHud.h"
#include "DriftResults.h"
void MWELLabProductionFinish(void* actor,void(__cdecl* nativeFinish)(void*)){
 auto previous=runtime_lifecycle::originalFinish;runtime_lifecycle::originalFinish=nativeFinish;
 __try{runtime_lifecycle::Finish(actor);}__finally{runtime_lifecycle::originalFinish=previous;}
}
void MWELLabProductionEnd(void* race,void(__thiscall* nativeEnd)(void*)){
 auto previous=runtime_lifecycle::originalEnd;runtime_lifecycle::originalEnd=nativeEnd;
 __try{runtime_lifecycle::End(race,nullptr);}__finally{runtime_lifecycle::originalEnd=previous;}
}
#include "EventRaceProgress.h"
#include "EventAudioExtension.h"
#include "EventLeaderboard.h"
bool MWELGauntletParticipant(unsigned,unsigned);
#include "CombatHealthRuntime.h"
#include "TougeFallRuntime.h"
bool MWELEnsureDriftImpact(){return combat_health_runtime::EnsureImpact();}
#include "HotPursuitRuntime.h"
bool MWELRpdFleetAllowsVehicle(unsigned);
const char* MWELRpdFleetSpawnModel(const char*);
#include "GauntletRuntime.h"
unsigned MWELRpdFleetHeat(){
 const auto p=event_race_policies::Current();if(!p||!mwel_rpd::Armed(p->authoring)||!combat_health_runtime::Racing())return 0;
 unsigned race=0,handle=0;if(!combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x18,handle))return 0;
 const auto player=gauntlet_runtime::Vehicle(handle);return player?gauntlet_runtime::Heat(player):0;
}
bool MWELRpdFleetAllowsVehicle(unsigned key){return rpd_vehicle_catalog::FleetAccepts(MWELRpdFleetHeat(),key);}
const char* MWELRpdFleetSpawnModel(const char* native){
 const auto heat=MWELRpdFleetHeat();if(!native||!rpd_vehicle_catalog::FleetEnabled(heat))return native;
 char name[64]{};if(!Copy(native,name,sizeof(name))||!memchr(name,0,sizeof(name)))return native;
 for(const auto& entry:rpd_vehicle_catalog::entries)if(!entry.helicopter&&_stricmp(name,entry.name.c_str())==0){
  if(const auto selected=rpd_vehicle_catalog::FleetVehicle(heat,entry.suv?1:0))return selected->name.c_str();
  break;
 }
 return native;
}
#include "NativePlayerReposition.h"
#include "NativePatrolScan.h"
#include "NativeMaterialRuntime.h"
#include "HotPursuitController.h"
#include "CombatPursuitLifecycle.h"
#include "HotPursuitFinish.h"
event_preview_stats::Finish MWELPreviewFinishMetrics(const mwel::EventPackage& package,void* nativeRecord){
 using expanded_leaderboard::Read;event_preview_stats::Finish result;
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto count=Read<unsigned>(race,0x1958);
 if(!race||!nativeRecord||!count||count>16)return result;
 unsigned slot=16;for(unsigned i=0;i<count;++i)if(nativeRecord==race+0x18+i*0x194){slot=i;break;}
 if(slot>=count)return result;
 const auto mode=package.authoring.value("combatMode",std::string("none"));
 if(mode=="drift"&&unsigned(race)==drift_race::owner&&slot<drift_race::count&&
    Read<unsigned>(nativeRecord)==drift_race::handles[slot]&&(drift_race::genuineMask&(1u<<slot))&&!(drift_race::forecastMask&(1u<<slot))){
  result.drift=true;result.angle=drift_race::scores[slot].bestAngle;
 }else if(mode=="hotpursuit"){
  for(unsigned i=0;i<count;++i){auto info=race+0x18+i*0x194;
   if(!Read<unsigned>(info,0x1C)&&!Read<BYTE>(info,0x190))++result.survivors;
  }
  result.hotPursuit=result.survivors>0;
 }
 return result;
}
int MWELArmsNativeSlot(void* actor){return reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor);}
using MWELArmsPlacementFn=void(__cdecl*)(void*,void*,unsigned);
MWELArmsPlacementFn MWELArmsNativePlacement(){
 if(hot_pursuit_finish::installed)return hot_pursuit_finish::originalPlacement;
 const BYTE expected[]={0x83,0xEC,0x20,0x8B,0x54,0x24,0x24};
 if(memcmp(reinterpret_cast<void*>(0x60AA00),expected,sizeof(expected)))return nullptr;
 return reinterpret_cast<MWELArmsPlacementFn>(0x60AA00);
}
#include "ArmsSurvival.h"
#include "RpdResultCameraRuntime.h"
#include "RpdMissionRuntime.h"
#include "RpdHotPursuitLedger.h"
#include "RpdSprintCaptureCamera.h"
#include "RpdHotPursuitRuntime.h"
#include "HotPursuitArrestRuntime.h"
#include "RpdSprintWarp.h"
#include "NativePlayerRepositionTest.h"
bool RpdOfficerIgnoresGuards(unsigned worldId){
 using gauntlet_runtime::Read;
 unsigned race=0,handle=0,driver=~0u,actual=0;
 if(!worldId||!rpd_mission::Current()||!Read(0x91E000,race)||!race||!Read(race+0x18,handle))return false;
 const auto player=gauntlet_runtime::Vehicle(handle);
 return player&&Read(player+0x94,driver)&&driver==0&&Read(player-0x20,actual)&&actual==worldId;
}
#include "RpdMissionPresentation.h"
#include "RpdOfficerSignals.h"
#include "RpdNativePursuit.h"
#include "RpdPoliceAutodrive.h"
#include "RpdOfficerHandoff.h"
void MWELRpdLabEndAutodrive(){rpd_police_autodrive::EndAutodrive();}
#include "RpdPolicePopulation.h"
#include "RpdPursuitActionGuard.h"
#include "RpdRoadNavigation.h"
#include "RpdPursuitPresentation.h"
#include "RpdPresentationTracking.h"
#include "RpdPursuitPerspective.h"
#include "RpdSpeechSubject.h"
namespace rpd_radio_tracking {
unsigned Subject(unsigned officer){return hot_pursuit_runtime::TargetHandle(rpd_presentation_tracking::RadioPursuit(officer));}
bool Visible(unsigned declaration){
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!rpd_mission::Current()||
    !combat_health_runtime::Racing()||rpd_mission::ResultPending())return false;
 unsigned race=0,officer=0;
 if(!gauntlet_runtime::Read(0x91E000,race)||!gauntlet_runtime::Read(race+0x18,officer))return false;
 const auto group=rpd_presentation_tracking::RadioPursuit(officer);
 const auto witness=hot_pursuit_runtime::SightWitness(group);
 if(witness&&worldLogEnabled)
  Log("RPD_RADIO_STALE_LOSS declaration=%08X racer=%u witness=%u native_sight_age_verified=1",
      declaration,hot_pursuit_runtime::TargetHandle(group),witness);
 return witness!=0;
}
struct Bind {Bind(){rpd_speech_subject::officerTarget=Subject;rpd_speech_subject::stillInSight=Visible;}} bind;
}
struct HotPursuitRadioBinding {HotPursuitRadioBinding(){rpd_speech_subject::trackedCount=hot_pursuit_runtime::SpeechTargets;}} hotPursuitRadioBinding;
#include "RpdIntroMusic.h"
#include "RpdPursuitMusic.h"
void(__thiscall* armsOriginalRanking)(void*)=nullptr;
void __fastcall ArmsRanking(void* self,void*){
 armsOriginalRanking(self);arms_survival_runtime::WinningRanks(unsigned(self));drift_race::Ranking(unsigned(self));rpd_mission::Ranking(unsigned(self));rpd_hot_pursuit_runtime::Ranking(unsigned(self));
}
bool MWELArmsPrepareRanking(){
 static bool installed=false,refused=false;if(installed)return true;if(refused)return false;
 static_assert(offsetof(NFSPluginSDK::MW05::GRacerInfo,mTotalled)==0x1D);
 const BYTE expected[]={0x83,0xEC,0x4C,0x53,0x56,0x57,0x8B,0xF9};
 auto site=reinterpret_cast<void*>(0x5FDE80);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,ArmsRanking,reinterpret_cast<void**>(&armsOriginalRanking))!=MH_OK){refused=true;Log("ARMS_SURVIVAL_REFUSED native_ranking_owner=1");return false;}
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);armsOriginalRanking=nullptr;refused=true;Log("ARMS_SURVIVAL_REFUSED native_ranking_enable=1");return false;}
 installed=true;return true;
}
bool MWELCombatResultPending(){return MWELRpdSprintResult()||rpd_mission::ResultPending()||hot_pursuit_finish::WinningResult()||arms_survival_runtime::ResultPending()||drift_race::ResultPending();}
#include "HotPursuitRegeneration.h"
#include "GauntletPursuit.h"
#include "GauntletHud.h"
bool MWELGauntletParticipant(unsigned slot,unsigned handle){return gauntlet_runtime::Participant(slot,handle);}
#pragma comment(linker,"/EXPORT:MWEL_CombatSlotV1=_MWEL_CombatSlotV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatSlotV1(unsigned handle,unsigned driver){return driver==2&&gauntlet_runtime::Participant(1,handle)?2u:0u;}
#pragma comment(linker,"/EXPORT:MWEL_GauntletLeaderV1=_MWEL_GauntletLeaderV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_GauntletLeaderV1(unsigned version){return version==mwcombat::Version&&gauntlet_runtime::Participant(1,gauntlet_runtime::opponentHandle)?gauntlet_runtime::opponentHandle:0;}
#pragma comment(linker,"/EXPORT:MWEL_GauntletV1=_MWEL_GauntletV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_GauntletV1(){return gauntlet_runtime::Surface()?1u:0u;}
#include "CombatDamageVisuals.h"
#include "CombatRetiredMarkers.h"
#include "CombatDefeatExperiment.h"
bool MWELCombatVisualHeld(unsigned slot){return !productionRuntime&&slot==0&&combat_defeat_experiment::enabled&&combat_defeat_experiment::epochs==1&&((combat_defeat_experiment::visual&&GetTickCount64()-combat_defeat_experiment::visualBegan>=12000)||(combat_defeat_experiment::arrest&&combat_defeat_experiment::visualPlayerReleased));}
void MWELCombatNewGeneration(){
 rpd_intro_music::Reset();
 rpd_finish_camera::Reset();
 rpd_police_autodrive::Reset();
 rpd_officer_handoff::Reset();
 rpd_officer_signals::Reset();
 rpd_mission::Reset();rpd_hot_pursuit_runtime::Reset();
 drift_hud::Reset();drift_race::Reset();
 drift_results::Reset();
 arms_survival_runtime::Reset();
 hot_pursuit_finish::Reset();
 hot_pursuit_runtime::Reset();gauntlet_runtime::Reset();combat_visuals::Reset();combat_markers::Reset();
 combat_health_runtime::NewGeneration();
 if(race_start_experiment::enabled)race_start_experiment::ResetObservation();
}
void MWELCombatRaceExit(bool preserveResult){
 static bool releasing=false;if(releasing)return;releasing=true;
 __try {
  if(preserveResult){rpd_sprint_capture_camera::Reset();rpd_hot_pursuit_runtime::ReleaseLocks();}
  else rpd_hot_pursuit_runtime::Reset();
  rpd_intro_music::Reset();
  if(!preserveResult||!rpd_finish_camera::RetainResult())rpd_finish_camera::Reset();
  rpd_police_autodrive::Reset();
  rpd_officer_handoff::Reset();
  rpd_controls::Forget();
  drift_hud::Reset();drift_race::Exit();
  hot_pursuit_runtime::Reset();gauntlet_runtime::Reset();combat_visuals::Reset();combat_markers::Reset();combat_health_runtime::Restore();
 }__finally {releasing=false;}
}
#pragma comment(linker,"/EXPORT:MWEL_CombatAttackSourceV1=_MWEL_CombatAttackSourceV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatAttackSourceV1(unsigned source){
 using namespace combat_health_runtime;const auto prior=attackSource;
 if(installed&&gameThread==GetCurrentThreadId())attackSource=source;
 return prior;
}
bool MWELCombatRetired(unsigned slot,unsigned handle){if(touge_fall::Retired(handle)||MWELRpdSprintRetired(handle)||MWELHotPursuitNormalRetired(handle))return true;mwcombat::HealthV1 health;return combat_health_runtime::Query(slot,handle,&health,sizeof(health))&&(health.flags&1)&&health.remaining==0;}
void ObserveDatabase(void* self){
 if(!worldLogEnabled)return;
 unsigned words[16]{};if(!Copy(self,words,sizeof(words))){Log("DATABASE_AUDIT_FAULT");return;}
 Log("DATABASE_CONSTRUCTED native_events=%u temporary_events=%u bins=%u event_records=%08X progress_records=%08X gameplay_class=%08X packages=%u native_registration=pending",
     words[0],words[1],words[8],words[3],words[15],words[10],unsigned(packages.size()));
 mwel::Json audit={{"schema",1},{"databaseWords",words},{"events",mwel::Json::array()}};
 if(registration_experiment::enabled&&words[8]<=64&&words[9]){
  for(unsigned i=0;i<words[8];++i){
   auto instance=reinterpret_cast<void*>(words[9]+i*0x1C);
   auto key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(instance);
   auto clone=registration_experiment::Find(key);if(!clone||!clone->bin)continue;
   auto fields=registration_experiment::ReadFields(*clone);auto node=fields.at(registration_experiment::Hash("WorldRaces"));unsigned short header[4]{};
   if(!Copy(reinterpret_cast<void*>(node.value),header,8))throw std::runtime_error("native career bin audit header unreadable");
   bool exact=true;for(unsigned index=0;index<header[1];++index){
    unsigned expected=0,actual=0;Copy(reinterpret_cast<void*>(node.value+8+index*4),&expected,4);
    auto pointer=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454810)(instance,node.key,index);
    exact&=pointer&&Copy(pointer,&actual,4)&&actual==expected;
   }
   Log("EXPERIMENT_NATIVE_CAREER_BIN key=%08X races=%u all_references_verified=%u native_file_modified=0",key,header[1],unsigned(exact));
   if(!exact)throw std::runtime_error("native career bin references differ");
  }
 }
 if(words[0]>4096||!words[3]){Log("DATABASE_AUDIT_REFUSED unreasonable_native_count");return;}
 for(unsigned index=0;index<words[0];++index){
  unsigned descriptor[5]{},instance[5]{},collection[11]{};
  if(!Copy(reinterpret_cast<void*>(words[3]+index*20),descriptor,sizeof(descriptor)))break;
  mwel::Json event={{"index",index},{"descriptor",descriptor}};
  auto parameters=reinterpret_cast<void*>(words[3]+index*20);
  auto eventId=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x5FAB40)(parameters);
  for(const auto& package:packages)if(eventId==registration_experiment::Hash(package.nativeEventId.c_str())){
   // Exercise the same accessor used by the menu and native Finish payout,
   // including the career metadata cache populated during DB construction.
   const auto cash=reinterpret_cast<float(__thiscall*)(void*)>(0x5FB920)(parameters);
   const auto expected=native_cash::Value(mwel_rpd::CaseFile(package.authoring)?0u:package.authoring.at("cashValue").get<unsigned>());
   event["cashReward"]={{"authored",package.authoring.at("cashValue")},{"native",cash},{"matches",cash==expected}};
   Log("EVENT_NATIVE_CASH_PARAMETERS id=%s event=%08X authored=%u native=%.0f matches=%u getter=005FB920 native_payout_accessor=1",package.id.c_str(),eventId,package.authoring.at("cashValue").get<unsigned>(),double(cash),unsigned(cash==expected));
  }
  unsigned metadata[12]{};if(descriptor[1]&&Copy(reinterpret_cast<void*>(descriptor[1]),metadata,sizeof(metadata)))event["metadata"]=metadata;
  if(descriptor[2]&&Copy(reinterpret_cast<void*>(descriptor[2]),instance,sizeof(instance))){
   event["instance"]=instance;
   if(instance[1]&&Copy(reinterpret_cast<void*>(instance[1]),collection,sizeof(collection))){
    event["collection"]=collection;
    if(collection[0]&&collection[1]<4096){
     mwel::Json nodes=mwel::Json::array();
     for(unsigned slot=0;slot<collection[1];++slot){unsigned node[3]{};if(!Copy(reinterpret_cast<void*>(collection[0]+slot*12),node,sizeof(node)))break;nodes.push_back(node);}
     event["collectionNodes"]=std::move(nodes);
    }
   }
  }
  audit["events"].push_back(std::move(event));
 }
 mwel::AtomicWrite(mwel_paths::Diagnostic(game,L"Diagnostics/database.json"),audit.dump(2)+"\n");
}
void* __fastcall DatabaseConstructor(void* self,void*){
 native_runtime_diagnostics::Heap("before_database");
 // ASI loading order is not initialization order. NFRR initializes on its
 // worker after DllMain; evaluate live capabilities at native DB publication,
 // not while Event Loader itself is still under the loader lock.
 packages.erase(std::remove_if(packages.begin(),packages.end(),[](const auto& p){
  if(!mwel_rpd::Armed(p.authoring))return false;
  auto nfr=GetModuleHandleW(L"NFSMWNativeFreeRoamRacers.asi");
  auto ready=nfr?reinterpret_cast<rpd_ai_contract::Ready>(GetProcAddress(nfr,"NFRR_RpdCustomAIReadyV1")):nullptr;
  const bool unavailable=!ready||!ready(rpd_ai_contract::Version)||(!mwel_rpd::FreeRun(p.authoring)&&!mwel::NeoEnabled(p.authoring));
  if(unavailable)Log("PACKAGE_RPD_AI_DEPENDENCY_MISSING id=%s requires=NFRR_CustomAI_free_run_or_NeoAI_sprint not_registered=1",p.id.c_str());
  return unavailable;
 }),packages.end());
 // Redux's Widescreen Fix scans the native resource factory during startup.
 // Install the package detours only after those startup scans have completed.
 if(!package_resources::Activate()){
  Log("REFUSED package_resource_adapter_activation database_boundary=1");
  runtimeReady=false;race_start_experiment::aborted=true;registration_experiment::clones.clear();
  return originalDatabaseConstructor(self);
 }
 // Map resources become available here, after startup pattern scans. Preparing
 // the bounds hook before this boundary silently skipped it while enabled=false.
 if(!checkpoint_display::Install())Log("EVENT_CHECKPOINT_RADAR_UNAVAILABLE native_minimap_hook_conflict=1 other_hud_retained=1");
 if(!event_maps::Install()){
  Log("REFUSED event_map_adapter_activation database_boundary=1");
  runtimeReady=false;race_start_experiment::aborted=true;registration_experiment::clones.clear();
  return originalDatabaseConstructor(self);
 }
 try{MWELRpdPrepareVehicles();
  packages.erase(std::remove_if(packages.begin(),packages.end(),[](const auto& p){
   if(!mwel_rpd::CaseFile(p.authoring))return false;
   const bool unavailable=!MWELRpdPresetAvailable(p.authoring.value("policeVehicle",std::string()));
   if(unavailable)Log("PACKAGE_RPD_VEHICLE_UNAVAILABLE id=%s not_registered=1",p.id.c_str());return unavailable;
  }),packages.end());
  if(registration_experiment::authoredGeometry&&!MWELCompleteBarriers())throw std::runtime_error("Package wall preload unavailable");registration_experiment::Prepare();}catch(const std::exception& error){Log("EXPERIMENT_REFUSED reason=%s",error.what());registration_experiment::clones.clear();}
 void* result=originalDatabaseConstructor(self);ObserveDatabase(result);
 native_runtime_diagnostics::Heap("after_database_before_previews");
 package_resources::LoadPreviews();
 native_runtime_diagnostics::Heap("after_preview_queue");
 event_presentation::Load();
 event_titles::Audit();
 if(mwel::fs::exists(game/L"MWEventLoader.native-geometry-test")){
  if(mwel::Read(game/L"MWEventLoader.native-geometry-test",256)!="MW_EVENT_LOADER_NATIVE_GEOMETRY_EXPERIMENT_V1\n"||!MWELNativeGeometryAudit())Log("EXPERIMENT_RACE_ABORT native_geometry_audit_failed");
 }
 if(race_start_experiment::enabled){try{race_start_experiment::SetSelection();}catch(const std::exception& error){Log("EXPERIMENT_RACE_ABORT setup=%s",error.what());}}
 return result;
}
void*(__cdecl* originalDatabasePublish)()=nullptr;
void AuditPublishedDatabaseUnsafe(void* result){
 void* published=nullptr;
 if(!result||!Copy(reinterpret_cast<void*>(0x91E004),&published,4)||published!=result){Log("EXPERIMENT_RACE_ABORT database_publication_mismatch");return;}
 try{native_save_isolation::Audit(result);}
 catch(const std::exception& error){Log("EXPERIMENT_RACE_ABORT native_save_audit=%s",error.what());}
}
void AuditPublishedDatabase(void* result){
 __try{AuditPublishedDatabaseUnsafe(result);}
 __except(EXCEPTION_EXECUTE_HANDLER){Log("EXPERIMENT_RACE_ABORT database_publication_audit_fault=%08X",unsigned(GetExceptionCode()));}
}
void* __cdecl DatabasePublish(){
 auto result=originalDatabasePublish();AuditPublishedDatabase(result);return result;
}
const char* Target(const std::string& digest){
 if(digest=="80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C")return "nfspatcher-13";
 if(digest=="B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06")return "nfspatcher-13-laa";
 if(digest=="0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342")return "redux-304-laa";
 return nullptr;
}
void Start(){
 wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))return;
 game=mwel::fs::path(executable).parent_path();auto marker=game/L"MWEventLoader.local-test";
 // The first build is a read-only registration probe. It cannot silently
 // replace an existing event loader or claim that authored races are playable.
 bool lab=mwel::fs::exists(marker);if(lab&&mwel::Read(marker,256)!="MW_EVENT_LOADER_SANDBOX_ONLY_V1\n")return;
 productionRuntime=!lab;
 combat_defeat_experiment::Configure();
 char arsenalPursuitSetting[16]{};
 GetPrivateProfileStringA("Pursuit Rule","Arsenal Pursuit","1",arsenalPursuitSetting,sizeof(arsenalPursuitSetting),mwel_paths::Settings(game).string().c_str());
 rpd_mission::arsenalPursuit=strcmp(arsenalPursuitSetting,"0")!=0;
 InitWorldDebugLog(mwel_paths::Settings(game).string().c_str(),mwel_paths::Diagnostic(game,L"EventLoader.log").string().c_str());
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 rpd_pursuit_action_guard::Configure();
#endif
 if(worldLogEnabled){
  wchar_t name[96]{};swprintf_s(name,L"NativeFault-%lu-%llu.bin",GetCurrentProcessId(),GetTickCount64());
  mwel_fault_record::Configure(true,mwel_paths::Diagnostic(game,name).c_str());
 }
 char neoDiagnosticSetting[8]{};
 GetPrivateProfileStringA("Debug","NeoDiagnostics","0",neoDiagnosticSetting,sizeof(neoDiagnosticSetting),mwel_paths::Settings(game).string().c_str());
 neoDiagnosticsEnabled=worldLogEnabled&&strcmp(neoDiagnosticSetting,"1")==0;
 if(GetModuleHandleW(nullptr)!=reinterpret_cast<HMODULE>(0x400000)){Log("REFUSED image_base");return;}
 auto digest=mwel::Digest(mwel::Read(executable));const char* target=Target(digest);
 if(!target){Log("REFUSED executable_identity sha256=%s",digest.c_str());return;}
 Log("EXECUTABLE_ACCEPTED target=%s sha256=%s",target,digest.c_str());
 if(productionRuntime)event_loader_update::StartNotices(mwel_paths::Settings(game));
 auto stagingInit=MH_Initialize();if((stagingInit!=MH_OK&&stagingInit!=MH_ERROR_ALREADY_INITIALIZED)||!event_race_policies::InstallStaging()){Log("REFUSED event_staging_boundaries");return;}
 native_runtime_diagnostics::Install();
 if(!native_player_reposition::Install()){Log("REFUSED player_reposition_native_surface");return;}
 const bool sharedHud=gauntlet_hud::InstallFeatures();
 Log("EVENT_SHARED_HUD_FEATURES active=%u",unsigned(sharedHud));
 if(sharedHud)MWELStartCarbonIntro();
 world_compatibility::Context world(game);
 std::vector<mwel::fs::path> eventFolders{mwel_paths::Events(game)};
 for(const auto& directory:eventFolders)if(mwel::fs::exists(directory))for(const auto& entry:mwel::fs::directory_iterator(directory)){
  if(!entry.is_directory()||!mwel::EventId(entry.path().filename().string()))continue;
  // An empty UUID folder left after manual event removal is not a package.
  // Partially present or damaged packages still take the integrity-error path.
  if(!mwel::fs::exists(entry.path()/L"manifest.json")&&!mwel::fs::exists(entry.path()/L"event.json"))continue;
  try{auto package=mwel::ParsePackage(entry.path());mwel_rpd::Validate(package.authoring);
   if(!world.Allows(package)){Log("PACKAGE_WORLD_MISMATCH id=%s world=%s not_registered=1",package.id.c_str(),package.worldSnapshotId.c_str());continue;}
   const auto combat=mwcombat::Parse(package.authoring.value("combatMode",std::string("none")).c_str());
   if(combat==mwcombat::Mode::Drift){
    auto player=GetModuleHandleW(L"MWArcadeDrift.asi"),ai=GetModuleHandleW(L"MWArcadeDriftAI.asi");
    if(!player||!ai||!GetProcAddress(player,"MWArcadeDriftAcquireSpecialV1")||!GetProcAddress(ai,"MWArcadeDriftAI_SetIntentV1")||!mwel::NeoEnabled(package.authoring)){
     Log("PACKAGE_DRIFT_DEPENDENCY_MISSING id=%s requires=MWArcadeDrift+MWArcadeDriftAI+NeoAI",package.id.c_str());continue;
    }
    if(!hot_pursuit_finish::Install()||!MWELArmsPrepareRanking()||!drift_results::Install()){Log("PACKAGE_DRIFT_UNAVAILABLE native_finish_ranking_or_results_owner_conflict=1");continue;}
   }else if(mwcombat::Weapons(combat)){
    const auto dependency=GetModuleHandleW(L"MWArmsAssist.asi");
    const auto contract=dependency?reinterpret_cast<mwcombat::Contract>(GetProcAddress(dependency,"MWAA_CombatContractV1")):nullptr;
    if(!GetModuleHandleW(L"MWArsenal.asi")||!contract||!(contract(mwcombat::Version)&mwcombat::Bit(combat))){
     Log("PACKAGE_DEPENDENCY_MISSING id=%s mode=%s not_registered=1",package.id.c_str(),package.authoring.at("combatMode").get_ref<const std::string&>().c_str());continue;
    }
   }
   if(combat==mwcombat::Mode::HotPursuit&&(!hot_pursuit_runtime::Surface()||!gauntlet_runtime::Install()||!hot_pursuit_runtime::Install()||!hot_pursuit_finish::Install())){Log("PACKAGE_HOT_PURSUIT_UNAVAILABLE hook_conflict_or_surface=1 not_registered=1");continue;}
   if(combat==mwcombat::Mode::HotPursuit&&!rpd_speech_subject::Install())Log("HOT_PURSUIT_RADIO_UNAVAILABLE native_speech_hook=0");
   if(mwel_rpd::Armed(package.authoring)){
    if(GetPrivateProfileIntA("CarbonIntro","Enabled",1,mwel_paths::Settings(game).string().c_str())!=1){Log("PACKAGE_RPD_UNAVAILABLE required_builtin_intro_disabled=1 not_registered=1");continue;}
    const auto assist=GetModuleHandleW(L"MWArmsAssist.asi");
    const auto police=assist?reinterpret_cast<mwcombat::Contract>(GetProcAddress(assist,"MWAA_RpdCaseFilesV1")):nullptr;
    if(!police||!police(mwcombat::Version)||!rpd_mission::Surface()||!rpd_mission::InstallEscapeObserver()||!rpd_police_autodrive::InstallLabLoan()||!MWELInstallGauntletPower()||!rpd_police_autodrive::InstallAudio()||!rpd_native_pursuit::Install()||!rpd_police_population::Install()||!rpd_road_navigation::Install()||!rpd_pursuit_presentation::Install()||!rpd_native_arrest_nis::Install()||!rpd_pursuit_perspective::Install()||!rpd_speech_subject::Install()||!drift_results::Install()||!rpd_pursuit_music::Install()||!MWELArmsPrepareRanking()){Log("PACKAGE_RPD_UNAVAILABLE requires_private_police_side_assist_and_native_surface=1");continue;}
   }
   if(combat==mwcombat::Mode::Gauntlet&&(!gauntlet_runtime::Install()||!hot_pursuit_runtime::Install())){Log("PACKAGE_GAUNTLET_UNAVAILABLE id=%s hook_conflict_or_surface=1 not_registered=1",package.id.c_str());continue;}
   if(combat==mwcombat::Mode::Gauntlet||combat==mwcombat::Mode::HotPursuit){
    // Logging OFF deliberately skips argument evaluation. Install gameplay
    // hooks independently; diagnostic settings must not change HUD behavior.
    const bool hudInstalled=gauntlet_hud::Install();
    Log("GAUNTLET_HUD_INSTALL active=%u scope=bottom_native_meters",unsigned(hudInstalled));
   }
   Log("PACKAGE_VALIDATED id=%s revision=%u native_registration=pending",package.id.c_str(),package.revision);packages.push_back(std::move(package));}
  catch(const std::exception& error){Log("PACKAGE_REFUSED id=%s reason=%s",entry.path().filename().string().c_str(),error.what());}
 }
 mwel::SelectPackages(packages,[](const auto& p){std::error_code error;auto time=mwel::fs::last_write_time(p.directory/L"event.json",error);return error?mwel::fs::file_time_type::min():time;},[](const auto& skipped,const auto& retained,const char* reason){Log("PACKAGE_SUPERSEDED id=%s retained=%s target=%s reason=%s newest_installed_event=1 files_preserved=1",skipped.id.c_str(),retained.id.c_str(),skipped.nativeEventId.c_str(),reason);});
 if(packages.empty()){Log("EVENT_LOADER_IDLE packages=0");return;}
 auto address=reinterpret_cast<void*>(0x6039C0);const BYTE expected[]={0x56,0x8B,0xF1,0x57,0x33,0xFF,0x89,0x3E};
 if(memcmp(address,expected,sizeof(expected))){Log("REFUSED database_constructor_guard");return;}
 if(*reinterpret_cast<void**>(0x91E004)){ObserveDatabase(*reinterpret_cast<void**>(0x91E004));Log("REFUSED database_already_constructed");return;}
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED){Log("REFUSED MinHook_initialize");return;}
 auto experiment=game/L"MWEventLoader.registration-test";
 if(productionRuntime||mwel::fs::exists(experiment)){
  if((!productionRuntime&&mwel::Read(experiment,256)!="MW_EVENT_LOADER_ADDITIONAL_DESCRIPTOR_EXPERIMENT_V1\n")||!registration_experiment::Install()){Log("REFUSED experiment_activation");return;}
  if(productionRuntime)registration_experiment::authoredGeometry=true;
  Log("ADDITIONAL_DESCRIPTOR_EXPERIMENT_ACTIVE playable_event_verified=0");
  auto geometry=game/L"MWEventLoader.geometry-test";
  if(mwel::fs::exists(geometry)){
   if(mwel::Read(geometry,256)!="MW_EVENT_LOADER_AUTHORED_GEOMETRY_EXPERIMENT_V1\n"){Log("REFUSED geometry_marker");return;}
   registration_experiment::authoredGeometry=true;
  }
  // Authored walls are shared by native menu starts and marker-gated lab
  // starts. Preparing only in production silently omitted them in the lab.
  if(!MWELPrepareBarriers(packages,registration_experiment::authoredGeometry)){Log("REFUSED package_wall_geometry");return;}
  if(registration_experiment::authoredGeometry){if(!event_race_progress::ResolveHost()){Log("REFUSED authored_progress_host");return;}event_race_progress::Prepare();if(!event_race_progress::Install()){Log("REFUSED authored_progress_adapter");return;}}
  crashObserver=AddVectoredExceptionHandler(1,ObserveCrash);
  if(!event_race_policies::Install()){Log("REFUSED event_race_policies");return;}
  if(!event_audio::Install())Log("EVENT_NATIVE_AUDIO_FALLBACK optional_extension_unavailable=1 native_ceiling=4");
  if(event_leaderboard::Install())Log("EVENT_NATIVE_LEADERBOARD_ACTIVE capacity=16 activation_count=5-16 native_assets=1 native_name_value_phase=1 native_hud_mask=1 package_reconnect=0");
  else Log("EVENT_NATIVE_LEADERBOARD_FALLBACK conflicting_hook=1 native_ceiling=4");
  if(!event_titles::Install()){Log("REFUSED event_title_adapter_activation");return;}
  if(!rpd_case_files::Install()){Log("REFUSED RPD_case_files_frontend");return;}
  if(!combat_visuals::Install())Log("COMBAT_HEADLIGHT_ANCHOR_UNAVAILABLE dimensions_fallback=1");
  if(!event_presentation::Install())Log("EVENT_COMBAT_PRESENTATION_REFUSED native_surface_mismatch=1");
  if(std::any_of(packages.begin(),packages.end(),[](const auto& p){return mwel_rpd::Armed(p.authoring);})&&!rpd_presentation::InstallRetry())
   Log("RPD_SPRINT_RETRY_HOOK_REFUSED native_surface_mismatch=1");
  if(!event_preview_stats_runtime::Install())Log("EVENT_PREVIEW_STATS_UNAVAILABLE native_surface_mismatch=1");
  if(productionRuntime)Log("NATIVE_PATROL_SCAN_GUARD installed=%u",unsigned(native_patrol_scan::Install()));
  if(!package_resources::Install()){Log("REFUSED package_resource_adapter_activation");return;}
  if(!native_save_isolation::Install()){Log("REFUSED native_save_isolation_activation");return;}
  auto race=game/L"MWEventLoader.race-test";
  if(lab&&mwel::fs::exists(race)){
   auto neo=game/L"MWEventLoader.neo-test";
   if(mwel::fs::exists(neo)){
    if(mwel::Read(neo,256)!="MW_EVENT_LOADER_NEOAI_OBSERVER_EXPERIMENT_V1\n"){Log("REFUSED NeoAI_observer_marker");return;}
    race_start_experiment::neoObserver=true;
   }
   auto field=game/L"MWEventLoader.field-test";
   if(mwel::fs::exists(field)){
    const auto fieldMode=mwel::Read(field,256);
    if(!race_start_experiment::neoObserver||(fieldMode!="MW_EVENT_LOADER_FULL_FIELD_EXPERIMENT_V1\n"&&fieldMode!="MW_EVENT_LOADER_FULL_FIELD_RETIREMENT_EXPERIMENT_V2\n")){Log("REFUSED full_field_marker");return;}
    race_start_experiment::waitForField=true;
    race_start_experiment::retireObservedFinishers=fieldMode=="MW_EVENT_LOADER_FULL_FIELD_RETIREMENT_EXPERIMENT_V2\n";
   }
   const bool transition= race_start_experiment::ConfigureCustomTransition();race_start_experiment::ConfigureSelectedLabPackage();
   if(mwel::Read(race,256)!="MW_EVENT_LOADER_TEMPLATE_ROUTE_START_EXPERIMENT_V1\n"||(packages.size()!=1&&!transition&&race_start_experiment::selectedLabPackage.empty())||!race_start_experiment::Install()){Log("REFUSED race_experiment_activation");return;}
   race_start_experiment::enabled=true;Log("NATIVE_RACE_START_EXPERIMENT_ACTIVE geometry=%s",registration_experiment::authoredGeometry?"authored_owned":"inherited_template");
   auto actuators=game/L"MWEventLoader.actuator-test";
   if(mwel::fs::exists(actuators)){
    if(mwel::Read(actuators,256)!="MW_EVENT_LOADER_NEOAI_ACTUATORS_EXPERIMENT_V1\n"||!race_start_experiment::neoObserver||!MWELInstallNeoDriver()){Log("REFUSED independent_NeoAI_driver_activation");return;}
   }
   auto baseline=game/L"MWEventLoader.native-baseline";
   if(mwel::fs::exists(baseline)){
    if(mwel::Read(baseline,256)!="MW_EVENT_LOADER_EXISTING_EVENT_BASELINE_V1\n"){Log("REFUSED baseline_identity");return;}
    race_start_experiment::nativeBaseline=true;registration_experiment::enumerateAdditional=false;
    Log("EXISTING_EVENT_BASELINE_ACTIVE additional_event_enumeration=0");
   }
  }
  const bool finishProbe=lab&&race_start_experiment::enabled&&mwel::fs::exists(game/L"MWEventLoader.production-finish-test");
  const bool lifecycleProbe=finishProbe||(lab&&mwel::fs::exists(game/L"MWEventLoader.radar-roaming-test")&&mwel::Read(game/L"MWEventLoader.radar-roaming-test",128)=="MW_EVENT_LOADER_RADAR_ROAMING_V1\n");
  if(((productionRuntime||lifecycleProbe)&&!runtime_lifecycle::Install(lifecycleProbe))||(productionRuntime&&!MWELInstallNeoDriver())){Log("REFUSED production_lifecycle_or_driver");return;}
  // The same shared driving owner suppresses cop steering for a human RPD
  // officer. A direct manual lab must install it just as production does.
  if(std::any_of(packages.begin(),packages.end(),[](const auto& p){return mwel_rpd::Armed(p.authoring);})&&
     !MWELNeoDriverInstalled()&&!MWELInstallNeoDriver()){Log("REFUSED RPD_manual_input_owner");return;}
 }
 if(race_start_experiment::enabled)race_start_experiment::PrimeBootstrap();
 // Native initialization publishes 0091E004 only after the constructor returns.
 // Audit at that boundary without taking another MOD's display-frame hook.
 if(native_save_isolation::enabled){
  const BYTE publicationGuard[]={0x6A,0xFF,0x68,0x44,0x63,0x87,0,0x64,0xA1,0,0,0,0};
  auto publication=reinterpret_cast<void*>(0x603A40);
  if(memcmp(publication,publicationGuard,sizeof(publicationGuard))||MH_CreateHook(publication,DatabasePublish,reinterpret_cast<void**>(&originalDatabasePublish))!=MH_OK){Log("REFUSED database_publication_hook");return;}
  if(MH_EnableHook(publication)!=MH_OK){MH_RemoveHook(publication);Log("REFUSED database_publication_activation");return;}
  Log("EVENT_DATABASE_PUBLICATION_AUDIT_ACTIVE native_boundary=00603A40 display_hook_taken=0");
 }
 if(MH_CreateHook(address,DatabaseConstructor,reinterpret_cast<void**>(&originalDatabaseConstructor))!=MH_OK){Log("REFUSED database_constructor_hook");return;}
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);Log("REFUSED database_constructor_activation");return;}
 Log("REGISTRATION_PROBE_ACTIVE original_collections_modified=0 additional_descriptor_experiment=%u save_virtualizer_required=0 ready=0",unsigned(registration_experiment::enabled&&registration_experiment::enumerateAdditional));
 runtimeReady=productionRuntime;
}
}
#pragma comment(linker,"/EXPORT:InitializeASI=_InitializeASI")
#pragma comment(linker,"/EXPORT:MWEL_ReadyV1=_MWEL_ReadyV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_ReadyV1(){return runtimeReady&&runtime_lifecycle::enabled;}
#pragma comment(linker,"/EXPORT:MWEL_InheritedPreviewV1=_MWEL_InheritedPreviewV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_InheritedPreviewV1(){return 1;}
#pragma comment(linker,"/EXPORT:MWEL_DriftRaceV1=_MWEL_DriftRaceV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_DriftRaceV1(unsigned version){return version==1?1u:0u;}
bool MWELRpdOfficerIgnoresGuards(unsigned worldId){return RpdOfficerIgnoresGuards(worldId);}
#pragma comment(linker,"/EXPORT:MWEL_FrameV1=_MWEL_FrameV1")
extern "C" __declspec(dllexport) void __cdecl MWEL_FrameV1(){
 if(!runtime_lifecycle::enabled&&!race_start_experiment::enabled)return;
 __try{
  // ASI initialization runs concurrently with the loader. Wait for the first
  // game frame before deciding whether SunSet or World2MW owns rendering.
  static bool materialChecked=false;
  if(!materialChecked){materialChecked=true;event_material_guard::Install();}
  package_resources::CompleteTextureAdapter();
  rpd_case_files::Frame();
  event_preview_stats_runtime::Frame();
  // A custom race is identifiable during world streaming, before GO. Load
  // native collision data here so the first AI tick cannot block rendering.
  static unsigned warmedKey=0;auto frameKey=MWELNeoCurrentKey();
  if(frameKey&&frameKey!=warmedKey&&*reinterpret_cast<unsigned*>(0x925E90)==5){
   warmedKey=frameKey;auto started=GetTickCount64();auto ready=MWELNativeGeometryWarmup();
   const bool resetRoad=MWELPrepareNeoCourse(frameKey);
   Log("EVENT_PLAYER_REPOSITION_PREWARM key=%08X ready=%u before_go=1",frameKey,unsigned(resetRoad));
   Log("EVENT_NATIVE_GEOMETRY_PREWARM key=%08X ready=%u before_go=1 elapsed_ms=%llu",frameKey,unsigned(ready),GetTickCount64()-started);
  }
  MWELNeoGroundFrame();
  if(!MWELBarrierFrame()){
   runtimeReady=false;runtime_lifecycle::enabled=false;race_start_experiment::aborted=true;
   Log("EXPERIMENT_RACE_ABORT event_barrier_runtime_bridge");return;
  }
  event_race_policies::HeatFrame();
  if(worldLogEnabled)package_resources::AuditPreviews();
  combat_pursuit_lifecycle::BeforeFrame();
  rpd_presentation::RetryFrame();
  rpd_road_navigation::PreparePursuit();
  rpd_police_autodrive::Frame();
  gauntlet_runtime::Frame();hot_pursuit_finish::Frame();hot_pursuit_runtime::Frame();hot_pursuit_radar::Frame();gauntlet_hud::Frame();
  combat_health_runtime::Frame();
  if(!rpd_vehicle_catalog::exported)rpd_vehicle_catalog::Export();
  arms_survival_runtime::Frame();
  MWELRpdFreeRecoveryFrame();
  rpd_officer_handoff::Frame();
  rpd_sprint_capture_camera::Frame();
  rpd_mission::Frame();rpd_hot_pursuit_runtime::Frame();hot_pursuit_arrest::Frame();rpd_sprint_warp::Frame();rpd_speech_subject::ReportMultiple();
  rpd_pursuit_music::Frame();
  rpd_officer_signals::Frame();
  combat_visuals::Frame();combat_markers::Frame();
  combat_defeat_experiment::Frame();
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
  native_player_reposition_test::Frame();
#endif
  runtime_lifecycle::Frame();touge_fall::Frame();drift_race::Frame();checkpoint_display::Tick();
 }__except(EXCEPTION_EXECUTE_HANDLER){
  runtimeReady=false;runtime_lifecycle::enabled=false;race_start_experiment::aborted=true;
  Log("EXPERIMENT_RACE_ABORT common_frame_fault=%08X",unsigned(GetExceptionCode()));
 }
}
#pragma comment(linker,"/EXPORT:MWEL_AttributeHookOwnerV1=_MWEL_AttributeHookOwnerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_AttributeHookOwnerV1(unsigned address,unsigned destination){
 if(!registration_experiment::enabled)return 0;
 const struct {unsigned address;void* function;} hooks[]={
  {0x455960,registration_experiment::GetCollection},{0x455FD0,registration_experiment::FindCollection},
  {0x456B00,registration_experiment::FirstCollection},{0x456B20,registration_experiment::NextCollection},
  {0x453FC0,registration_experiment::CountCollections}};
 for(const auto& hook:hooks)if(hook.address==address&&unsigned(hook.function)==destination){
  BYTE branch[5]{};if(!Copy(reinterpret_cast<void*>(address),branch,5)||branch[0]!=0xE9)return 0;
  int displacement=0;memcpy(&displacement,branch+1,4);return address+5+displacement==destination;
 }return 0;
}
#pragma comment(linker,"/EXPORT:MWEL_LocalRaceTestV1=_MWEL_LocalRaceTestV1")
// NFRR may initialize after combat hooks are installed. Attest only this
// owned forwarding detour, including the intact native trampoline prologue.
// Never approve arbitrary hooks merely because Event Loader is present.
#pragma comment(linker,"/EXPORT:MWEL_EncounterTargetHookOwnerV1=_MWEL_EncounterTargetHookOwnerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_EncounterTargetHookOwnerV1(unsigned address,unsigned destination){
 if(address!=0x423860||destination!=unsigned(hot_pursuit_runtime::AcquireTarget)||!hot_pursuit_runtime::originalTarget)return 0;
 const BYTE expected[]={0x56,0x8B,0xF1,0x8B,0x46,0x1C};BYTE original[6]{},branch[5]{},tail[5]{};
 if(!Copy(reinterpret_cast<void*>(hot_pursuit_runtime::originalTarget),original,6)||memcmp(original,expected,6)||
    !Copy(reinterpret_cast<const BYTE*>(hot_pursuit_runtime::originalTarget)+6,tail,5)||tail[0]!=0xE9||
    !Copy(reinterpret_cast<void*>(address),branch,5)||branch[0]!=0xE9)return 0;
 int displacement=0;memcpy(&displacement,branch+1,4);if(address+5+displacement!=destination)return 0;
 memcpy(&displacement,tail+1,4);
 return unsigned(hot_pursuit_runtime::originalTarget)+6+5+displacement==address+6;
}

extern "C" __declspec(dllexport) unsigned __cdecl MWEL_LocalRaceTestV1(){return (race_start_experiment::enabled&&race_start_experiment::neoObserver&&!race_start_experiment::manualPlayer&&!race_start_experiment::aborted)||(runtime_lifecycle::careerAutodrive&&runtime_lifecycle::validationEnabled);}
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_PlayerEngineTestV1(){return race_start_experiment::enabled&&race_start_experiment::neoObserver&&race_start_experiment::playerEngineTest&&!race_start_experiment::aborted?(race_start_experiment::playerPadEngineTest?2u:1u):0u;}
#pragma comment(linker,"/EXPORT:MWEL_PlayerEngineTestV1=_MWEL_PlayerEngineTestV1")
#pragma comment(linker,"/EXPORT:MWEL_MapOwnerV1=_MWEL_MapOwnerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_MapOwnerV1(unsigned key){return event_maps::active&&event_maps::calibrations.count(key)!=0;}
#pragma comment(linker,"/EXPORT:MWEL_NeoDriverOwnerV1=_MWEL_NeoDriverOwnerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoDriverOwnerV1(){return MWELNeoDriverInstalled();}
#pragma comment(linker,"/EXPORT:MWEL_RaceActiveV1=_MWEL_RaceActiveV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RaceActiveV1(){return (runtime_lifecycle::enabled||race_start_experiment::enabled)&&*reinterpret_cast<unsigned*>(0x925E90)==6&&race_start_experiment::OwnedRace(*reinterpret_cast<BYTE**>(0x91E000));}
#pragma comment(linker,"/EXPORT:MWEL_RpdAuthoringV1=_MWEL_RpdAuthoringV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdAuthoringV1(){return rpd_vehicle_catalog::exported?1u:0u;}
#pragma comment(linker,"/EXPORT:MWEL_RpdUndercoverV1=_MWEL_RpdUndercoverV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdUndercoverV1(){return 1u;}
#pragma comment(linker,"/EXPORT:MWEL_RpdInGameSelectionV1=_MWEL_RpdInGameSelectionV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdInGameSelectionV1(){return 1u;}
#pragma comment(linker,"/EXPORT:MWEL_RpdEditableHeatV1=_MWEL_RpdEditableHeatV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdEditableHeatV1(){return 1u;}
#pragma comment(linker,"/EXPORT:MWEL_RpdHandoffOptionV1=_MWEL_RpdHandoffOptionV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdHandoffOptionV1(){return 1u;}
#pragma comment(linker,"/EXPORT:MWEL_HprSceneRoadFramesV1=_MWEL_HprSceneRoadFramesV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_HprSceneRoadFramesV1(){return 3u;}
#pragma comment(linker,"/EXPORT:MWEL_HprModeScenesV1=_MWEL_HprModeScenesV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_HprModeScenesV1(){return 1u;}
#pragma comment(linker,"/EXPORT:MWEL_RpdSprintIntroPacksV1=_MWEL_RpdSprintIntroPacksV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdSprintIntroPacksV1(){return 1u;}
#pragma comment(linker,"/EXPORT:MWEL_CombatEventV1=_MWEL_CombatEventV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatEventV1(unsigned status,unsigned params,mwcombat::EventV1* output,unsigned size){
 if(!output||size!=sizeof(*output)||status!=*reinterpret_cast<unsigned*>(0x91E000)||!status)return 0;
 unsigned currentParams=0;if(!Copy(reinterpret_cast<BYTE*>(status)+0x1968,&currentParams,4)||params!=currentParams)return 0;
 const auto package=event_race_policies::Current();if(!package)return 0;
 const auto mode=mwcombat::Parse(package->authoring.value("combatMode",std::string("none")).c_str());
 if(!mwcombat::Weapons(mode)||!combat_health_runtime::installed)return 0;
 mwcombat::EventV1 event;event.eventKey=MWELNeoCurrentKey();event.mode=mode;event.racerCount=package->authoring.at("racerCount").get<unsigned>();
 event.flags=1|(mwel_rpd::Armed(package->authoring)?mwcombat::PoliceSide:0u)|
  (mwel_rpd::PoliceSprint(package->authoring)?mwcombat::PoliceSprint:0u);event.epoch=combat_health_runtime::generation;*output=event;return event.eventKey!=0;
}
#pragma comment(linker,"/EXPORT:MWEL_CombatAmmoV1=_MWEL_CombatAmmoV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatAmmoV1(unsigned status,unsigned params,mwcombat::AmmoV1* output,unsigned size){
 if(!output||size!=sizeof(*output))return 0;mwcombat::EventV1 event;
 if(!MWEL_CombatEventV1(status,params,&event,sizeof(event)))return 0;
 const auto package=event_race_policies::Current();if(!package)return 0;
 mwcombat::AmmoV1 ammo;ammo.eventKey=event.eventKey;ammo.epoch=event.epoch;
 if(package->authoring.contains("combatAmmo")){
  const auto& settings=package->authoring.at("combatAmmo");const char* names[]={"emp","turbo","spike","shockwave","jammer"};
  for(unsigned i=0;i<5;++i)if(settings.contains(names[i])){ammo.mask|=1u<<i;ammo.counts[i]=settings.at(names[i]).get<unsigned>();}
 }
 mwel_rpd_weapons::RestrictAmmo((event.flags&mwcombat::PoliceSide)!=0,ammo.mask,ammo.counts);
 *output=ammo;return 1;
}
#pragma comment(linker,"/EXPORT:MWEL_CombatRacerWeaponsV1=_MWEL_CombatRacerWeaponsV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatRacerWeaponsV1(unsigned status,unsigned params,mwcombat::RacerWeaponsV1* output,unsigned size){
 mwcombat::EventV1 event;
 if(!output||size!=sizeof(*output)||!MWEL_CombatEventV1(status,params,&event,sizeof(event))||!(event.flags&mwcombat::PoliceSide))return 0;
 const auto package=event_race_policies::Current();if(!package)return 0;
 mwcombat::RacerWeaponsV1 result;
 const auto weapons=package->authoring.find("policeRacerWeapons");
 if(weapons!=package->authoring.end())for(unsigned i=0;i<4;++i){
  const auto row=weapons->find(mwcombat::RacerWeaponNames[i]);if(row==weapons->end())continue;
  if(row->contains("ammo")){result.ammoMask|=1u<<i;result.counts[i]=row->at("ammo").get<unsigned>();}
  if(row->contains("cooldown")){result.cooldownMask|=1u<<i;result.cooldownSeconds[i]=row->at("cooldown").get<unsigned>();}
 }
 *output=result;return 1;
}
#pragma comment(linker,"/EXPORT:MWEL_CombatObserveHealthV1=_MWEL_CombatObserveHealthV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatObserveHealthV1(unsigned slot,unsigned handle,unsigned damage,unsigned driver,mwcombat::HealthV1* output,unsigned size){return combat_health_runtime::Observe(slot,handle,damage,driver,output,size);}
#pragma comment(linker,"/EXPORT:MWEL_CombatQueryHealthV1=_MWEL_CombatQueryHealthV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatQueryHealthV1(unsigned slot,unsigned handle,mwcombat::HealthV1* output,unsigned size){return combat_health_runtime::Query(slot,handle,output,size);}
#pragma comment(linker,"/EXPORT:MWEL_CombatEffectV1=_MWEL_CombatEffectV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatEffectV1(unsigned damage,unsigned handle,unsigned kind,unsigned phase){return combat_health_runtime::Effect(damage,handle,kind,phase);}
#pragma comment(linker,"/EXPORT:MWEL_CombatPoliceEffectV1=_MWEL_CombatPoliceEffectV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatPoliceEffectV1(unsigned damage,unsigned handle,unsigned kind,unsigned phase){return combat_health_runtime::Effect(damage,handle,kind,phase,true);}
extern "C" __declspec(dllexport) void __cdecl InitializeASI(){
 // Both DllMain and ASI loaders can enter here. Claim initialization once
 // without waiting on another thread while the Windows loader lock is held.
 static volatile LONG initialized=0;if(InterlockedCompareExchange(&initialized,1,0))return;
 try{Start();}catch(const std::exception& error){Log("REFUSED initialization reason=%s",error.what());}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);InitializeASI();}
 if(reason==DLL_PROCESS_DETACH){if(crashObserver){RemoveVectoredExceptionHandler(crashObserver);crashObserver=nullptr;}}
 return TRUE;
}
void MWELUpdateLog(const char* text){Log("%s",text);}
void MWELNeoLog(const char* text){Log("%s",text);}
bool MWELRpdManualDriving(void* primary){return rpd_police_autodrive::ManualDriving(unsigned(primary));}
bool MWELRpdNOSWrite(void* input,bool pressed,unsigned caller){return rpd_police_autodrive::NOSWrite(unsigned(input),pressed,caller);}
void MWELRpdObserveInput(void* input,unsigned channel,float value,unsigned caller){rpd_police_autodrive::ObserveInput(unsigned(input),channel,value,caller);}
bool MWELNeoLoggingEnabled(){return worldLogEnabled;}
bool MWELNeoDetailedLoggingEnabled(){return neoDiagnosticsEnabled;}
bool MWELGauntletBoostOutput(void* vehicle,float* output){return rpd_police_autodrive::Boost(unsigned(vehicle),output)||gauntlet_runtime::Boost(unsigned(vehicle),output);}
bool MWELNeoHoldingAtGenuineGoal(unsigned handle){
 if(!handle||race_start_experiment::ObservedFinisherRetired(handle)||!race_start_experiment::FieldCoastPending())return false;
 for(unsigned slot=0;slot<16;++slot)if((race_start_experiment::genuineGoals&(1u<<slot))&&race_start_experiment::motions[slot].handle==handle)return true;
 return false;
}
unsigned MWELNeoCurrentKey(){
 auto race=race_start_experiment::Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
 auto parameters=race_start_experiment::Read<BYTE*>(race,0x1968),record=race_start_experiment::Read<BYTE*>(parameters,8);
 auto collection=race_start_experiment::Read<registration_experiment::Collection*>(record,4);
 registration_experiment::Collection* visited[16]{};
 for(unsigned i=0;collection&&i<16;++i){for(unsigned j=0;j<i;++j)if(visited[j]==collection)return 0;visited[i]=collection;auto key=race_start_experiment::Read<unsigned>(collection,0x20);auto owned=registration_experiment::Find(key);if(owned&&owned->race)return key;collection=race_start_experiment::Read<registration_experiment::Collection*>(collection,0x10);}return 0;
}
unsigned MWELNeoDriftMode(){
 auto package=event_race_policies::Current();if(!package||!mwel::NeoEnabled(package->authoring))return 0;
 if(package->authoring.value("combatMode",std::string("none"))=="drift")return 2;
 const auto& a=package->authoring;auto field=a.find("driftAIMode");
 if(field==a.end()||!field->is_number_integer())return 0;
 int mode=field->get<int>();return mode>=1&&mode<=2?unsigned(mode):0;
}
unsigned MWELNeoDriftLimit(){
 auto package=event_race_policies::Current();if(!package)return 0;
 if(package->authoring.value("combatMode",std::string("none"))=="drift")return 16;
 const auto& a=package->authoring;auto field=a.find("driftAIMaxRacers");
 if(field==a.end())return 16;if(!field->is_number_integer())return 0;
 return unsigned(std::clamp(field->get<int>(),0,16));
}
bool MWELNeoCourseEnabled(){auto package=event_race_policies::Current();return package&&!(mwel_rpd::CaseFile(package->authoring)&&mwel_rpd::FreeRun(package->authoring))&&mwel::NeoEnabled(package->authoring);}
bool MWELNeoCourseFile(unsigned key,const char* suffix,char* output,unsigned size){
 if(!suffix||!output||!size)return false;auto clone=registration_experiment::Find(key);if(!clone||!clone->race)return false;
 auto asset=std::string("course/")+suffix;
 for(const auto& package:packages)if(package.nativeNamespace==clone->name&&package.assets.contains(asset)){
  auto path=(package.directory/mwel::fs::u8path(asset)).string();if(path.size()>=size)return false;memcpy(output,path.c_str(),path.size()+1);return true;
 }return false;
}
bool MWELNeoSettingsFile(char* output,unsigned size){auto path=mwel_paths::Settings(game).string();if(!output||path.size()>=size)return false;memcpy(output,path.c_str(),path.size()+1);return true;}
float MWELCarbonGestureGo(const char* folder,unsigned count,float tail,float go,float end){
 const auto clip=scene_gesture_timing::Identify(count,tail,go,end);
 if(clip==scene_gesture_timing::Clip::None||!folder)return go;
 try{
  const mwel::fs::path base(folder);
  const auto character=mwel::Digest(mwel::Read(base/L"character.cni",8*1024*1024));
  const auto clock=mwel::Digest(mwel::Read(base/L"actor-clock.bin",128*1024));
  const auto second=clip==scene_gesture_timing::Clip::Paired?mwel::Digest(mwel::Read(base/L"prop-3DAE5F2D.cni",8*1024*1024)):std::string();
  const auto adjusted=scene_gesture_timing::Correct(clip,go,character,clock,second);
  if(adjusted!=go)Log("SCENE_GESTURE_CUE_ADAPTED original=%.6f corrected=%.6f verified_motion_identity=1",go,adjusted);
  return adjusted;
 }catch(...){return go;}
}
bool MWELCarbonNativeStartPose(float* pose,const float* source);
void* __cdecl MWELCarbonOriginalCollection(unsigned classKey,unsigned key){return registration_experiment::findCollection?registration_experiment::findCollection(classKey,key):nullptr;}
const char* MWELCarbonSelection(){
 try{
  auto clone=registration_experiment::Find(MWELNeoCurrentKey());
  if(clone&&clone->race)for(const auto& package:packages)if(package.nativeNamespace==clone->name){
   if(package.authoring.value("combatMode",std::string("none"))=="gauntlet"&&!mwel_carbon::HprGauntletScene(package.authoring.value("carbonIntro",std::string())))return "stock";
   auto intro=package.authoring.find("carbonIntro");
   if(intro!=package.authoring.end()&&intro->is_string())return intro->get_ref<const std::string&>().c_str();
   break;
  }
 }catch(...){return "stock";}
 return "auto";
}
bool MWELCarbonReplaceSourceIntro(unsigned sceneKey){
 try{auto package=event_race_policies::Current();if(!package||!mwel::Correction(package->authoring))return false;
  auto choice=package->authoring.value("carbonIntro",std::string("auto"));if(choice=="auto"||choice=="stock")return false;
  auto race=*reinterpret_cast<BYTE**>(0x91E000);float elapsed=0;if(!race||race[0x1978]||!Copy(race+0x168,&elapsed,4)||!std::isfinite(elapsed)||elapsed>.05f)return false;
  auto clone=registration_experiment::Find(MWELNeoCurrentKey());if(!clone)return false;auto source=registration_experiment::getCollection(clone->collection.klass,clone->templateKey);if(!source)return false;
  auto value=reinterpret_cast<const char**(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,registration_experiment::Hash("IntroNIS"),0);const char* name=nullptr;
  if(!value||!Copy(value,&name,4)||!name||strnlen_s(name,128)>=128||!*name)return false;
  if(reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x460BF0)(name)==sceneKey)return true;
  auto story=reinterpret_cast<BYTE*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,registration_experiment::Hash("DDayRace"),0);BYTE flag=0;return story&&Copy(story,&flag,1)&&flag==1;
 }catch(...){return false;}
}
bool MWELCarbonUsesAuthoredPose(){
 try{auto clone=registration_experiment::Find(MWELNeoCurrentKey());return clone&&clone->race;}catch(...){return false;}
}
bool MWELCarbonRollingStartAllowed(){
 // Explicit HP scene choices retain the source motion and camera through GO.
 // Ordinary auto/native choices retain their established staging behavior.
 const auto p=event_race_policies::Current();if(!p)return false;
 return mwel_rpd::Armed(p->authoring)||(!mwel_rpd::CaseFile(p->authoring)&&mwel_carbon::HprRacerAllowed(p->authoring.value("carbonIntro",std::string()),p->authoring.value("mode",std::string()),p->authoring.value("combatMode",std::string("none"))));
}
bool MWELCarbonLaunchOnly(){const auto p=event_race_policies::Current();return p&&p->authoring.value("combatMode",std::string("none"))=="drift";}
bool MWELCarbonRoadGraph(mwel_scene_road::Graph& graph){
 // These are the live CARP RNnd/RNsg tables. The editor's road_graph.py
 // reads the same 32-byte nodes and 22-byte segments from TRACKS/L2RA.BUN.
 // EN1.3 loader 00789A52/60 and 00789B37/71 establish counts and pointers.
 graph={};if(!MWELCarbonRollingStartAllowed())return false;
 unsigned nodes=0,edges=0,nc=0,ec=0;
 if(!Copy(reinterpret_cast<void*>(0x9B38BC),&nodes,4)||!Copy(reinterpret_cast<void*>(0x9B38C0),&edges,4)||
    !Copy(reinterpret_cast<void*>(0x9B387C),&nc,4)||!Copy(reinterpret_cast<void*>(0x9B3874),&ec,4)||!nodes||!edges||nc<2||nc>65535||!ec||ec>65535)return false;
 graph.nodes.reserve(nc);graph.edges.reserve(ec);
 for(unsigned i=0;i<nc;++i){BYTE raw[32]{};float p[3]{};unsigned short ordinal=0;
  if(!Copy(reinterpret_cast<void*>(nodes+i*32),raw,32))return false;memcpy(p,raw,12);memcpy(&ordinal,raw+12,2);
  mwel_scene_road::Point point{p[2],-p[0],p[1]};if(ordinal!=i||!mwel_scene_road::Finite(point)||raw[16]>7)return false;graph.nodes.push_back(point);
 }
 for(unsigned i=0;i<ec;++i){unsigned short raw[11]{};
  if(!Copy(reinterpret_cast<void*>(edges+i*22),raw,22)||raw[4]!=i||raw[0]>=nc||raw[1]>=nc)return false;
  graph.edges.push_back({raw[0],raw[1]});
 }
 return true;
}
#pragma comment(linker,"/EXPORT:MWEL_RpdAIScopeV1=_MWEL_RpdAIScopeV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdAIScopeV1(unsigned handle){
 const auto p=event_race_policies::Current();unsigned race=0,count=0,suspect=0;BYTE racing=0;
 if(!p||!mwel_rpd::Armed(p->authoring)||rpd_mission::ResultPending()||rpd_finish_camera::Context()||
    !combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x1958,count)||count<2||count>9||
    !combat_health_runtime::Read(race+0x198A,racing)||!racing||!combat_health_runtime::Read(race+0x18+0x194,suspect)||
    !suspect)return 0;
 if(mwel_rpd::PoliceSprint(p->authoring)){
  if(count!=p->authoring.at("racerCount")||MWELRpdSprintRetired(handle))return 0;
  if(!handle)return rpd_ai_contract::NeoSprint;
  for(unsigned slot=1;slot<count;++slot){unsigned entry=0;if(combat_health_runtime::Read(race+0x18+slot*0x194,entry)&&entry==handle)return rpd_ai_contract::NeoSprint;}
  return 0;
 }
 if(count!=2||(handle&&handle!=suspect))return 0;
 return mwel_rpd::FreeRun(p->authoring)?rpd_ai_contract::CustomFreeRun:rpd_ai_contract::NeoSprint;
}
namespace rpd_free_run_runtime {
 unsigned epoch=0,key=0,owned=0,placementDepth=0,physicsOwner=0;
 mwel_scene_road::Graph graph; rpd_free_run::Route route;
 rpd_interceptor_policy::Stuck stuck;float lastTime=-1,nextAttempt=0,nextClockLog=0;
 void Scope(unsigned handle){if(epoch!=combat_health_runtime::generation||key!=MWELNeoCurrentKey()||owned!=handle){
  route.Reset();graph={};stuck={};physicsOwner=0;lastTime=-1;nextAttempt=nextClockLog=0;epoch=combat_health_runtime::generation;key=MWELNeoCurrentKey();owned=handle;
 }}
}
#pragma comment(linker,"/EXPORT:MWEL_RpdCustomTargetV1=_MWEL_RpdCustomTargetV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdCustomTargetV1(unsigned handle,const float* position,const float* heading,float speed,float* output){
 if(!position||!heading||!output||MWEL_RpdAIScopeV1(handle)!=rpd_ai_contract::CustomFreeRun)return 0;
 using namespace rpd_free_run_runtime;Scope(handle);
 if(graph.nodes.empty()&&!MWELCarbonRoadGraph(graph))return 0;
 mwel_scene_road::Point aim;
 static event_geometry::WorldQueries geometry;
 if(geometry.nodes.empty())return 0;
 event_geometry::WorldQueries::RayQueryScope query(geometry);
 auto floor=[&](mwel_scene_road::Point p,float& height){event_geometry::RayHit hit;
  if(!geometry.Ray({-p.y,p.z+3.f,p.x},{-p.y,p.z-3.f,p.x},hit,true,false)||hit.normal.y<.65f||std::abs(hit.point.y-p.z)>3.f)return false;
  height=hit.point.y;return true;
 };
 auto wall=[&](mwel_scene_road::Point p,mwel_scene_road::Point q){event_geometry::RayHit hit;return geometry.Ray({-p.y,p.z,p.x},{-q.y,q.z,q.x},hit,false,true);};
 auto clear=[&](mwel_scene_road::Point a,mwel_scene_road::Point b){return rpd_free_run::RoadCorridor(a,b,floor,wall);};
 if(!route.AimClear(graph,{position[2],-position[0],position[1]},{heading[2],-heading[0],heading[1]},speed,aim,clear))return 0;
 output[0]=-aim.y;output[1]=aim.z;output[2]=aim.x;
 const float dx=output[0]-position[0],dz=output[2]-position[2];
 return dx*dx+dz*dz<1?rpd_ai_contract::TargetHold:rpd_ai_contract::TargetReady;
}
bool MWELRpdResumeTarget(unsigned action){
 unsigned ai=0,car=0,handle=0;combat_impact::Motion motion;
 if(!combat_health_runtime::Racing()||!combat_health_runtime::Read(action+0x40,ai)||!combat_health_runtime::Read(ai-4,car)||
    !gauntlet_runtime::Table(car,0x8AA828)||!(handle=hot_pursuit_runtime::Handle(car))||MWEL_RpdAIScopeV1(handle)!=rpd_ai_contract::CustomFreeRun||
    rpd_mission::LocalArrestInProgress(handle)||!combat_health_runtime::NativeMotion(car-0x80,motion))return false;
 const float position[]={motion.position.x,motion.position.y,motion.position.z},heading[]={motion.forward.x,motion.forward.y,motion.forward.z};float target[3]{};
 return MWEL_RpdCustomTargetV1(handle,position,heading,std::sqrt(combat_impact::Dot(motion.velocity,motion.velocity)),target)==rpd_ai_contract::TargetReady;
}
bool MWELRpdReinforcementAhead(unsigned handle,const float* position,const float* forward,float* point,float* tangent){
 if(!position||!forward||!point||!tangent||MWEL_RpdAIScopeV1(handle)!=rpd_ai_contract::CustomFreeRun)return false;
 using namespace rpd_free_run_runtime;Scope(handle);
 if(graph.nodes.empty()&&!MWELCarbonRoadGraph(graph))return false;
 mwel_scene_road::Frame frame;
 if(!rpd_free_run::ReinforcementAhead(graph,{position[2],-position[0],position[1]},{forward[2],-forward[0],forward[1]},frame))return false;
 point[0]=-frame.center.y;point[1]=frame.center.z;point[2]=frame.center.x;
 tangent[0]=-frame.forward.y;tangent[1]=frame.forward.z;tangent[2]=frame.forward.x;return true;
}
bool MWELRpdLiveIntroAvailable(){return std::any_of(packages.begin(),packages.end(),[](const auto& p){return mwel::RpdStartOnly(p.authoring);});}
bool MWELRpdIntroRequired(){const auto p=event_race_policies::Current();return p&&mwel_rpd::CaseFile(p->authoring);}
void MWELRpdIntroFailed(const char* reason){if(MWELRpdIntroRequired())runtime_lifecycle::RpdIntroFailed(reason);}
bool MWELRpdLiveIntroCurrent(){const auto p=event_race_policies::Current();return p&&mwel::RpdStartOnly(p->authoring);}
bool MWELRpdPrepareLiveGrid(){
 // NIS cleanup restores its own last keyframe. Reapply the two authored
 // positions once, AFTER native UnBind, through IVehicle::SetVehicleOnGround
 // (the same +0x68 call made by WarpPlayerToTrigger at 00611659).
 const auto p=event_race_policies::Current();unsigned race=0,count=0;
 if(!p||!mwel::RpdStartOnly(p->authoring)||!combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x1958,count)||count!=2)return false;
 std::array<unsigned,2> cars{};float positions[2][3]{},bases[2][3]{};
 const mwel::Json* start=nullptr;for(const auto& g:p->authoring.at("gates"))if(g.at("kind")=="start"){start=&g;break;}
 if(!start)return false;
 for(unsigned slot=0;slot<2;++slot){
  unsigned handle=0,table=0,ground=0;
  if(!combat_health_runtime::Read(race+0x18+slot*0x194,handle))return false;
  cars[slot]=gauntlet_runtime::Vehicle(handle);
  if(!cars[slot]||!combat_health_runtime::Read(cars[slot],table)||table!=0x8AA828||!combat_health_runtime::Read(table+0x68,ground)||ground!=0x685810)return false;
  const auto& marker=slot?mwel::RpdRacerStart(p->authoring):*start;
  const auto& xyz=marker.at("position");const float a=marker.at("rotation").get<float>()*3.14159265358979323846f/180.f;
  positions[slot][0]=-xyz.at(1).get<float>();positions[slot][1]=xyz.at(2).get<float>();positions[slot][2]=xyz.at(0).get<float>();
  // SetVehicleOnGround consumes the actual PHYSICS forward vector. GMarker
  // trigger bases use a different axis; reusing that basis rotated both cars
  // 90 degrees in live-s. Match the observed native grid at heading 209.
  bases[slot][0]=-std::sin(a);bases[slot][2]=std::cos(a);
 }
 bool success=true;for(unsigned slot=0;slot<2;++slot){
  const bool placed=reinterpret_cast<bool(__thiscall*)(unsigned,const float*,const float*)>(0x685810)(cars[slot],positions[slot],bases[slot]);
  success=success&&placed;Log("RPD_NATIVE_GRID_RESTORED slot=%u placed=%u native_ground=1 per_frame_pose_writes=0",slot,unsigned(placed));
 }
 return success;
}
bool MWELRpdLiveIntroSample(float* positions,float* directions,unsigned* owner){
 if(!positions||!directions||!owner)return false;
 const auto p=event_race_policies::Current();unsigned race=0,count=0;
 if(!p||!mwel::RpdStartOnly(p->authoring)||!combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x1958,count)||count!=2)return false;
 for(unsigned slot=0;slot<2;++slot){unsigned handle=0;combat_impact::Motion motion;
  if(!combat_health_runtime::Read(race+0x18+slot*0x194,handle))return false;
  const auto vehicle=gauntlet_runtime::Vehicle(handle);
  if(!vehicle||!combat_health_runtime::NativeMotion(vehicle-0x80,motion))return false;
  positions[slot*3]=motion.position.z;positions[slot*3+1]=-motion.position.x;positions[slot*3+2]=motion.position.y;
  directions[slot*3]=motion.forward.z;directions[slot*3+1]=-motion.forward.x;directions[slot*3+2]=motion.forward.y;
 }
 *owner=race;return true;
}
bool MWELCarbonStartPose(float* pose,const float* source){
 if(!pose)return false;
 try{
  auto key=MWELNeoCurrentKey();auto clone=registration_experiment::Find(key);
  if(clone&&clone->race){
   for(const auto& package:packages)if(package.nativeNamespace==clone->name)
    for(const auto& gate:package.authoring.at("gates"))if(gate.at("kind")=="start"){
     for(unsigned i=0;i<3;++i)pose[i]=gate.at("position").at(i).get<float>();pose[3]=gate.at("rotation").get<float>();return true;
    }
   return false;
  }
 }catch(...){return false;}
 return MWELCarbonNativeStartPose(pose,source);
}
#include "CarbonIntroPose.h"
bool MWELCarbonNativeMarkerPose(float* pose){
 using namespace registration_experiment;
 if(!pose||!getCollection)return false;
 const auto attribute=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190);
 auto race=race_start_experiment::Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
 auto parameters=race_start_experiment::Read<BYTE*>(race,0x1968);
 auto instance=race_start_experiment::Read<BYTE*>(parameters,8);
 auto collection=race_start_experiment::Read<Collection*>(instance,4);Collection value{};
 if(!collection||!Copy(collection,&value,sizeof(value))||!value.klass)return false;
 unsigned key=0;auto reference=attribute(collection,Hash("racestart"),0);
 if(!reference||!Copy(reference,&key,sizeof(key))||!key)return false;
 auto marker=getCollection(value.klass,key);if(!marker)return false;
 float result[4]{};auto position=attribute(marker,Hash("Position"),0),rotation=attribute(marker,Hash("Rotation"),0);
 if(!position||!rotation||!Copy(position,result,12)||!Copy(rotation,result+3,4))return false;
 if(!mwel_carbon::StartMarkerPose(pose,result,result+3))return false;
 if(worldLogEnabled)Log("SCENE_NATIVE_MARKER_POSE collection=%08X marker=%08X pose=%.6f,%.6f,%.6f,%.6f",value.key,key,result[0],result[1],result[2],result[3]);
 return true;
}
void MWELCarbonGridSnapshot(const char* boundary){
 if(!worldLogEnabled)return;
 __try{
  auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race)return;
  auto count=*reinterpret_cast<unsigned*>(race+0x1958);
  auto list=*reinterpret_cast<BYTE***>(0x92CD1C);auto vehicles=*reinterpret_cast<unsigned*>(0x92CD24);
  if(!list||count>8||vehicles>128)return;
  for(unsigned slot=0;slot<count;++slot){
   auto handle=*reinterpret_cast<unsigned*>(race+0x18+slot*0x194);
   bool matched=false;
   for(unsigned i=0;i<vehicles;++i)if(list[i]&&*reinterpret_cast<unsigned*>(list[i])==0x8AA828&&*reinterpret_cast<unsigned*>(list[i]-0x78)==handle){
    matched=true;const auto vehicle=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(list[i]);
    Log("SCENE_GRID_IDENTITY boundary=%s slot=%u handle=%u driver=%u vehicle_key=%08X active=%u loading=%u",boundary,slot,handle,unsigned(vehicle->GetDriverClass()),vehicle->GetVehicleKey(),unsigned(vehicle->IsActive()),unsigned(vehicle->IsLoading()));
    NeoBodyMotion body{};if(NeoReadBody(list[i],body))Log("SCENE_GRID_SNAPSHOT boundary=%s slot=%u handle=%u position=%.6f,%.6f,%.6f forward=%.6f,%.6f,%.6f",boundary,slot,handle,body.position[0],body.position[1],body.position[2],body.heading[0],body.heading[1],body.heading[2]);
    break;
   }
   if(!matched)Log("SCENE_GRID_IDENTITY boundary=%s slot=%u handle=%u vehicle_missing=1",boundary,slot,handle);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){Log("SCENE_GRID_SNAPSHOT_UNAVAILABLE boundary=%s",boundary);}
}
bool MWELCarbonNativeStartPose(float* pose,const float*){
 // Native and untouched exported events share the same racestart marker.
 // A racer's body is an animated grid slot, not the scene's AnimLocator.
 // Never reconstruct that locator from one car or shift individual actors.
 return MWELCarbonNativeMarkerPose(pose);
}
bool MWELNeoDifficultySample(unsigned handle,mwel_difficulty::RaceSample& sample){
 sample={};auto race=runtime_lifecycle::Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
 if(!handle||!MWELNeoCurrentKey()||!race||runtime_lifecycle::Read<unsigned>(race,0x1960)!=1||
    !runtime_lifecycle::Read<BYTE>(race,0x198A)||!runtime_lifecycle::Read<BYTE>(race,0x1978))return false;
 unsigned count=runtime_lifecycle::Read<unsigned>(race,0x1958);if(!count||count>16)return false;
 // Slot zero is the native local player. Use route progress, including laps,
 // rather than world-space proximity on loops, shortcuts or stacked roads.
 auto player=race+0x18;const float length=runtime_lifecycle::Read<float>(race,0x4484);
 const float playerPercent=runtime_lifecycle::Read<float>(player,0x18);
 if(!std::isfinite(length)||length<=0||!std::isfinite(playerPercent)||playerPercent<0||playerPercent>100||
    runtime_lifecycle::Read<unsigned>(player,0x1C)||runtime_lifecycle::Read<BYTE>(player,0x21)||runtime_lifecycle::Read<BYTE>(player,0x190))return false;
 for(unsigned slot=1;slot<count;++slot){auto info=race+0x18+slot*0x194;if(runtime_lifecycle::Read<unsigned>(info)!=handle)continue;
  const unsigned rank=runtime_lifecycle::Read<unsigned>(info,0x10);const float percent=runtime_lifecycle::Read<float>(info,0x18);
  if(!rank||rank>count||!std::isfinite(percent)||percent<0||percent>100||runtime_lifecycle::Read<unsigned>(info,0x1C)||
     runtime_lifecycle::Read<BYTE>(info,0x21)||runtime_lifecycle::Read<BYTE>(info,0x190))return false;
  unsigned blacklist=0;
  auto clone=registration_experiment::Find(MWELNeoCurrentKey());
  if(clone&&clone->race)for(const auto& package:packages)if(package.nativeNamespace==clone->name){
   auto found=package.authoring.find("blacklistNo");
   if(found!=package.authoring.end()&&found->is_number_integer()){
    auto number=found->get<long long>();if(number>=1&&number<=15)blacklist=unsigned(number);
   }
   break;
  }
  sample={rank,length*(playerPercent-percent)*.01f,true,blacklist};return true;
 }return false;
}
bool MWELNeoGate(unsigned key,unsigned leg,float* center,float* dimensions,float* rotation){
 auto clone=registration_experiment::Find(key);if(!clone||!clone->race||!center||!dimensions||!rotation)return false;
 for(const auto& package:packages)if(package.nativeNamespace==clone->name){
  const mwel::Json* target=nullptr;unsigned index=0;
  for(const auto& gate:package.authoring.at("gates"))if(gate.at("kind")=="checkpoint"){if(index++==leg){target=&gate;break;}}
  if(!target&&leg==index)for(const auto& gate:package.authoring.at("gates"))if(gate.at("kind")=="finish"){target=&gate;break;}
  if(!target)return false;auto p=target->at("position"),d=target->at("dimensions");center[0]=-p.at(1).get<float>();center[1]=p.at(2).get<float>();center[2]=p.at(0).get<float>();for(unsigned i=0;i<3;++i)dimensions[i]=d.at(i).get<float>();*rotation=target->at("rotation").get<float>();return true;
 }return false;
}


void* MWELRpdFreeRunVehicle(){
 unsigned race=0,handle=0,driver=0;
 if(MWEL_RpdAIScopeV1(0)!=rpd_ai_contract::CustomFreeRun||!combat_health_runtime::Racing()||
    !combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x18+0x194,handle))return nullptr;
 const auto car=gauntlet_runtime::Vehicle(handle);
 return car&&combat_health_runtime::Read(car+0x94,driver)&&driver==3?reinterpret_cast<void*>(car):nullptr;
}
bool MWELRpdSprintLocked(unsigned handle){return rpd_hot_pursuit_runtime::Locked(handle);}
bool MWELRpdSprintRecoveryHeld(unsigned handle){return MWELRpdSprintRetired(handle)||MWELRpdSprintArresting(handle)||MWELHotPursuitAIArresting(handle)||MWELHotPursuitNormalRetired(handle);}
bool MWELRpdRetiredDriving(void* primary){
 // Retired native racers can leave Neo scope. Do not hand them back to the
 // stock driver when that happens; retirement owns both controller paths.
 const auto p=unsigned(primary);unsigned car=0,bound=0;
 if(!gauntlet_runtime::Table(p,0x892720)||!gauntlet_runtime::Read(p+0x48,car)||
    !gauntlet_runtime::Table(car,0x8AA828)||!gauntlet_runtime::Read(car+0x54,bound)||bound!=p+0x4C)return false;
 const auto handle=hot_pursuit_runtime::Handle(car);
 const bool retired=(MWELRpdSprintRetired(handle)&&!rpd_hot_pursuit_runtime::NativeSceneOwns(handle))||MWELHotPursuitNormalRetired(handle);
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(retired&&worldLogEnabled){static ULONGLONG next=0;const auto now=GetTickCount64();if(now>=next){next=now+1000;Log("EXPERIMENT_RPD_RETIRED_DRIVER_BLOCK handle=%u native_and_neo_drive_suppressed=1",handle);}}
#endif
 return retired;
}
bool MWELRpdVehiclePlacementAllowed(void* vehicle){
 const auto car=unsigned(vehicle);
 if(gauntlet_runtime::Table(car,0x8AA828)){
  const auto handle=hot_pursuit_runtime::Handle(car);
  if(MWELRpdSprintRecoveryHeld(handle))return false;
 }
 return rpd_free_run_runtime::placementDepth||!vehicle||vehicle!=MWELRpdFreeRunVehicle();
}
bool MWELRpdNativePlacementAllowed(void* ai){
 // Both AIVehicle placement entry points receive the +4Ch AI interface,
 // not IVehicle. Validate its reciprocal vehicle binding before scoping it.
 unsigned v=0,bound=0,handle=0;const auto aiAddress=unsigned(ai);
 if(!combat_health_runtime::Racing()||aiAddress<4||
    !combat_health_runtime::Read(aiAddress-4,v)||!gauntlet_runtime::Table(v,0x8AA828)||
    !combat_health_runtime::Read(v+0x54,bound)||bound!=aiAddress||
    !(handle=hot_pursuit_runtime::Handle(v)))return true;
 if(MWELRpdSprintRecoveryHeld(handle))return false;
 if(MWEL_RpdAIScopeV1(handle)!=rpd_ai_contract::CustomFreeRun)return true;
 if(!rpd_free_run_runtime::placementDepth&&worldLogEnabled){static ULONGLONG next=0;const auto now=GetTickCount64();if(now>=next){next=now+1000;Log("RPD_NATIVE_PLACEMENT_RETAINED handle=%u ai=%08X vehicle=%08X same_deck_recovery_required=1",handle,aiAddress,v);}}
 return rpd_free_run_runtime::placementDepth!=0;
}
bool MWELRpdRaceMode(){const auto p=event_race_policies::Current();return p&&mwel_rpd::Armed(p->authoring);}
bool MWELRpdPoliceSprint(){return rpd_hot_pursuit_runtime::Current();}
void MWELRpdFreeRecoveryFrame(){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(combat_defeat_experiment::rpdAIResume)return; // Independent drive-state fixture: never hide failure with relocation.
#endif
 using namespace rpd_free_run_runtime;
 unsigned race=0,handle=0;float now=0;
 if(!combat_health_runtime::Racing()||!combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x18+0x194,handle)||
    MWEL_RpdAIScopeV1(handle)!=rpd_ai_contract::CustomFreeRun||!combat_health_runtime::Read(0x9885D8,now))return;
 Scope(handle);const float dt=lastTime>=0?now-lastTime:0;lastTime=now;
 if(physicsOwner!=handle){const auto car=MWELRpdFreeRunVehicle();if(!car)return;
  reinterpret_cast<void(__thiscall*)(void*,int)>(0x688390)(car,1);physicsOwner=handle;
 }
 // A stopped target under the local officer's real arrest is intentional.
 // Do not teleport it away just before native arrest completes.
 if(rpd_mission::LocalArrestInProgress(handle)){stuck={};return;}
 if(worldLogEnabled&&now>=nextClockLog){nextClockLog=now+2;Log("RPD_CUSTOM_AI_RECOVERY_CLOCK handle=%u dt=%.6f stationary_seconds=%.3f",handle,dt,stuck.seconds);}
 const auto car=gauntlet_runtime::Vehicle(handle);combat_impact::Motion m;
 if(!car||!combat_health_runtime::NativeMotion(car-0x80,m)||!stuck.Step(dt,m.position.x,m.position.y,m.position.z)||now<nextAttempt)return;
 nextAttempt=now+1;
 if(graph.nodes.empty()&&!MWELCarbonRoadGraph(graph))return;
 // A cached forward station may already be on another ramp. Re-anchor to
 // the physical car every time and independently verify the connected deck.
 static event_geometry::WorldQueries ground;
 event_geometry::WorldQueries::RayQueryScope recoveryQuery(ground);
 if(!route.RebuildRecoveryClear(graph,{m.position.z,-m.position.x,m.position.y},{m.forward.z,-m.forward.x,m.forward.y},[&](mwel_scene_road::Point p,float height){
  event_geometry::RayHit hit;
  return ground.Ray({-p.y,height+1.f,p.x},{-p.y,height-3.f,p.x},hit,true)&&hit.normal.y>=.65f&&std::abs(hit.point.y-height)<3.f;
 },[&](mwel_scene_road::Point center,mwel_scene_road::Point along){
  // A centre ray also supports narrow medians and pavement beside a wall.
  // Reject those anchors before picking the nearest road, so their blocked
  // footprint cannot hide another clear lane on the same physical deck.
  event_geometry::RayHit anchor;if(!ground.Ray({-center.y,m.position.y+1.f,center.x},{-center.y,m.position.y-3.f,center.x},anchor,true)||anchor.normal.y<.65f)return false;
  const mwel_scene_road::Frame frame{center,along};
  const auto floor=[&](mwel_scene_road::Point p,float& h){event_geometry::RayHit hit;
   const float expected=rpd_free_run::ProjectedFloorHeight(frame,p,anchor.point.y);
   if(!std::isfinite(expected)||!ground.Ray({-p.y,expected+1.25f,p.x},{-p.y,expected-1.5f,p.x},hit,true)||hit.normal.y<.65f||std::abs(hit.point.y-expected)>1.5f)return false;h=hit.point.y;return true;};
  const auto wall=[&](mwel_scene_road::Point a,mwel_scene_road::Point b){event_geometry::RayHit hit;return ground.Ray({-a.y,a.z,a.x},{-b.y,b.z,b.x},hit,false,true);};
  return rpd_free_run::RoadCorridor(center-along*2.f,center+along*2.f,floor,wall);
 })){
  if(worldLogEnabled)Log("RPD_CUSTOM_AI_RECOVERY_REFUSED handle=%u reason=no_same_deck_road physical_seconds=%.3f",handle,stuck.seconds);return;
 }
 unsigned unsupported=0,occupied=0,overhead=0,footprints=0,vehicles=0;
 // Nearby police may occupy every supported point in the first 20 metres.
 // Continue along the same validated deck instead of refusing recovery at
 // that boundary. Every added candidate still requires physical road support,
 // overhead clearance and an empty vehicle footprint before any placement.
 for(float laneShift:{0.f,2.f,-2.f,4.f,-4.f,6.f,-6.f})for(float ahead:{4.f,8.f,12.f,16.f,20.f,24.f,28.f,32.f,-4.f,-8.f,-12.f,-16.f}){
  mwel_scene_road::Frame f;if(!route.Recovery(ahead,f,laneShift))continue;
  float position[]={-f.center.y,f.center.z,f.center.x},direction[]={-f.forward.y,f.forward.z,f.forward.x};
  const float dxCurrent=position[0]-m.position.x,dzCurrent=position[2]-m.position.z;
  if(dxCurrent*dxCurrent+dzCurrent*dzCurrent>32.f*32.f)continue;
  event_geometry::RayHit support;float floor=0;bool supported=true;
  // The wreck may be in a planter or against a wall with no floor directly
  // underneath. Validate the connected same-deck ROAD from its projection,
  // rather than requiring that invalid wreck footprint to be driveable.
  const unsigned steps=std::max(1u,unsigned(std::ceil(std::abs(ahead)/2.f)));
  float previousRoad=0,previousFloor=0;
  for(unsigned step=0;step<=steps;++step){mwel_scene_road::Frame road;
   if(!route.Recovery(ahead*float(step)/steps,road,laneShift)){supported=false;break;}
   const float x=-road.center.y,z=road.center.x,h=road.center.z;
   // Lock the initial real deck to the stopped body, then follow its grade.
   // Correct graph bias without widening every sample onto a tunnel roof.
   const float expected=step?previousFloor+h-previousRoad:m.position.y;
   if(!ground.Ray({x,expected+(step?1.25f:1.f),z},{x,expected-(step?1.5f:3.f),z},support,true)||support.normal.y<.65f||std::abs(support.point.y-expected)>(step?1.5f:3.f)){supported=false;break;}
   floor=previousFloor=support.point.y;previousRoad=h;
  }
  if(!supported||std::abs(floor+.75f-m.position.y)>2.5f){++unsupported;continue;}
  position[1]=floor+.75f;
  const auto roadFloor=[&](mwel_scene_road::Point p,float& h){event_geometry::RayHit hit;
   const float expected=rpd_free_run::ProjectedFloorHeight(f,p,floor);
   if(!std::isfinite(expected)||!ground.Ray({-p.y,expected+1.25f,p.x},{-p.y,expected-1.5f,p.x},hit,true)||hit.normal.y<.65f||std::abs(hit.point.y-expected)>1.5f)return false;h=hit.point.y;return true;};
  const auto roadWall=[&](mwel_scene_road::Point a,mwel_scene_road::Point b){event_geometry::RayHit hit;return ground.Ray({-a.y,a.z,a.x},{-b.y,b.z,b.x},hit,false,true);};
  if(!rpd_free_run::RoadCorridor(f.center-f.forward*2.f,f.center+f.forward*2.f,roadFloor,roadWall)){++occupied;
   if(worldLogEnabled&&footprints==0)Log("RPD_RECOVERY_FOOTPRINT_BLOCKED position=%.3f,%.3f,%.3f graph_height=%.3f lane_shift=%.1f",position[0],position[1],position[2],f.center.z,laneShift);++footprints;continue;}
  const auto floorNormal=support.normal;
  if(ground.Ray({position[0],floor+.15f,position[2]},{position[0],floor+2.5f,position[2]},support,false,false)){++overhead;continue;}
  std::vector<unsigned> cars;bool clear=hot_pursuit_runtime::List(cars);
  for(auto other:cars)if(other!=car){combat_impact::Motion occupied;
   if(combat_health_runtime::NativeMotion(other-0x80,occupied)){
    const float dx=position[0]-occupied.position.x,dy=position[1]-occupied.position.y,dz=position[2]-occupied.position.z;
    if(dx*dx+dz*dz<64&&std::abs(dy)<4){clear=false;
     if(worldLogEnabled&&vehicles==0)Log("RPD_RECOVERY_VEHICLE_BLOCKED target=%08X other=%08X position=%.3f,%.3f,%.3f other_position=%.3f,%.3f,%.3f",car,other,position[0],position[1],position[2],occupied.position.x,occupied.position.y,occupied.position.z);++vehicles;break;}
   }
  }
  if(!clear){++occupied;continue;}
  // SetVehicleOnGround also writes a fallback pose when its stream query
  // returns false. It cannot own our already verified deck. Place the native
  // rigid body exactly once, without re-querying native streamed scenery.
  auto body=rpd_officer_handoff::Sim(car)->GetRigidBody();float matrix[16]{},previousMatrix[16]{};
  const float up[]={floorNormal.x,floorNormal.y,floorNormal.z};
  if(!body||!rpd_free_run::RecoveryMatrix(position,direction,up,matrix)||!rpd_officer_handoff::BodyMatrix(body,previousMatrix,false))continue;
  NFSPluginSDK::MW05::UMath::Vector3 nativePosition{},zero{};memcpy(&nativePosition,position,12);
  if(!rpd_officer_handoff::BodyMatrix(body,matrix,true))return;
  body->SetPosition(nativePosition);body->SetLinearVelocity(zero);body->SetAngularVelocity(zero);
  combat_impact::Motion after;const bool read=combat_health_runtime::NativeMotion(car-0x80,after);
  if(!read){if(worldLogEnabled)Log("RPD_CUSTOM_AI_RECOVERY_REFUSED handle=%u reason=placed_body_unreadable one_placement_only=1",handle);return;}
  const float dx=after.position.x-position[0],dz=after.position.z-position[2];
  if(dx*dx+dz*dz>4||std::abs(after.position.y-position[1])>1){
   NFSPluginSDK::MW05::UMath::Vector3 previousPosition{};memcpy(&previousPosition,&m.position,12);
   rpd_officer_handoff::BodyMatrix(body,previousMatrix,true);body->SetPosition(previousPosition);
   if(worldLogEnabled)Log("RPD_CUSTOM_AI_RECOVERY_REFUSED handle=%u reason=pose_mismatch original_pose_restored=1 one_placement_only=1",handle);return;
  }
  Log("RPD_CUSTOM_AI_RECOVERY handle=%u stuck_physics_seconds=%.3f native_placement_verified=1 native_ground_snap=0 exact_body_pose=1 CP_lap_writes=0 route_offset=%.1f lane_shift=%.1f",handle,stuck.seconds,ahead,laneShift);
  if(combat_defeat_experiment::rpdRecovery&&combat_defeat_experiment::heldAlly==handle){
   rpd_officer_handoff::Vehicle(car)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));
   combat_defeat_experiment::heldAlly=0;combat_defeat_experiment::hits=2;
   Log("EXPERIMENT_RPD_RECOVERY_RELEASE after_verified_placement=1 actual_body_resumption_required=1");
  }
  route.Reset();stuck={};return;
 }
 if(worldLogEnabled)Log("RPD_CUSTOM_AI_RECOVERY_REFUSED handle=%u reason=no_clear_same_deck_candidate unsupported=%u overhead=%u occupied=%u footprints=%u vehicles=%u physical_seconds=%.3f",handle,unsupported,overhead,occupied,footprints,vehicles,stuck.seconds);
}

bool MWELRpdConstrainResultCamera(const float* target,float* eye,float* look){
 if(!target||!eye||!look)return false;
 static event_geometry::WorldQueries geometry;if(geometry.nodes.empty())return false;
 event_geometry::WorldQueries::RayQueryScope query(geometry);event_geometry::RayHit hit;bool changed=false;
 if(geometry.Ray({target[0],target[1],target[2]},{eye[0],eye[1],eye[2]},hit,false,true)){
  float delta[3]={eye[0]-target[0],eye[1]-target[1],eye[2]-target[2]};
  float length=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
  const float dx=hit.point.x-target[0],dy=hit.point.y-target[1],dz=hit.point.z-target[2];
  const float distance=std::max(.6f,std::sqrt(dx*dx+dy*dy+dz*dz)-.4f);
  if(length>distance){for(unsigned i=0;i<3;++i)eye[i]=target[i]+delta[i]*(distance/length);changed=true;}
 }
 if(geometry.Ray({eye[0],eye[1]+2,eye[2]},{eye[0],eye[1]-4,eye[2]},hit,true,false)&&
    hit.normal.y>.6f&&std::abs(hit.point.y-(target[1]-1))<3&&eye[1]<hit.point.y+.35f){eye[1]=hit.point.y+.35f;changed=true;}
 if(changed){look[0]=target[0];look[1]=target[1];look[2]=target[2];}
 return changed;
}
bool MWELRpdCamera(float* matrix){
 return rpd_finish_camera::View(matrix)||
  (!rpd_sprint_warp::warping&&rpd_sprint_capture_camera::View(matrix))||rpd_officer_handoff::Camera(matrix);
}
void MWELRpdIntroMusicFrame(unsigned race,unsigned generation,float elapsed,float countdown,float go,bool skipped){rpd_intro_music::Frame(race,generation,elapsed,countdown,go,skipped);}
void MWELRpdIntroMusicGo(){rpd_intro_music::Go();}
void MWELRpdIntroMusicEnd(){rpd_intro_music::Reset();}
float MWELRpdFade(){return rpd_officer_handoff::fade;}
void MWELRpdDiscardRestartSpawns(){
 // ICopMgr::ResetCopsForRestart releases cars and activities, but leaves
 // mSpawnRequests queued while resetting its spawn lockout to 60 seconds.
 // NotifyScriptWhenLoaded waits for that queue to empty. Its old positions
 // belong to the abandoned race, so cancel them through the native list
 // clear routine before Lua enters ResetCopsForRestart/NotifyScriptWhenLoaded.
 using namespace combat_health_runtime;
 const auto key=MWELNeoCurrentKey();const auto policy=event_race_policies::Current();
 unsigned race=0,play=0,manager=0,head=0,count=0;BYTE active=1;
 if(GetCurrentThreadId()!=gameThread||!key||hot_pursuit_runtime::owner!=key||
    !policy||!mwel_rpd::Armed(policy->authoring)||!Read(0x91E000,race)||
    !Read(race+0x1960,play)||play!=1||!Read(race+0x198A,active)||active||
    !Read(0x92C67C,manager)||!hot_pursuit_runtime::Table(manager,0x891900)||
    !Read(manager+0x18,head)||!Read(manager+0x1C,count)||!count||count>64)return;
 const BYTE expected[]={0x57,0x8B,0xF9,0x8B,0x4F,4,0x8B,1,0x89,9,0x8B,0x4F,4,0x89,0x49,4};
 BYTE actual[sizeof(expected)]{};
 if(!Copy(reinterpret_cast<void*>(0x40D1E0),actual,sizeof(actual))||memcmp(actual,expected,sizeof(actual)))return;
 unsigned node=0,previous=head,tail=0;if(!Read(head,node)||!Read(head+4,tail))return;
 for(unsigned i=0;i<count;++i){unsigned back=0,next=0;
  if(!node||node==head||!Read(node+4,back)||back!=previous||!Read(node,next))return;
  previous=node;node=next;
 }
 if(node!=head||previous!=tail)return;
 reinterpret_cast<void(__thiscall*)(unsigned)>(0x40D1E0)(manager+0x14);
 unsigned remaining=~0u;Read(manager+0x1C,remaining);
 Log("RPD_RESTART_SPAWN_QUEUE_CLEARED discarded=%u remaining=%u native_clear=0040D1E0 lockout_and_load_flags_unchanged=1",count,remaining);
}
void MWELRpdPrepareRaceTeardown(){
 rpd_officer_handoff::ReturnOriginal();
 if(!rpd_police_autodrive::car)return;
 // SetActive(false) precedes the native restart's pursuit detachment and
 // the next Stage(true). Without a handoff ReturnOriginal is a no-op, so
 // leaving this override until Stage lets the old cop action dereference
 // its now-null pursuit at 00427186. Remove our cop behaviour while its
 // pursuit and ICop are still live, using the existing ordered reset.
 rpd_controls::BeginTransition();
 Log("RPD_OFFICER_TEARDOWN_BOUNDARY before_native_active_false=1 car=%08X",rpd_police_autodrive::car);
 rpd_police_autodrive::Reset();
 rpd_controls::Restore("active_false_cop_removed");
}
