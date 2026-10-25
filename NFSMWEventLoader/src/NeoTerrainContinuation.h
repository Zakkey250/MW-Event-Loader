#pragma once
// A lattice ending at a supported downward transition is not a physical
// cul-de-sac. Extend only its committed lane, after a complete future body
// arc proves takeoff AND landing on the same route. Normal truncations keep
// their PlanEnd brake. No native floor/progress/physics writes are involved.
static bool NeoTerrainContinuation(const CollisionWorld& world,const YellowRacingLine& plan,const std::vector<YellowRecoveryPoint>& road,
 float station,float speed,float halfWidth,float halfLength,const YellowTrafficPoint* cars,unsigned count,YellowRacingLine& output,YellowArcDiagnostic* failure=nullptr){
 if(!yellowTraversableSteps||!plan.truncated||plan.points.size()<3||road.size()<2||std::abs(speed)<=15)return false;
 const float stop=plan.points.back().station,begin=std::max(plan.points.front().station,stop-12.f);
 const float end=std::min(road.back().station,begin+std::clamp(std::abs(speed)*2.5f+20.f,80.f,150.f));
 if(end<=stop+12)return false;
 YellowRacingLine continuation;continuation.passage=plan.passage;continuation.points=plan.points;
 for(float s=stop+2;;s=std::min(s+2,end)){
  Vec3 p;if(!NeoPreviewPoint(plan,road,s,p))return false;
  continuation.points.push_back({{p.x,p.y,p.z},s});if(s>=end)break;
 }
 Vec3 from,nextPoint,goal;if(!YellowLineSample(continuation.points,begin,from)||!YellowLineSample(continuation.points,begin+2,nextPoint)||!YellowLineSample(continuation.points,end,goal))return false;
 auto forward=mwneo::UnitXZ({nextPoint.x-from.x,nextPoint.y-from.y,nextPoint.z-from.z});
 float p[]={from.x,from.y,from.z},h[]={forward.x,0,forward.z};YellowArcDiagnostic diagnostic;
 bool clear=YellowLineArc(world,continuation,begin,p,h,goal,halfWidth,cars,count,speed,0,&diagnostic,3.5f,halfLength,.025f,true);
 if(failure)*failure=diagnostic;
 if(!clear||!diagnostic.terrainLanded)return false;
 continuation.truncated=false;output=std::move(continuation);return true;
}
