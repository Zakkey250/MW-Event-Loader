#pragma once
#include "../../src/CarbonIntroPolicy.h"
struct CameraSample {D3DXVECTOR3 eye,look;float dutch,lens,timeScale,fade;};
struct SceneAssets {
 static constexpr unsigned MaxCars=9;
 struct Cue{float time;unsigned slot;float speed;};std::vector<Cue> cues;
 std::vector<float> actorClock;unsigned participants=8;
 std::vector<CameraSample> cameras;unsigned rate=0;float duration=0;
 std::vector<D3DXMATRIX> cars[MaxCars];
 bool hasActor=true,rollingStart=false;
 bool StartsStopped()const{
  if(cars[0].size()<8)return false;
  for(unsigned i=1;i<8;++i){const auto d=D3DXVECTOR3(cars[0][i]._41-cars[0][i-1]._41,cars[0][i]._42-cars[0][i-1]._42,cars[0][i]._43-cars[0][i-1]._43);const auto speed=D3DXVec3Length(&d)*15;if(!std::isfinite(speed)||speed>.2f)return false;}
  return true;
 }
 std::vector<float> cameraRoadStations;
 std::vector<unsigned> cameraRoadReferences;
 std::vector<float> cameraLookRoadStations;
 std::vector<unsigned> cameraLookRoadReferences;
 std::vector<D3DXVECTOR3> cameraRoadOrigins,cameraLookRoadOrigins;
 template<class T>static void Read(std::ifstream& f,T* p,size_t n=1){if(!f.read(reinterpret_cast<char*>(p),sizeof(T)*n))throw std::runtime_error("Private scene data truncated");}
 void Load(const std::string& folder){
  const auto sceneName=folder.substr(folder.find_last_of("\\/")+1);
  const auto rearField=mwel_carbon::RpdSprintFieldCount(sceneName);
  std::ifstream presentation(folder+"/presentation.bin",std::ios::binary);
  if(presentation){unsigned p[2];Read(presentation,p,2);char extra;
   const auto name=folder.substr(folder.find_last_of("\\/")+1);
   if(p[0]!=0x31504943||p[1]!=0||presentation.read(&extra,1))throw std::runtime_error("Invalid camera-only presentation");
   hasActor=false;
  }
  std::ifstream f(folder+"/camera.cic",std::ios::binary);unsigned h[3];Read(f,h,3);Read(f,&duration);
  if(h[0]!=0x31434943||h[1]<2||h[1]>20000||h[2]!=120||!std::isfinite(duration)||duration<5||duration>90)throw std::runtime_error("Invalid source camera");
  rate=h[2];cameras.resize(h[1]);Read(f,cameras.data(),cameras.size());
  for(auto& s:cameras)for(float v:*reinterpret_cast<float(*)[10]>(&s))if(!std::isfinite(v))throw std::runtime_error("Invalid camera sample");
  // Older exported packs treated signed fade undershoot as uint32. Float32
  // has already lost the low bits, so retire this malformed percentage as
  // transparent rather than guessing the original negative interpolation.
  for(auto& s:cameras)if(s.fade>=2147483648.f)s.fade=0;
  std::ifstream c(folder+"/cars.cic",std::ios::binary);Read(c,h,3);
  if(h[0]!=0x52434943||h[1]<1||(rearField?h[1]!=rearField:h[1]>8)||h[2]!=15)throw std::runtime_error("Invalid source car tracks");
  participants=h[1];
  std::ifstream rolling(folder+"/rolling-start.bin",std::ios::binary);
  if(rolling){unsigned header[2];Read(rolling,header,2);char extra;
   const auto name=folder.substr(folder.find_last_of("\\/")+1);
   if(name.rfind("HPR_",0)!=0||hasActor||header[0]!=0x31535052||header[1]!=1||rolling.read(&extra,1))throw std::runtime_error("Invalid rolling intro");
   rollingStart=true;
  }
  for(unsigned slot=0;slot<participants;slot++){auto& car=cars[slot];unsigned count;Read(c,&count);if(!count||count>1000)throw std::runtime_error("Invalid car track size");car.resize(count);Read(c,car.data(),count);
   for(auto& m:car){
    for(float v:*reinterpret_cast<float(*)[16]>(&m))if(!std::isfinite(v))throw std::runtime_error("Invalid car matrix");
    D3DXVECTOR3 scale,position;D3DXQUATERNION rotation;
    if(FAILED(D3DXMatrixDecompose(&scale,&rotation,&position,&m))||scale.x<.5f||scale.y<.5f||scale.z<.5f||scale.x>2||scale.y>2||scale.z>2||std::abs(m._14)>.001f||std::abs(m._24)>.001f||std::abs(m._34)>.001f||std::abs(m._44-1)>.001f)throw std::runtime_error("Invalid car transform");
   }}
  if(rearField&&!rollingStart)throw std::runtime_error("Invalid police rear staging field");
  if(rollingStart){std::ifstream r(folder+"/camera-road-stations.bin",std::ios::binary);unsigned header[2];Read(r,header,2);
   if((header[0]!=0x32534352&&header[0]!=0x33534352)||header[1]!=cameras.size())throw std::runtime_error("Rolling camera road frames required");
   cameraRoadStations.resize(header[1]);cameraRoadReferences.resize(header[1]);
   cameraLookRoadStations.resize(header[1]);cameraLookRoadReferences.resize(header[1]);
   for(unsigned i=0;i<header[1];++i){Read(r,&cameraRoadStations[i]);Read(r,&cameraRoadReferences[i]);
    if(header[0]==0x33534352){Read(r,&cameraLookRoadStations[i]);Read(r,&cameraLookRoadReferences[i]);}
    else {cameraLookRoadStations[i]=cameraRoadStations[i];cameraLookRoadReferences[i]=cameraRoadReferences[i];}
    if(cameraRoadReferences[i]>64||cameraLookRoadReferences[i]>64)throw std::runtime_error("Invalid camera avatar");}
   char extra;if(r.read(&extra,1))throw std::runtime_error("Trailing camera road frames");
   for(float station:cameraRoadStations)if(!std::isfinite(station)||std::abs(station)>3000)throw std::runtime_error("Invalid camera road frame");
   for(float station:cameraLookRoadStations)if(!std::isfinite(station)||std::abs(station)>3000)throw std::runtime_error("Invalid look road frame");
  }
  cameraRoadOrigins.clear();cameraLookRoadOrigins.clear();
  if(rollingStart){std::ifstream origins(folder+"/camera-road-origins.bin",std::ios::binary);if(origins){unsigned header[2];Read(origins,header,2);
   if(header[0]!=0x314F4352||header[1]!=cameras.size())throw std::runtime_error("Invalid camera road origins");
   cameraRoadOrigins.resize(header[1]);cameraLookRoadOrigins.resize(header[1]);
   for(unsigned i=0;i<header[1];++i){Read(origins,&cameraRoadOrigins[i]);Read(origins,&cameraLookRoadOrigins[i]);
    for(auto p:{cameraRoadOrigins[i],cameraLookRoadOrigins[i]})if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||D3DXVec3Length(&p)>6000)throw std::runtime_error("Invalid camera reference origin");
    if(std::abs(cameraRoadOrigins[i].y-cameraRoadStations[i])>.01f||std::abs(cameraLookRoadOrigins[i].y-cameraLookRoadStations[i])>.01f)throw std::runtime_error("Camera origin/station mismatch");
   }char extra;if(origins.read(&extra,1))throw std::runtime_error("Trailing camera road origins");
  }}
  std::ifstream q(folder+"/cues.cic",std::ios::binary);Read(q,h,2);if(h[0]!=0x43424943||h[1]>128)throw std::runtime_error("Invalid cue header");cues.resize(h[1]);Read(q,cues.data(),cues.size());
  float previous=-1;for(auto cue:cues){if(!std::isfinite(cue.time)||cue.time<previous||cue.time>duration||cue.slot>=participants||(cue.speed!=0&&cue.speed!=100))throw std::runtime_error("Invalid source cue");previous=cue.time;}
  std::ifstream clock(folder+"/actor-clock.bin",std::ios::binary);
  if(clock){unsigned h[2];Read(clock,h,2);if(h[0]!=rate||h[1]!=cameras.size())throw std::runtime_error("Invalid actor clock");actorClock.resize(h[1]);Read(clock,actorClock.data(),actorClock.size());for(float t:actorClock)if(!std::isfinite(t)||t<0||t>90)throw std::runtime_error("Invalid actor time");}
 }
 void CompressRollingTravel(float goSeconds){
  if(!rollingStart||cameraRoadOrigins.size()!=cameras.size()||cameraLookRoadOrigins.size()!=cameras.size())return;
  // Source Y is longitudinal road station. Compress this spatial coordinate,
  // not time, model size or the local camera arm. Each eye/look may follow a
  // different native avatar; translating both by the player's delta breaks
  // drive-by shots and sends their road frames to unrelated road segments.
  const auto go=Car(0,ActorTime(goSeconds));const D3DXVECTOR3 anchor(go._41,go._42,go._43);
  auto shortened=[&](D3DXVECTOR3 origin){return anchor+(origin-anchor)/3.f;};
  for(unsigned slot=0;slot<participants;++slot)for(auto& m:cars[slot]){auto p=shortened({m._41,m._42,m._43});m._41=p.x;m._42=p.y;m._43=p.z;}
  for(unsigned n=0;n<cameras.size();++n){
   auto eye=cameraRoadOrigins[n],look=cameraLookRoadOrigins[n];
   cameraRoadOrigins[n]=shortened(eye);cameraLookRoadOrigins[n]=shortened(look);
   cameras[n].eye+=cameraRoadOrigins[n]-eye;cameras[n].look+=cameraLookRoadOrigins[n]-look;
   // HP's elevated camera-only avatar has a 116m sideways boom. A dense
   // city has rooftops in that space, often without vehicle collision at
   // roof height. Keep that aerial shot over its connected road corridor;
   // ordinary car-relative close shots retain their exact native arms.
   if(eye.z>20.f){
    cameras[n].eye.x=cameraRoadOrigins[n].x+std::clamp(cameras[n].eye.x-cameraRoadOrigins[n].x,-4.f,4.f);
    cameras[n].eye.z=std::min(cameras[n].eye.z,30.f);
   }
   cameraRoadStations[n]=cameraRoadOrigins[n].y;cameraLookRoadStations[n]=cameraLookRoadOrigins[n].y;
  }
 }
 float ActorTime(float seconds)const{
  if(actorClock.empty())return seconds;
  // Native cuts may jump backwards or forwards. Do not interpolate across cuts.
  auto i=std::min(unsigned(std::max(0.f,seconds)*rate),unsigned(actorClock.size()-1));return actorClock[i];
 }
 CameraSample Camera(float seconds)const{
  float t=std::clamp(seconds*rate,0.f,float(cameras.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(cameras.size()-1));float a=t-i;
  if(!cameraRoadReferences.empty()&&(cameraRoadReferences[i]!=cameraRoadReferences[j]||cameraLookRoadReferences[i]!=cameraLookRoadReferences[j]))return cameras[i];
  CameraSample s;auto p=reinterpret_cast<float*>(&s);auto x=reinterpret_cast<const float*>(&cameras[i]);auto y=reinterpret_cast<const float*>(&cameras[j]);
  for(unsigned k=0;k<10;k++)p[k]=x[k]+(y[k]-x[k])*a;return s;
 }
 float CameraRoadStation(float seconds,bool look=false)const{
  const auto& stations=look?cameraLookRoadStations:cameraRoadStations;const auto& references=look?cameraLookRoadReferences:cameraRoadReferences;
  if(stations.empty())return 0;float t=std::clamp(seconds*rate,0.f,float(stations.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(stations.size()-1));
  if(cameraRoadReferences[i]!=cameraRoadReferences[j]||cameraLookRoadReferences[i]!=cameraLookRoadReferences[j])return stations[i];
  return references[i]==references[j]?stations[i]+(stations[j]-stations[i])*(t-i):stations[i];
 }
 bool CameraSharedRoadReference(float seconds)const{
  if(cameraRoadReferences.empty())return false;
  const auto i=std::min(unsigned(std::max(0.f,seconds)*rate),unsigned(cameraRoadReferences.size()-1));
  return cameraRoadReferences[i]==cameraLookRoadReferences[i];
 }
 D3DXVECTOR3 CameraRoadOrigin(float seconds,bool look=false)const{
  const auto& origins=look?cameraLookRoadOrigins:cameraRoadOrigins;
  if(origins.empty()){const auto car=Car(0,ActorTime(seconds));return {car._41,car._42,car._43};}
  const float t=std::clamp(seconds*rate,0.f,float(origins.size()-1));const auto i=unsigned(t),j=std::min(i+1,unsigned(origins.size()-1));
  if(cameraRoadReferences[i]!=cameraRoadReferences[j]||cameraLookRoadReferences[i]!=cameraLookRoadReferences[j])return origins[i];
  return origins[i]+(origins[j]-origins[i])*(t-i);
 }
 D3DXMATRIX Car(unsigned index,float seconds)const{
  auto& track=cars[index];float t=std::clamp(seconds*15,0.f,float(track.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(track.size()-1));
  D3DXVECTOR3 sa,sb,pa,pb;D3DXQUATERNION qa,qb,q;D3DXMatrixDecompose(&sa,&qa,&pa,&track[i]);D3DXMatrixDecompose(&sb,&qb,&pb,&track[j]);float a=t-i;
  D3DXQuaternionSlerp(&q,&qa,&qb,a);D3DXMATRIX m;D3DXMatrixRotationQuaternion(&m,&q);auto p=pa+(pb-pa)*a;m._41=p.x;m._42=p.y;m._43=p.z;return m;
 }
};
