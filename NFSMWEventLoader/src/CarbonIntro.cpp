#include <atomic>
#include "SceneRoadPath.h"
// The production adapter and private scene harness share the accepted playback.
#define MWEL_CARBON_RUNTIME
#define MWEL_CARBON_VARIANTS
#ifndef MWEL_CARBON_TORCH_RUNTIME
#define MWEL_CARBON_TORCH_RUNTIME
#endif
bool MWELCarbonStartPose(float*,const float*);
float MWELCarbonGestureGo(const char*,unsigned,float,float,float);
bool MWELCarbonUsesAuthoredPose();
void MWELCarbonGridSnapshot(const char*);
#include "../../integration/EventLoaderPaths.h"
bool MWELCarbonReplaceSourceIntro(unsigned);
const char* MWELCarbonSelection();
bool MWELCarbonRollingStartAllowed();
bool MWELCarbonLaunchOnly();
bool MWELCarbonRoadGraph(mwel_scene_road::Graph&);
bool MWELRpdLiveIntroAvailable();
bool MWELRpdLiveIntroCurrent();
bool MWELRpdIntroRequired();
void MWELRpdIntroFailed(const char*);
bool MWELRpdPrepareLiveGrid();
bool MWELRpdLiveIntroSample(float*,float*,unsigned*);
bool MWELNeoLoggingEnabled();
void MWELNeoLog(const char*);
void MWELRpdIntroMusicFrame(unsigned,unsigned,float,float,float,bool);
void MWELRpdIntroMusicGo();
void MWELRpdIntroMusicEnd();
#include "../tools/CarbonIntro/Probe.cpp"
bool MWELCarbonOwnsAudioSurface(){
 const auto site=reinterpret_cast<const BYTE*>(0x694160);
 return sceneAudioOriginal&&site[0]==0xE9&&reinterpret_cast<unsigned>(site+5)+*reinterpret_cast<const int*>(site+1)==reinterpret_cast<unsigned>(&SceneAudioInfo);
}

void MWELCancelCarbonIntro(){rpdLiveIntro.Cancel();CancelStagedScene(true);}

#pragma comment(linker,"/EXPORT:MWEL_WorldLoadPlanV1=_MWEL_WorldLoadPlanV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_WorldLoadPlanV1(mw_world_load::Plan* output,unsigned bytes){
 if(!output||bytes!=sizeof(*output))return mw_world_load::Invalid;
 // Public W2C holds native NIS::Play until flow 6. On a cold start, Play
 // can be the callback that stages the replacement and publishes its plan.
 // Let W2C activate the loaded start grid, then submit that scene's plan.
 if(!scene.playing||scene.released||!scene.race||scene.race!=*reinterpret_cast<BYTE**>(0x91E000))
  return mw_world_load::Inactive;
 if(!scene.worldLoadPlanValid)return mw_world_load::Invalid;
 *output=scene.worldLoadPlan;return mw_world_load::Ready;
}
#pragma comment(linker,"/EXPORT:MWEL_WorldPresentationReadyV1=_MWEL_WorldPresentationReadyV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_WorldPresentationReadyV1(){
 if(!scene.playing||!scene.race||scene.race!=*reinterpret_cast<BYTE**>(0x91E000))return 0;
 if(scene.released||scene.skipped)return 1; // Native gameplay camera can complete a skipped preparation.
 return !scene.awaitingCars&&scene.cameraApplied?3u:0u; // bit 0 prepared, bit 1 authored source camera
}
#pragma comment(linker,"/EXPORT:MWEL_WorldSceneGenerationV1=_MWEL_WorldSceneGenerationV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_WorldSceneGenerationV1(){
 return scene.playing&&scene.race&&scene.race==*reinterpret_cast<BYTE**>(0x91E000)?scene.generation:0;
}

void MWELStartCarbonIntro(){
 HMODULE owner=nullptr;
 if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
     reinterpret_cast<LPCSTR>(&MWELStartCarbonIntro),&owner))return;
 module=owner;
 auto thread=CreateThread(nullptr,0,Init,nullptr,0,nullptr);if(thread)CloseHandle(thread);
}

void MWELCarbonStageBefore(bool value){if(!nativeCallbacksReady.load(std::memory_order_acquire))return;State("staging",value);if(value){rpdLiveIntro.Cancel();CancelStagedScene();}else pendingSceneBegin=false;}
void MWELCarbonStageAfter(bool value){if(!value)return;if(!nativeCallbacksReady.load(std::memory_order_acquire)){MWELRpdIntroFailed("required_intro_hooks_unavailable");return;}MWELCarbonGridSnapshot("native_stage_after");const auto selection=MWELCarbonSelection();const bool original=!selection||!*selection||!strcmp(selection,"stock")||!strcmp(selection,"auto");if(!MWELRpdIntroRequired()&&MWELRpdLiveIntroCurrent()&&original){if(nativeNisDepth)pendingRpdBegin=true;else if(MWELRpdPrepareLiveGrid())rpdLiveIntro.Begin();return;}if(nativeNisDepth){pendingSceneBegin=true;Log("SCENE_BEGIN_DEFERRED native_NIS_cleanup_in_progress=1");}else BeginStagedScene();}
void MWELCarbonDoneBefore(){if(!nativeCallbacksReady.load(std::memory_order_acquire))return;rpdLiveIntro.Go();MWELCarbonGridSnapshot("native_go_before");pendingSceneBegin=false;State("countdown_done",1);if(scene.playing){if(logging){char text[160];sprintf_s(text,"SCENE_GO_TIMING elapsed=%.4f target=%.4f",scene.elapsed,scene.goAt);Log(text);}scene.Release();Log("SCENE_GO native_callback=1 cars_unbound=1");}}
void MWELCarbonDoneAfter(){if(nativeCallbacksReady.load(std::memory_order_acquire))scene.CompleteRollingRelease();}

unsigned long long MWELCarbonFilterHud(void* self,unsigned long long features){
 if(!nativeCallbacksReady.load(std::memory_order_acquire))return features;
 const bool hidden=scene.hudHidden;auto applied=scene.FilterHud(self,features);
 if(logging&&!hidden&&scene.hudHidden){char text[160];sprintf_s(text,"SCENE_HUD_HIDE elapsed=%.3f show_at=%.3f mask=%016llX",scene.elapsed,scene.hudShowAt,features);Log(text);}
 return applied;
}
