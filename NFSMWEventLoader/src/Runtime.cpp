#include "EventCore.h"
#include "UpdateNotice.h"
#include "PackageSelection.h"
#include "WorldCompatibility.h"
#include "NativeCashValue.h"
#include "CareerTuningPolicy.h"
#include "EventBarriers.h"
#include "CarbonParticleFX.h"
#include "NeoDriverBridge.h"
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
#include <mutex>
#include <NFSPluginSDK/Game.MW05/Types/IPlayer.h>
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
void MWELCancelCarbonIntro();

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
#include "RegistrationExperiment.h"
void MWELCombatNewGeneration();
void MWELCombatRaceExit();
bool MWELHotPursuitEscaping(unsigned);
void MWELHotPursuitFinish(void*,bool,bool,int,void(__cdecl*)(void*));
bool MWELHotPursuitResultPending();
bool MWELCombatRetired(unsigned,unsigned);
void MWELLabProductionFinish(void*,void(__cdecl*)(void*));
void MWELLabProductionEnd(void*,void(__thiscall*)(void*));
#include "EventRacePolicies.h"
#include "EventTitles.h"
#include "PackageResources.h"
#include "EventPresentation.h"
#include "EventMaps.h"
#include "CheckpointDisplay.h"
#include "ProfileProgress.h"
#include "NativeSaveIsolation.h"
#include "EventSpeedTraps.h"
bool MWELCombatVisualHeld(unsigned);
#include "RaceStartExperiment.h"
#include "RuntimeLifecycle.h"
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
#include "HotPursuitRuntime.h"
#include "GauntletRuntime.h"
#include "NativeMaterialRuntime.h"
#include "HotPursuitController.h"
#include "CombatPursuitLifecycle.h"
#include "HotPursuitFinish.h"
bool MWELHotPursuitResultPending(){return hot_pursuit_finish::WinningResult();}
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
 hot_pursuit_finish::Reset();
 hot_pursuit_runtime::Reset();gauntlet_runtime::Reset();combat_visuals::Reset();combat_markers::Reset();
 combat_health_runtime::NewGeneration();
 if(race_start_experiment::enabled)race_start_experiment::ResetObservation();
}
void MWELCombatRaceExit(){
 static bool releasing=false;if(releasing)return;releasing=true;
 __try {
  hot_pursuit_runtime::Reset();gauntlet_runtime::Reset();combat_visuals::Reset();combat_markers::Reset();combat_health_runtime::Restore();
 }__finally {releasing=false;}
}
#pragma comment(linker,"/EXPORT:MWEL_CombatAttackSourceV1=_MWEL_CombatAttackSourceV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatAttackSourceV1(unsigned source){
 using namespace combat_health_runtime;const auto prior=attackSource;
 if(gameThread==GetCurrentThreadId()&&Active())attackSource=source;
 return prior;
}
bool MWELCombatRetired(unsigned slot,unsigned handle){mwcombat::HealthV1 health;return combat_health_runtime::Query(slot,handle,&health,sizeof(health))&&(health.flags&1)&&health.remaining==0;}
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
   const auto expected=native_cash::Value(package.authoring.at("cashValue").get<unsigned>());
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
 try{if(registration_experiment::authoredGeometry&&!MWELCompleteBarriers())throw std::runtime_error("Package wall preload unavailable");registration_experiment::Prepare();}catch(const std::exception& error){Log("EXPERIMENT_REFUSED reason=%s",error.what());registration_experiment::clones.clear();}
 void* result=originalDatabaseConstructor(self);ObserveDatabase(result);
 package_resources::LoadPreviews();
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
 InitWorldDebugLog(mwel_paths::Settings(game).string().c_str(),mwel_paths::Diagnostic(game,L"EventLoader.log").string().c_str());
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
 const bool sharedHud=gauntlet_hud::InstallFeatures();
 Log("EVENT_SHARED_HUD_FEATURES active=%u",unsigned(sharedHud));
 if(sharedHud)MWELStartCarbonIntro();
 world_compatibility::Context world(game);
 std::vector<mwel::fs::path> eventFolders{mwel_paths::Events(game)};
 for(const auto& directory:eventFolders)if(mwel::fs::exists(directory))for(const auto& entry:mwel::fs::directory_iterator(directory)){
  if(!entry.is_directory()||!mwel::EventId(entry.path().filename().string()))continue;
  try{auto package=mwel::ParsePackage(entry.path());
   if(!world.Allows(package)){Log("PACKAGE_WORLD_MISMATCH id=%s world=%s not_registered=1",package.id.c_str(),package.worldSnapshotId.c_str());continue;}
   const auto combat=mwcombat::Parse(package.authoring.value("combatMode",std::string("none")).c_str());
   if(combat!=mwcombat::Mode::None){
    const auto dependency=GetModuleHandleW(mwcombat::Weapons(combat)?L"MWArmsAssist.asi":L"ArcadeDrift.asi");
    const auto contract=dependency?reinterpret_cast<mwcombat::Contract>(GetProcAddress(dependency,mwcombat::Weapons(combat)?"MWAA_CombatContractV1":"MWDRIFT_RaceContractV1")):nullptr;
    if((mwcombat::Weapons(combat)&&!GetModuleHandleW(L"MWArsenal.asi"))||!contract||!(contract(mwcombat::Version)&mwcombat::Bit(combat))){
     Log("PACKAGE_DEPENDENCY_MISSING id=%s mode=%s not_registered=1",package.id.c_str(),package.authoring.at("combatMode").get_ref<const std::string&>().c_str());continue;
    }
   }
   if(combat==mwcombat::Mode::HotPursuit&&(!hot_pursuit_runtime::Surface()||!gauntlet_runtime::Install()||!hot_pursuit_runtime::Install()||!hot_pursuit_finish::Install())){Log("PACKAGE_HOT_PURSUIT_UNAVAILABLE hook_conflict_or_surface=1 not_registered=1");continue;}
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
  if(!combat_visuals::Install())Log("COMBAT_HEADLIGHT_ANCHOR_UNAVAILABLE dimensions_fallback=1");
  if(!event_presentation::Install())Log("EVENT_COMBAT_PRESENTATION_REFUSED native_surface_mismatch=1");
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
    if(!race_start_experiment::neoObserver||mwel::Read(field,256)!="MW_EVENT_LOADER_FULL_FIELD_EXPERIMENT_V1\n"){Log("REFUSED full_field_marker");return;}
    race_start_experiment::waitForField=true;
   }
   const bool transition= race_start_experiment::ConfigureCustomTransition();
   if(mwel::Read(race,256)!="MW_EVENT_LOADER_TEMPLATE_ROUTE_START_EXPERIMENT_V1\n"||(packages.size()!=1&&!transition)||!race_start_experiment::Install()){Log("REFUSED race_experiment_activation");return;}
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
#pragma comment(linker,"/EXPORT:MWEL_FrameV1=_MWEL_FrameV1")
extern "C" __declspec(dllexport) void __cdecl MWEL_FrameV1(){
 if(!runtime_lifecycle::enabled&&!race_start_experiment::enabled)return;
 __try{
  // ASI initialization runs concurrently with the loader. Wait for the first
  // game frame before deciding whether SunSet or World2MW owns rendering.
  static bool materialChecked=false;
  if(!materialChecked){materialChecked=true;event_material_guard::Install();}
  // A custom race is identifiable during world streaming, before GO. Load
  // native collision data here so the first AI tick cannot block rendering.
  static unsigned warmedKey=0;auto frameKey=MWELNeoCurrentKey();
  if(frameKey&&frameKey!=warmedKey&&*reinterpret_cast<unsigned*>(0x925E90)==5){
   warmedKey=frameKey;auto started=GetTickCount64();auto ready=MWELNativeGeometryWarmup();
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
  gauntlet_runtime::Frame();hot_pursuit_finish::Frame();hot_pursuit_runtime::Frame();hot_pursuit_radar::Frame();gauntlet_hud::Frame();
  combat_health_runtime::Frame();
  combat_visuals::Frame();combat_markers::Frame();
  combat_defeat_experiment::Frame();
  runtime_lifecycle::Frame();checkpoint_display::Tick();
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

extern "C" __declspec(dllexport) unsigned __cdecl MWEL_LocalRaceTestV1(){return (race_start_experiment::enabled&&race_start_experiment::neoObserver&&!race_start_experiment::aborted)||(runtime_lifecycle::careerAutodrive&&runtime_lifecycle::validationEnabled);}
#pragma comment(linker,"/EXPORT:MWEL_MapOwnerV1=_MWEL_MapOwnerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_MapOwnerV1(unsigned key){return event_maps::active&&event_maps::calibrations.count(key)!=0;}
#pragma comment(linker,"/EXPORT:MWEL_NeoDriverOwnerV1=_MWEL_NeoDriverOwnerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoDriverOwnerV1(){return MWELNeoDriverInstalled();}
#pragma comment(linker,"/EXPORT:MWEL_RaceActiveV1=_MWEL_RaceActiveV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RaceActiveV1(){return (runtime_lifecycle::enabled||race_start_experiment::enabled)&&*reinterpret_cast<unsigned*>(0x925E90)==6&&race_start_experiment::OwnedRace(*reinterpret_cast<BYTE**>(0x91E000));}
#pragma comment(linker,"/EXPORT:MWEL_CombatEventV1=_MWEL_CombatEventV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CombatEventV1(unsigned status,unsigned params,mwcombat::EventV1* output,unsigned size){
 if(!output||size!=sizeof(*output)||status!=*reinterpret_cast<unsigned*>(0x91E000)||!status)return 0;
 unsigned currentParams=0;if(!Copy(reinterpret_cast<BYTE*>(status)+0x1968,&currentParams,4)||params!=currentParams)return 0;
 const auto package=event_race_policies::Current();if(!package)return 0;
 const auto mode=mwcombat::Parse(package->authoring.value("combatMode",std::string("none")).c_str());
 if(!mwcombat::Weapons(mode)||!combat_health_runtime::installed)return 0;
 mwcombat::EventV1 event;event.eventKey=MWELNeoCurrentKey();event.mode=mode;event.racerCount=package->authoring.at("racerCount").get<unsigned>();
 event.flags=1;event.epoch=combat_health_runtime::generation;*output=event;return event.eventKey!=0;
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
 }*output=ammo;return 1;
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
bool MWELNeoLoggingEnabled(){return worldLogEnabled;}
bool MWELNeoDetailedLoggingEnabled(){return neoDiagnosticsEnabled;}
bool MWELGauntletBoostOutput(void* vehicle,float* output){return gauntlet_runtime::Boost(unsigned(vehicle),output);}
bool MWELNeoHoldingAtGenuineGoal(unsigned handle){
 if(!handle||!race_start_experiment::FieldPending())return false;
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
bool MWELNeoCourseEnabled(){auto package=event_race_policies::Current();return package&&mwel::NeoEnabled(package->authoring);}
bool MWELNeoCourseFile(unsigned key,const char* suffix,char* output,unsigned size){
 if(!suffix||!output||!size)return false;auto clone=registration_experiment::Find(key);if(!clone||!clone->race)return false;
 auto asset=std::string("course/")+suffix;
 for(const auto& package:packages)if(package.nativeNamespace==clone->name&&package.assets.contains(asset)){
  auto path=(package.directory/mwel::fs::u8path(asset)).string();if(path.size()>=size)return false;memcpy(output,path.c_str(),path.size()+1);return true;
 }return false;
}
bool MWELNeoSettingsFile(char* output,unsigned size){auto path=mwel_paths::Settings(game).string();if(!output||path.size()>=size)return false;memcpy(output,path.c_str(),path.size()+1);return true;}
bool MWELCarbonNativeStartPose(float* pose,const float* source);
void* __cdecl MWELCarbonOriginalCollection(unsigned classKey,unsigned key){return registration_experiment::findCollection?registration_experiment::findCollection(classKey,key):nullptr;}
const char* MWELCarbonSelection(){
 try{
  auto clone=registration_experiment::Find(MWELNeoCurrentKey());
  if(clone&&clone->race)for(const auto& package:packages)if(package.nativeNamespace==clone->name){
   if(package.authoring.value("combatMode",std::string("none"))=="gauntlet")return "stock";
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
bool MWELCarbonNativeStartPose(float* pose,const float* source){
 // Ordinary eight-car races: preserve the native player's staged position.
 // The adapter uses its actual body pose rather than a W2C-specific location.
 __try{
  auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race||!source||*reinterpret_cast<unsigned*>(race+0x1958)<1||*reinterpret_cast<unsigned*>(race+0x1958)>8)return false;
  unsigned handle=*reinterpret_cast<unsigned*>(race+0x18);
  auto list=*reinterpret_cast<BYTE***>(0x92CD1C);auto count=*reinterpret_cast<unsigned*>(0x92CD24);
  if(!list||count>128)return false;
  for(unsigned i=0;i<count;++i)if(list[i]&&*reinterpret_cast<unsigned*>(list[i])==0x8AA828&&*reinterpret_cast<unsigned*>(list[i]-0x78)==handle){
   NeoBodyMotion body{};if(!NeoReadBody(list[i],body))return false;
   const float radians=std::atan2(-body.heading[0],body.heading[2])-std::atan2(source[4],source[5]);
   const float x=source[12],y=source[13];
   pose[0]=body.position[2]-(x*std::cos(radians)-y*std::sin(radians));
   pose[1]=-body.position[0]-(x*std::sin(radians)+y*std::cos(radians));
   pose[2]=body.position[1];pose[3]=radians*180.f/3.141592654f+90.f;return true;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return false;
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
