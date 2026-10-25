#pragma once
#include "NeoBridgeFlight.h"
// A ground-only lane planner can stay on the side of a bridge that has
// support longest, then aim across a lower road's wall. Try a smooth return
// to the committed centre before takeoff. This never changes native poses.
static bool NeoBridgeApproach(const CollisionWorld& world,const std::vector<YellowRecoveryPoint>& road,
 const mwneo::Vehicle& car,float station,YellowGatePassage passage,YellowRacingLine& seed){
 seed={};if(road.size()<2)return false;
 Vec3 center{},next{};if(!YellowLineSample(road,station,center)||!YellowLineSample(road,station+2,next))return false;
 auto tangent=mwneo::UnitXZ({next.x-center.x,0,next.z-center.z});
 float alignment=mwneo::DotXZ(tangent,mwneo::UnitXZ(car.heading));if(alignment<.7f)return false;
 float lateral=mwneo::CrossXZ(tangent,{car.position.x-center.x,0,car.position.z-center.z});if(std::abs(lateral)>10)return false;
 float derivative=mwneo::CrossXZ(tangent,mwneo::UnitXZ(car.heading))/alignment;
 const float blend=std::clamp(std::abs(car.speed)*.6f,24.f,48.f),width=std::clamp(car.halfWidth,.8f,1.5f)+.35f;
 Vec3 previous{car.position.x,car.position.y,car.position.z};RayHit floor;
 if(!world.Ray({previous.x,previous.y+1,previous.z},{previous.x,previous.y-3,previous.z},floor,true))return false;
 float clearance=std::clamp(previous.y-floor.point.y,.15f,1.f);auto before=mwneo::UnitXZ(car.heading);
 seed.passage=passage;seed.points.push_back({{previous.x,previous.y,previous.z},station});
 for(float s=station+2;s<std::min(road.back().station,station+160);s+=2){
  Vec3 p{},q{};if(!YellowLineSample(road,s,p)||!YellowLineSample(road,s+2,q))return false;
  auto forward=mwneo::UnitXZ({q.x-p.x,0,q.z-p.z});float t=std::clamp((s-station)/blend,0.f,1.f);
  float offset=lateral*(2*t*t*t-3*t*t+1)+derivative*blend*(t*t*t-2*t*t+t);
  p.x-=forward.z*offset;p.z+=forward.x*offset;
  Vec3 surface;if(!NeoDownstepAimGround(world,p,previous.y,clearance,surface)){
   seed.truncated=true;return seed.points.size()>=5&&s-station>10;
  }
  unsigned failure=0;
  if(!NeoDownstepLineSupported(world,previous,surface,width,clearance,true,&failure)){
   if(failure==2){seed.truncated=true;return seed.points.size()>=5&&s-station>10;}
   return false;
  }
  if(!YellowGateEdgeClear(passage,previous,surface))return false;
  auto direction=mwneo::UnitXZ({surface.x-previous.x,0,surface.z-previous.z});
  const float grade=std::clamp((surface.y-previous.y)/std::max(.1f,std::hypot(surface.x-previous.x,surface.z-previous.z)),-.35f,.35f);
  for(float front:{-car.halfLength,car.halfLength})for(float side:{-car.halfWidth,0.f,car.halfWidth}){
   Vec3 a{previous.x+before.x*front-before.z*side,previous.y+grade*front,previous.z+before.z*front+before.x*side};
   Vec3 b{surface.x+direction.x*front-direction.z*side,surface.y+grade*front,surface.z+direction.z*front+direction.x*side};
   if(!NeoDownstepWallCorridor(world,a,b,.05f,true,.025f,&previous))return false;
  }
  seed.points.push_back({{surface.x,surface.y,surface.z},s});previous=surface;before=direction;
 }
 return false;
}
