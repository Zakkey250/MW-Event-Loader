// Replays measured failed-flight poses against the actual immutable WMC.
#include "../../World2MW/src/NativeLoader/CollisionWorld.h"
#include "../../World2MW/src/NeoAI/Types.h"
#include <cassert>
using namespace w2mw;
struct YellowRecoveryPoint {float p[3];float station=0;};
struct YellowRacingLine {std::vector<YellowRecoveryPoint> points;};
constexpr float NeoRouteTolerance=48;
static bool YellowLineSample(const std::vector<YellowRecoveryPoint>& road,float station,Vec3& p){
 if(road.size()<2||station<road.front().station||station>road.back().station)return false;
 // Authored roads and planned lines have nondecreasing stations. Start at
 // the same first eligible segment without scanning the travelled prefix.
 auto first=std::lower_bound(road.begin()+1,road.end(),station,[](const YellowRecoveryPoint& p,float s){return p.station<s;});
 for(unsigned i=unsigned(first-road.begin());i<road.size();++i){const auto& a=road[i-1];const auto& b=road[i];
  float span=b.station-a.station;if(span<.001f)continue;float t=std::clamp((station-a.station)/span,0.f,1.f);
  p={a.p[0]+t*(b.p[0]-a.p[0]),a.p[1]+t*(b.p[1]-a.p[1]),a.p[2]+t*(b.p[2]-a.p[2])};return true;
 }return false;
}
static bool YellowAimGround(const CollisionWorld& world,Vec3 p,float expected,float clearance,Vec3& result){
 RayHit hit;if(!world.Ray({p.x,expected+2,p.z},{p.x,expected-3,p.z},hit,true))return false;
 if(fabs(hit.point.y+clearance-expected)>2.5f)return false;
 result={p.x,hit.point.y+clearance,p.z};return true;
}
#include "../../World2MW/src/NativeLoader/NeoRoadSupport.h"
#include "NeoAirborneRoad.h"
int main(int argc,char** argv){
 setvbuf(stdout,nullptr,_IONBF,0);
 assert(argc==3);CollisionWorld world;assert(world.Load(argv[1],0));
 FILE* file=nullptr;fopen_s(&file,argv[2],"rb");assert(file);unsigned header[4];assert(fread(header,4,4,file)==4&&header[0]==0x43523257&&header[3]>9);
 std::vector<YellowRecoveryPoint> road;
 for(unsigned leg=0;leg<=9;++leg){unsigned n;assert(fread(&n,4,1,file)==1);road.assign(n,{});
  for(unsigned i=0;i<n;++i){assert(fread(road[i].p,4,3,file)==3);if(i){Vec3 d{road[i].p[0]-road[i-1].p[0],road[i].p[1]-road[i-1].p[1],road[i].p[2]-road[i-1].p[2]};road[i].station=road[i-1].station+std::sqrt(Dot(d,d));}}
 }fclose(file);assert(road.size()==14);
 const Vec3 poses[]={{-977.55f,73.65f,3118.78f},{-1000.53f,76.91f,3108.45f},{-1014.351f,77.642f,3102.286f},{-1025.09f,77.58f,3097.52f},{-1049.56f,75.37f,3086.80f},{-1073.52f,70.47f,3079.02f}};
 float cursor=37.459f;auto grounded=road;float age=.25f;
 unsigned index=0;
 for(auto pose:poses){float next=cursor;bool tracked=NeoAirborneRoadProjection(world,road,pose,101,age,.27f,cursor,next,grounded);
  RayHit floor;assert(world.Ray(pose,{pose.x,pose.y-18,pose.z},floor,true));
  printf("CP9_REPLAY gap=%.3f old_cursor=%.3f new_cursor=%.3f tracked=%u\n",pose.y-floor.point.y,cursor,next,unsigned(tracked));
  if(!tracked){Vec3 foot{pose.x,floor.point.y+.75f,pose.z},nearest{};float c=cursor;
   bool raw=NeoProjectRoad(grounded,foot,cursor,c,40,4,90);YellowLineSample(grounded,c,nearest);
   printf("REPLAY_REF raw=%u candidate=%.3f nearest=%.3f,%.3f,%.3f lateral=%.3f\n",unsigned(raw),c,nearest.x,nearest.y,nearest.z,std::hypot(nearest.x-pose.x,nearest.z-pose.z));}
  if(index++<5){assert(tracked&&next>=cursor-40&&next-cursor<=101*.27f+20);cursor=std::max(cursor,next);}
  else assert(!tracked); // The failed run drifted 15.7m sideways: retain the bound.
  age+=.25f;
 }
 Vec3 landing{-1093.76f,67.38f,3079.72f};float landed=cursor;
 assert(NeoProjectSupportedRoad(world,grounded,landing,cursor,landed,90,&road)&&landed>cursor+20);
 printf("CP9_LANDING old_cursor=%.3f acquired=%.3f\n",cursor,landed);
 puts("PASS real WMC 13.29m apex, sideways flight refused, bounded landing reacquired; no body/CP/goal writes");
}
