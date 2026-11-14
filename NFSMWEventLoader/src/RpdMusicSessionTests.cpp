#include <Windows.h>
#include <unordered_map>
#include <cassert>
#include <cstdio>
#include <limits>
unsigned race=0x10000,count=4;bool mission=true,racing=true,pending=false;
bool sprint=true;bool MWELRpdHotPursuitScope(){return sprint;}
unsigned activeSuspects=~0u;unsigned MWELRpdActiveSuspects(){return activeSuspects;}
std::unordered_map<unsigned,unsigned> memory,groups;std::unordered_map<unsigned,bool> running;
namespace rpd_mission {bool Current(){return mission;}bool ResultPending(){return pending;}}
namespace combat_health_runtime {DWORD gameThread=GetCurrentThreadId();unsigned generation=1;bool Racing(){return racing;}}
namespace gauntlet_runtime {template<class T>bool Read(unsigned p,T& out){auto it=memory.find(p);if(it==memory.end())return false;out=T(it->second);return true;}}
namespace hot_pursuit_runtime {bool RunningTarget(unsigned h){return running[h];}unsigned FindPursuit(unsigned h){return groups[h];}}
#include "RpdMusicSession.h"
namespace combat_impact {struct Vector {float x=0,y=0,z=0;};struct Motion {unsigned handle=0;Vector position;};}
std::unordered_map<unsigned,combat_impact::Vector> positions;
namespace gauntlet_runtime {unsigned Vehicle(unsigned h){return positions.count(h)?h+0x10000:0;}}
namespace combat_health_runtime {bool NativeMotion(unsigned sim,combat_impact::Motion& out){const auto h=sim+0x80-0x10000;auto it=positions.find(h);if(it==positions.end())return false;out={h,it->second};return true;}}
unsigned completedArrests=0,initialSuspects=8;bool progressBound=true;
bool MWELRpdArrestProgress(unsigned* a,unsigned* n){*a=completedArrests;*n=initialSuspects;return progressBound;}
#include "RpdMusicProximity.h"
unsigned currentKey=99,localArrest=0,sprintArrest=0;
unsigned MWELNeoCurrentKey(){return currentKey;}
bool MWELRpdSprintArresting(unsigned h){return h==sprintArrest;}
namespace rpd_mission {bool LocalArrestInProgress(unsigned h){return h==localArrest;}}
namespace rpd_native_arrest_nis {
 bool victory=false,nativeSetup=false,owns=false;unsigned requestSuspect=0,requestRace=0,requestKey=0,requestEpoch=0;
 bool Owns(){return owns;}
 bool Requested(unsigned r,unsigned k,unsigned g,unsigned h){return requestRace==r&&requestKey==k&&requestEpoch==g&&h&&requestSuspect==h;}
}
#include "RpdMusicCapture.h"

int main(){
 memory={{0x925E90,6},{0x91E000,race},{race+0x1958,count},{race+0x195C,0},{race+0x18+0x1D,0}};
 for(unsigned s=1;s<count;++s){memory[race+0x18+s*0x194]=s+1;running[s+1]=true;groups[s+1]=0x20000+s*0x1000;memory[groups[s+1]+0x218]=0;memory[groups[s+1]+0xE8]=memory[groups[s+1]+0xE9]=0;}
 unsigned r=0,e=0;assert(MWEL_RpdMusicSessionV1(&r,&e)==3&&r==race&&e==1);
 activeSuspects=2;racing=false;assert(MWEL_RpdMusicSessionV1(&r,&e)==2); // intermediate native scene is not dispatch end
 activeSuspects=0;assert(!MWEL_RpdMusicSessionV1(&r,&e)); // authoritative last retirement releases outro
 activeSuspects=~0u;racing=true;
 memory[groups[2]+0x218]=3;assert(MWEL_RpdMusicSessionV1(&r,&e)==2); // native arrest before ledger retirement
 running[2]=false;memory[groups[3]+0xE8]=1;assert(MWEL_RpdMusicSessionV1(&r,&e)==1);
 groups[4]=0;assert(MWEL_RpdMusicSessionV1(&r,&e)==1); // new pursuit may be absent briefly
 groups[4]=0x24000;memory[groups[4]+0x218]=4;memory[groups[4]+0xE9]=1;
 assert(MWEL_RpdMusicSessionV1(&r,&e)==1); // Native loss is not a sprint goal escape.
 sprint=false;assert(!MWEL_RpdMusicSessionV1(&r,&e));sprint=true;
 running[4]=false;assert(!MWEL_RpdMusicSessionV1(&r,&e)); // final retirement releases outro
 running[4]=true;++combat_health_runtime::generation;assert(MWEL_RpdMusicSessionV1(&r,&e)==1&&e==2);
 memory[race+0x195C]=1;assert(!MWEL_RpdMusicSessionV1(&r,&e)&&!r&&!e);memory[race+0x195C]=0;
 memory[race+0x18+0x1D]=1;assert(!MWEL_RpdMusicSessionV1(&r,&e));memory[race+0x18+0x1D]=0;
 pending=true;assert(!MWEL_RpdMusicSessionV1(&r,&e));pending=false;
 mission=false;assert(!MWEL_RpdMusicSessionV1(&r,&e));mission=true;
 combat_health_runtime::gameThread=0;assert(!MWEL_RpdMusicSessionV1(&r,&e));
 combat_health_runtime::gameThread=GetCurrentThreadId();memory[race+0x1958]=9;
 for(unsigned s=1;s<=8;++s){const auto h=100+s;memory[race+0x18+s*0x194]=h;running[h]=true;groups[h]=0;}
 assert(MWEL_RpdMusicSessionV1(&r,&e)==8);
 for(unsigned s=1;s<=8;++s){running[100+s]=false;assert(MWEL_RpdMusicSessionV1(&r,&e)==8-s);}
 memory[race+0x1958]=2;memory[race+0x18+0x194]=500;running[500]=true;
 assert(MWEL_RpdMusicSessionV1(&r,&e)==1);running[500]=false;assert(!MWEL_RpdMusicSessionV1(&r,&e));
 // Exercise the real bridge: radius boundary, complete 8-racer roster,
 // retirement, missing bodies, vertical separation and retry player identity.
 memory[race+0x18]=1;positions[1]={0,0,0};memory[race+0x1958]=9;activeSuspects=8;
 for(unsigned slot=1;slot<=8;++slot){auto h=100+slot;memory[race+0x18+slot*0x194]=h;running[h]=true;groups[h]=0;positions[h]={float(slot*100),0,0};}
 unsigned nearby=99,total=99;
 assert(MWEL_RpdMusicProximityV1(&nearby,&total)&&nearby==3&&total==8); // exactly 300 m included
 positions[103]={300.001f,0,0};assert(MWEL_RpdMusicProximityV1(&nearby,&total)&&nearby==2&&total==8);
 positions[101]={0,301,0};assert(MWEL_RpdMusicProximityV1(&nearby,&total)&&nearby==1);
 running[102]=false;activeSuspects=7;assert(MWEL_RpdMusicProximityV1(&nearby,&total)&&nearby==0&&total==7);
 positions.erase(104);assert(!MWEL_RpdMusicProximityV1(&nearby,&total)&&!nearby&&!total);
 positions[104]={std::numeric_limits<float>::quiet_NaN(),0,0};assert(!MWEL_RpdMusicProximityV1(&nearby,&total));
 positions[104]={400,0,0};positions[1]={600,0,0};assert(MWEL_RpdMusicProximityV1(&nearby,&total)&&nearby==6&&total==7);
 ++combat_health_runtime::generation;memory[race+0x18]=9;positions[9]={2000,0,0};assert(MWEL_RpdMusicProximityV1(&nearby,&total)&&nearby==0&&total==7);
 activeSuspects=0;assert(!MWEL_RpdMusicProximityV1(&nearby,&total)&&!nearby&&!total);
 // Nearest body, rather than nearby/whole-field count, drives music.
 activeSuspects=8;memory[race+0x18]=1;positions[1]={0,0,0};
 for(unsigned slot=1;slot<=8;++slot){running[100+slot]=true;positions[100+slot]={float(slot*100),0,0};}
 float distance=-1;unsigned arrests=99,initial=99;
 assert(MWEL_RpdMusicProximityV2(&nearby,&total,&distance)&&distance==100&&total==8);
 positions[101]={400,0,0};positions[102]={0,150,0};
 assert(MWEL_RpdMusicProximityV3(&nearby,&total,&distance,&arrests,&initial)&&distance==150&&arrests==0&&initial==8);
 running[102]=false;activeSuspects=7;completedArrests=1;
 assert(MWEL_RpdMusicProximityV3(&nearby,&total,&distance,&arrests,&initial)&&distance==300&&arrests==1&&initial==8);
 running[103]=false;activeSuspects=6; // Escape: do not increment completedArrests.
 assert(MWEL_RpdMusicProximityV3(&nearby,&total,&distance,&arrests,&initial)&&distance==400&&arrests==1&&initial==8);
 positions[104]={std::numeric_limits<float>::quiet_NaN(),0,0};
 assert(!MWEL_RpdMusicProximityV3(&nearby,&total,&distance,&arrests,&initial)&&!nearby&&!total&&distance==300&&!arrests&&!initial);
 positions[104]={400,0,0};progressBound=false;
 assert(MWEL_RpdMusicProximityV3(&nearby,&total,&distance,&arrests,&initial)&&!arrests&&initial==8);
 ++combat_health_runtime::generation;memory[race+0x18]=9;positions[9]={2000,0,0};completedArrests=0;progressBound=true;
 assert(MWEL_RpdMusicProximityV3(&nearby,&total,&distance,&arrests,&initial)&&distance==1200&&!arrests);
 // Restore the final-capture regression's intended roster.
 for(unsigned slot=1;slot<=8;++slot)running[100+slot]=slot!=2;
 // The real capture bridge distinguishes local capture, AI capture, scene
 // loading and generic retry loading, including a final retired suspect.
 unsigned cr=0,cg=0;activeSuspects=7;sprintArrest=101;
 assert(!MWEL_RpdMusicCaptureV1(&cr,&cg)&&cr==race&&cg==combat_health_runtime::generation);
 // Intermediate human captures must keep the ordinary 300 m ratio, never
 // force the securing phrase. Only the dispatch's final survivor may do so.
 for(unsigned slot=3;slot<=8;++slot)running[100+slot]=false;
 activeSuspects=1;
 assert(MWEL_RpdMusicCaptureV1(&cr,&cg)==1);
 activeSuspects=7;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg)); // ledger wins over stale native slots
 activeSuspects=~0u;assert(MWEL_RpdMusicCaptureV1(&cr,&cg)==1); // unbound ledger: count live targets
 running[103]=true;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));running[103]=false;
 activeSuspects=1;
 sprintArrest=0;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg)); // AI countdown is not the human envelope.
 sprint=false;memory[race+0x1958]=2;localArrest=101;assert(MWEL_RpdMusicCaptureV1(&cr,&cg)==1);
 localArrest=0;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));sprint=true;memory[race+0x1958]=9;
 memory[race+0x195C]=1;memory[0x925854]=1;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg)); // ordinary retry
 using namespace rpd_native_arrest_nis;
 requestRace=race;requestKey=currentKey;requestEpoch=combat_health_runtime::generation;requestSuspect=101;nativeSetup=true;
 activeSuspects=0;pending=true;racing=false;running[101]=false;
 assert(MWEL_RpdMusicCaptureV1(&cr,&cg)==2); // native loader inside originalRequest, before ownership assignment
 activeSuspects=1;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));activeSuspects=0; // intermediate camera never requests dispatch outro
 nativeSetup=false;owns=true;memory[0x925854]=0;assert(MWEL_RpdMusicCaptureV1(&cr,&cg)==2); // actual scene
 victory=true;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));victory=false;
 ++combat_health_runtime::generation;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));--combat_health_runtime::generation;
 ++currentKey;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));--currentKey;
 memory[race+0x18+0x1D]=1;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));memory[race+0x18+0x1D]=0;
 combat_health_runtime::gameThread=0;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg)&&!cr&&!cg);combat_health_runtime::gameThread=GetCurrentThreadId();
 owns=false;assert(!MWEL_RpdMusicCaptureV1(&cr,&cg));
 puts("PASS RPD dispatch survivor count: partial/final arrest, native/ledger timing, missing pursuit, retry identity, load, wreck, result, scope and thread guards");
 puts("PASS RPD music capture context: final-survivor human envelope only, intermediate capture excluded, ledger/fallback count, AI exclusion, final NIS loading/playing, generic load, victory, generation, key and wreck guards");
 puts("PASS RPD V2/V3 distance bridge: nearest live 3D body, 300m boundary, arrest/escape distinction, unavailable data, unbound ledger and retry identity");
}
