#include "GridLaunchPolicy.h"
#include "NeoRecoveryObservation.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
struct Vec3{float x,y,z;};
struct RayHit{Vec3 point{},normal{0,1,0};};
struct YellowRecoveryPoint{float p[3];float station=0;};
constexpr float NeoLaneExtent=42;
struct CollisionWorld{
 bool median=true,gap=false,upperBody=false;
 bool Ray(Vec3 from,Vec3 to,RayHit& hit,bool)const{
  if(median&&from.x>1&&from.x<7&&from.z< -50)return false;
  if(gap&&from.z> -45&&from.z< -35)return false;
  float floor=upperBody&&from.x>7?110.f:100.f;
  if(floor>from.y||floor<to.y)return false;
  hit.point={from.x,floor,from.z};return true;
 }
};
bool YellowLineSupported(const CollisionWorld&,Vec3,Vec3,float,float,bool=false){return false;}
#include "NeoSurfaceApproach.h"
int main(){
 using mwel_recovery_observation::Resumed;
 assert(!Resumed(1500,1000,{0,0,0},{0,0,1},{0,0,0},1));
 assert(!Resumed(1500,1000,{0,0,0},{0,0,1},{0,0,-3},1));
 assert(!Resumed(6000,1000,{0,0,0},{0,0,1},{0,0,3},1));
 assert(!Resumed(1500,1000,{0,0,0},{0,0,1},{0,0,3},2));
 assert(Resumed(1500,1000,{0,0,0},{0,0,1},{0,0,3},1));
 bool used=false;assert(!mwel_recovery_observation::VerifyOnce(used,false)&&!used);
 assert(mwel_recovery_observation::VerifyOnce(used,true)&&used);
 assert(!mwel_recovery_observation::VerifyOnce(used,true));
 CollisionWorld world;mwneo::Point body{8,100.5f,-64},boundary{0,100.5f,-62},grid{0,100.5f,0},next{0,100.5f,10};
 mwel_grid::Launch launch;std::vector<YellowRecoveryPoint> prefix;unsigned failure=0;
 assert(!NeoSurfaceApproach(world,body,grid,next,1,launch,prefix,failure)&&failure==4);
 assert(NeoBoundarySurfaceApproach(world,body,boundary,grid,next,1,launch,prefix,failure));
 assert(launch.Active(body)&&prefix.size()>2&&prefix.front().p[0]==0&&prefix.back().station==launch.length);
 for(const auto& p:prefix)assert(p.p[0]==0&&p.p[1]==100.5f);
 auto driving=prefix,recovery=prefix;std::vector<YellowRecoveryPoint> canonical={{{0,100.5f,0},0},{{0,100.5f,100},100}};
 mwel_grid::AppendCanonical(canonical,launch.length,driving);mwel_grid::AppendCanonical(canonical,launch.length,recovery);
 assert(driving.size()==recovery.size()&&recovery.front().p[2]== -62&&recovery.back().station==162);
 assert(mwel_grid::BeforeRecoveryEntry(0,false,0,-50,2.3f));
 assert(!mwel_grid::BeforeRecoveryEntry(200,false,0,-50,2.3f)); // hairpin midway behind the entry tangent
 assert(!mwel_grid::BeforeRecoveryEntry(0,true,45,-50,2.3f)); // prefix owns its local cursor
 assert(mwel_grid::BeforeRecoveryEntry(0,true,0,-10,2.3f));
 mwel_grid::Launch initial;assert(initial.Initialize({0,100.5f,-62},grid,next));
 auto gridPrefix=mwel_grid::ApproachPrefix(initial,std::vector<YellowRecoveryPoint>{});
 mwel_grid::AppendCanonical(canonical,initial.length,gridPrefix);
 assert(gridPrefix.front().p[2]== -62&&gridPrefix.back().station==162);
 world.gap=true;launch={};prefix.clear();
 assert(!NeoBoundarySurfaceApproach(world,body,boundary,grid,next,1,launch,prefix,failure)&&!launch.length);
 world.gap=false;world.upperBody=true;body.y=110.5f;
 assert(!NeoBoundarySurfaceApproach(world,body,boundary,grid,next,1,launch,prefix,failure)&&failure==9);
 body={100,100.5f,-64};
 assert(!NeoBoundarySurfaceApproach(world,body,boundary,grid,next,1,launch,prefix,failure)&&failure==7);
 std::puts("PASS displaced-lane boundary reference, unchanged floor/wall policy, real support-gap and stacked-deck rejection, shared driving/recovery coordinates");
}
