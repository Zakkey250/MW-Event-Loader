#pragma once
#include "TougeFallPolicy.h"
namespace touge_fall {
using touge_fall_policy::Point;using runtime_lifecycle::Read;
struct Body {unsigned handle=0;bool sampled=false,retired=false;Point previous{};};
unsigned owner=0;std::array<Body,16> bodies{};
void Reset(){owner=0;bodies={};}
bool Retired(unsigned handle){return handle&&std::any_of(bodies.begin(),bodies.end(),[&](const auto& b){return b.handle==handle&&b.retired;});}
bool Destroy(unsigned handle){
 auto count=Read<unsigned>(reinterpret_cast<void*>(0x92CD24));auto vehicles=Read<BYTE**>(reinterpret_cast<void*>(0x92CD1C));if(!vehicles||count>128)return false;
 for(unsigned i=0;i<count;++i){auto v=Read<BYTE*>(vehicles,i*4);auto table=Read<void**>(v);auto get=Read<void*>(table,43*4);if(get!=reinterpret_cast<void*>(0x688230))continue;
  auto ai=reinterpret_cast<BYTE*(__thiscall*)(void*)>(get)(v);if(!ai)continue;auto primary=ai-0x4C;auto sim=Read<BYTE*>(primary,0x34);
  if(Read<BYTE*>(primary,0x48)!=v||Read<unsigned>(sim,8)!=handle)continue;
  auto object=Read<void*>(sim,4);if(!object)return false;
  auto damage=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(object,0x405000);
  if(!combat_health_runtime::Native(unsigned(damage),handle))return false;
  auto vt=Read<void**>(damage);if(Read<void*>(vt,7*4)!=reinterpret_cast<void*>(0x69C540))return false;
  auto native=combat_health_runtime::originalDestroy;
  if(!native){const BYTE guard[]={0x6A,0xFF,0x68,0x1F,0xC4,0x87,0,0x64,0xA1};if(memcmp(reinterpret_cast<void*>(0x69C540),guard,sizeof(guard)))return false;native=reinterpret_cast<decltype(native)>(0x69C540);}
  native(damage);return true;
 }return false;
}
Point Authored(const float* p){return {p[2],-p[0],p[1]};}
void FrameUnsafe(){
 auto p=event_race_policies::Current();if(!p||!touge_grid::Active(*p)){if(owner)Reset();return;}
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));if(!race||Read<unsigned>(race,0x1960)!=1||!Read<BYTE>(race,0x198A)||!Read<BYTE>(race,0x1978))return;
 auto key=MWELNeoCurrentKey();if(owner!=key){Reset();owner=key;}
 auto found=event_race_progress::courses.find(key);if(found==event_race_progress::courses.end())return;
 unsigned count=Read<unsigned>(race,0x1958);if(!count||count>16)return;
 for(unsigned slot=0;slot<count;++slot){auto info=race+0x18+slot*0x194;unsigned handle=Read<unsigned>(info);auto& state=bodies[slot];
  if(state.handle!=handle){state={};state.handle=handle;}
  if(!handle||state.retired||Read<BYTE>(info,0x21)||Read<unsigned>(info,0x1C))continue;
  NeoBodyMotion body{};if(!runtime_lifecycle::Body(handle,body))continue;Point current=Authored(body.position);
  int pending=Read<int>(info,0x2C);if(pending<0||unsigned(pending)>=found->second.legs.size()){state.sampled=false;continue;}
  const auto& leg=found->second.legs[unsigned(pending)];bool fell=false;
  if(state.sampled)for(const auto& g:p->authoring.value("extras",mwel::Json::array()))if(g.value("nativeTemplate",std::string())=="touge_fall"){
   touge_fall_policy::Gate gate;for(unsigned i=0;i<3;++i)gate.center[i]=g.at("position").at(i).get<float>();gate.depth=g.at("dimensions").at(0).get<float>();gate.width=g.at("dimensions").at(1).get<float>();gate.rotation=g.at("rotation").get<float>();
   if(touge_fall_policy::DownCrossing(gate,state.previous,current)){fell=true;break;}
  }
  // Detached cliff falls also work without individually placed gates. Compare
  // only the pending road leg, not an unrelated mainland/upper/lower route.
  float best=std::numeric_limits<float>::infinity(),height=0,horizontal=0;
  auto project=[&](const mwel_progress::Path& path){for(size_t i=1;i<path.size();++i){const auto& a=path[i-1].position;const auto& b=path[i].position;float x=b[0]-a[0],z=b[2]-a[2],den=x*x+z*z;if(den<1e-6f)continue;
   float t=std::clamp(((body.position[0]-a[0])*x+(body.position[2]-a[2])*z)/den,0.f,1.f);float dx=body.position[0]-a[0]-t*x,dz=body.position[2]-a[2]-t*z,d=dx*dx+dz*dz;
   const float roadHeight=a[1]+t*(b[1]-a[1]),dy=body.position[1]-roadHeight,score=d+dy*dy;
   // At stacked/overlapping bends, the nearby lower road is a valid deck.
   // Select by full body-to-road separation before measuring a vertical fall.
   if(score<best){best=score;height=roadHeight;horizontal=std::sqrt(d);}
  }
  };project(leg.main);for(const auto& path:leg.alternatives)project(path);
  if(state.sampled&&touge_fall_policy::Continuous(state.previous,current)&&touge_fall_policy::BelowRoad(height,body.position[1],body.velocity[1],horizontal))fell=true;
  state.previous=current;state.sampled=true;
  if(fell){if(!Destroy(handle)){Log("EXPERIMENT_RACE_ABORT touge_native_fall_retirement_unavailable slot=%u handle=%u",slot,handle);return;}
   state.retired=true;Log("TOUGE_FALL_CRASH slot=%u handle=%u player=%u body=%.3f,%.3f,%.3f native_destroy_event=1 direct_result_written=0",slot,handle,unsigned(slot==0),current[0],current[1],current[2]);return;}
 }
}
void Frame(){__try{FrameUnsafe();}__except(EXCEPTION_EXECUTE_HANDLER){Log("EXPERIMENT_RACE_ABORT touge_fall_runtime_fault=%08X",unsigned(GetExceptionCode()));}}
}
void MWELTougeNewGeneration(){touge_fall::Reset();runtime_lifecycle::tougeReturnInFlight=runtime_lifecycle::tougeExitAwaiting=runtime_lifecycle::tougeReturnPending=false;}
