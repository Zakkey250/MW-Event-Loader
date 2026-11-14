#pragma once
#include "SceneRoadPath.h"
namespace mwel_scene_motion {
using mwel_scene_road::Point;
struct Motion {Point velocity{};float speed=0,longitudinal=0,lateral=0,rev=0;bool valid=false;};
// Native SoundTraffic uses signed longitudinal slip and negative lateral
// skid velocity, in m/s. A cut is silence, not a tyre impact. Retain the
// native burnout simulation rather than replacing its authored wheelspin.
inline float Skid(const Motion& motion,float burnout){
 if(!motion.valid||!std::isfinite(burnout)||std::abs(burnout)>.01f||motion.speed<3)return 0;
 return std::clamp(std::abs(motion.lateral)-1.5f,0.f,8.f);
}
inline Motion Measure(Point a,Point b,Point forward,float seconds,float burnout=0){
 using namespace mwel_scene_road;
 Motion out;if(!Finite(a)||!Finite(b)||!Finite(forward)||!std::isfinite(seconds)||seconds<.001f||seconds>.25f||!std::isfinite(burnout))return out;
 forward=Unit(forward);if(Length(forward)<.9f)return out;
 const auto velocity=(b-a)*(1/seconds);
 const float speed=Length(velocity);
 // Cuts/teleports do not become a one-frame 1000 km/h tyre/audio command.
 if(speed>130||!Finite(velocity))return out;
 out.velocity=velocity;out.speed=speed;out.longitudinal=Dot(velocity,forward);
 out.lateral=Dot(velocity,{forward.y,-forward.x,0});out.valid=true;
 out.rev=std::abs(burnout)>1?.85f:std::clamp(.18f+speed/130.f,.18f,.92f);
 return out;
}
}
