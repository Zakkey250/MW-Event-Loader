#pragma once
#include "NeoDriftRoad.h"
#include <vector>
#include "DriftCurvePolicy.h"
namespace mwel_drift_line {
struct Bend {float begin=0,end=0,apex=0;int side=0;float turn=0;bool linked=false;};
inline float Smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
// A distant grip target cuts across the outside preparation and early apex.
// Velocity-path control needs a nearer, continuously swept execution target.
inline float SteeringLook(float speed,bool nativeSpecial=false){
 // Native-physics slide exits were preceded by the turn request falling
 // while gas remained full. Follow a nearer swept execution arc instead of
 // straightening toward a target already beyond the bend. No pose/force help.
 return nativeSpecial?std::clamp(4.f+std::abs(speed)*.4f,14.f,22.f):std::clamp(6.f+std::abs(speed)*.65f,18.f,28.f);
}
inline float NativeAimRate(const mwneo::Vehicle& car,mwneo::Point aim){
 const auto travel=mwneo::UnitXZ(car.velocity),delta=mwneo::Sub(aim,car.position);
 const float square=mwneo::DotXZ(delta,delta);
 if(!mwneo::Finite(aim)||!mwneo::Finite(car.position)||!mwneo::Finite(car.velocity)||
    !std::isfinite(car.speed)||car.speed<=5||mwneo::LengthXZ(travel)<.9f||square<=4||mwneo::DotXZ(travel,delta)<2)
  return std::numeric_limits<float>::quiet_NaN();
 return 2*mwneo::CrossXZ(travel,delta)/square*car.speed;
}
template<class Road>std::vector<Bend> Bends(const Road& road,bool linked=false){
 std::vector<Bend> out;if(road.size()<3)return out;
 bool active=false;Bend current{};float angle=0;
 auto finish=[&](){if(active&&current.end-current.begin>=18&&angle>=.20f){
  // Half of the accumulated road heading is the apex. It is independent of
  // the car's current lookahead, checkpoint number, speed and frame rate.
  float sum=0;current.apex=(current.begin+current.end)*.5f;
  for(float s=current.begin;s<current.end;s+=2){sum+=std::abs(NeoDriftRoadCurvature(road,s))*std::min(2.f,current.end-s);if(sum>=angle*.5f){current.apex=s;break;}}
  current.turn=angle;current.linked=linked;out.push_back(current);
 }active=false;angle=0;};
 for(float s=road.front().station;s<road.back().station;s+=2){
  const float k=NeoDriftRoadCurvature(road,s),step=std::min(2.f,road.back().station-s);const int side=k>mwel_drift_curve::Minimum?1:k<-mwel_drift_curve::Minimum?-1:0;
  if(!side){finish();continue;}if(active&&side!=current.side)finish();
  if(!active){active=true;current={s,s+step,s,side};}else current.end=s+step;
  angle+=std::abs(k)*step;
 }finish();return out;
}
inline float Offset(const std::vector<Bend>& bends,float station,float speed,float outside=2.2f){
 const float prepare=std::clamp(speed*2.5f,45.f,90.f),exit=std::clamp(speed*1.8f,35.f,65.f);
 float sum=0,weight=0;
 for(const auto& b:bends){if(station<b.begin-prepare||station>b.end+exit)continue;
  float side=0,w=1;
  if(station<b.begin){w=Smooth((station-b.begin+prepare)/prepare);side=-outside;}
  else if(station<=b.apex)side=-outside+(outside+.25f)*Smooth((station-b.begin)/std::max(1.f,b.apex-b.begin));
  else if(station<=b.end)side=.25f-(outside+.25f)*Smooth((station-b.apex)/std::max(1.f,b.end-b.apex));
  else {w=1-Smooth((station-b.end)/exit);side=-outside;}
  // Connect shallow, long bends with steering arcs across the road width.
  // Station anchors the phase across replans/checkpoints. This changes only
  // reference geometry: support, gate, rotated-body and wall sweeps still
  // validate every chosen point and the actual execution arc.
  const float length=b.end-b.begin,mean=length>0?b.turn/length:0;
  if(b.linked&&length>=100&&mean>=.002f&&mean<=.02f&&station>b.begin&&station<b.end&&outside>0){
   constexpr float wavelength=60.f,tau=6.28318530718f;
   // The full wave remains inside the original OIO lateral reservation,
   // including both signs. Never buy a slide by crossing a wall boundary.
   const float amplitude=std::min(1.6f,std::max(0.f,outside-std::abs(side))*.8f);
   const float fade=Smooth((station-b.begin)/20)*Smooth((b.end-station)/20);
   side+=amplitude*fade*std::sin(tau*(station-b.begin)/wavelength);
  }
  sum+=side*b.side*w;weight+=w;
 }
 return weight>1?sum/weight:sum;
}
inline bool EntryPrepared(const std::vector<Bend>& bends,float station,float lateral){
 for(const auto& b:bends)if(station>=b.begin-12&&station<=b.end-30)
  return lateral*b.side<=.2f;
 return false;
}
}
