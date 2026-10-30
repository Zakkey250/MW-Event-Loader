#pragma once
#include <limits>
#include "../../World2MW/src/NeoAI/Types.h"
#include "DriftSpeedPolicy.h"
#include "../../MW-Arcade-Drift-public/src/NativeDriftSteering.h"
// Curvature belongs to the road's knots, not the interpolated chord between
// them. Interpolating knot curvature avoids repeatedly ending a slide in the
// middle of a sparsely authored circular bend. This never changes the path.
template<class Road>float NeoDriftRoadCurvature(const Road& road,float station){
 if(road.size()<3)return 0;
 auto curvature=[&](size_t index){
  index=std::clamp(index,size_t(1),road.size()-2);
  size_t before=index-1,after=index+1;
  auto point=[&](size_t i){return mwneo::Point{road[i].p[0],road[i].p[1],road[i].p[2]};};
  while(before>0&&mwneo::Distance(point(before),point(index))<3)--before;
  while(after+1<road.size()&&mwneo::Distance(point(after),point(index))<3)++after;
  const auto a=point(before),b=point(index),c=point(after);
  return std::copysign(mwneo::Curve(a,b,c),mwneo::CrossXZ(mwneo::Sub(b,a),mwneo::Sub(c,b)));
 };
 auto upper=std::upper_bound(road.begin(),road.end(),station,[](float s,const auto& p){return s<p.station;});
 size_t i=std::clamp(size_t(upper-road.begin()),size_t(1),road.size()-1);
 const float span=road[i].station-road[i-1].station;
 const float t=span>.001f?std::clamp((station-road[i-1].station)/span,0.f,1.f):0;
 return curvature(i-1)*(1-t)+curvature(i)*t;
}
template<class Road>float NeoDriftRoadPeak(const Road& road,float station,float distance){
 if(road.size()<3||!std::isfinite(station)||!std::isfinite(distance)||distance<0)return 0;
 float peak=std::max(std::abs(NeoDriftRoadCurvature(road,station)),std::abs(NeoDriftRoadCurvature(road,station+distance)));
 auto begin=std::lower_bound(road.begin(),road.end(),station,[](const auto& point,float s){return point.station<s;});
 for(auto p=begin;p!=road.end()&&p->station<=station+distance;++p)
  peak=std::max(peak,std::abs(NeoDriftRoadCurvature(road,p->station)));
 return peak;
}
// Back-propagate native steering/tyre limits through remaining braking room.
// A bend eighty metres ahead does not require its apex speed at this position.
// This is a pedal target only; the instantaneous checked steering arc and all
// full-body/world/traffic guards remain authoritative at execution time.
template<class Road>float NeoDriftRoadSpeed(const Road& road,float station,float distance,float speed,float decel){
 if(road.size()<3||!std::isfinite(station)||!std::isfinite(distance)||distance<0||distance>400||
    !std::isfinite(speed)||speed<0||!std::isfinite(decel)||decel<=0)return std::numeric_limits<float>::quiet_NaN();
 float limit=mwel_drift_speed::Maximum;
 const float braking=std::min(decel,4.f),reserve=4.f+speed*.35f;
 auto sample=[&](float ahead){
  const float k=std::abs(NeoDriftRoadCurvature(road,station+ahead));
  if(!std::isfinite(k)){limit=std::numeric_limits<float>::quiet_NaN();return;}
  if(k>.001f){
   const float corner=std::min(mwel_drift_speed::Corner(k),std::max(8.f,mcd::NativeDriftTurnLimit/k));
   limit=std::min(limit,std::sqrt(corner*corner+2*braking*std::max(0.f,ahead-reserve)));
  }
 };
 for(float ahead=0;;ahead=std::min(ahead+4.f,distance)){
  sample(ahead);if(ahead>=distance)break;
 }
 auto knot=std::lower_bound(road.begin(),road.end(),station,[](const auto& p,float s){return p.station<s;});
 for(;knot!=road.end()&&knot->station<=station+distance;++knot)sample(knot->station-station);
 return limit;
}
