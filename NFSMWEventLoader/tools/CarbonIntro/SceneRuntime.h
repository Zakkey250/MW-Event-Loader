#pragma once
#include "SceneAssets.h"
#include "SceneGround.h"
#include "SceneCrowd.h"
#include "Ug2Flare.h"
#include "SceneWave.h"
#include "SceneEngineRevs.h"
#include "SceneWheelMotion.h"
#include "../../../integration/WorldLoadPlanV1.h"
#include "../../src/WorldProviderModule.h"
// Carbon scene adapter. Native staging/countdown owns race release.
struct SceneRuntime {
 static constexpr unsigned MaxCars=SceneAssets::MaxCars;
 unsigned RaceSlot(unsigned sceneSlot)const{return mwel_carbon::RpdRearOfficerScene(variantName)?(sceneSlot+1)%assets.participants:sceneSlot;}
 bool enabled=false,playing=false,released=false,skipped=false;BYTE* race=nullptr;
 float begun=0,elapsed=0,last=0;unsigned generation=0;D3DXMATRIX world{},stage{};
 SceneAssets assets;SceneWave audio;SceneEngineRevs engineRevs;SceneWheelMotion wheelMotion;std::string variantName,assetFolder;D3DXVECTOR3 origin{};float heading=0;D3DXMATRIX actorLocator{};
 mw_world_load::Plan worldLoadPlan{};bool worldLoadPlanValid=false,cameraApplied=false;
 struct WorldPreparation {
  unsigned (__cdecl* active)()=nullptr;
  unsigned (__cdecl* frame)()=nullptr;
  unsigned (__cdecl* request)(const mw_world_load::Plan*,unsigned)=nullptr;
  unsigned (__cdecl* ready)(unsigned)=nullptr;

  void Resolve(){
   auto module=mwel_world_provider::Module();
   active=module?reinterpret_cast<decltype(active)>(GetProcAddress(module,"W2MW_IsWorldActiveV1")):nullptr;
   frame=module?reinterpret_cast<decltype(frame)>(GetProcAddress(module,"W2MW_InitialPresentationFrameV1")):nullptr;
   request=module?reinterpret_cast<decltype(request)>(GetProcAddress(module,"W2MW_RequestScenePreparationV1")):nullptr;
   ready=module?reinterpret_cast<decltype(ready)>(GetProcAddress(module,"W2MW_IsScenePreparationReadyV1")):nullptr;
  }
  bool Available()const{return frame&&request&&ready;}
 } worldPreparation;
 std::vector<D3DXVECTOR3> roots;float groundOffset=0,cameraLift=0;
#ifdef MWEL_CARBON_RUNTIME
 mwel_scene_road::Path roadPath;bool roadFitted=false;
#endif
 BYTE* vehicles[MaxCars]{};void* owners[MaxCars]{};bool acquired[MaxCars]{};void* controls[MaxCars]{};unsigned handles[MaxCars]{};bool bound=false;unsigned applied=0;
 unsigned error=0,errorSlot=0,errorTable=0;bool awaitingCars=false,awaitingInitialWorld=false;float carsWaitStarted=0;
 unsigned readyHandles[MaxCars]{};BYTE* readyVehicles[MaxCars]{};void* readyOwners[MaxCars]{};double readySince=-1;unsigned readyPresentationFrame=0;bool readyPresentationClock=false;
 float waitTraceAt=-1;
 unsigned nextCue=0;
 mwel_scene_motion::Motion soundMotion[MaxCars]{};float soundBurnout[MaxCars]{};
 DWORD effectsThread=0;unsigned soundUpdates=0,skidUpdates=0;float maximumSkid=0;
 unsigned renderWheelReads=0;float renderWheelMaximum=0;
 float WheelAngular(void* suspension,unsigned wheel,float native,unsigned caller){
  if(wheel>=4||!playing||released||skipped||!bound||awaitingCars||GetCurrentThreadId()!=effectsThread||race!=*reinterpret_cast<BYTE**>(0x91E000))return native;
  for(unsigned i=0;i<assets.participants;++i){
   if(!acquired[i]||!vehicles[i]||FindVehicle(handles[i])!=vehicles[i]||*reinterpret_cast<void**>(vehicles[i]+4)!=owners[i]||LiveControl(vehicles[i])!=controls[i]||SceneWheelMotion::Resolve(owners[i])!=suspension)continue;
   if(!SceneWheelMotion::Valid(suspension)||std::abs(soundBurnout[i])>.01f)return native;
   const float radius=reinterpret_cast<float(__thiscall*)(void*,unsigned)>(0x6AE220)(suspension,wheel);
   if(!std::isfinite(radius)||radius<.1f||radius>2)return native;
   const float angular=soundMotion[i].valid?soundMotion[i].longitudinal/radius:0;
   // Vehicle render/FX consumer 006B71C5 calls this exact interface. Prove
   // the consumer read, separately from merely setting the suspension value.
   if(logging&&caller==0x6B71C8){++renderWheelReads;renderWheelMaximum=std::max(renderWheelMaximum,std::abs(angular));}
   return angular;
  }
  return native;
 }
 void AudioInfo(unsigned self,unsigned info){
  if(!playing||released||skipped||!bound||awaitingCars||!info||!effectsThread||GetCurrentThreadId()!=effectsThread||race!=*reinterpret_cast<BYTE**>(0x91E000))return;
  const auto sim=*reinterpret_cast<unsigned*>(self+0x34);
  for(unsigned i=0;i<assets.participants;++i){
   if(!acquired[i]||!vehicles[i]||sim!=reinterpret_cast<unsigned>(vehicles[i])-0x80||FindVehicle(handles[i])!=vehicles[i]||*reinterpret_cast<void**>(vehicles[i]+4)!=owners[i]||LiveControl(vehicles[i])!=controls[i])continue;
   ++soundUpdates;
   const float skid=mwel_scene_motion::Skid(soundMotion[i],soundBurnout[i]);
   if(skid<=0)return;
   // 00694160 (SoundTraffic::FillAudioInfo), 006945AE..006945D6:
   // native four-wheel output [slip, -GetWheelSkid] at 98/9C, stride 8.
   // Leave surfaces, contact flags, engine, siren, mixer and voice ownership
   // intact. The native callback supplies pause/volume/3D sound lifecycle.
   bool supplied=false;
   for(unsigned wheel=0;wheel<4;++wheel)if(*reinterpret_cast<BYTE*>(info+0xD8+wheel)){
    auto& nativeSkid=*reinterpret_cast<float*>(info+0x9C+wheel*8);
    if(std::isfinite(nativeSkid))nativeSkid=std::min(nativeSkid,-skid);
    supplied=true;
   }
   if(supplied){++skidUpdates;maximumSkid=std::max(maximumSkid,skid);}return;
  }
 }
 float carClearance[MaxCars]{};bool countdownControl=false;
 float maxPlacementError=0;D3DXVECTOR3 carTargets[MaxCars]{};
 void* rollingBodies[MaxCars]{};void** rollingTables[MaxCars]{};D3DXVECTOR3 rollingVelocities[MaxCars]{};bool rollingPending=false;
 bool lensOwned=false;unsigned short savedFov=0;
 void* hud=nullptr;unsigned long long hudFeatures=0;bool hudHidden=false,hudBypass=false;float hudShowAt=0;
 bool safeTail=false;float releasedAt=0;D3DXMATRIX lastView{},releaseView{};unsigned short releaseFov=0;
 std::vector<int> cameraSubjects;
 std::vector<D3DXVECTOR3> preparedCameraPoints;
 float inspectAt=0,inspectUntil=0,inspectHold=12;bool inspected=false;unsigned lifecycleTest=0;bool lifecycleDone=false;
#ifdef MWEL_PRIVATE_SCENE_INSPECTION
 float inspectionNext=1;
#endif
 static float Ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
 float TailBlend()const{return safeTail&&!assets.rollingStart?Ease((elapsed-cameraTailAt)/3.f):0;}
 bool CameraOwned()const{return playing&&!awaitingCars&&!skipped&&(!released||(safeTail&&elapsed-releasedAt<1.f));}
 void Release(){
#ifdef MWEL_CARBON_RUNTIME
  MWELRpdIntroMusicGo();
#endif
  releaseView=lastView;releaseFov=*reinterpret_cast<unsigned short*>(0x986870+0xC4);releasedAt=elapsed;
  rollingPending=false;for(unsigned i=0;i<MaxCars;++i){rollingBodies[i]=nullptr;rollingTables[i]=nullptr;}
  if(assets.rollingStart&&bound)for(unsigned i=0;i<assets.participants;++i){
   if(!acquired[i]||!vehicles[i]||FindVehicle(handles[i])!=vehicles[i]||*reinterpret_cast<void**>(vehicles[i]+4)!=owners[i]||LiveControl(vehicles[i])!=controls[i]||!ValidControl(controls[i]))continue;
   auto body=*reinterpret_cast<void**>(static_cast<BYTE*>(controls[i])+0x44);if(!body)continue;
   auto table=*reinterpret_cast<void***>(body);
   if(table[0x20/4]!=reinterpret_cast<void*>(0x671180)||table[0x60/4]!=reinterpret_cast<void*>(0x696FD0))continue;
   const auto a=CarPose(i,goAt-1.f/15.f),b=CarPose(i,goAt);
   const D3DXVECTOR3 mapVelocity((b._41-a._41)*15,(b._42-a._42)*15,(b._43-a._43)*15);
   if(D3DXVec3Length(&mapVelocity)>100)continue;
   rollingBodies[i]=body;rollingTables[i]=table;rollingVelocities[i]={-mapVelocity.y,mapVelocity.z,mapVelocity.x};rollingPending=true;
  }
  Unbind();
  RestoreView();released=true;
 }
 void CompleteRollingRelease(){
  if(!rollingPending)return;rollingPending=false;
  // Countdown::Done clears staging and can zero velocity. Commit only after
  // that native operation, once, to the same live body captured before Unbind.
  if(!playing||!released||race!=*reinterpret_cast<BYTE**>(0x91E000))return;
  for(unsigned i=0;i<assets.participants;++i)if(rollingBodies[i]&&vehicles[i]&&FindVehicle(handles[i])==vehicles[i]&&*reinterpret_cast<void**>(vehicles[i]+4)==owners[i]&&*reinterpret_cast<void***>(rollingBodies[i])==rollingTables[i]){
   // Native IVehicle::SetSpeed (EN1.3 670210, slot 37) also matches the
   // SuspensionSpline wheel rates and EngineSpline gear/RPM. Setting only
   // rigid-body velocity left a stationary drivetrain behind at native GO,
   // causing severe tyre/engine braking in the first physical frames.
   // Match once after Countdown::Done, then retain the authored full vector
   // including road grade/side slip. No ongoing speed or input override.
   const auto vehicleTable=*reinterpret_cast<void***>(vehicles[i]);
   const bool powertrainMatched=vehicleTable==reinterpret_cast<void**>(0x8AA828)&&vehicleTable[37]==reinterpret_cast<void*>(0x670210);
   if(powertrainMatched){const auto pose=CarPose(i,goAt);const D3DXVECTOR3 forward(-pose._12,pose._13,pose._11);const float sign=D3DXVec3Dot(&rollingVelocities[i],&forward)<0?-1.f:1.f;reinterpret_cast<void(__thiscall*)(void*,float)>(vehicleTable[37])(vehicles[i],sign*D3DXVec3Length(&rollingVelocities[i]));}
   reinterpret_cast<void(__thiscall*)(void*,const D3DXVECTOR3*)>(rollingTables[i][0x60/4])(rollingBodies[i],&rollingVelocities[i]);
   if(logging){char note[220];sprintf_s(note,"SCENE_ROLLING_RELEASE slot=%u velocity=%.3f,%.3f,%.3f source_path=1 one_shot=1 after_native_done=1 native_powertrain_matched=%u",i,rollingVelocities[i].x,rollingVelocities[i].y,rollingVelocities[i].z,unsigned(powertrainMatched));Log(note);}
  }
 }
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
   cameraTailAt=t[0];goAt=t[1];hideAt=t[2];
#ifdef MWEL_CARBON_RUNTIME
   goAt=MWELCarbonGestureGo(folder.c_str(),assets.participants,cameraTailAt,goAt,hideAt);
#endif
   countdownAt=goAt-3;
  }else if(assets.participants!=8)throw std::runtime_error("Variant timing required");
  if(variantName.rfind("HPR_INTERCEPTOR_",0)==0)assets.CompressRollingTravel(goAt);
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
#ifdef MWEL_CARBON_RUNTIME
  roadFitted=false;roadPath.samples.clear();
  if(assets.rollingStart&&MWELCarbonRollingStartAllowed()){
   mwel_scene_road::Graph graph;float low=0,high=0;
   for(unsigned i=0;i<assets.participants;++i)for(const auto& m:assets.cars[i]){low=std::min(low,m._42);high=std::max(high,m._42);}
   for(float s:assets.cameraRoadStations){low=std::min(low,s);high=std::max(high,s);}
   for(float s:assets.cameraLookRoadStations){low=std::min(low,s);high=std::max(high,s);}
   const float a=heading*D3DX_PI/180.f;
   roadFitted=MWELCarbonRoadGraph(graph)&&roadPath.Build(graph,{origin.x,origin.y,origin.z},{std::cos(a),std::sin(a),0},-low+10,high+10);
   if(logging){char note[160];sprintf_s(note,"SCENE_ROAD_FIT prepared=%u nodes=%zu edges=%zu samples=%zu backward=%.2f forward=%.2f",unsigned(roadFitted),graph.nodes.size(),graph.edges.size(),roadPath.samples.size(),-low,high);Log(note);}
  }
#endif
 }
 D3DXMATRIX LocalRoadStage(float station)const{
#ifdef MWEL_CARBON_RUNTIME
  mwel_scene_road::Frame frame;
  if(roadFitted&&roadPath.Sample(station,frame)){
   D3DXMATRIX result;D3DXMatrixRotationZ(&result,std::atan2(frame.forward.y,frame.forward.x)-D3DX_PI/2);
   D3DXVECTOR3 origin(frame.center.x+frame.forward.y*roadPath.lateral,frame.center.y-frame.forward.x*roadPath.lateral,frame.center.z),normal;float height=0;
   if(sceneGround.Query(origin,height,&normal)&&normal.z>.6f){
    D3DXVECTOR3 right(result._11,result._12,0),along;
    right.z=-(normal.x*right.x+normal.y*right.y)/normal.z;D3DXVec3Normalize(&right,&right);
    D3DXVec3Cross(&along,&normal,&right);D3DXVec3Normalize(&along,&along);
    result._11=right.x;result._12=right.y;result._13=right.z;
    result._21=along.x;result._22=along.y;result._23=along.z;
    result._31=normal.x;result._32=normal.y;result._33=normal.z;origin.z=height;
   }
   result._41=origin.x-result._21*station;result._42=origin.y-result._22*station;result._43=origin.z-result._23*station;return result;
  }
#endif
  return stage;
 }
 D3DXMATRIX CarPose(unsigned slot,float seconds)const{auto source=assets.Car(slot,seconds);return source*LocalRoadStage(source._42);}
 D3DXMATRIX CameraRoadStage(float station)const{
#ifdef MWEL_CARBON_RUNTIME
  mwel_scene_road::CameraFrame f;
  auto ground=[](mwel_scene_road::Point p,float& h){return sceneGround.Query({p.x,p.y,p.z},h);};
  if(roadFitted&&mwel_scene_road::FitCameraFrame(roadPath,station,ground,f))
   return D3DXMATRIX(f.right.x,f.right.y,f.right.z,0,f.forward.x,f.forward.y,f.forward.z,0,
    f.up.x,f.up.y,f.up.z,0,f.center.x-f.forward.x*station,f.center.y-f.forward.y*station,f.center.z-f.forward.z*station,1);
#endif
  return stage;
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
  auto handle=*reinterpret_cast<unsigned*>(race+0x18+RaceSlot(slot)*0x194);auto vehicle=FindVehicle(handle);
  char note[320];sprintf_s(note,"SCENE_WAIT variant=%s seconds=%.3f error=%u slot=%u handle=%u vehicle=%p ready_vehicle=%p table=%08X ready_age=%.3f bound=%u",variantName.c_str(),now-carsWaitStarted,error,slot,handle,vehicle,readyVehicles[slot],errorTable,readySince>=0?now-readySince:-1.f,unsigned(bound));MWELNeoLog(note);
#endif
 }
 void StopRevs(){
  for(unsigned i=0;i<assets.participants;i++)if(vehicles[i]&&FindVehicle(handles[i])==vehicles[i]&&*reinterpret_cast<void**>(vehicles[i]+4)==owners[i])engineRevs.Stop(i,*reinterpret_cast<void**>(vehicles[i]+4));
  engineRevs.Forget();
 }
 void* LiveControl(BYTE* vehicle)const{
  auto owner=vehicle?*reinterpret_cast<void**>(vehicle+4):nullptr;
  auto live=owner?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x445650):nullptr;
#ifdef MWEL_SCENE_BINDING_PROOF
  return MWELSceneBindingProofControl(vehicle,live,generation,elapsed);
#else
  return live;
#endif
 }
 void Unbind(){
  effectsThread=0;for(auto& motion:soundMotion)motion={};
  StopRevs();
  if(!bound)return;
  for(unsigned i=0;i<assets.participants;i++)if(acquired[i]&&vehicles[i]&&FindVehicle(handles[i])==vehicles[i]){
   auto owner=*reinterpret_cast<void**>(vehicles[i]+4);
   if(owner!=owners[i])continue; // A recycled vehicle belongs to the new native owner.
   auto live=LiveControl(vehicles[i]);
   if(controls[i]&&live==controls[i]&&ValidControl(live)){reinterpret_cast<void(__thiscall*)(void*,float)>(0x6AE0D0)(live,0);reinterpret_cast<void(__thiscall*)(void*)>(0x6A0100)(live);}
   // Release only our acquired spline (or its retired tail). A different live
   // control belongs to a subsequent native bind and must remain untouched.
   if(live==controls[i]||!ValidControl(live))reinterpret_cast<void(__thiscall*)(void*,bool)>(0x686410)(vehicles[i],false);
#ifdef MWEL_SCENE_BINDING_PROOF
   if(!vehicles[i][0xB0])MWELSceneBindingProofReleased(vehicles[i]);
#endif
   acquired[i]=false;
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
 void ClearVehicles(bool preserveReadiness=false){for(unsigned i=0;i<MaxCars;++i){vehicles[i]=nullptr;owners[i]=nullptr;acquired[i]=false;controls[i]=nullptr;handles[i]=0;if(!preserveReadiness){readyHandles[i]=0;readyVehicles[i]=nullptr;readyOwners[i]=nullptr;}}bound=false;if(!preserveReadiness){readySince=-1;readyPresentationFrame=0;readyPresentationClock=false;}}
 void Cancel(){
#ifdef MWEL_CARBON_RUNTIME
  MWELRpdIntroMusicEnd();
#endif
  rollingPending=false;
  audio.Stop();
  sceneCrowd.Clear();
  ug2Flare.Clear();
#ifdef MWEL_CARBON_TORCH_RUNTIME
  MWELCarbonParticleRelease();LogTorchDiagnostic("cancel_or_skip");
#endif
  Unbind();RestoreView();ClearVehicles();playing=false;released=false;skipped=false;race=nullptr;nextCue=0;countdownControl=false;inspectUntil=0;hud=nullptr;hudFeatures=0;hudHidden=false;awaitingCars=awaitingInitialWorld=false;carsWaitStarted=0;}
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
 unsigned PresentationFrame()const{return worldPreparation.Available()?worldPreparation.frame():0;}
 bool FieldStable(){
  // Manual failure replaced slot 1 within the first 25 ms after staging.
  // Observing the whole field across simulation ticks lets native teardown
  // finish before installing Carbon's suspension behaviors.
  // Only this owned FE hold can pause the simulation during preparation.
  // Require separate engine updates, and retain the normal simulation clock
  // for ordinary scenes/replays. Never advance either source or game clocks.
  const unsigned presentation=PresentationFrame();const bool wall=bool(presentation);
  const double now=wall?double(GetTickCount64())/1000.:*reinterpret_cast<float*>(0x9885D8);
  bool changed=wall!=readyPresentationClock;readyPresentationClock=wall;
  if(wall&&presentation==readyPresentationFrame)return false;
  readyPresentationFrame=presentation;
  if(!std::isfinite(now)||!race||*reinterpret_cast<unsigned*>(race+0x1958)!=assets.participants){readySince=-1;return false;}
  for(unsigned i=0;i<assets.participants;++i){
   errorSlot=i;auto handle=*reinterpret_cast<unsigned*>(race+0x18+RaceSlot(i)*0x194);auto vehicle=FindVehicle(handle);
   if(!handle||!vehicle){readySince=-1;return false;}
   auto owner=*reinterpret_cast<void**>(vehicle+4);if(!owner){readySince=-1;return false;}
   for(unsigned j=0;j<i;++j)if(readyHandles[j]==handle||readyVehicles[j]==vehicle||readyOwners[j]==owner){readySince=-1;return false;}
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
  for(unsigned i=0;i<assets.participants;i++){vehicles[i]=nullptr;owners[i]=nullptr;acquired[i]=false;controls[i]=nullptr;handles[i]=0;}bound=true;
  for(unsigned i=0;i<assets.participants;i++){
   errorSlot=i;handles[i]=*reinterpret_cast<unsigned*>(race+0x18+RaceSlot(i)*0x194);vehicles[i]=FindVehicle(handles[i]);if(!vehicles[i]){error=1;return false;}
   // SetAnimating installs the native SuspensionSpline behavior and its
   // INISCarControl interface; ordinary driving does not expose it.
   owners[i]=*reinterpret_cast<void**>(vehicles[i]+4);if(!owners[i]){error=2;return false;}
   reinterpret_cast<void(__thiscall*)(void*,bool)>(0x686410)(vehicles[i],true);acquired[i]=true;
   auto owner=*reinterpret_cast<void**>(vehicles[i]+4);if(owner!=owners[i]){error=2;return false;}
   controls[i]=LiveControl(vehicles[i]);
   errorTable=controls[i]?*reinterpret_cast<unsigned*>(controls[i]):0;
   if(!ValidControl(controls[i])){error=3;return false;}
  }
  bound=true;applied=0;
  return true;
 }
 void Cars(float dt){
  if(!bound||released)return;
  bool renewed[MaxCars]{};
  // Native CAnimScene re-queries INISCarControl on every animated-car update.
  // NIS cleanup/behavior changes can replace it without replacing IVehicle.
  for(unsigned i=0;i<assets.participants;++i){
   errorSlot=i;
   if(!vehicles[i]||FindVehicle(handles[i])!=vehicles[i]){error=7;TraceCancel("vehicle_changed_waiting");Unbind();ClearVehicles();awaitingCars=true;return;}
   auto owner=*reinterpret_cast<void**>(vehicles[i]+4);
   if(owner!=owners[i]){error=2;Unbind();ClearVehicles();awaitingCars=true;return;}
   auto live=LiveControl(vehicles[i]);
   if(!ValidControl(live)&&!vehicles[i][0xB0]){
    renewed[i]=true;
    // A late native NIS UnBind retired its spline. This scene still owns
    // staging; install a fresh spline instead of touching the retired tail.
    reinterpret_cast<void(__thiscall*)(void*,bool)>(0x686410)(vehicles[i],true);
    owner=*reinterpret_cast<void**>(vehicles[i]+4);
    live=LiveControl(vehicles[i]);
   }
   if(!ValidControl(live)){
#ifdef MWEL_CARBON_RUNTIME
    if(logging&&error!=8){char note[256];sprintf_s(note,"SCENE_CONTROL_PENDING slot=%u handle=%u vehicle=%p old=%p live=%p parent_table=%08X animating=%u elapsed=%.5f",i,handles[i],vehicles[i],controls[i],live,ControlHead(live),unsigned(vehicles[i][0xB0]),elapsed);MWELNeoLog(note);}
#endif
    // Re-enter the complete-field acquisition path. Leaving bound=true here
    // skipped Bind forever, so an animating flag with a retired control could
    // freeze the camera until the ten-second fallback released countdown.
    error=8;Unbind();ClearVehicles();awaitingCars=true;return;
   }
   renewed[i]=renewed[i]||live!=controls[i];controls[i]=live;
  }
  while(nextCue<assets.cues.size()&&assets.cues[nextCue].time<=elapsed){auto c=assets.cues[nextCue++];reinterpret_cast<void(__thiscall*)(void*,float)>(0x6AE0D0)(controls[c.slot],c.speed);}
  for(unsigned i=0;i<assets.participants;i++){
   auto render=CarPose(i,assets.ActorTime(assets.rollingStart&&skipped?goAt:elapsed));D3DXMATRIX physical;
   D3DXVECTOR3 point(render._41,render._42,assets.rollingStart?render._43:stage._43),normal;float floor=0;
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
   if(!ok){if(logging){char note[300];sprintf_s(note,"SCENE_POSITION_REJECTED slot=%u grounded=%u source_clock=%.5f map=%.5f,%.5f,%.5f physical=%.5f,%.5f,%.5f stage=%.5f,%.5f,%.5f",i,unsigned(grounded),assets.ActorTime(elapsed),render._41,render._42,render._43,physical._41,physical._42,physical._43,stage._41,stage._42,stage._43);Log(note);}error=4;errorSlot=i;TraceCancel("native_position_rejected");Cancel();return;}
   const float before=std::max(0.f,elapsed-1.f/30.f),after=std::min(assets.duration,elapsed+1.f/30.f);
   const auto previousPose=CarPose(i,assets.ActorTime(before)),nextPose=CarPose(i,assets.ActorTime(after));
   const float burnout=reinterpret_cast<float(__thiscall*)(void*)>(0x6AE0E0)(controls[i]);
   const auto motion=mwel_scene_motion::Measure({previousPose._41,previousPose._42,previousPose._43},{nextPose._41,nextPose._42,nextPose._43},{physical._33,-physical._31,0},after-before,burnout);
   effectsThread=GetCurrentThreadId();soundMotion[i]=motion;soundBurnout[i]=burnout;
   if(!skipped&&(assets.rollingStart||elapsed<countdownAt))engineRevs.Apply(i,*reinterpret_cast<void**>(vehicles[i]+4),elapsed,motion.valid?motion.rev:.18f);
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
   // Position pinning remains unchanged. Supply tyre speed AFTER native NIS
   // and our zero-velocity hold, through the real SuspensionSpline interface.
   wheelMotion.Apply(*reinterpret_cast<void**>(vehicles[i]+4),motion.valid?motion.longitudinal:0,burnout);
  }
  ++applied;
 }
 bool ValidateRoadPath(bool diagnostics){
#ifdef MWEL_CARBON_RUNTIME
  if(assets.rollingStart&&MWELCarbonRollingStartAllowed()&&!roadFitted)return false;
#endif
  float h=0;const float base=Ground(origin,h)?h:origin.z;
  for(unsigned i=0;i<assets.participants;++i){
   auto m=CarPose(i,assets.ActorTime(0));
   if(!Ground({m._41,m._42,assets.rollingStart?m._43:base},h)||(!assets.rollingStart&&std::abs(h-base)>3))return false;
   if(assets.rollingStart){
    D3DXVECTOR3 previous(-m._42,h+1,m._41);
    for(unsigned n=1;n<=unsigned(goAt*15);++n){
     auto p=CarPose(i,float(n)/15);
     if(!Ground({p._41,p._42,p._43},h)){if(logging&&diagnostics){char note[180];sprintf_s(note,"SCENE_ROLLING_ROAD_REFUSED slot=%u frame=%u map=%.2f,%.2f base=%.2f ground=%.2f",i,n,p._41,p._42,p._43,h);Log(note);}return false;}
     D3DXVECTOR3 point(-p._42,h+1,p._41);event_geometry::RayHit hit;
     // A rolling source crosses a road grade; do not require a 626 m road to
     // be within 3 m of its GO height. Keep the existing bounded floor query,
     // reject discontinuities and sweep the intervening segment for walls.
     // Source tracks include stationary samples. Quantized triangle heights
     // can differ by millimetres while horizontal travel is effectively zero.
     // Keep a 5 cm numerical allowance; deck jumps still fail before binding.
     if(!mwel_scene_road::ContinuousRoadGrade({previous.x,previous.z,previous.y},{point.x,point.z,point.y})){if(logging&&diagnostics){char note[200];sprintf_s(note,"SCENE_ROLLING_ROAD_REFUSED discontinuous_grade=1 slot=%u frame=%u dz=%.5f travel=%.5f",i,n,point.y-previous.y,std::hypot(point.x-previous.x,point.z-previous.z));Log(note);}return false;}
     if(!sceneGround.geometry.nodes.empty()&&sceneGround.geometry.Ray({previous.x,previous.y,previous.z},{point.x,point.y,point.z},hit,false,true)){if(diagnostics)Log("SCENE_ROLLING_ROAD_REFUSED wall_intersection=1");return false;}
     previous=point;
    }
   }
  }
  return true;
 }
 bool ValidateStartRoad(){
  if(ValidateRoadPath(false))return true;
#ifdef MWEL_CARBON_RUNTIME
  if(assets.rollingStart&&MWELCarbonRollingStartAllowed()){
   mwel_scene_road::Graph graph;if(!MWELCarbonRoadGraph(graph))return false;
   float low=0,high=0;
   for(unsigned i=0;i<assets.participants;++i)for(const auto& m:assets.cars[i]){low=std::min(low,m._42);high=std::max(high,m._42);}
   for(const auto* stations:{&assets.cameraRoadStations,&assets.cameraLookRoadStations})for(float s:*stations){low=std::min(low,s);high=std::max(high,s);}
   const auto original=roadPath;const bool originallyFitted=roadFitted;unsigned attempts=0;
   mwel_scene_road::Path accepted;const float a=heading*D3DX_PI/180.f;
   const bool found=mwel_scene_road::ChoosePath(graph,{origin.x,origin.y,origin.z},{std::cos(a),std::sin(a),0},-low+10,high+10,accepted,
    [&](const mwel_scene_road::Path& candidate){roadPath=candidate;roadFitted=true;return ValidateRoadPath(false);},attempts);
   roadPath=found?std::move(accepted):original;roadFitted=found||originallyFitted;
   if(logging){char note[200];sprintf_s(note,"SCENE_ROAD_SEARCH prepared=%u attempts=%u connected_junctions_only=1 complete_field_preflight=1 requested_pose_preserved=1",unsigned(found),attempts);Log(note);}
   if(found)return true;
  }
#endif
  return ValidateRoadPath(true);
 }
 void PrepareWorldLoadPlan(){
  worldLoadPlan={};worldLoadPlan.generation=generation;worldLoadPlanValid=true;
  auto append=[&](D3DXVECTOR3 p,unsigned kind){
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||std::abs(p.x)>100000||std::abs(p.y)>100000){worldLoadPlanValid=false;return;}
   for(unsigned i=0;i<worldLoadPlan.count;++i){const auto& q=worldLoadPlan.points[i];const float dx=p.x-q.x,dy=p.y-q.y;if(q.kind==kind&&dx*dx+dy*dy<=100.f*100.f)return;}
   if(worldLoadPlan.count==mw_world_load::Capacity){worldLoadPlanValid=false;return;}
   worldLoadPlan.points[worldLoadPlan.count++]={p.x,p.y,p.z,kind};
  };
  auto first=CarPose(0,assets.ActorTime(0));append({first._41,first._42,first._43},mw_world_load::Car);
  // Publish the already checked camera path, including retargeted shots.
  // Loading the original off-road avatar would warm unrelated scenery even
  // though the camera was corrected back to an actual participant. Publishing
  // never calls Cars/Bind/CameraMatrix or changes clock/lens/render state.
  const float end=std::min(assets.duration,goAt+1.f);
  if(assets.rollingStart&&!preparedCameraPoints.empty())for(const auto& p:preparedCameraPoints)append(p,mw_world_load::Camera);
  else for(unsigned n=0;n<assets.cameras.size()&&float(n)/assets.rate<=end;++n){
   const float t=float(n)/assets.rate;auto m=stage;
#ifdef MWEL_CARBON_RUNTIME
   if(roadFitted&&!assets.cameraRoadStations.empty())m=CameraRoadStage(assets.CameraRoadStation(t));
#endif
   D3DXVECTOR3 eye;D3DXVec3TransformCoord(&eye,&assets.cameras[n].eye,&m);append(eye,mw_world_load::Camera);
  }
  for(unsigned i=0;i<assets.participants;++i)for(unsigned n=0;float(n)/15<=end;++n){auto m=CarPose(i,assets.ActorTime(float(n)/15));append({m._41,m._42,m._43},mw_world_load::Car);}
  if(logging){char note[160];sprintf_s(note,"SCENE_WORLD_LOAD_PLAN generation=%u points=%u valid=%u first=(%.3f,%.3f,%.3f) clock_advanced=0",generation,worldLoadPlan.count,unsigned(worldLoadPlanValid),first._41,first._42,first._43);Log(note);}
 }
 bool RequestWorldPreparation(){
  if(!worldPreparation.Available()||(worldPreparation.active&&!worldPreparation.active()))return false;
  if(worldPreparation.request(&worldLoadPlan,sizeof(worldLoadPlan)))return true;
  error=11;Log("SCENE_WORLD_PREPARATION_REJECTED");Cancel();return false;
 }
 bool WorldActive()const{
  return !worldPreparation.Available()||worldPreparation.ready(generation)||
   (worldPreparation.active&&!worldPreparation.active()&&!PresentationFrame());
 }
 void Begin(){
  Cancel();cameraApplied=false;if(!enabled)return;auto r=*reinterpret_cast<BYTE**>(0x91E000);
  if(!r||*reinterpret_cast<unsigned*>(r+0x1958)!=assets.participants)return;
#ifdef MWEL_CARBON_RUNTIME
  auto source=assets.Car(0,assets.ActorTime(goAt));float pose[4]{};if(!MWELCarbonStartPose(pose,reinterpret_cast<const float*>(&source))){error=5;return;}
  origin={pose[0],pose[1],pose[2]};heading=pose[3];SetStage();
  if((!MWELCarbonUsesAuthoredPose()||assets.rollingStart)&&(!ValidateStartRoad()||(assets.rollingStart&&!ValidateCameraPath()))){error=10;Log("SCENE_NATIVE_FALLBACK final_pose_changed_before_bind=1 all_cars_unmodified=1");return;}
  if(logging){char note[200];sprintf_s(note,"SCENE_STAGE_POSE origin=%.3f,%.3f,%.3f heading=%.3f source0=%.3f,%.3f,%.3f",origin.x,origin.y,origin.z,heading,source._41,source._42,source._43);MWELNeoLog(note);}
#endif
#ifdef MWEL_CARBON_TORCH_RUNTIME
  if(!PrepareSceneTorch()){error=6;return;}
#endif
  #ifdef MWEL_PRIVATE_SCENE_INSPECTION
  inspectionNext=1;
#endif
  race=r;last=begun=*reinterpret_cast<float*>(0x9885D8);elapsed=0;nextCue=0;error=0;playing=true;skipped=false;inspectUntil=0;inspected=false;countdownControl=false;maxPlacementError=0;waitTraceAt=-1;engineRevs.updates=0;engineRevs.minRpm=engineRevs.maxRpm=0;wheelMotion={};soundUpdates=skidUpdates=renderWheelReads=0;maximumSkid=renderWheelMaximum=0;effectsThread=0;++generation;
  float height;if(Ground(origin,height)){groundOffset=height-origin.z;stage._43=height;world._43=height;}
  PrepareWorldLoadPlan();
  if(!worldLoadPlanValid){error=11;Cancel();return;}
  worldPreparation.Resolve();
  // A world loader may dispatch native Stage while flow 5 still owns the
  // initial load. Keep countdown ownership, but acquire no car controls and
  // advance no source clock until this same race enters flow 6.
  if(*reinterpret_cast<unsigned*>(0x925E90)==5){awaitingInitialWorld=awaitingCars=true;Log("SCENE_INITIAL_WORLD_WAIT source_clock_held=1 native_car_controls_unacquired=1");return;}
  if(RequestWorldPreparation()){awaitingInitialWorld=awaitingCars=true;Log("SCENE_INITIAL_WORLD_WAIT source_clock_held=1 warm_retry=1");return;}
  if(!playing)return; // Explicit rejection returns through BeginStagedScene required-intro failure.
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
  const auto flow=*reinterpret_cast<unsigned*>(0x925E90);
  if(race!=*reinterpret_cast<BYTE**>(0x91E000)||*reinterpret_cast<unsigned*>(race+0x1960)!=1||
     (flow!=6&&!(awaitingInitialWorld&&flow==5))){TraceCancel("race_boundary");Cancel();return;}
  if(awaitingInitialWorld&&(flow==5||!WorldActive()))return;
  if(awaitingInitialWorld){awaitingInitialWorld=false;last=begun=*reinterpret_cast<float*>(0x9885D8);carsWaitStarted=last;Log("SCENE_INITIAL_WORLD_READY source_clock_reset=1 native_countdown_retained=1");}
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
  if(PresentationFrame()){Cars(0);return;} // First source pose stays at t=0 behind the native loading cover.
#ifdef MWEL_CARBON_RUNTIME
  MWELRpdIntroMusicFrame(unsigned(race),generation,elapsed,countdownAt,goAt,skipped);
#endif
  if(!skipped)audio.Tick(elapsed,assets.Camera(elapsed).timeScale);
  if(!lifecycleDone&&lifecycleTest&&elapsed>=5){lifecycleDone=true;if(lifecycleTest==1)Skip();else if(lifecycleTest==2)Cancel();else Begin();return;}
  if(inspectUntil>now){Cars(0);return;}
#ifdef MWEL_PRIVATE_SCENE_INSPECTION
  // Private build only: hold original camera samples for visual comparison.
  // This switch is absent from ordinary and distribution builds.
  if(assets.rollingStart&&!skipped&&inspectionNext<goAt&&elapsed>=inspectionNext){
   elapsed=inspectionNext;inspectionNext+=2;inspectUntil=now+2;Cars(0);
   if(logging){char note[100];sprintf_s(note,"SCENE_INSPECTION time=%.3f private_build=1",elapsed);Log(note);}return;
  }
#endif
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
   countdownControl=true;
   if(!assets.rollingStart){Unbind();
   // Reapply only the native racer staging flag after switching suspension
   // behavior, without restarting the countdown or replaying its audio.
   for(unsigned i=0;i<assets.participants;i++)if(vehicles[i]&&FindVehicle(handles[i])==vehicles[i]){
    auto owner=*reinterpret_cast<void**>(vehicles[i]+4);
    auto racer=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x4040E0);
    if(racer)reinterpret_cast<void(__thiscall*)(void*,bool)>((*reinterpret_cast<void***>(racer))[7])(racer,true);
   }
   ClearVehicles();
   }
  }
  if(hudHidden&&!HideHud())RestoreHud();
  if(elapsed>=hideAt)Cancel();
 }
 unsigned Frame()const{return std::min(unsigned(roots.size()-1),unsigned(std::max(0.f,assets.ActorTime(elapsed))*15));}
 bool HoldCountdown()const{return playing&&!released&&(awaitingCars||elapsed<countdownAt||inspectUntil>*reinterpret_cast<float*>(0x9885D8));}
 void GroundCameraPoint(D3DXVECTOR3& p,bool eye){
#ifdef MWEL_CARBON_RUNTIME
  // The moving road frame already includes the reference car's grade.
  // Only prevent an eye from entering the road; applying the old flat-stage
  // height offset again would double the grade and distort the composition.
  if(roadFitted){float height;if(eye&&Ground(p,height))p.z=std::max(p.z,height+.5f);return;}
#endif
  float height;const float relative=p.z-stage._43;
  D3DXVECTOR3 query(p.x,p.y,stage._43);
  if(Ground(query,height))p.z=height+(eye?std::max(.5f,relative):relative);
 }
 bool CameraMatrix(D3DXMATRIX& m,D3DXVECTOR3* outputEye=nullptr,D3DXVECTOR3* outputLook=nullptr){
  auto s=assets.Camera(elapsed);D3DXVECTOR3 e,c,up(0,0,1);
  auto cameraStage=stage,lookStage=stage;
#ifdef MWEL_CARBON_RUNTIME
  if(roadFitted&&!assets.cameraRoadStations.empty()){
   // Use the original native avatar (including stationary drive-by spaces).
   // Do not interpolate a reference across source cuts.
   cameraStage=CameraRoadStage(assets.CameraRoadStation(elapsed));
   lookStage=CameraRoadStage(assets.CameraRoadStation(elapsed,true));
  }
#endif
  D3DXVec3TransformCoord(&e,&s.eye,&cameraStage);D3DXVec3TransformCoord(&c,&s.look,&lookStage);
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
#ifdef MWEL_CARBON_RUNTIME
  // Close native shots encode orientation with a look point about 0.5 m from
  // the eye. Lifting only the eye at a road seam rotated that short ray by
  // tens of degrees. Move its shared-reference ray with the eye; a separately
  // referenced target car keeps its own world position.
  if(roadFitted&&assets.CameraSharedRoadReference(elapsed))c.z+=cameraLift;
  if(assets.rollingStart){
   const auto frame=std::min(unsigned(std::max(0.f,elapsed)*assets.rate),unsigned(assets.cameras.size()-1));
   const int subject=frame<cameraSubjects.size()?cameraSubjects[frame]:-1;
   auto safetyStage=cameraStage;float safetyStation=assets.CameraRoadStation(elapsed);
   const auto car=CarPose(subject>=0?unsigned(subject):0,assets.ActorTime(elapsed));
   if(subject>=0){
    // Some HP shots follow decorative cops or racers absent from a smaller
    // field. Retain the source camera arm, but anchor the whole shot to an
    // actual participant. Selection is fixed for each source shot, not per
    // frame, so crossing cars cannot make the camera oscillate between them.
    const auto sourceCar=assets.Car(unsigned(subject),assets.ActorTime(elapsed));
    safetyStation=sourceCar._42;safetyStage=CameraRoadStage(safetyStation);
    auto boom=s.eye-assets.CameraRoadOrigin(elapsed);boom.x=std::clamp(boom.x,-5.f,5.f);boom.y=std::clamp(boom.y,-10.f,10.f);boom.z=std::clamp(boom.z,1.5f,5.5f);
    const float span=std::hypot(boom.x,boom.y);if(span<4){if(span>.01f){boom.x*=4/span;boom.y*=4/span;}else boom.y=-6;}
    D3DXVECTOR3 arm;D3DXVec3TransformNormal(&arm,&boom,&safetyStage);c={car._41,car._42,car._43+1.2f};e=c+arm;
   }
   const D3DXVECTOR3 focus(car._41,car._42,car._43+1.5f);auto beforeEye=e;
   // Keep every external camera inside the central connected road corridor.
   // Render-only bridge beams/roofs have no physics triangles, including low
   // side shots. A ray miss alone cannot certify an original HP camera arm.
   if(roadFitted&&!assets.cameraRoadStations.empty()){
    const float station=safetyStation;
    const D3DXVECTOR3 center(safetyStage._41+safetyStage._21*station,safetyStage._42+safetyStage._22*station,safetyStage._43+safetyStage._23*station);
    const D3DXVECTOR3 right(safetyStage._11,safetyStage._12,safetyStage._13),along(safetyStage._21,safetyStage._22,safetyStage._23),up(safetyStage._31,safetyStage._32,safetyStage._33);
    const auto roadCenter=center-right*roadPath.lateral;
    const mwel_scene_road::CameraFrame corridor{{roadCenter.x,roadCenter.y,roadCenter.z},{right.x,right.y,right.z},{along.x,along.y,along.z},{up.x,up.y,up.z}};
    const auto clipped=mwel_scene_road::CameraCorridorEye(corridor,{e.x,e.y,e.z});e={clipped.x,clipped.y,clipped.z};
    const auto sourceRay=c-beforeEye;if(subject<0&&D3DXVec3Length(&sourceRay)<3.f)c+=e-beforeEye;beforeEye=e;
   }
   if(sceneGround.ConstrainCamera(focus,e)){
    // Native close shots carry direction in a very short look ray. Preserve
    // that direction when moving their eye out of the building.
    const auto lookRay=c-beforeEye;if(subject<0&&D3DXVec3Length(&lookRay)<3.f)c+=e-beforeEye;
   }
   auto separation=e-focus;
   auto occupied=[&](const D3DXVECTOR3& eye){for(unsigned slot=0;slot<assets.participants;++slot){const auto p=CarPose(slot,assets.ActorTime(elapsed));const D3DXVECTOR3 delta=eye-D3DXVECTOR3(p._41,p._42,p._43),along(p._11,p._12,p._13),side(p._21,p._22,p._23);if(std::abs(D3DXVec3Dot(&delta,&along))<2.8f&&std::abs(D3DXVec3Dot(&delta,&side))<1.3f&&delta.z>-.3f&&delta.z<2.4f)return true;}return false;};
   if(D3DXVec3Length(&separation)<(subject>=0?3.5f:1.f)||!sceneGround.CameraPocketClear(e)||occupied(e)){
    bool safe=false;const D3DXVECTOR3 forward(car._11,car._12,car._13),right(-car._21,-car._22,-car._23);
    for(float side:{0.f,-3.f,3.f}){auto alternate=focus-forward*6.f+right*side+D3DXVECTOR3(0,0,2);sceneGround.ConstrainCamera(focus,alternate);auto span=alternate-focus;if(D3DXVec3Length(&span)>=3.5f&&sceneGround.CameraPocketClear(alternate)&&!occupied(alternate)){e=alternate;c=focus;safe=true;break;}}
    if(!safe)return false;
   }
  }
#endif
  const auto viewRay=c-e;if(!std::isfinite(D3DXVec3Length(&viewRay))||D3DXVec3Length(&viewRay)<.0001f)return false;
  if(outputEye)*outputEye=e;if(outputLook)*outputLook=c;
  reinterpret_cast<void(__cdecl*)(D3DXMATRIX*,D3DXVECTOR3*,D3DXVECTOR3*,D3DXVECTOR3*)>(0x6CF0A0)(&m,&e,&c,&up);
  // Carbon 4991E5 scales Dutch turns by 65536, then 48059A rotates
  // the view around Z. D3DX uses the same row-vector rotation convention.
  D3DXMATRIX roll;D3DXMatrixRotationZ(&roll,s.dutch*(1-tail)*2*D3DX_PI);m=m*roll;lastView=m;
  return true;
 }
 bool ValidateCameraPath(){
  if(!assets.rollingStart)return true;
  if(sceneGround.geometry.nodes.empty())return false;
  const auto savedTime=elapsed;const auto savedView=lastView;const auto savedLift=cameraLift;bool valid=true;unsigned samples=0;
  cameraSubjects.assign(assets.cameras.size(),-1);
  preparedCameraPoints.clear();
  std::vector<bool> visible(assets.cameras.size(),true);unsigned end=0;
  for(;end<assets.cameras.size()&&float(end)/assets.rate<=goAt;++end){
   elapsed=float(end)/assets.rate;D3DXMATRIX candidate;D3DXVECTOR3 eye,look;if(!CameraMatrix(candidate,&eye,&look)){valid=false;break;}
   if(assets.cameras[end].fade>=95)continue;visible[end]=false;
   for(unsigned slot=0;slot<assets.participants;++slot){const auto pose=CarPose(slot,assets.ActorTime(elapsed));if(mwel_scene_road::SubjectInView({eye.x,eye.y,eye.z},{look.x,look.y,look.z},{pose._41,pose._42,pose._43+1.2f},assets.cameras[end].lens)){visible[end]=true;break;}}
  }
  unsigned repairedShots=0;
  if(valid)for(unsigned first=0;first<end;){
   unsigned last=first+1;for(;last<end;++last){const auto& a=assets.cameras[last-1];const auto& b=assets.cameras[last];auto jump=b.eye-a.eye;
    auto direction=[](D3DXVECTOR3 p){const float n=D3DXVec3Length(&p);return n>.0001f?p/n:D3DXVECTOR3(0,0,0);};auto av=direction(a.look-a.eye),bv=direction(b.look-b.eye);
    if(D3DXVec3Length(&jump)>5||D3DXVec3Dot(&av,&bv)<.9f||std::abs(a.lens-b.lens)>1||(!assets.cameraRoadReferences.empty()&&(assets.cameraRoadReferences[last]!=assets.cameraRoadReferences[last-1]||assets.cameraLookRoadReferences[last]!=assets.cameraLookRoadReferences[last-1])))break;
   }
   bool repair=false;for(unsigned i=first;i<last;++i)repair=repair||!visible[i];
   if(repair){const float time=float((first+last)/2)/assets.rate;const auto reference=assets.CameraRoadOrigin(time,true);unsigned chosen=0;float best=1e30f;
    for(unsigned slot=0;slot<assets.participants;++slot){const auto pose=assets.Car(slot,assets.ActorTime(time));auto delta=reference-D3DXVECTOR3(pose._41,pose._42,pose._43);const float distance=D3DXVec3LengthSq(&delta);if(distance<best){chosen=slot;best=distance;}}
    std::fill(cameraSubjects.begin()+first,cameraSubjects.begin()+last,int(chosen));++repairedShots;
   }first=last;
  }
  if(valid)for(unsigned i=0;i<end;++i){elapsed=float(i)/assets.rate;D3DXMATRIX candidate;D3DXVECTOR3 eye,look;if(!CameraMatrix(candidate,&eye,&look)){valid=false;break;}
   bool framed=assets.cameras[i].fade>=95;for(unsigned slot=0;slot<assets.participants&&!framed;++slot){const auto pose=CarPose(slot,assets.ActorTime(elapsed));framed=mwel_scene_road::SubjectInView({eye.x,eye.y,eye.z},{look.x,look.y,look.z},{pose._41,pose._42,pose._43+1.2f},assets.cameras[i].lens);}
   if(!framed){valid=false;break;}preparedCameraPoints.push_back(eye);++samples;
  }
  elapsed=savedTime;lastView=savedView;cameraLift=savedLift;
  if(!valid)preparedCameraPoints.clear();
  if(logging){char note[240];sprintf_s(note,"SCENE_CAMERA_PREFLIGHT passed=%u samples=%u collision_volume=1 subject_shots_adapted=%u visible_participant_each_frame=%u controls_unmodified=1",unsigned(valid),samples,repairedShots,unsigned(valid));Log(note);}return valid;
 }
 void BlendRelease(D3DXMATRIX& m,const D3DXMATRIX& native){
  D3DXMATRIX a,b;D3DXMatrixInverse(&a,nullptr,&releaseView);D3DXMatrixInverse(&b,nullptr,&native);
  D3DXVECTOR3 sa,sb,pa,pb;D3DXQUATERNION qa,qb,q;D3DXMatrixDecompose(&sa,&qa,&pa,&a);D3DXMatrixDecompose(&sb,&qb,&pb,&b);
  float t=Ease(elapsed-releasedAt);D3DXQuaternionSlerp(&q,&qa,&qb,t);D3DXMatrixRotationQuaternion(&a,&q);auto p=pa+(pb-pa)*t;a._41=p.x;a._42=p.y;a._43=p.z;D3DXMatrixInverse(&m,nullptr,&a);
 }
} scene;
