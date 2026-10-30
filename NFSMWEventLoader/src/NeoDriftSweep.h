#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
// Constant measured motion forecast. Checks the swept, rotated chassis;
// it neither requests a route nor changes the live vehicle's pose.
template<class CheckSegment>
float NeoDriftSweep(const mwneo::Vehicle& v,float bodyRate,float travelRate,CheckSegment check){
 if(!mwneo::Finite(v.position)||!mwneo::Finite(v.heading)||!mwneo::Finite(v.velocity)||
    !std::isfinite(bodyRate)||!std::isfinite(travelRate)||std::abs(bodyRate)>3||std::abs(travelRate)>3||
    !std::isfinite(v.speed)||!std::isfinite(v.halfWidth)||!std::isfinite(v.halfLength)||
    mwneo::LengthXZ(v.heading)<.5f||mwneo::LengthXZ(v.velocity)<1||
    v.speed<1||v.speed>120||v.halfWidth<.2f||v.halfWidth>5||v.halfLength<.5f||v.halfLength>12)return -1;
 float heading=std::atan2(v.heading.z,v.heading.x),travel=std::atan2(v.velocity.z,v.velocity.x);
 auto position=v.position;
 const unsigned steps=unsigned(std::ceil(std::max(.8f/.1f,v.speed*.8f/1.5f)));
 const float dt=.8f/steps;
 for(unsigned i=0;i<steps;++i){
  const float nextHeading=heading+bodyRate*dt,mid=travel+travelRate*dt*.5f;
  mwneo::Point next{position.x+std::cos(mid)*v.speed*dt,position.y+v.velocity.y*dt,position.z+std::sin(mid)*v.speed*dt};
  for(float along:{-v.halfLength,0.f,v.halfLength})for(float side:{-v.halfWidth,0.f,v.halfWidth}){
   const mwneo::Point a{position.x+std::cos(heading)*along-std::sin(heading)*side,position.y,position.z+std::sin(heading)*along+std::cos(heading)*side};
   const mwneo::Point b{next.x+std::cos(nextHeading)*along-std::sin(nextHeading)*side,next.y,next.z+std::sin(nextHeading)*along+std::cos(nextHeading)*side};
   if(!check(a,b,position))return float(i)*dt;
  }
  position=next;heading=nextHeading;travel+=travelRate*dt;
 }
 return .8f;
}
