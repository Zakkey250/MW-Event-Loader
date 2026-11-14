#include <Windows.h>
#include <unordered_map>
#include <cassert>
#include <cstdio>
bool worldLogEnabled=false;void Log(const char*,...){}
unsigned race=0x10000,count=3;bool mission=true,racing=true,pending=false;
std::unordered_map<unsigned,unsigned> memory,groups,witnesses;
namespace combat_impact {struct Vector{float x=0,y=0,z=0;Vector operator-(Vector b)const{return{x-b.x,y-b.y,z-b.z};}};struct Motion{Vector position;};float Dot(Vector a,Vector b){return a.x*b.x+a.y*b.y+a.z*b.z;}}
namespace rpd_mission {bool Current(){return mission;}bool ResultPending(){return pending;}}
namespace combat_health_runtime {DWORD gameThread=GetCurrentThreadId();unsigned generation=1;bool Racing(){return racing;}bool NativeMotion(unsigned sim,combat_impact::Motion& m){m.position={float(sim),0,0};return true;}}
namespace gauntlet_runtime {
template<class T>bool Read(unsigned p,T& out){const auto it=memory.find(p);if(it==memory.end())return false;out=T(it->second);return true;}
unsigned Vehicle(unsigned h){return 0x20000+h*0x200;}
}
namespace hot_pursuit_runtime {
bool RunningTarget(unsigned h){return groups.count(h)!=0;}
unsigned FindPursuit(unsigned h){const auto it=groups.find(h);return it==groups.end()?0:it->second;}
unsigned OfficerPursuit(unsigned){return FindPursuit(2);}
unsigned SightWitness(unsigned g){return witnesses[g];}
}
#include "RpdPresentationTracking.h"
int main(){
 using namespace rpd_presentation_tracking;
 memory={{0x91E000,race},{race+0x1958,count},{race+0x18,1},{race+0x18+0x194,2},{race+0x18+2*0x194,3},
  {0x993CC8,0x30000},{0x30000+0x130,0x40000},{0x40000+0xE8,0},{0x40000+0xE9,0},{0x50000+0xE8,0},{0x50000+0xE9,0}};
 groups={{2,0x40000},{3,0x50000}};witnesses={{0x40000,0},{0x50000,33}};
 assert(RadioPursuit(1)==0x50000); // Another real unit still sees a racer while the officer's group has lost sight.
 assert(selected.target==3&&selected.witness==33);
 witnesses[0x40000]=22;selected.refresh=0;
 assert(RadioPursuit(1)==0x40000); // The officer's observed suspect has priority again.
 witnesses[0x40000]=witnesses[0x50000]=0;selected.refresh=0;
 assert(RadioPursuit(1)==0x40000&&selected.witness==0); // Real loss is retained, not manufactured into sight.
 groups.erase(2); // Retirement invalidates cached group immediately, inside the refresh interval.
 assert(RadioPursuit(1)==0x50000&&selected.target==3);
 memory[0x50000+0xE8]=1;++combat_health_runtime::generation;
 assert(!RadioPursuit(1)); // A new attempt must not retain a busted previous target.
 mission=false;assert(!RadioPursuit(1));mission=true;pending=true;assert(!RadioPursuit(1));pending=false;
 combat_health_runtime::gameThread=0;assert(!RadioPursuit(1));combat_health_runtime::gameThread=GetCurrentThreadId();
 memory[race+0x1958]=10;assert(!RadioPursuit(1));memory[race+0x1958]=3;
 assert(!RadioPursuit(4));
 std::puts("PASS shared radio fresh sight, officer priority, true loss, immediate retirement, retry epoch, scope and thread guards");
}
