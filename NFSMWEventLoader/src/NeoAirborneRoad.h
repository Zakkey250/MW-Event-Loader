#pragma once
// Airborne progress is still constrained to the already committed CP road.
// A floor must exist under the body and agree with that road's actual deck.
// The surrogate pose is used only for projection, never native physics/CPs.
static bool NeoAirborneRoadProjection(const CollisionWorld& world,const std::vector<YellowRecoveryPoint>& road,
 Vec3 body,float speed,float sinceClear,float sinceProjection,float reference,float& cursor,
 std::vector<YellowRecoveryPoint>& grounded){
 if(!std::isfinite(sinceClear)||sinceClear<0||sinceClear>2.f||!std::isfinite(speed)||speed<=15||
    !std::isfinite(sinceProjection)||sinceProjection<0||sinceProjection>2.f)return false;
 RayHit floor;
 if(!world.Ray(body,{body.x,body.y-18,body.z},floor,true))return false;
 float gap=body.y-floor.point.y;if(!std::isfinite(gap)||gap<=1.5f||gap>16)return false;
 Vec3 foot{body.x,floor.point.y+.75f,body.z};float candidate=reference;
 const float allowed=std::min(220.f,std::abs(speed)*sinceProjection+20.f);
 const float ahead=std::max(std::clamp(std::abs(speed)*.7f+20,40.f,90.f),allowed);
 auto project=[&](const auto& path){Vec3 nearest{};
  // A nearby later crossing can share this floor and CP. Mere proximity is
  // not proof that the body travelled that far since its last projection.
  return NeoProjectSupportedRoad(world,path,foot,reference,candidate,ahead,&road,220)&&
   candidate-reference<=allowed&&
   YellowLineSample(path,candidate,nearest)&&std::hypot(nearest.x-body.x,nearest.z-body.z)<=12;
 };
 if(!project(grounded)){
  const float p[]={foot.x,foot.y,foot.z};auto measured=NeoGroundTrajectory(world,road,reference,p,speed,16.f,true);
  if(!project(measured))return false;grounded=std::move(measured);
 }
 cursor=candidate;return true;
}
// A short floor-ray/lateral miss in flight must not strand the body after
// landing beyond the ordinary +90m search. Actual near-body support, same
// authored deck and physically bounded progress are required again.
static bool NeoLandingRoadProjection(const CollisionWorld& world,const std::vector<YellowRecoveryPoint>& road,
 Vec3 body,float speed,float sinceFlight,float sinceProjection,float reference,float& cursor,
 std::vector<YellowRecoveryPoint>& grounded){
 if(!std::isfinite(sinceFlight)||sinceFlight<0||sinceFlight>2.f||!std::isfinite(sinceProjection)||sinceProjection<0||sinceProjection>2.f||!std::isfinite(speed)||speed<=15)return false;
 const float allowed=std::min(220.f,std::abs(speed)*sinceProjection+20.f);float candidate=reference;
 auto project=[&](const auto& path){return NeoProjectSupportedRoad(world,path,body,reference,candidate,allowed,&road,220)&&candidate-reference<=allowed;};
 if(!project(grounded)){
  const float p[]={body.x,body.y,body.z};auto measured=NeoGroundTrajectory(world,road,reference,p,speed,16.f,true);
  if(!project(measured))return false;grounded=std::move(measured);
 }
 cursor=candidate;return true;
}
