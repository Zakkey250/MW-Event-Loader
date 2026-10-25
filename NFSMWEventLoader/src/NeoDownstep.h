#pragma once
#include "NeoDownstepArc.h"
// A short supported drop can become a deadlock after PlanEnd slows the car
// below the normal 15 m/s flight policy. Prove the complete landing at both
// crawl speeds; a partial clear prefix, wall or different deck cannot qualify.
static bool NeoDownstepProof(const CollisionWorld& world,const YellowRacingLine& line,float station,
 const mwneo::Vehicle& v,const YellowTrafficPoint* traffic,unsigned count,Vec3& aim){
 if(std::abs(v.speed)>8||line.points.size()<2||v.recovery==mwneo::Recovery::Reverse)return false;
 YellowTraversableStepScope steps;
 const float p[]={v.position.x,v.position.y,v.position.z},h[]={v.heading.x,v.heading.y,v.heading.z};
 for(float look:{32.f,24.f,16.f,10.f}){
  Vec3 goal;if(!YellowLineSample(line.points,std::min(line.points.back().station,station+look),goal))continue;
  auto delta=mwneo::Sub({goal.x,goal.y,goal.z},v.position);const float distance=mwneo::LengthXZ(delta);
  if(distance<6||distance>40||mwneo::DotXZ(mwneo::UnitXZ(v.heading),mwneo::UnitXZ(delta))<.85f)continue;
  bool proven=true;
  for(float speed:{5.f,6.f,7.f,8.f}){NeoDownstepArcDiagnostic evidence;
   if(!NeoDownstepArc(world,line,station,p,h,goal,v.halfWidth,traffic,count,speed,0,&evidence,12.f,v.halfLength,.05f,true)||!evidence.terrainLanded){proven=false;break;}
  }
  if(proven){aim=goal;return true;}
 }
 return false;
}
struct NeoDownstepCommit {Vec3 origin{},aim{};unsigned edge=~0u;unsigned long long until=0;};
static bool NeoDownstepGuidance(const CollisionWorld& world,const mwneo::Vehicle& v,const YellowRacingLine& line,
 float station,const YellowTrafficPoint* traffic,unsigned count,unsigned long long now,NeoDownstepCommit& commit,mwneo::Guidance& g){
 bool coast=false;
 if(commit.until>now&&commit.edge==g.edge&&std::hypot(v.position.x-commit.origin.x,v.position.z-commit.origin.z)<20&&std::abs(v.speed)<=8){
  RayHit floor;
  const auto direction=mwneo::UnitXZ(mwneo::Sub({commit.aim.x,commit.aim.y,commit.aim.z},v.position));
  coast=mwneo::DotXZ(mwneo::UnitXZ(v.heading),direction)>.7f&&world.Ray({v.position.x,v.position.y+1,v.position.z},{v.position.x,v.position.y-10,v.position.z},floor,true);
 }
 Vec3 aim{};
 if(coast)aim=commit.aim;
 else if(NeoDownstepProof(world,line,station,v,traffic,count,aim))commit={{v.position.x,v.position.y,v.position.z},aim,g.edge,now+3000};
 else {commit={};return false;}
 g.status=1;g.confidence=1;g.aim={aim.x,aim.y,aim.z};g.forward=mwneo::UnitXZ(mwneo::Sub(g.aim,v.position));
 g.predictiveAvoidance|=mwneo::CheckedDownstep|mwneo::SupportedAirborne;g.maneuverSpeed=6.5f;
 g.obstacleDistance=10000;g.leadVehicle=0;g.count=3;
 for(unsigned i=0;i<3;++i){float t=i*.5f;auto& s=g.samples[i];s={};s.position={v.position.x+(aim.x-v.position.x)*t,v.position.y+(aim.y-v.position.y)*t,v.position.z+(aim.z-v.position.z)*t};
  s.distance=std::hypot(s.position.x-v.position.x,s.position.z-v.position.z);s.widthLeft=s.widthRight=5;s.flags=mwneo::Main|mwneo::CheckedExecution;s.roadCurvature=0;}
 return true;
}
