#pragma once
#include "../../src/SceneMotionPolicy.h"
// Verified EN1.3 SuspensionSpline ISuspension. SetWheelAngularVelocity is a
// no-op in this implementation; native MatchSpeed supplies all four radii.
// Never replace a vtable, cache a wheel object, or write an unknown tyre field.
struct SceneWheelMotion {
 unsigned updates=0;float maximumAngular=0;
 static bool Valid(void* p){
  __try{return p&&*static_cast<unsigned*>(p)==0x8ABDC0&&(*reinterpret_cast<unsigned**>(p))[28]==0x691610;}
  __except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
 static void* Resolve(void* owner){return owner?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x5D59F0)(owner,0x404040):nullptr;}
 void Apply(void* owner,float signedSpeed,float burnout){
  auto live=Resolve(owner);if(!Valid(live)||!std::isfinite(signedSpeed)||std::abs(signedSpeed)>130)return;
  // Source burnout owns its driven wheels. MatchSpeed clears skid/slip, so
  // do not overwrite an active authored burnout with ordinary rolling speed.
  if(std::abs(burnout)<=.01f)reinterpret_cast<void(__thiscall*)(void*,float)>(0x691610)(live,signedSpeed);
  ++updates;
 }
 void Observe(void* owner){
  auto live=Resolve(owner);if(!Valid(live))return;
  for(unsigned i=0;i<4;++i){float angular=reinterpret_cast<float(__thiscall*)(void*,unsigned)>(0x6AE200)(live,i);if(std::isfinite(angular))maximumAngular=std::max(maximumAngular,std::abs(angular));}
 }
};
