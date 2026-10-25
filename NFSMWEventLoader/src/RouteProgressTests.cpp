#include "RouteProgress.h"
#include <cstdio>
#include <cstdlib>
using namespace mwel_progress;
void Check(bool condition,const char* label){if(!condition){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 Leg straight{{{{0,0,0},0},{{100,0,0},100}}, {}};
 Leg second{{{{100,0,0},0},{{200,0,0},100}}, {}};
 std::vector<Leg> course{straight,second};
 auto a=Measure(course,0,0,1,{75,0,0}),b=Measure(course,0,0,1,{25,0,0});
 Check(a.valid&&Near(a.remaining,25)&&Near(a.racePercent,37.5f)&&a.racePercent>b.racePercent,"body order within one CP");
 auto backwards=Measure(course,0,0,1,{50,0,0});Check(backwards.racePercent<a.racePercent,"reverse movement must reduce progress");
 auto next=Measure(course,1,0,1,{100,0,0});Check(next.racePercent>=a.racePercent&&Near(next.racePercent,50),"native CP transition continuity");
 auto noGate=Measure(course,0,0,1,{200,0,0});Check(Near(noGate.racePercent,50),"body beyond CP cannot grant a skipped CP");
 straight.alternatives.push_back({{{0,0,0},0},{{50,0,50},50},{{100,0,0},100}});
 auto branch=Measure({straight,second},0,0,1,{50,0,50});Check(branch.valid&&Near(branch.racePercent,25)&&Near(branch.remaining,50),"branch rejoin station normalisation");
 Leg elevated{{{{0,20,0},0},{{100,20,0},100}},{{{{0,0,0},100},{{100,0,0},0}}}};
 Check(Near(Project(elevated,{25,20,0}).station,25),"height separates overlapping roads");
 auto lap=Measure(course,1,1,3,{150,0,0});Check(Near(lap.lapPercent,75)&&Near(lap.racePercent,58.333333f),"native lap count measures repeated authored road");
 Check(!Measure(course,2,0,1,{0,0,0}).valid&&!Measure(course,0,1,1,{0,0,0}).valid,"invalid CP or completed lap cannot manufacture progress");
 Check(!Measure({},0,0,1,{0,0,0}).valid&&!Project(straight,{NAN,0,0}).valid,"invalid geometry or body rejected");
 std::puts("PASS 9 authored body-progress cases; CP and goal state untouched");
}
