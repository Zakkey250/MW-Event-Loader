#include <atomic>
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
bool MWELNeoLoggingEnabled();
void MWELNeoLog(const char*);
#include "../tools/CarbonIntro/Probe.cpp"

void MWELCancelCarbonIntro(){CancelStagedScene(true);}

void MWELStartCarbonIntro(){
 HMODULE owner=nullptr;
 if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
     reinterpret_cast<LPCSTR>(&MWELStartCarbonIntro),&owner))return;
 module=owner;
 auto thread=CreateThread(nullptr,0,Init,nullptr,0,nullptr);if(thread)CloseHandle(thread);
}

void MWELCarbonStageBefore(bool value){if(!nativeCallbacksReady.load(std::memory_order_acquire))return;State("staging",value);if(value)CancelStagedScene();else pendingSceneBegin=false;}
void MWELCarbonStageAfter(bool value){if(!value||!nativeCallbacksReady.load(std::memory_order_acquire))return;MWELCarbonGridSnapshot("native_stage_after");if(nativeNisDepth){pendingSceneBegin=true;Log("SCENE_BEGIN_DEFERRED native_NIS_cleanup_in_progress=1");}else BeginStagedScene();}
void MWELCarbonDoneBefore(){if(!nativeCallbacksReady.load(std::memory_order_acquire))return;MWELCarbonGridSnapshot("native_go_before");pendingSceneBegin=false;State("countdown_done",1);if(scene.playing){if(logging){char text[160];sprintf_s(text,"SCENE_GO_TIMING elapsed=%.4f target=%.4f",scene.elapsed,scene.goAt);Log(text);}scene.Release();Log("SCENE_GO native_callback=1 cars_unbound=1");}}

unsigned long long MWELCarbonFilterHud(void* self,unsigned long long features){
 if(!nativeCallbacksReady.load(std::memory_order_acquire))return features;
 const bool hidden=scene.hudHidden;auto applied=scene.FilterHud(self,features);
 if(logging&&!hidden&&scene.hudHidden){char text[160];sprintf_s(text,"SCENE_HUD_HIDE elapsed=%.3f show_at=%.3f mask=%016llX",scene.elapsed,scene.hudShowAt,features);Log(text);}
 return applied;
}
