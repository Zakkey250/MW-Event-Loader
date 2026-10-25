#include "ProbeRenderer.h"
#include <MinHook.h>
#include <memory>
#include <atomic>
#include <cstdio>
#include <array>
#include <filesystem>
#include <set>
#include <random>
#include <intrin.h>
#include <mmsystem.h>
#include "ScenePcm.h"
#include "../../src/CarbonIntroPolicy.h"
#ifdef MWEL_CARBON_TORCH_RUNTIME
#include "../../src/CarbonParticleFX.h"
void* __cdecl MWELCarbonOriginalCollection(std::uint32_t,std::uint32_t);
#endif
namespace {
HMODULE module=nullptr;std::unique_ptr<carbonprobe::Renderer> renderer;
using EndScene=HRESULT(WINAPI*)(IDirect3DDevice9*);EndScene original=nullptr;
#ifdef MWEL_CARBON_RUNTIME
void* endSceneCallTrampoline=nullptr;
HRESULT WINAPI NativeEndScene(IDirect3DDevice9* device){return device->EndScene();}
HRESULT WINAPI Draw(IDirect3DDevice9* device);
void __declspec(naked) EndSceneCall(){
 // Native caller already pushed the COM device argument. Draw and the live
 // device method retain stdcall cleanup and HRESULT. Other HUD mods keep
 // ownership of the shared D3D9 method entry; no second global detour races
 // their initialization. Our fade now precedes those EndScene HUD overlays.
 __asm {call Draw
  push 0x6E75BD
  ret
 }
}
#endif
std::string sceneDirectory;std::string directory;bool logging=false,failed=false;ULONGLONG started=0,lastLog=0;
void Log(const char*);
#ifndef MWEL_CARBON_RUNTIME
// Private renderer evidence only. No capture path is compiled into the ASI.
std::string captureFolder;bool captureFrames=false;unsigned captureIndex=0;
bool diagnoseHudMask=false;unsigned hudMaskPhase=0;void* diagnosticHud=nullptr;unsigned long long diagnosticHudFeatures=0;
#endif
#ifdef MWEL_CARBON_TORCH_RUNTIME
bool IsTorchScene();bool PrepareSceneTorch();bool UpdateTorchAttachments();void LogTorchDiagnostic(const char*);
#endif
#include "SceneRuntime.h"
#ifdef MWEL_CARBON_TORCH_RUNTIME
bool IsTorchScene(){return renderer&&renderer->HasTorches();}
bool PrepareSceneTorch(){
 if(!IsTorchScene())return true;
 MWELCarbonParticleDiagnosticMode(logging);
 const bool ready=renderer&&renderer->HasTorches()&&MWELCarbonParticlePrepare((scene.assetFolder+"/native-torch.bin").c_str(),MWELCarbonOriginalCollection);
 if(!ready&&logging)MWELNeoLog(MWELCarbonParticleStatus());return ready;
}
void LogTorchDiagnostic(const char* boundary){
 if(!logging||!IsTorchScene())return;const auto s=MWELCarbonParticleDiagnostics();char line[400];
 sprintf_s(line,"CARBON_TORCH boundary=%s created=%u destroyed=%u live=%u create_refused=%u release_refused=%u native_groups=%u native_emitters=%u status=%s",boundary,s.created,s.destroyed,s.live,s.createRefused,s.releaseRefused,s.nativeGroups,s.nativeEmitters,MWELCarbonParticleStatus());MWELNeoLog(line);
 if(s.live){char positions[400];sprintf_s(positions,"CARBON_TORCH_MATRIX boundary=%s max_error=%.6f requested0=%.3f,%.3f,%.3f native0=%.3f,%.3f,%.3f requested1=%.3f,%.3f,%.3f native1=%.3f,%.3f,%.3f",boundary,s.positionError,s.requested[0][0],s.requested[0][1],s.requested[0][2],s.native[0][0],s.native[0][1],s.native[0][2],s.requested[1][0],s.requested[1][1],s.requested[1][2],s.native[1][0],s.native[1][1],s.native[1][2]);MWELNeoLog(positions);}
}
bool UpdateTorchAttachments(){float positions[2][3]{};return renderer&&renderer->TorchPositions(scene.assets.ActorTime(scene.elapsed),scene.world,positions)&&MWELCarbonParticleUpdate(positions);}
#endif
struct PreparedVariant{std::unique_ptr<SceneRuntime> runtime;std::unique_ptr<carbonprobe::Renderer> draw;};
std::vector<PreparedVariant> variants;
bool SelectVariant(unsigned participants,bool advance=false){
 const char* selection="auto";
#ifdef MWEL_CARBON_RUNTIME
 selection=MWELCarbonSelection();
#endif
 if(strcmp(selection,"stock")==0)return false;
 auto matches=[&](const SceneRuntime& candidate){return candidate.enabled&&mwel_carbon::Matches(selection,candidate.variantName,participants,candidate.assets.participants);};
 if((!advance||strcmp(selection,"auto")!=0)&&matches(scene))return true;
 if(scene.playing)return false;
 std::vector<size_t> candidates;
 // Include the current scene in the draw; every compatible user scene has an
 // equal chance, independent of folder ordering or the previous field size.
 if(matches(scene))candidates.push_back(variants.size());
 for(size_t i=0;i<variants.size();++i)if(matches(*variants[i].runtime))candidates.push_back(i);
 if(candidates.empty())return false;
 static std::mt19937 random(GetTickCount()^GetCurrentProcessId());
 const auto choice=candidates[std::uniform_int_distribution<size_t>(0,candidates.size()-1)(random)];
 if(choice<variants.size()){std::swap(scene,*variants[choice].runtime);renderer.swap(variants[choice].draw);}
 return matches(scene);
}
void Log(const char* text){if(!logging)return;
#ifdef MWEL_CARBON_RUNTIME
 MWELNeoLog(text);
#else
 auto file=fopen((directory+"/CarbonIntroProbe.log").c_str(),"ab");if(file){fprintf(file,"%llu %s\n",GetTickCount64(),text);fclose(file);}
#endif
}
void(__cdecl* stageOriginal)(bool);void(__cdecl* doneOriginal)();void(__thiscall* activeOriginal)(void*,bool);
void State(const char* name,int value){if(!logging)return;auto r=*reinterpret_cast<BYTE**>(0x91E000);char line[256];sprintf_s(line,"NATIVE_BOUNDARY %s value=%d race=%p sim=%.3f active=%u timer=%u",name,value,r,*reinterpret_cast<float*>(0x9885D8),r?r[0x198A]:0,r?r[0x1978]:0);Log(line);}
unsigned nativeNisDepth=0;bool pendingSceneBegin=false;void* retainedIntroRace=nullptr;
#ifdef MWEL_CARBON_RUNTIME
std::atomic<bool> nativeCallbacksReady{false};
#endif
void CancelStagedScene(){pendingSceneBegin=false;scene.Cancel();}
void BeginStagedScene(){auto race=*reinterpret_cast<BYTE**>(0x91E000);if(race&&retainedIntroRace==race){Log("SCENE_NATIVE_FALLBACK original_intro_already_played=1 double_playback_prevented=1");return;}if(!race||!SelectVariant(*reinterpret_cast<unsigned*>(race+0x1958))){Log("SCENE_NATIVE_FALLBACK participants_mismatch_or_disabled=1");return;}scene.Begin();if(scene.playing){if(logging){char info[240];sprintf_s(info,"SCENE_BEGIN variant=%s participants=%u native_staging=1 source_camera=1 native_NIS_cars=1",scene.variantName.c_str(),scene.assets.participants);Log(info);}}else Log("SCENE_REFUSED native_NIS_bind_or_pose_failed=1");if(logging){char text[160];sprintf_s(text,"SCENE_BIND error=%u slot=%u table=%08X applied=%u",scene.error,scene.errorSlot,scene.errorTable,scene.applied);Log(text);}}
void __cdecl Stage(bool value){State("staging",value);if(value)CancelStagedScene();else pendingSceneBegin=false;stageOriginal(value);if(value){if(nativeNisDepth){pendingSceneBegin=true;Log("SCENE_BEGIN_DEFERRED native_NIS_cleanup_in_progress=1");}else BeginStagedScene();}}
void __cdecl Done(){pendingSceneBegin=false;State("countdown_done",1);if(scene.playing){if(logging){char text[160];sprintf_s(text,"SCENE_GO_TIMING elapsed=%.4f target=%.4f",scene.elapsed,scene.goAt);Log(text);}scene.Release();Log("SCENE_GO native_callback=1 cars_unbound=1");}doneOriginal();}
void __fastcall Active(void* self,void*,bool value){State("active",value);if(!value)CancelStagedScene();activeOriginal(self,value);}
void(__thiscall* nisPlayOriginal)(void*);
void(__thiscall* hudFeaturesOriginal)(void*,unsigned long long);
void __fastcall HudFeatures(void* self,void*,unsigned long long features){
 bool hidden=scene.hudHidden;auto applied=scene.FilterHud(self,features);
#ifndef MWEL_CARBON_RUNTIME
 if(diagnoseHudMask&&hudMaskPhase==1)applied=0;
#endif
 if(logging&&!hidden&&scene.hudHidden){char text[160];sprintf_s(text,"SCENE_HUD_HIDE elapsed=%.3f show_at=%.3f mask=%016llX",scene.elapsed,scene.hudShowAt,features);Log(text);}
 if(logging&&scene.hudBypass&&scene.hudHidden){char text[160];sprintf_s(text,"SCENE_HUD_RESTORE elapsed=%.3f mask=%016llX",scene.elapsed,features);Log(text);}
 hudFeaturesOriginal(self,applied);
}
void __fastcall NisPlay(void* self,void*){
 // EN1.3 NISActivity::Play / INIS::SkipOverNIS. Let the native sequencer
 // initialize, then complete it through its own skip path before the first
 // rendered frame. Do not omit its cleanup/event tags or alter race release.
 auto n=static_cast<BYTE*>(self);auto r=*reinterpret_cast<BYTE**>(0x91E000);
 bool ordinary=*reinterpret_cast<unsigned*>(n+0x2FC)==0&&!n[0x3E5]&&!n[0x3E6]&&!n[0x3E7]&&!n[0x3E8]&&!n[0x3E9]&&!n[0x314]&&!n[0x354];
 bool requestedSource=false;
#ifdef MWEL_CARBON_RUNTIME
 requestedSource=MWELCarbonReplaceSourceIntro(*reinterpret_cast<unsigned*>(n+0x394));
#endif
 bool replace=!scene.playing&&r&&(ordinary||requestedSource)&&SelectVariant(*reinterpret_cast<unsigned*>(r+0x1958),true);
 if(replace)for(unsigned i=0;i<scene.assets.participants;i++)if(!scene.FindVehicle(*reinterpret_cast<unsigned*>(r+0x18+i*0x194)))replace=false;
#ifdef MWEL_CARBON_RUNTIME
 if(replace){auto source=scene.assets.Car(0,scene.assets.ActorTime(scene.goAt));float pose[4]{};replace=MWELCarbonStartPose(pose,reinterpret_cast<const float*>(&source));
  if(replace){scene.origin={pose[0],pose[1],pose[2]};scene.heading=pose[3];scene.SetStage();replace=scene.ValidateStartRoad();if(!replace)Log("SCENE_NATIVE_FALLBACK source_grid_not_on_drivable_road=1 native_NIS_retained=1");}
 }
#endif
#ifdef MWEL_CARBON_TORCH_RUNTIME
 if(replace&&!PrepareSceneTorch()){replace=false;Log("SCENE_NATIVE_FALLBACK torch_preparation_failed=1");}
#endif
 if(logging){char text[160];sprintf_s(text,"NATIVE_NIS_PLAY type=%u scene=%08X state=%u replace=%u",*reinterpret_cast<unsigned*>(n+0x2FC),*reinterpret_cast<unsigned*>(n+0x394),*reinterpret_cast<unsigned*>(n+0x2F4),unsigned(replace));Log(text);}
 retainedIntroRace=replace?nullptr:r;
 ++nativeNisDepth;
 nisPlayOriginal(self);
 if(replace){
  // A user-selected replacement may skip a protected story START intro.
  // Use the original skip/cleanup path; restore the local restriction after
  // this call and never change global story progress or movie playback.
  BYTE restricted=n[0x3E8];if(requestedSource)n[0x3E8]=0;
  bool skipped=reinterpret_cast<bool(__thiscall*)(void*)>(0x6EA960)(n+0x48);
  if(requestedSource)n[0x3E8]=restricted;if(!skipped)retainedIntroRace=r;
  Log(skipped?"NATIVE_INTRO_SKIPPED completion_tags=1 native_cleanup=1":"NATIVE_INTRO_RETAINED skip_refused=1");}
 --nativeNisDepth;
 // Stage can be dispatched inside Play/Skip before AnimatedCars_UnBind has
 // finished. Acquire Carbon's car controls only after that native cleanup.
 if(!nativeNisDepth&&pendingSceneBegin){pendingSceneBegin=false;BeginStagedScene();}
}
void(__thiscall* countdownOriginal)(void*,float);
void __fastcall Countdown(void* self,void*,float dt){
 scene.Tick();
 if(scene.HoldCountdown()){
  // The native widget consumes real ticks (0x925AE8), independently of Sim.
  // Keep its pending transition fresh; native GO/event dispatch remains intact.
  *reinterpret_cast<unsigned*>(static_cast<BYTE*>(self)+0x48)=*reinterpret_cast<unsigned*>(0x925AE8);return;
 }
 countdownOriginal(self,dt);
}
int(__thiscall* cameraOriginal)(void*,D3DXMATRIX*,float);unsigned cameraLogs=0;
int __fastcall Camera(void* self,void*,D3DXMATRIX* m,float dt){
 scene.Tick();D3DXMATRIX replacement;
 bool replacing=scene.CameraOwned()&&self==reinterpret_cast<void*>(0x986870);
 if(replacing){if(scene.released)scene.BlendRelease(replacement,*m);else scene.CameraMatrix(replacement);m=&replacement;}
 auto r=*reinterpret_cast<BYTE**>(0x91E000);
 if(logging&&r&&r[0x198A]&&cameraLogs++<5){char line[256];sprintf_s(line,"CAMERA self=%p eview_camera=%p matrix_pos=%.3f,%.3f,%.3f dt=%.5f",self,*reinterpret_cast<void**>(0x919650),m->_41,m->_42,m->_43,dt);Log(line);}
 auto result=cameraOriginal(self,m,dt);if(replacing&&!scene.released)scene.Lens();return result;
}
void(__cdecl* commitOriginal)()=reinterpret_cast<void(__cdecl*)()>(0x6E2F50);
void* commitTrampoline=nullptr;
unsigned sceneWorldDraws=0;
void __cdecl DrawWorldActors(){
 // The EN1.3 player colour pass commits scenery here, before vegetation,
 // vehicles, postprocessing and HUD. Other commits include SunSet's R32F
 // prepass, mirrors, shadows and UI: never draw an actor into those surfaces.
 if(failed||!renderer||!scene.enabled||!scene.playing||scene.awaitingCars||scene.skipped)return;
 auto device=*reinterpret_cast<IDirect3DDevice9**>(0x982BDC);auto target=*reinterpret_cast<BYTE**>(0x982A20);
 if(!device||!target||*reinterpret_cast<unsigned*>(target+4)!=1)return;
 IDirect3DSurface9* colour=nullptr;IDirect3DSurface9* depth=nullptr;D3DSURFACE_DESC desc{};
 bool ready=SUCCEEDED(device->GetRenderTarget(0,&colour))&&colour&&SUCCEEDED(colour->GetDesc(&desc))&&desc.Format!=D3DFMT_R32F&&colour==*reinterpret_cast<IDirect3DSurface9**>(target+8)&&SUCCEEDED(device->GetDepthStencilSurface(&depth))&&depth;
 if(colour)colour->Release();if(depth)depth->Release();if(!ready)return;
 try{
  auto camera=*reinterpret_cast<BYTE**>(0x919650);if(!camera)return;
  D3DVIEWPORT9 viewport{};if(FAILED(device->GetViewport(&viewport))||viewport.Width<64||viewport.Height<64)return;
  carbonprobe::WorldActorShader().RefreshLight();
  auto view=reinterpret_cast<D3DXMATRIX*>(camera),projection=reinterpret_cast<D3DXMATRIX*>(camera+64);
  unsigned draws=0;if(scene.assets.hasActor)draws=renderer->DrawFrame(device,renderer->FrameAt(scene.assets.ActorTime(scene.elapsed)),viewport,false,&scene.world,view,projection);
  draws+=sceneCrowd.Draw(device,scene.elapsed,viewport,view,projection);
  draws+=ug2Flare.Draw(device,scene.assets.ActorTime(scene.elapsed),scene.world,viewport,view,projection);sceneWorldDraws+=draws;
  if(logging){static ULONGLONG last=0;auto now=GetTickCount64();if(now-last>=1000){last=now;carbonprobe::SceneLight light;bool native=carbonprobe::ReadSceneLight(light);char text[340];sprintf_s(text,"SCENE_WORLD_RENDER draws=%u before_hud=1 depth=1 target=%u native_light=%u diffuse=%.3f,%.3f,%.3f ambient=%.3f,%.3f,%.3f compressed=%.0f",draws,unsigned(desc.Format),unsigned(native),light.diffuse[0],light.diffuse[1],light.diffuse[2],light.ambient[0],light.ambient[1],light.ambient[2],light.settings[0]);Log(text);}}
 }catch(const std::exception& e){Log(e.what());failed=true;}
}
void __declspec(naked) CommitModels(){
 // Hook this one callsite, not CommitRenderedModels itself: W2C owns a
 // function-level reflection hook. Calling the live entry preserves it.
 __asm {call dword ptr[commitOriginal]
  pushfd
  pushad
  call DrawWorldActors
  popad
  popfd
  push 0x6DED3C
  ret
 }
}
HRESULT WINAPI Draw(IDirect3DDevice9* device){
 if(scene.enabled&&!scene.playing&&!logging)return original(device);
 if(!failed&&renderer&&SUCCEEDED(device->TestCooperativeLevel())){
  try{
   if(!started)started=GetTickCount64();
   D3DVIEWPORT9 full{};device->GetViewport(&full);
   if(full.Width<64||full.Height<64)return original(device);
#ifdef MWEL_CARBON_TORCH_RUNTIME
   if(!IsTorchScene())scene.Tick(false);
#else
   scene.Tick();
#endif
   unsigned draws=sceneWorldDraws,frame=0;sceneWorldDraws=0;
   if(scene.enabled){
    if(scene.playing){
#ifndef MWEL_CARBON_RUNTIME
     // Private inspection shortcut only. Async key transitions are global and
     // can survive a menu/app switch; never consume them in the game adapter.
     if((GetAsyncKeyState(VK_SPACE)&1)&&!scene.released){scene.Skip();Log("SCENE_SKIP native_countdown_retained=1");}
#endif
     frame=renderer->FrameAt(scene.assets.ActorTime(scene.elapsed));
     if(!scene.awaitingCars&&!scene.skipped&&!scene.released)renderer->Fade(device,full,scene.assets.Camera(scene.elapsed).fade);
#ifndef MWEL_CARBON_RUNTIME
     if(diagnoseHudMask&&logging&&scene.elapsed>=scene.goAt+2&&hudMaskPhase==0){
      auto list=*reinterpret_cast<void***>(0x92FD8C);if(list&&*reinterpret_cast<unsigned*>(0x92FD94)==1){diagnosticHud=list[0];diagnosticHudFeatures=*reinterpret_cast<unsigned long long*>(static_cast<BYTE*>(diagnosticHud)+0x18);hudMaskPhase=1;reinterpret_cast<void(__thiscall*)(void*,unsigned long long)>(0x57CDB0)(diagnosticHud,0);Log("PRIVATE_HUD_MASK_ZERO");}
     }
     if(diagnoseHudMask&&hudMaskPhase==1&&scene.elapsed>=scene.goAt+3){hudMaskPhase=2;reinterpret_cast<void(__thiscall*)(void*,unsigned long long)>(0x57CDB0)(diagnosticHud,diagnosticHudFeatures);Log("PRIVATE_HUD_MASK_RESTORED");}
     const float captureAt=captureIndex<2?1.f+captureIndex*4.f:captureIndex<4?scene.goAt+.1f+(captureIndex-2)*1.2f:scene.goAt+2.2f+(captureIndex-4);
     if(captureFrames&&logging&&!scene.awaitingCars&&captureIndex<(diagnoseHudMask?6u:4u)&&scene.elapsed>=captureAt){
      char name[40];sprintf_s(name,"/scene-frame-%u.png",captureIndex++);auto file=captureFolder+name;
      if(GetFileAttributesA(file.c_str())==INVALID_FILE_ATTRIBUTES){IDirect3DSurface9* target=nullptr;
       if(SUCCEEDED(device->GetRenderTarget(0,&target))){auto status=D3DXSaveSurfaceToFileA(file.c_str(),D3DXIFF_PNG,target,nullptr,nullptr);target->Release();Log(SUCCEEDED(status)?"PRIVATE_FRAME_SAVED":"PRIVATE_FRAME_FAILED");}
      }
     }
#endif
    }
   }else{
    D3DVIEWPORT9 inset{full.X+20,full.Y+20,std::min<DWORD>(360,full.Width-20),std::min<DWORD>(440,full.Height-20),0,1};
    frame=unsigned((GetTickCount64()-started)*30/1000)%renderer->FrameCount();draws=renderer->DrawFrame(device,frame,inset);
   }
   if(logging&&GetTickCount64()-lastLog>=1000){lastLog=GetTickCount64();char line[640];sprintf_s(line,"PRIVATE_SCENE frame=%u draws=%u integrated=%u playing=%u elapsed=%.3f released=%u generation=%u bound=%u skipped=%u lifecycle=%u lifecycle_done=%u ground_hits=%u ground_misses=%u actor_z=%.3f stage_z=%.3f camera_lift=%.3f audio_active=%u audio_bytes=%zu rev_mask=%u rev_updates=%u rev_rpm_min=%.1f rev_rpm_max=%.1f",frame,draws,unsigned(scene.enabled),unsigned(scene.playing),scene.elapsed,unsigned(scene.released),scene.generation,unsigned(scene.bound),unsigned(scene.skipped),scene.lifecycleTest,unsigned(scene.lifecycleDone),sceneGround.hits,sceneGround.misses,scene.world._43,scene.stage._43,scene.cameraLift,unsigned(scene.audio.Active()),scene.audio.source.size(),scene.engineRevs.activeMask,scene.engineRevs.updates,scene.engineRevs.minRpm,scene.engineRevs.maxRpm);Log(line);
    if(scene.playing&&scene.bound&&!scene.awaitingCars){
     for(unsigned i=0;i<scene.assets.participants;i++){
      scene.engineRevs.Observe(*reinterpret_cast<void**>(scene.vehicles[i]+4));
      auto body=*reinterpret_cast<void**>(static_cast<BYTE*>(scene.controls[i])+0x44);auto table=*reinterpret_cast<void***>(body);
      auto p=reinterpret_cast<const D3DXVECTOR3*(__thiscall*)(void*)>(table[0x20/4])(body);auto v=reinterpret_cast<const D3DXVECTOR3*(__thiscall*)(void*)>(table[0x24/4])(body);
      auto t=scene.carTargets[i];char state[256];sprintf_s(state,"SCENE_CAR elapsed=%.3f slot=%u pos=%.3f,%.3f,%.3f target=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f clearance=%.3f max_pre_pin_error=%.3f",scene.elapsed,i,p->x,p->y,p->z,t.x,t.y,t.z,v->x,v->y,v->z,scene.carClearance[i],scene.maxPlacementError);Log(state);
     }
    }
   }
  }catch(const std::exception& e){scene.Cancel();failed=true;Log(e.what());}
 }
 return original(device);
}
DWORD WINAPI Init(void*){
 try{
  char path[MAX_PATH]{};GetModuleFileNameA(module,path,MAX_PATH);directory=path;directory.resize(directory.find_last_of("\\/"));
#ifdef MWEL_CARBON_RUNTIME
  auto root=directory.substr(0,directory.find_last_of("\\/"));auto ini=mwel_paths::Settings(root).string();
  logging=MWELNeoLoggingEnabled();
  if(GetPrivateProfileIntA("CarbonIntro","Enabled",1,ini.c_str())!=1)return 0;
  sceneDirectory=mwel_paths::Scenes(root).string();
  std::vector<std::string> folders;std::set<std::string> discovered;
  std::uintmax_t assetBytes=0;
  for(const auto& base:{mwel_paths::Scenes(root)}){
   std::error_code scanError;
   for(const auto& entry:std::filesystem::directory_iterator(base,scanError)){
    const auto id=entry.path().filename().string();
    if(entry.is_symlink()||!entry.is_directory()||!mwel_carbon::SceneId(id)||!discovered.insert(id).second)continue;
    if(folders.size()>=128)break;
    bool ordinary=true;std::uintmax_t bytes=0;
    for(const auto& asset:std::filesystem::directory_iterator(entry.path())){
     if(asset.is_symlink()){ordinary=false;break;}
     if(asset.is_regular_file()){bytes+=asset.file_size();if(asset.file_size()>128ull*1024*1024){ordinary=false;break;}}
    }
    if(!ordinary||assetBytes+bytes>512ull*1024*1024){Log("SCENE_ASSET_BUDGET_REFUSED scene_count_or_bytes=1");continue;}
    assetBytes+=bytes;folders.push_back(entry.path().string());
   }
  }
  std::sort(folders.begin(),folders.end());
  if(folders.empty())return 0;
#else
  auto ini=directory+"/CarbonIntroProbe.ini";
  if(GetPrivateProfileIntA("Probe","Enabled",0,ini.c_str())!=1)return 0;
  char logSetting[32]{};GetPrivateProfileStringA("Probe","Logging","0",logSetting,sizeof(logSetting),ini.c_str());
  logging=strcmp(logSetting,"1")==0;
  char assets[MAX_PATH]{};GetPrivateProfileStringA("Probe","PrivateAssets","",assets,MAX_PATH,ini.c_str());
  if(!*assets){Log("Missing private assets");return 0;}
  char capturePath[MAX_PATH]{};GetPrivateProfileStringA("Probe","CaptureFolder",assets,capturePath,MAX_PATH,ini.c_str());
  captureFolder=capturePath;captureFrames=logging&&GetFileAttributesA((std::string(assets)+"/capture-test").c_str())!=INVALID_FILE_ATTRIBUTES;
  diagnoseHudMask=logging&&GetPrivateProfileIntA("Probe","DiagnoseHudMask",0,ini.c_str())==1;
#endif
  // Address is used only by this isolated EN1.3 lab experiment; host verifies SHA-256.
  auto base=reinterpret_cast<BYTE*>(GetModuleHandle(nullptr));auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew>4096)return 0;
  auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
  if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||reinterpret_cast<uintptr_t>(base)!=0x400000||pe->OptionalHeader.SizeOfImage<0x583000)return 0;
  sceneGround.diagnostics=logging;
#ifdef MWEL_CARBON_RUNTIME
  // A malformed optional scene is isolated, and never disables other packs.
  for(const auto& folder:folders)try{
   PreparedVariant v;v.runtime=std::make_unique<SceneRuntime>();v.draw=std::make_unique<carbonprobe::Renderer>();
   v.runtime->Load(folder,ini);v.draw->Load(folder,"character.cni",v.runtime->assets.hasActor);
   const auto expected=mwel_carbon::Participants(v.runtime->variantName);
   if(expected&&v.runtime->assets.participants!=expected)throw std::runtime_error("Variant field mismatch");
   if(!renderer){std::swap(scene,*v.runtime);renderer=std::move(v.draw);}else variants.push_back(std::move(v));
   Log(folder.c_str());
  }catch(const std::exception& error){Log(error.what());}
  if(!renderer)return 0;
#else
  renderer=std::make_unique<carbonprobe::Renderer>();scene.Load(assets,ini);renderer->Load(assets,"character.cni",scene.assets.hasActor);
#endif
  Log("PRIVATE_PROBE ready");
  if(memcmp(reinterpret_cast<void*>(0x6DED37),"\xE8\x14\x42\x00\x00",5)){Log("CARBON_INTRO_REFUSED world_pass_conflict=1");return 0;}
#ifdef MWEL_CARBON_RUNTIME
  struct Guard{unsigned address;const char* bytes;};
  const Guard guards[]={{0x604050,"\x8A\x44\x24\x04\x88\x81\x8A\x19"},{0x4700D0,"\x55\x8B\xEC\x83\xE4\xF0\x81\xEC"},{0x5679B0,"\x64\xA1\x00\x00\x00\x00\x6A\xFF"},{0x6F5750,"\x6A\xFF\x68\xDF\xEB\x87\x00\x64"}};
  for(auto g:guards)if(memcmp(reinterpret_cast<void*>(g.address),g.bytes,8)){if(logging){char text[96];sprintf_s(text,"CARBON_INTRO_REFUSED conflicting_hook=1 address=%08X",g.address);Log(text);}return 0;}
  const BYTE endSceneGuard[]={0xFF,0x92,0xA8,0,0,0,0xE8};
  if(memcmp(reinterpret_cast<void*>(0x6E75B7),endSceneGuard,sizeof(endSceneGuard))){Log("CARBON_INTRO_REFUSED frame_end_callsite_conflict=1");return 0;}
#endif
  for(unsigned attempt=0;attempt<600;attempt++){
   auto device=*reinterpret_cast<IDirect3DDevice9**>(0x982BDC);
   if(device){auto target=(*reinterpret_cast<void***>(device))[42];
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED){Log("MinHook init failed");return 0;}
    struct H {void* address;void* hook;void** old;};
#ifdef MWEL_CARBON_RUNTIME
    original=NativeEndScene;
    H frameHook{reinterpret_cast<void*>(0x6E75B7),EndSceneCall,&endSceneCallTrampoline};
#else
    H frameHook{target,Draw,reinterpret_cast<void**>(&original)};
#endif
    H hs[]={frameHook,{reinterpret_cast<void*>(0x60DF70),Stage,reinterpret_cast<void**>(&stageOriginal)},{reinterpret_cast<void*>(0x60E160),Done,reinterpret_cast<void**>(&doneOriginal)},{reinterpret_cast<void*>(0x604050),Active,reinterpret_cast<void**>(&activeOriginal)},{reinterpret_cast<void*>(0x4700D0),Camera,reinterpret_cast<void**>(&cameraOriginal)},{reinterpret_cast<void*>(0x5679B0),Countdown,reinterpret_cast<void**>(&countdownOriginal)}};
    H nisHook{reinterpret_cast<void*>(0x6F5750),NisPlay,reinterpret_cast<void**>(&nisPlayOriginal)};
    // Include NIS observation in the same atomic installation as scene hooks.
    std::vector<H> hooks(std::begin(hs),std::end(hs));
#ifdef MWEL_CARBON_RUNTIME
    hooks.erase(std::remove_if(hooks.begin(),hooks.end(),[](const H& h){return h.address==reinterpret_cast<void*>(0x60DF70)||h.address==reinterpret_cast<void*>(0x60E160);}),hooks.end());
#endif
    hooks.push_back(nisHook);
    // Production HUD dispatch is owned by gauntlet_hud and calls MWELCarbonFilterHud.
    hooks.push_back({reinterpret_cast<void*>(0x6DED37),CommitModels,&commitTrampoline});
    unsigned count=scene.enabled||GetPrivateProfileIntA("Probe","TraceBoundaries",0,ini.c_str())==1?unsigned(hooks.size()):1;
    unsigned created=0;bool ok=true;
    for(;created<count;++created)if(MH_CreateHook(hooks[created].address,hooks[created].hook,hooks[created].old)!=MH_OK){ok=false;break;}
    if(ok)for(unsigned i=0;i<count;i++)if(MH_QueueEnableHook(hooks[i].address)!=MH_OK){ok=false;break;}
    if(ok)ok=MH_ApplyQueued()==MH_OK;
    if(!ok){for(unsigned i=0;i<created;i++){MH_DisableHook(hooks[i].address);MH_RemoveHook(hooks[i].address);}Log("Scene hooks rolled back");return 0;}
#ifdef MWEL_CARBON_RUNTIME
    nativeCallbacksReady.store(true,std::memory_order_release);
#endif
#ifdef MWEL_CARBON_RUNTIME
    Log("SCENE_FRAME_END_ACTIVE callsite=006E75B7 shared_D3D9_method_modified=0 live_HUD_chain_preserved=1");
#else
    Log("PRIVATE_PROBE hooked EndScene");
#endif
    return 0;}
   Sleep(100);
  }Log("Device unavailable");
 }catch(const std::exception& e){Log(e.what());}
 return 0;
}
}
#ifndef MWEL_CARBON_RUNTIME
BOOL WINAPI DllMain(HMODULE h,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);auto thread=CreateThread(nullptr,0,Init,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
#endif

