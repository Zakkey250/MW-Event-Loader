#pragma once
// Reacquire an actual native placement on the already selected CP road.
// This changes only the geometric search anchor, never committed progress.
template<class World>
static bool NeoObservedPlacement(const World& world,const std::vector<YellowRecoveryPoint>& road,Vec3 body,float committed,float& cursor){
 float candidate=committed;
 if(!NeoProjectRoad(road,body,committed,candidate,350,8,16)||candidate>committed)return false;
 Vec3 path{};RayHit bodyFloor,pathFloor;
 if(!YellowLineSample(road,candidate,path)||std::hypot(body.x-path.x,body.z-path.z)>24.f||
    !world.Ray({body.x,body.y+1,body.z},{body.x,body.y-3,body.z},bodyFloor,true)||
    !world.Ray({path.x,path.y+1,path.z},{path.x,path.y-3,path.z},pathFloor,true))return false;
 float tolerance=std::min(4.f,1.f+.35f*std::hypot(body.x-path.x,body.z-path.z));
 if(fabs(bodyFloor.point.y-pathFloor.point.y)>tolerance)return false;
 cursor=candidate;return true;
}
