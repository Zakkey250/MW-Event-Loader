#include <algorithm>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdio>
struct Vec3{float x,y,z;};
struct RayHit{Vec3 point{},normal{0,1,0};};
struct YellowRecoveryPoint{float p[3];float station;};
namespace mwneo{float SpeedHorizon(float){return 120;}}
struct CollisionWorld{
 bool bridge=false;float Floor(float x)const{return x<20?100:x<40?100-(x-20)*.4f:92;}
 bool Ray(Vec3 from,Vec3 to,RayHit& hit,bool)const{
  float y=Floor(from.x);bool found=false;
  if(y<=from.y&&y>=to.y){hit.point={from.x,y,from.z};hit.normal={from.x>=20&&from.x<40?.4f:0,1,0};found=true;}
  if(bridge&&from.x>5&&from.x<75&&106<=from.y&&106>=to.y){hit.point={from.x,106,from.z};hit.normal={0,1,0};found=true;}
  return found;
 }
};
bool YellowLineSample(const std::vector<YellowRecoveryPoint>& road,float station,Vec3& out){
 if(station<road.front().station||station>road.back().station)return false;
 for(unsigned i=1;i<road.size();++i)if(station<=road[i].station){auto a=road[i-1],b=road[i];float t=(station-a.station)/(b.station-a.station);out={a.p[0]+(b.p[0]-a.p[0])*t,a.p[1]+(b.p[1]-a.p[1])*t,a.p[2]+(b.p[2]-a.p[2])*t};return true;}return false;
}
bool YellowAimGround(const CollisionWorld& world,Vec3 q,float expected,float clearance,Vec3& out){RayHit h;if(!world.Ray({q.x,expected+2,q.z},{q.x,expected-3,q.z},h,true)||std::abs(h.point.y+clearance-expected)>2.5f)return false;out={q.x,h.point.y+clearance,q.z};return true;}
constexpr float NeoRouteTolerance=48.f;
struct YellowRacingLine{std::vector<YellowRecoveryPoint> points;};
#include "../../World2MW/src/NativeLoader/NeoRoadSupport.h"
#include "NeoAirborneRoad.h"
int main(){
 CollisionWorld world;std::vector<YellowRecoveryPoint> road={{{0,100.5f,0},0},{{80,100.5f,0},80}};float car[]={16,100.5f,0};
 auto old=NeoGroundTrajectory(world,road,16,car,40);auto fixed=NeoGroundTrajectory(world,road,16,car,40,16,true);Vec3 a,b;
 assert(YellowLineSample(old,60,a)&&a.y>99);assert(YellowLineSample(fixed,60,b)&&std::abs(b.y-92.5f)<.01f);
 for(const auto& p:fixed){assert(std::abs(p.p[0]-p.station)<.01f);assert(p.p[2]==0);}
 world.bridge=true;auto lower=NeoGroundTrajectory(world,road,16,car,40,16,true);assert(YellowLineSample(lower,60,b)&&std::abs(b.y-92.5f)<.01f);
 float upperCar[]={16,106.5f,0};auto mismatch=NeoGroundTrajectory(world,road,16,upperCar,40,16,true);assert(YellowLineSample(mismatch,16,b)&&std::abs(b.y-100.5f)<.01f);
 world.bridge=false;float cursor=16;auto tracked=road;
 assert(NeoAirborneRoadProjection(world,road,{16,105,0},70,.9f,.05f,16,cursor,tracked)&&std::abs(cursor-16)<.01f);
 assert(!NeoAirborneRoadProjection(world,road,{16,105,0},70,2.1f,.05f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{16,105,0},70,-.1f,.05f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{16,105,0},14,.5f,.05f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{16,100.75f,0},70,.5f,.05f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{16,117,0},70,.5f,.05f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{16,105,20},70,.5f,.05f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{200,97,0},70,.5f,.05f,16,cursor,tracked));
 assert(NeoAirborneRoadProjection(world,road,{16,113.5f,0},100,1.8f,.05f,16,cursor,tracked));
 world.bridge=true;assert(!NeoAirborneRoadProjection(world,road,{16,111,0},70,.5f,.05f,16,cursor,tracked));
 assert(NeoAirborneRoadProjection(world,road,{16,105,0},70,.5f,.05f,16,cursor,tracked));
 // The same floor/CP may loop back beside the launch. A point 54 metres
 // later is geometrically near, but cannot be acquired in 50 ms at 70 m/s.
 world.bridge=false;std::vector<YellowRecoveryPoint> crossing={{{16,100.5f,0},0},{{16,100.5f,80},80}};tracked=crossing;cursor=16;
 assert(!NeoAirborneRoadProjection(world,crossing,{16,105,70},70,.5f,.05f,16,cursor,tracked));
 assert(NeoAirborneRoadProjection(world,crossing,{16,105,70},70,.9f,.9f,16,cursor,tracked));
 assert(!NeoAirborneRoadProjection(world,road,{16,105,0},70,.5f,-.01f,16,cursor,tracked));
 // More than 90m travelled during a brief failed floor/flight query. Landing
 // reacquisition is recent-flight-only, bounded by elapsed native time and
 // still refuses a different supported deck or an impossible station leap.
 std::vector<YellowRecoveryPoint> longRoad={{{16,100.5f,0},0},{{16,100.5f,400},400}};tracked=longRoad;cursor=16;
 assert(!NeoProjectSupportedRoad(world,longRoad,{16,100.75f,160},16,cursor,90,&longRoad));
 assert(NeoLandingRoadProjection(world,longRoad,{16,100.75f,160},100,1.5f,1.5f,16,cursor,tracked)&&std::abs(cursor-160)<.01f);
 assert(!NeoLandingRoadProjection(world,longRoad,{16,100.75f,160},100,1.5f,.1f,16,cursor,tracked));
 assert(!NeoLandingRoadProjection(world,longRoad,{16,100.75f,160},100,2.1f,1.5f,16,cursor,tracked));
 assert(!NeoLandingRoadProjection(world,longRoad,{16,100.75f,300},100,1.9f,1.9f,16,cursor,tracked));
 world.bridge=true;assert(!NeoLandingRoadProjection(world,longRoad,{16,106.75f,160},100,1.5f,1.5f,16,cursor,tracked));
 puts("NEO_GROUND_PASS downhill follows floor, unchanged XZ/station, bridge deck isolated, explicit different deck refused");
 puts("NEO_AIRBORNE_PASS bounded simulation age, floor-backed projection, no upper/lower deck switch, no future-road jump");
 puts("NEO_LANDING_PASS recent physical-time landing beyond90m acquired, impossible leap/old flight/other deck refused");
}
