#pragma once
// EN1.3 EngineSpline INISCarEngine, verified against the target image:
// ctor 6B3EF0 registers 620460 at +60, table 8AB7AC; entry +4 is
// SetNeutralRev (6A12D0). It maps 0..1 onto this vehicle's IDLE..RED_LINE.
// No sound bank/physics RPM address is overwritten and no audio thread is added.
struct SceneEngineRevs {
 unsigned rate=0,count=0,slots=0,activeMask=0,updates=0;
 float minRpm=0,maxRpm=0;
 std::vector<float> values;
 void* owned[8]{};
 void Load(const std::string& folder,unsigned participants,unsigned cameraRate,unsigned cameraCount){
  values.clear();rate=count=slots=0;Forget();
  std::ifstream f(folder+"/engine-revs.bin",std::ios::binary);if(!f)return;
  unsigned h[4];SceneAssets::Read(f,h,4);
  if(h[0]!=0x3156524E||h[1]!=participants||h[2]!=cameraRate||h[3]!=cameraCount||h[3]>20000)throw std::runtime_error("Invalid engine rev directory");
  slots=h[1];rate=h[2];count=h[3];values.resize(size_t(slots)*count);SceneAssets::Read(f,values.data(),values.size());
  char extra;if(f.read(&extra,1))throw std::runtime_error("Trailing engine rev data");
  for(auto v:values)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid engine rev value");
 }
 static bool Valid(void* p){
  __try{return p&&*static_cast<unsigned*>(p)==0x8AB7AC&&*reinterpret_cast<unsigned*>(static_cast<BYTE*>(p)-0x60)==0x8AB838;}
  __except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
 static void* Resolve(void* owner){return owner?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x620460):nullptr;}
 void Stop(unsigned slot,void* owner){
  if(!owned[slot])return;
  auto live=Resolve(owner);
  if(live==owned[slot]&&Valid(live))reinterpret_cast<void(__thiscall*)(void*,bool,float,float)>(0x6A12D0)(live,false,0,0);
  owned[slot]=nullptr;activeMask&=~(1u<<slot);
 }
 void Forget(){for(auto& p:owned)p=nullptr;activeMask=0;}
 void Apply(unsigned slot,void* owner,float elapsed){
  if(values.empty()||slot>=slots)return;
  auto live=Resolve(owner);if(!Valid(live)){owned[slot]=nullptr;activeMask&=~(1u<<slot);return;}
  float t=std::clamp(elapsed*rate,0.f,float(count-1));auto i=unsigned(t),j=std::min(i+1,count-1);float a=t-i;
  float rpm=values[slot*count+i]*(1-a)+values[slot*count+j]*a;
  reinterpret_cast<void(__thiscall*)(void*,bool,float,float)>(0x6A12D0)(live,true,rpm,3.f);
  owned[slot]=live;activeMask|=1u<<slot;++updates;
  // Read diagnostics only when caller requests them separately; the target
  // and actual RPM fields belong to this verified live EngineSpline tail.
 }
 void Observe(void* owner){
  auto live=Resolve(owner);if(!Valid(live))return;
  float rpm=*reinterpret_cast<float*>(static_cast<BYTE*>(live)+0xDC);
  if(std::isfinite(rpm)&&rpm>0&&rpm<30000){if(!minRpm||rpm<minRpm)minRpm=rpm;maxRpm=std::max(maxRpm,rpm);}
 }
};
