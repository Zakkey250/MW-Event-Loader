#pragma once
#include "SceneAssets.h"
#include "SceneGround.h"
#include "SceneCrowd.h"
#include "Ug2Flare.h"
#include "SceneWave.h"
#include "SceneEngineRevs.h"
// Carbon scene adapter. Native staging/countdown owns race release.
struct SceneRuntime {
 bool enabled=false,playing=false,released=false,skipped=false;BYTE* race=nullptr;
 float begun=0,elapsed=0,last=0;unsigned generation=0;D3DXMATRIX world{},stage{};
 SceneAssets assets;SceneWave audio;SceneEngineRevs engineRevs;std::string variantName,assetFolder;D3DXVECTOR3 origin{};float heading=0;D3DXMATRIX actorLocator{};
 std::vector<D3DXVECTOR3> roots;float groundOffset=0,cameraLift=0;
 BYTE* vehicles[8]{};void* controls[8]{};unsigned handles[8]{};bool bound=false;unsigned applied=0;
 unsigned error=0,errorSlot=0,errorTable=0;bool awaitingCars=false;float carsWaitStarted=0;
 unsigned readyHandles[8]{};BYTE* readyVehicles[8]{};void* readyOwners[8]{};float readySince=-1;
 float waitTraceAt=-1;
 unsigned nextCue=0;
 float carClearance[8]{};bool countdownControl=false;
 float maxPlacementError=0;D3DXVECTOR3 carTargets[8]{};
 bool lensOwned=false;unsigned short savedFov=0;
 void* hud=nullptr;unsigned long long hudFeatures=0;bool hudHidden=false,hudBypass=false;float hudShowAt=0;
 bool safeTail=false;float releasedAt=0;D3DXMATRIX lastView{},releaseView{};unsigned short releaseFov=0;
 float inspectAt=0,inspectUntil=0,inspectHold=12;bool inspected=false;unsigned lifecycleTest=0;bool lifecycleDone=false;
 static float Ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
 float TailBlend()const{return safeTail?Ease((elapsed-cameraTailAt)/3.f):0;}
 bool CameraOwned()const{return playing&&!awaitingCars&&!skipped&&(!released||(safeTail&&elapsed-releasedAt<1.f));}
 void Release(){releaseView=lastView;releaseFov=*reinterpret_cast<unsigned short*>(0x986870+0xC4);releasedAt=elapsed;Unbind();RestoreView();released=true;}
 // Camera timing remains the accepted composition. The FGIRL02 palm reaches
 // first contacts the ground at frame 291 / 15 Hz. Frame 323 is a later
 // settled pose and must not be mistaken for the starting gesture.
 float cameraTailAt=13.001500f,goAt=291.f/15.f;
 float countdownAt=goAt-3.f,hideAt=27.0f;
 void Load(const std::string& folder,const std::string& ini){
#ifndef MWEL_CARBON_RUNTIME
  if(GetPrivateProfileIntA("Probe","IntegratedScene",0,ini.c_str())!=1)return;
#endif
  assets.Load(folder);
  assetFolder=folder;
  variantName=folder.substr(folder.find_last_of("\\/")+1);
  D3DXMatrixRotationZ(&actorLocator,-D3DX_PI/2);actorLocator._41=.07455066f;actorLocator._42=-6.06879187f;
  std::ifstream locatorFile(folder+"/actor-locator.bin",std::ios::binary);
  if(locatorFile){SceneAssets::Read(locatorFile,&actorLocator);char extra;if(locatorFile.read(&extra,1))throw std::runtime_error("Trailing actor locator");
   for(float v:*reinterpret_cast<float(*)[16]>(&actorLocator))if(!std::isfinite(v))throw std::runtime_error("Invalid actor locator");
   if(std::abs(actorLocator._14)>.001f||std::abs(actorLocator._24)>.001f||std::abs(actorLocator._34)>.001f||std::abs(actorLocator._44-1)>.001f)throw std::runtime_error("Invalid actor matrix");
  }else if(assets.hasActor&&!assets.actorClock.empty())throw std::runtime_error("Variant actor locator required");
  std::ifstream timing(folder+"/scene-timing.bin",std::ios::binary);
  if(timing){unsigned h[2];float t[3];SceneAssets::Read(timing,h,2);SceneAssets::Read(timing,t,3);
   const float countdownOverlap=3.f; // Timing belongs to the pack, not its folder name.
   if(h[0]!=0x31544943||h[1]!=assets.participants||!std::isfinite(t[0])||!std::isfinite(t[1])||!std::isfinite(t[2])||t[0]<0||t[1]<=3||t[0]>=t[1]-3+countdownOverlap||t[1]>=t[2]||t[2]>assets.duration+.01f)throw std::runtime_error("Invalid scene timing");
   cameraTailAt=t[0];goAt=t[1];countdownAt=goAt-3;hideAt=t[2];
  }else if(assets.participants!=8)throw std::runtime_error("Variant timing required");
  // Reveal at the last authored blackout before countdown, not at the first
  // camera cut. Clips without that fade retain native countdown visibility.
  // The native countdown widget stops ticking when its feature is hidden.
  // UG2 can overlap its final source camera with countdown: reveal the native
  // widget at countdownAt even while retaining that last original camera.
  hudShowAt=countdownAt;bool inFade=false;float fadeStart=0;
  for(unsigned i=0;i<assets.cameras.size()&&float(i)/assets.rate<countdownAt;i++){
   const bool dark=assets.cameras[i].fade>=50.f;
   if(dark&&!inFade){fadeStart=float(i)/assets.rate;if(fadeStart>0)hudShowAt=fadeStart;}inFade=dark;
  }
#ifdef MWEL_CARBON_RUNTIME
  safeTail=true;
#else
  safeTail=GetPrivateProfileIntA("Probe","SafeCountdownCamera",0,ini.c_str())==1;
  inspectAt=GetPrivateProfileIntA("Probe","InspectSceneSecond",0,ini.c_str());if(inspectAt<1||inspectAt>15)inspectAt=0;
  inspectHold=float(std::clamp(GetPrivateProfileIntA("Probe","InspectHoldSeconds",12,ini.c_str()),1u,60u));
  lifecycleTest=GetPrivateProfileIntA("Probe","LifecycleTest",0,ini.c_str());if(lifecycleTest>3)lifecycleTest=0;
#endif
  if(assets.hasActor){
   std::ifstream rootFile(folder+"/root-path.bin",std::ios::binary);unsigned count;SceneAssets::Read(rootFile,&count);if(count<2||count>1500)throw std::runtime_error("Character root mismatch");roots.resize(count);SceneAssets::Read(rootFile,roots.data(),count);for(auto p:roots)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Invalid character root");for(float t:assets.actorClock)if(t>(count-1)/15.f+.01f)throw std::runtime_error("Actor clock exceeds motion");
  }else{roots.assign(2,D3DXVECTOR3(0,0,0));D3DXMatrixIdentity(&actorLocator);}
#ifndef MWEL_CARBON_RUNTIME
  char pose[160]{};GetPrivateProfileStringA("Probe","StartPose","",pose,sizeof(pose),ini.c_str());
  if(sscanf_s(pose,"%f,%f,%f,%f",&origin.x,&origin.y,&origin.z,&heading)!=4)throw std::runtime_error("Test start pose missing");
#endif
  audio.Load(folder,hideAt);
  engineRevs.Load(folder,assets.participants,assets.rate,unsigned(assets.cameras.size()));
  if(!audio.source.empty())for(auto& sample:assets.cameras)if(sample.timeScale!=100)throw std::runtime_error("Scene WAV requires unretimed source clock");
  SetStage();enabled=true;
 }
 void SetStage(){
  // Source AnimLocator: scene angle minus 90 degrees at the start line.
  D3DXMatrixRotationZ(&stage,(heading-90)*D3DX_PI/180.f);stage._41=origin.x;stage._42=origin.y;stage._43=origin.z;
  world=actorLocator*stage;
 }
 static bool Ground(const D3DXVECTOR3& map,float& height){
  return sceneGround.Query(map,height);
 }
 void GroundActor(){
  if(!assets.hasActor)return;
  D3DXVECTOR3 pelvis;D3DXVec3TransformCoord(&pelvis,&roots[Frame()],&world);float height;
  if(Ground(pelvis,height))world._43=height+.05f;
 }
 BYTE* FindVehicle(unsigned handle)const{
  auto list=*reinterpret_cast<BYTE***>(0x92CD1C);unsigned count=*reinterpret_cast<unsigned*>(0x92CD24);
  if(!list||count>128)return nullptr;
  for(unsigned i=0;i<count;i++)if(list[i]&&*reinterpret_cast<unsigned*>(list[i])==0x8AA828&&*reinterpret_cast<unsigned*>(list[i]-0x78)==handle)return list[i];
  return nullptr;
 }
 static bool ValidControl(void* control){
  // The INISCarControl tail can remain readable after the enclosing behavior
  // is freed. SetNISPosition first dispatches through the IServiceable head
  // 90h before it, so checking only the interface vtable is insufficient.
  __try{return control&&*static_cast<unsigned*>(control)==0x8ABD88&&*reinterpret_cast<unsigned*>(static_cast<BYTE*>(control)-0x90)==0x8ABE40;}
  __except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
 static unsigned ControlHead(void* control){__try{return control?*reinterpret_cast<unsigned*>(static_cast<BYTE*>(control)-0x90):0;}__except(EXCEPTION_EXECUTE_HANDLER){return ~0u;}}
 void TraceCancel(const char* reason){
#ifdef MWEL_CARBON_RUNTIME
  if(logging){char note[256];sprintf_s(note,"SCENE_CANCEL_REASON reason=%s error=%u slot=%u race=%p live_race=%p flow=%u mode=%u elapsed=%.5f begun=%.5f now=%.5f",reason,error,errorSlot,race,*reinterpret_cast<void**>(0x91E000),*reinterpret_cast<unsigned*>(0x925E90),race?*reinterpret_cast<unsigned*>(race+0x1960):~0u,elapsed,begun,*reinterpret_cast<float*>(0x9885D8));MWELNeoLog(note);}
#endif
 }
 void TraceWait(float now){
#ifdef MWEL_CARBON_RUNTIME
  if(!logging||now-carsWaitStarted<.25f||(waitTraceAt>=0&&now-waitTraceAt<1.f))return;
  waitTraceAt=now;auto slot=std::min(errorSlot,assets.participants-1);
  auto handle=*reinterpret_cast<unsigned*>(race+0x18+slot*0x194);auto vehicle=FindVehicle(handle);
  char note[320];sprintf_s(note,"SCENE_WAIT variant=%s seconds=%.3f error=%u slot=%u handle=%u vehicle=%p ready_vehicle=%p table=%08X ready_age=%.3f bound=%u",variantName.c_str(),now-carsWaitStarted,error,slot,handle,vehicle,readyVehicles[slot],errorTable,readySince>=0?now-readySince:-1.f,unsigned(bound));MWELNeoLog(note);
#endif
 }
 void StopRevs(){
  for(unsigned i=0;i<assets.participants;i++)if(vehicles[i]&&FindVehicle(handles[i])==vehicles[i])engineRevs.Stop(i,*reinterpret_cast<void**>(vehicles[i]+4));
  engineRevs.Forget();
 }
 void Unbind(){
  StopRevs();
  if(!bound)return;
  for(unsigned i=0;i<assets.participants;i++)if(vehicles[i]&&FindVehicle(handles[i])==vehicles[i]){
   auto owner=*reinterpret_cast<void**>(vehicles[i]+4);
   auto live=owner?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x445650):nullptr;
   if(controls[i]&&live==controls[i]&&ValidControl(live)){reinterpret_cast<void(__thiscall*)(void*,float)>(0x6AE0D0)(live,0);reinterpret_cast<void(__thiscall*)(void*)>(0x6A0100)(live);}
   reinterpret_cast<void(__thiscall*)(void*,bool)>(0x686410)(vehicles[i],false);
  }
  bound=false;
 }
 void RestoreHud(){
  if(hudHidden){
   auto list=*reinterpret_cast<void***>(0x92FD8C);auto count=*reinterpret_cast<unsigned*>(0x92FD94);
   hudBypass=true;
   if(list&&count<8)for(unsigned i=0;i<count;i++)if(list[i]==hud)reinterpret_cast<void(__thiscall*)(void*,unsigned long long)>(0x57CDB0)(hud,hudFeatures);
   hudBypass=false;
   hudHidden=false;hud=nullptr;
  }
 }
 void RestoreView(){if(lensOwned){*reinterpret_cast<unsigned short*>(0x986870+0xC4)=savedFov;lensOwned=false;}RestoreHud();}
 void ClearVehicles(bool preserveReadiness=false){for(unsigned i=0;i<8;++i){vehicles[i]=nullptr;controls[i]=nullptr;handles[i]=0;if(!preserveReadiness){readyHandles[i]=0;readyVehicles[i]=nullptr;readyOwners[i]=nullptr;}}bound=false;if(!preserveReadiness)readySince=-1;}
 void Cancel(){
  audio.Stop();
  sceneCrowd.Clear();
  ug2Flare.Clear();
#ifdef MWEL_CARBON_TORCH_RUNTIME
  MWELCarbonParticleRelease();LogTorchDiagnostic("cancel_or_skip");
#endif
  Unbind();RestoreView();ClearVehicles();playing=false;released=false;skipped=false;race=nullptr;nextCue=0;countdownControl=false;inspectUntil=0;hud=nullptr;hudFeatures=0;hudHidden=false;awaitingCars=false;carsWaitStarted=0;}
 void Skip(){
  StopRevs();
  audio.Stop();
  sceneCrowd.Clear();
  ug2Flare.Clear();
#ifdef MWEL_CARBON_TORCH_RUNTIME
  MWELCarbonParticleRelease();LogTorchDiagnostic("cancel_or_skip");
#endif
  elapsed=countdownAt;skipped=true;RestoreView();Cars(0);}
 bool HideHud()const{return playing&&!released&&!skipped&&elapsed<hudShowAt;}
 unsigned long long FilterHud(void* self,unsigned long long features){
  if(!hudBypass&&HideHud()){if(features){hudFeatures=features;hud=self;hudHidden=true;}return 0;}
  return features;
 }
 void Hud(void* self){
  if(HideHud()){
   auto features=*reinterpret_cast<unsigned long long*>(static_cast<BYTE*>(self)+0x18);
   if(features){hudFeatures=features;hud=self;hudHidden=true;reinterpret_cast<void(__thiscall*)(void*,unsigned long long)>(0x57CDB0)(self,0);}
  }else if(hudHidden)RestoreHud();
 }
 void Lens(){
  if(!lensOwned){savedFov=*reinterpret_cast<unsigned short*>(0x986870+0xC4);lensOwned=true;}
  // Carbon 47A7A0 / 47FC79: 2 * bATan(focal length, 15.96 mm).
  auto lens=assets.Camera(elapsed).lens;
  auto fov=2*reinterpret_cast<unsigned short(__cdecl*)(float,float)>(0x45DED0)(lens,15.96f);
  *reinterpret_cast<unsigned short*>(0x986870+0xC4)=static_cast<unsigned short>(fov+(savedFov-fov)*TailBlend());
 }
 bool FieldStable(){
  // Manual failure replaced slot 1 within the first 25 ms after staging.
  // Observing the whole field across simulation ticks lets native teardown
  // finish before installing Carbon's suspension behaviors.
  const float now=*reinterpret_cast<float*>(0x9885D8);bool changed=false;
  for(unsigned i=0;i<assets.participants;++i){
   errorSlot=i;auto handle=*reinterpret_cast<unsigned*>(race+0x18+i*0x194);auto vehicle=FindVehicle(handle);
   if(!handle||!vehicle){readySince=-1;return false;}
   auto owner=*reinterpret_cast<void**>(vehicle+4);if(!owner){readySince=-1;return false;}
   changed=changed||readyHandles[i]!=handle||readyVehicles[i]!=vehicle||readyOwners[i]!=owner;
   readyHandles[i]=handle;readyVehicles[i]=vehicle;readyOwners[i]=owner;
  }
  if(changed||readySince<0){readySince=now;return false;}
  return now-readySince>=.05f;
 }
 bool Bind(){
  // Restart dispatches staging before all replacement opponents have entered
  // the live vehicle list. Do not partially change their behaviors.
  if(!FieldStable())return false;
  for(unsigned i=0;i<assets.participants;i++){vehicles[i]=nullptr;controls[i]=nullptr;handles[i]=0;}bound=true;
  for(unsigned i=0;i<assets.participants;i++){
   errorSlot=i;handles[i]=*reinterpret_cast<unsigned*>(race+0x18+i*0x194);vehicles[i]=FindVehicle(handles[i]);if(!vehicles[i]){error=1;return false;}
   // SetAnimating installs the native SuspensionSpline behavior and its
   // INISCarControl interface; ordinary driving does not expose it.
   reinterpret_cast<void(__thiscall*)(void*,bool)>(0x686410)(vehicles[i],true);
   auto owner=*reinterpret_cast<void**>(vehicles[i]+4);if(!owner){error=2;return false;}
   controls[i]=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x445650);
   errorTable=controls[i]?*reinterpret_cast<unsigned*>(controls[i]):0;
   if(!ValidControl(controls[i])){error=3;return false;}
  }
  bound=true;applied=0;
  return true;
 }
 void Cars(float dt){
  if(!bound||released)return;
  bool renewed[8]{};
  // Native CAnimScene re-queries INISCarControl on every animated-car update.
  // NIS cleanup/behavior changes can replace it without replacing IVehicle.
  for(unsigned i=0;i<assets.participants;++i){
   errorSlot=i;
   if(!vehicles[i]||FindVehicle(handles[i])!=vehicles[i]){error=7;TraceCancel("vehicle_changed_waiting");Unbind();ClearVehicles();awaitingCars=true;return;}
   auto owner=*reinterpret_cast<void**>(vehicles[i]+4);
   auto live=owner?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x445650):nullptr;
   if(!ValidControl(live)&&!vehicles[i][0xB0]){
    renewed[i]=true;
    // A late native NIS UnBind retired its spline. This scene still owns
    // staging; install a fresh spline instead of touching the retired tail.
    reinterpret_cast<void(__thiscall*)(void*,bool)>(0x686410)(vehicles[i],true);
    owner=*reinterpret_cast<void**>(vehicles[i]+4);
    live=owner?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x445650):nullptr;
   }
   if(!ValidControl(live)){
#ifdef MWEL_CARBON_RUNTIME
    if(logging&&error!=8){char note[256];sprintf_s(note,"SCENE_CONTROL_PENDING slot=%u handle=%u vehicle=%p old=%p live=%p parent_table=%08X animating=%u elapsed=%.5f",i,handles[i],vehicles[i],controls[i],live,ControlHead(live),unsigned(vehicles[i][0xB0]),elapsed);MWELNeoLog(note);}
#endif
    StopRevs();error=8;awaitingCars=true;return;
   }
   renewed[i]=renewed[i]||live!=controls[i];controls[i]=live;
  }
  while(nextCue<assets.cues.size()&&assets.cues[nextCue].time<=elapsed){auto c=assets.cues[nextCue++];reinterpret_cast<void(__thiscall*)(void*,float)>(0x6AE0D0)(controls[c.slot],c.speed);}
  for(unsigned i=0;i<assets.participants;i++){
   auto render=assets.Car(i,assets.ActorTime(elapsed))*stage;D3DXMATRIX physical;
   D3DXVECTOR3 point(render._41,render._42,stage._43),normal;float floor=0;
   const bool grounded=sceneGround.Query(point,floor,&normal);
   if(grounded){
    // Retain the authored horizontal heading, conform pitch/roll to this car's
    // road plane. Do not keep all eight cars on the start gate's flat plane.
    D3DXVECTOR3 axis(render._11,render._12,0),side;
    axis.z=-(normal.x*axis.x+normal.y*axis.y)/normal.z;
    D3DXVec3Normalize(&axis,&axis);D3DXVec3Cross(&side,&normal,&axis);D3DXVec3Normalize(&side,&side);
    render._11=axis.x;render._12=axis.y;render._13=axis.z;
    render._21=side.x;render._22=side.y;render._23=side.z;
    render._31=normal.x;render._32=normal.y;render._33=normal.z;
    render._43=floor+(applied?carClearance[i]:.35f);
   }
   reinterpret_cast<void(__cdecl*)(D3DXMATRIX*,const D3DXMATRIX*)>(0x45E1A0)(&physical,&render);
   bool ok=reinterpret_cast<bool(__thiscall*)(void*,const D3DXMATRIX*,bool,float)>(0x691110)(controls[i],&physical,applied==0||renewed[i],dt);
   if(!ok&&grounded){
    // Native Warp can decline the initial NIS snap even on a validated road.
    // Retain the live suspension interface, update its spline without another
    // warp, then synchronize this exact rigid body to the verified road plane
    // below. Cancelling after a failed Warp leaves a car falling off its grid.
    ok=reinterpret_cast<bool(__thiscall*)(void*,const D3DXMATRIX*,bool,float)>(0x691110)(controls[i],&physical,false,dt);
    if(ok&&logging&&!applied){char note[160];sprintf_s(note,"SCENE_NATIVE_WARP_RECOVERED slot=%u exact_body_sync=1 verified_road=1",i);MWELNeoLog(note);}
   }
   if(!ok){error=4;errorSlot=i;TraceCancel("native_position_rejected");Cancel();return;}
   if(!skipped&&elapsed<countdownAt)engineRevs.Apply(i,*reinterpret_cast<void**>(vehicles[i]+4),elapsed);
   if(grounded){
    // SetNISPosition deliberately retains live vertical physics after its
    // first call. On an incline that lets the staged cars slide/fall while
    // the camera is elsewhere. Pin only during the authored NIS; release
    // before native countdown so throttle/launch control works normally.
    auto body=*reinterpret_cast<void**>(static_cast<BYTE*>(controls[i])+0x44);
    auto table=*reinterpret_cast<void***>(body);
    auto pos=reinterpret_cast<const D3DXVECTOR3*(__thiscall*)(void*)>(table[0x20/4])(body);
    if(!applied){float clearance=pos->y-floor;carClearance[i]=(clearance>=.1f&&clearance<=1.5f)?clearance:.35f;physical._42=floor+carClearance[i];}
    D3DXVECTOR3 target(physical._41,physical._42,physical._43),zero(0,0,0);
    auto drift=target-*pos;if(sceneGround.diagnostics){maxPlacementError=std::max(maxPlacementError,D3DXVec3Length(&drift));carTargets[i]=target;}
    reinterpret_cast<void(__thiscall*)(void*,const D3DXVECTOR3*)>(table[0x5C/4])(body,&target);
    reinterpret_cast<void(__thiscall*)(void*,const D3DXVECTOR3*)>(table[0x60/4])(body,&zero);
    reinterpret_cast<void(__thiscall*)(void*,const D3DXVECTOR3*)>(table[0x64/4])(body,&zero);
    reinterpret_cast<void(__thiscall*)(void*,const D3DXMATRIX*)>(table[0x70/4])(body,&physical);
   }
  }
  ++applied;
 }
 bool ValidateStartRoad(){
  float h=0;const float base=Ground(origin,h)?h:origin.z;
  for(unsigned i=0;i<assets.participants;++i){
   auto m=assets.Car(i,assets.ActorTime(0))*stage;
   if(!Ground({m._41,m._42,base},h)||std::abs(h-base)>3)return false;
  }
  return true;
 }
 void Begin(){
  Cancel();if(!enabled)return;auto r=*reinterpret_cast<BYTE**>(0x91E000);
  if(!r||*reinterpret_cast<unsigned*>(r+0x1958)!=assets.participants)return;
#ifdef MWEL_CARBON_RUNTIME
  auto source=assets.Car(0,assets.ActorTime(goAt));float pose[4]{};if(!MWELCarbonStartPose(pose,reinterpret_cast<const float*>(&source))){error=5;return;}
  origin={pose[0],pose[1],pose[2]};heading=pose[3];SetStage();
  if(logging){char note[200];sprintf_s(note,"SCENE_STAGE_POSE origin=%.3f,%.3f,%.3f heading=%.3f source0=%.3f,%.3f,%.3f",origin.x,origin.y,origin.z,heading,source._41,source._42,source._43);MWELNeoLog(note);}
#endif
#ifdef MWEL_CARBON_TORCH_RUNTIME
  if(!PrepareSceneTorch()){error=6;return;}
#endif
  race=r;last=begun=*reinterpret_cast<float*>(0x9885D8);elapsed=0;nextCue=0;error=0;playing=true;skipped=false;inspectUntil=0;inspected=false;countdownControl=false;maxPlacementError=0;waitTraceAt=-1;engineRevs.updates=0;engineRevs.minRpm=engineRevs.maxRpm=0;++generation;
  float height;if(Ground(origin,height)){groundOffset=height-origin.z;stage._43=height;world._43=height;}
  if(!Bind()){Unbind();ClearVehicles(true);awaitingCars=true;carsWaitStarted=last;return;}Cars(0);
  GroundActor();
  PrepareCrowd();
#ifdef MWEL_CARBON_TORCH_RUNTIME
  if(IsTorchScene()&&(!MWELCarbonParticleBegin()||!UpdateTorchAttachments())){error=6;Cancel();return;}
  LogTorchDiagnostic("begin");
#endif
 }
 void PrepareCrowd(){
  if(sceneCrowd.prepared)return;
  try{sceneCrowd.Load(assetFolder);sceneCrowd.Place(stage,assets);
   ug2Flare.Load(assetFolder);
   if(logging){char text[200];sprintf_s(text,"SCENE_CROWD source=%u placed=%u relocated=%u rejected=%u cpu_bytes=%zu",unsigned(sceneCrowd.actors.size()),sceneCrowd.placed,sceneCrowd.moved,sceneCrowd.rejected,sceneCrowd.cpuBytes);Log(text);}
   if(logging&&ug2Flare.frames)Log("SCENE_UG2_FLARE source_textures=3 source_curves=1 hand_tracks=2 native_ug2_renderer=0");
  }catch(const std::exception& e){sceneCrowd.Clear();ug2Flare.Clear();sceneCrowd.prepared=true;if(logging)Log(e.what());}
 }
 void Tick(bool updateParticles=true){
  if(!playing)return;
  if(race!=*reinterpret_cast<BYTE**>(0x91E000)||*reinterpret_cast<unsigned*>(0x925E90)!=6||*reinterpret_cast<unsigned*>(race+0x1960)!=1){TraceCancel("race_boundary");Cancel();return;}
  float now=*reinterpret_cast<float*>(0x9885D8);
  if(!std::isfinite(now)||now<begun){TraceCancel("clock_rewind");Cancel();return;}
  float dt=now-last;last=now;
  if(awaitingCars){
   if(!carsWaitStarted)carsWaitStarted=now;
   TraceWait(now);
   if(now-carsWaitStarted>10){error=9;TraceCancel("cars_wait_timeout");Cancel();return;}
   if(!bound){if(!Bind()){Unbind();ClearVehicles(true);return;}}
   awaitingCars=false;Cars(0);if(awaitingCars)return;
   carsWaitStarted=0;GroundActor();PrepareCrowd();
#ifdef MWEL_CARBON_TORCH_RUNTIME
   if(IsTorchScene()&&(!MWELCarbonParticleBegin()||!UpdateTorchAttachments())){error=6;Cancel();return;}
#endif
   return;
  }
  if(!skipped)audio.Tick(elapsed,assets.Camera(elapsed).timeScale);
  if(!lifecycleDone&&lifecycleTest&&elapsed>=5){lifecycleDone=true;if(lifecycleTest==1)Skip();else if(lifecycleTest==2)Cancel();else Begin();return;}
  if(inspectUntil>now){Cars(0);return;}
  if(inspectAt&&!inspected&&elapsed>=inspectAt){elapsed=inspectAt;inspected=true;inspectUntil=now+inspectHold;
#ifdef MWEL_CARBON_TORCH_RUNTIME
   if(IsTorchScene()){GroundActor();if(!UpdateTorchAttachments()){error=6;Cancel();return;}LogTorchDiagnostic("inspect");}
#endif
   return;}

  if(dt>0){auto previous=elapsed;elapsed+=dt*std::clamp(assets.Camera(elapsed).timeScale/100.f,.01f,1.f);Cars(dt);if(awaitingCars){elapsed=previous;carsWaitStarted=now;return;}GroundActor();}
#ifdef MWEL_CARBON_TORCH_RUNTIME
  if(updateParticles&&IsTorchScene()&&!skipped&&!UpdateTorchAttachments()){error=6;Cancel();return;}
#endif
  if(!countdownControl&&elapsed>=countdownAt){
   Unbind();countdownControl=true;
   // Reapply only the native racer staging flag after switching suspension
   // behavior, without restarting the countdown or replaying its audio.
   for(unsigned i=0;i<assets.participants;i++)if(vehicles[i]&&FindVehicle(handles[i])==vehicles[i]){
    auto owner=*reinterpret_cast<void**>(vehicles[i]+4);
    auto racer=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x4040E0);
    if(racer)reinterpret_cast<void(__thiscall*)(void*,bool)>((*reinterpret_cast<void***>(racer))[7])(racer,true);
   }
   ClearVehicles();
  }
  if(hudHidden&&!HideHud())RestoreHud();
  if(elapsed>=hideAt)Cancel();
 }
 unsigned Frame()const{return std::min(unsigned(roots.size()-1),unsigned(std::max(0.f,assets.ActorTime(elapsed))*15));}
 bool HoldCountdown()const{return playing&&!released&&(awaitingCars||elapsed<countdownAt||inspectUntil>*reinterpret_cast<float*>(0x9885D8));}
 void GroundCameraPoint(D3DXVECTOR3& p,bool eye){
  float height;const float relative=p.z-stage._43;
  D3DXVECTOR3 query(p.x,p.y,stage._43);
  if(Ground(query,height))p.z=height+(eye?std::max(.5f,relative):relative);
 }
 void CameraMatrix(D3DXMATRIX& m){
  auto s=assets.Camera(elapsed);D3DXVECTOR3 e,c,up(0,0,1);
  D3DXVec3TransformCoord(&e,&s.eye,&stage);D3DXVec3TransformCoord(&c,&s.look,&stage);
  GroundCameraPoint(c,false);
  float tail=TailBlend();
  if(tail>0){
   // Keep the accepted camera transition independent from countdown timing.
   auto begin=assets.Camera(cameraTailAt),end=assets.Camera(cameraTailAt+3.f);auto car=assets.Car(0,assets.ActorTime(cameraTailAt));
   // Move outside the left row before retreating behind the player. A direct
   // interpolation crosses both the rival in row two and the player's body.
   D3DXVECTOR3 d=end.eye;
   if(!assets.actorClock.empty()){
    // Composite variants may end on a close-up facing the field. Reuse the
    // accepted eight-car trailing offset in this scene's own player basis.
    d=D3DXVECTOR3(car._41,car._42,car._43)-D3DXVECTOR3(car._11,car._12,car._13)*7.43f-D3DXVECTOR3(car._21,car._22,car._23)*.069f+D3DXVECTOR3(0,0,2.65f);
   }
   D3DXVECTOR3 a=begin.eye,b(car._41-5,begin.eye.y,3.5f),cc(car._41-5,d.y,3.5f);
   float u=1-tail;auto local=a*(u*u*u)+b*(3*u*u*tail)+cc*(3*u*tail*tail)+d*(tail*tail*tail);
   D3DXVec3TransformCoord(&e,&local,&stage);
   D3DXVECTOR3 target;
   if(assets.hasActor){target=roots[Frame()];target.z+=.75f;D3DXVec3TransformCoord(&target,&target,&world);}
   else{target=D3DXVECTOR3(car._41,car._42,car._43+1.f);D3DXVec3TransformCoord(&target,&target,&stage);GroundCameraPoint(target,false);}
   D3DXVECTOR3 from;D3DXVec3TransformCoord(&from,&begin.look,&stage);GroundCameraPoint(from,false);c=from+(target-from)*tail;
  }
  float before=e.z;GroundCameraPoint(e,true);cameraLift=e.z-before;
  reinterpret_cast<void(__cdecl*)(D3DXMATRIX*,D3DXVECTOR3*,D3DXVECTOR3*,D3DXVECTOR3*)>(0x6CF0A0)(&m,&e,&c,&up);
  // Carbon 4991E5 scales Dutch turns by 65536, then 48059A rotates
  // the view around Z. D3DX uses the same row-vector rotation convention.
  D3DXMATRIX roll;D3DXMatrixRotationZ(&roll,s.dutch*(1-tail)*2*D3DX_PI);m=m*roll;lastView=m;
 }
 void BlendRelease(D3DXMATRIX& m,const D3DXMATRIX& native){
  D3DXMATRIX a,b;D3DXMatrixInverse(&a,nullptr,&releaseView);D3DXMatrixInverse(&b,nullptr,&native);
  D3DXVECTOR3 sa,sb,pa,pb;D3DXQUATERNION qa,qb,q;D3DXMatrixDecompose(&sa,&qa,&pa,&a);D3DXMatrixDecompose(&sb,&qb,&pb,&b);
  float t=Ease(elapsed-releasedAt);D3DXQuaternionSlerp(&q,&qa,&qb,t);D3DXMatrixRotationQuaternion(&a,&q);auto p=pa+(pb-pa)*t;a._41=p.x;a._42=p.y;a._43=p.z;D3DXMatrixInverse(&m,nullptr,&a);
 }
} scene;
