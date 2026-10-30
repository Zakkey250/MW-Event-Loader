#pragma once
#include "DriftLinePolicy.h"
// Drift-only reference line. Every shifted point and connecting segment is
// checked on the captured world/deck, including the complete rotated chassis.
// The ordinary grip optimizer must not pull this line back to the inner wall.
static bool NeoBuildDriftLine(const CollisionWorld& world,const std::vector<YellowRecoveryPoint>& road,const std::vector<mwel_drift_line::Bend>& bends,float station,
 const mwneo::Vehicle& vehicle,YellowRacingLine& out,const YellowGatePassage& passage){
 out={};out.passage=passage;if(road.size()<3)return false;
 if(bends.empty())return false;
 Vec3 origin{},ahead{};if(!YellowLineSample(road,station,origin)||!YellowLineSample(road,std::min(road.back().station,station+4),ahead))return false;
 auto tangent=mwneo::UnitXZ({ahead.x-origin.x,0,ahead.z-origin.z});
 float lateral=mwneo::CrossXZ(tangent,{vehicle.position.x-origin.x,0,vehicle.position.z-origin.z});
 const float blendDistance=std::clamp(vehicle.speed*1.2f,24.f,50.f),width=std::clamp(vehicle.halfWidth,.8f,3.5f)+.55f;
 const float end=std::min(road.back().station,station+std::clamp(vehicle.speed*3+90,150.f,250.f));
 out.points.push_back({{vehicle.position.x,vehicle.position.y,vehicle.position.z},station});
 for(float s=std::min(station+4,end);s>station;s=std::min(s+4,end)){
  Vec3 center{},before{},after{};
  if(!YellowLineSample(road,s,center)||!YellowLineSample(road,std::max(road.front().station,s-3),before)||!YellowLineSample(road,std::min(road.back().station,s+3),after))return false;
  auto t=mwneo::UnitXZ({after.x-before.x,0,after.z-before.z});if(mwneo::LengthXZ(t)<.9f)return false;
  float request=mwel_drift_line::Offset(bends,s,vehicle.speed),blend=mwel_drift_line::Smooth((s-station)/blendDistance);
  const float offset=lateral+(request-lateral)*blend;bool accepted=false;Vec3 chosen{};
  for(float scale:{1.f,.75f,.5f,.25f,0.f}){
   const float side=offset*scale;Vec3 target{center.x-t.z*side,center.y,center.z+t.x*side},supported{};
   if(!YellowAimGround(world,target,center.y,.65f,supported))continue;
   const auto& p=out.points.back();Vec3 prior{p.p[0],p.p[1],p.p[2]};
   if(!YellowLaneReserve(world,supported,t.x,t.z,width,.5f)||!YellowGateEdgeClear(passage,prior,supported,s,p.station)||!YellowLineSupported(world,prior,supported,width,.65f,out.points.size()==1))continue;
   chosen=supported;accepted=true;break;
  }
  if(!accepted){out.truncated=true;break;}
  out.points.push_back({{chosen.x,chosen.y,chosen.z},s});if(s>=end)break;
 }
 return out.points.size()>=5;
}
