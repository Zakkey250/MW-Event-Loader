#pragma once
struct CameraSample {D3DXVECTOR3 eye,look;float dutch,lens,timeScale,fade;};
struct SceneAssets {
 struct Cue{float time;unsigned slot;float speed;};std::vector<Cue> cues;
 std::vector<float> actorClock;unsigned participants=8;
 std::vector<CameraSample> cameras;unsigned rate=0;float duration=0;
 std::vector<D3DXMATRIX> cars[8];
 bool hasActor=true;
 template<class T>static void Read(std::ifstream& f,T* p,size_t n=1){if(!f.read(reinterpret_cast<char*>(p),sizeof(T)*n))throw std::runtime_error("Private scene data truncated");}
 void Load(const std::string& folder){
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
  if(h[0]!=0x52434943||(h[1]<1||h[1]>8)||h[2]!=15)throw std::runtime_error("Invalid source car tracks");
  participants=h[1];
  for(unsigned slot=0;slot<participants;slot++){auto& car=cars[slot];unsigned count;Read(c,&count);if(!count||count>1000)throw std::runtime_error("Invalid car track size");car.resize(count);Read(c,car.data(),count);
   for(auto& m:car){
    for(float v:*reinterpret_cast<float(*)[16]>(&m))if(!std::isfinite(v))throw std::runtime_error("Invalid car matrix");
    D3DXVECTOR3 scale,position;D3DXQUATERNION rotation;
    if(FAILED(D3DXMatrixDecompose(&scale,&rotation,&position,&m))||scale.x<.5f||scale.y<.5f||scale.z<.5f||scale.x>2||scale.y>2||scale.z>2||std::abs(m._14)>.001f||std::abs(m._24)>.001f||std::abs(m._34)>.001f||std::abs(m._44-1)>.001f)throw std::runtime_error("Invalid car transform");
   }}
  std::ifstream q(folder+"/cues.cic",std::ios::binary);Read(q,h,2);if(h[0]!=0x43424943||h[1]>128)throw std::runtime_error("Invalid cue header");cues.resize(h[1]);Read(q,cues.data(),cues.size());
  float previous=-1;for(auto cue:cues){if(!std::isfinite(cue.time)||cue.time<previous||cue.time>duration||cue.slot>=participants||(cue.speed!=0&&cue.speed!=100))throw std::runtime_error("Invalid source cue");previous=cue.time;}
  std::ifstream clock(folder+"/actor-clock.bin",std::ios::binary);
  if(clock){unsigned h[2];Read(clock,h,2);if(h[0]!=rate||h[1]!=cameras.size())throw std::runtime_error("Invalid actor clock");actorClock.resize(h[1]);Read(clock,actorClock.data(),actorClock.size());for(float t:actorClock)if(!std::isfinite(t)||t<0||t>90)throw std::runtime_error("Invalid actor time");}
 }
 float ActorTime(float seconds)const{
  if(actorClock.empty())return seconds;
  // Native cuts may jump backwards or forwards. Do not interpolate across cuts.
  auto i=std::min(unsigned(std::max(0.f,seconds)*rate),unsigned(actorClock.size()-1));return actorClock[i];
 }
 CameraSample Camera(float seconds)const{
  float t=std::clamp(seconds*rate,0.f,float(cameras.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(cameras.size()-1));float a=t-i;
  CameraSample s;auto p=reinterpret_cast<float*>(&s);auto x=reinterpret_cast<const float*>(&cameras[i]);auto y=reinterpret_cast<const float*>(&cameras[j]);
  for(unsigned k=0;k<10;k++)p[k]=x[k]+(y[k]-x[k])*a;return s;
 }
 D3DXMATRIX Car(unsigned index,float seconds)const{
  auto& track=cars[index];float t=std::clamp(seconds*15,0.f,float(track.size()-1));unsigned i=unsigned(t),j=std::min(i+1,unsigned(track.size()-1));
  D3DXVECTOR3 sa,sb,pa,pb;D3DXQUATERNION qa,qb,q;D3DXMatrixDecompose(&sa,&qa,&pa,&track[i]);D3DXMatrixDecompose(&sb,&qb,&pb,&track[j]);float a=t-i;
  D3DXQuaternionSlerp(&q,&qa,&qb,a);D3DXMATRIX m;D3DXMatrixRotationQuaternion(&m,&q);auto p=pa+(pb-pa)*a;m._41=p.x;m._42=p.y;m._43=p.z;return m;
 }
};
