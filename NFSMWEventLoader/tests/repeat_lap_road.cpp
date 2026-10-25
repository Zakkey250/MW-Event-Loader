#include "../src/RepeatLapRoad.h"
#include <cassert>
#include <fstream>
struct Point {float p[3];float station;};
int main(int argc,char** argv){
 std::vector<Point> road={{{-50,0,0},0},{{0,0,0},50},{{100,0,0},150},{{200,0,0},250}};
 auto initial=road;std::vector<unsigned> flags={1,1,2,1};float finish[]={105,0,0};
 assert(mwel_lap::TrimApproach(road,flags,finish));
 assert(road.size()==2&&road.front().p[0]==105&&road.front().station==0&&road.back().station==95);
 assert(flags.size()==road.size());
 // An off-road/other-deck lap line cannot authorize a progress jump.
 road=initial;flags={1,1,2,1};finish[1]=8;
 assert(!mwel_lap::TrimApproach(road,flags,finish)&&road.size()==initial.size()&&road.back().station==250);
 finish[1]=0;finish[0]=200;assert(!mwel_lap::TrimApproach(road,flags,finish));
 // A lap line coincident with the grid retains the full route.
 finish[0]=-50;assert(mwel_lap::TrimApproach(road,flags,finish)&&road.size()==4&&road.back().station==250);
 // A later overlapping visit must never be chosen by closest-point search.
 road={{{0,0,0},0},{{100,0,0},100},{{0,0,0},200},{{200,0,0},400}};flags={1,1,1,1};finish[0]=50;
 assert(!mwel_lap::TrimApproach(road,flags,finish));
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);unsigned h[4]{};input.read(reinterpret_cast<char*>(h),sizeof(h));
  assert(input&&h[0]==0x43523257&&h[1]==1&&h[3]==5);
  std::vector<std::vector<Point>> legs;
  for(unsigned i=0;i<h[3];++i){
   unsigned n=0;input.read(reinterpret_cast<char*>(&n),4);assert(n>=2&&n<8193);std::vector<Point> points(n);
   for(unsigned j=0;j<n;++j){input.read(reinterpret_cast<char*>(points[j].p),12);assert(input);points[j].station=0;
    if(j){float d=0;for(unsigned k=0;k<3;++k){float v=points[j].p[k]-points[j-1].p[k];d+=v*v;}points[j].station=points[j-1].station+std::sqrt(d);}}
   legs.push_back(points);
  }
  road=legs.front();flags.assign(road.size(),1);float first=road.back().station;
  assert(mwel_lap::TrimApproach(road,flags,legs.back().back().p));
  assert(first-road.back().station>155&&first-road.back().station<156);
  assert(road.back().station>94&&road.back().station<95);
  // The removed grid approach is not a recovery destination on later laps.
  assert(std::abs(road.front().p[2]-legs.front().front().p[2])>150);
 }
}
