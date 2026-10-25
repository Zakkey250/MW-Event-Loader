#include <unordered_map>
#include <cassert>
#include <cstdio>
#include "../../World2MW/src/NativeLoader/CollisionWorld.h"
#include "../../World2MW/src/NeoAI/Types.h"
using namespace w2mw;
static std::vector<std::pair<Vec3,Vec3>> barriers;
#include "../../World2MW/src/NativeLoader/YellowBarrierCorridor.h"
static bool YellowEditorWallCorridor(Vec3 a,Vec3 b,float width,bool contact=false,float padding=.2f,const Vec3* center=nullptr){return YellowBarrierSegmentsClear(barriers,a,b,width,contact,padding,center);}
struct YellowRecoveryPoint{float p[3];float station;};
#include "../../World2MW/src/NativeLoader/NeoLinePlanner.h"
#include "GridLaunchPolicy.h"
#include "NeoSurfaceApproach.h"
#include "NeoGateApproach.h"
#include "NeoSimulationStep.h"
#include "PlayerLapNotice.h"
#include "NeoCourseDirection.h"
#include "NeoRecoveryPrefix.h"
#include "../../World2MW/src/NativeLoader/NeoRespawnGeometry.h"
static void Floor(CollisionWorld& world,float x0,float x1,float z0,float z1,float y=0){
 world.triangles.push_back({{{x0,y,z0},{x1,y,z0},{x1,y,z1}},0,0});
 world.triangles.push_back({{{x0,y,z0},{x1,y,z1},{x0,y,z1}},0,0});
}
static void Prepare(CollisionWorld& world){world.nodes={{{-200,-2,-200},{200,12,200},0,0x80000000u|unsigned(world.triangles.size())}};assert(world.PrepareQueries());}
static bool Valid(const CollisionWorld& world,const YellowRacingLine& line){
 for(unsigned i=1;i<line.points.size();++i){auto a=line.points[i-1].p,b=line.points[i].p;
  if(!YellowGateEdgeClear(line.passage,{a[0],a[1],a[2]},{b[0],b[1],b[2]})||!YellowLineSupported(world,{a[0],a[1],a[2]},{b[0],b[1],b[2]},1.35f,.75f))return false;
 }return true;
}
int main(){
 PlayerLapNotice notice;int raceId=0,boardId=0;
 assert(!notice.Observe(&raceId,&boardId,1,10,0,7,0));
 // Another racer completing a lap changes neither the observed player lap
 // nor the notification. Rank and four-row display indices are absent.
 assert(!notice.Observe(&raceId,&boardId,1,10,0,7,10));
 assert(notice.Observe(&raceId,&boardId,1,10,1,7,11));
 assert(!notice.Observe(&raceId,&boardId,1,10,1,7,12));
 assert(!notice.Observe(&raceId,&boardId,1,10,0,7,0)); // retry
 assert(notice.Observe(&raceId,&boardId,1,10,1,7,15));
 assert(!notice.Observe(&raceId,&boardId,1,10,7,7,90)); // final lap is the result screen
 NeoSimulationStep step;
 assert(mwel_grid::ApproachCandidate(8,0,1,0,40));
 assert(mwel_grid::ApproachCandidate(4,1,3,0,20));
 assert(!mwel_grid::ApproachCandidate(8,0,1,1,40));
 std::vector<YellowRecoveryPoint> priorRoad={{{0,0,-100},0},{{0,0,0},100}},nextRoad={{{0,0,0},0},{{0,0,50},50}},recoveryPrefix;
 mwel_grid::Launch recoveryApproach;
 assert(NeoRecoveryPrefix(priorRoad,nextRoad,recoveryApproach,recoveryPrefix));
 assert(recoveryApproach.Active({40,-80,-36})&&recoveryApproach.Cursor({40,-80,-36})>60);
 assert(recoveryPrefix.front().station==0&&recoveryPrefix.back().station==100);
 nextRoad.front().p[1]=20;assert(!NeoRecoveryPrefix(priorRoad,nextRoad,recoveryApproach,recoveryPrefix));
 assert(step.Begin(1.f));assert(!step.Begin(1.f));
 assert(step.Begin(1.f+1.f/60));assert(!step.Begin(1.f+1.f/60));
 assert(!step.Begin(NAN));assert(step.Begin(0.f));
 // The same shared road is forward in opposite directions on different
 // ordered CP legs. Ordinary one-way and genuine wrong-way cases still work.
 NeoCourseDirection outbound,inbound,otherDeck;
 outbound.Consider({0,.75f,5},{0,0,1},{0,0,0},{0,0,10});
 inbound.Consider({0,.75f,5},{0,0,-1},{0,0,10},{0,0,0});
 assert(!outbound.Resolve(true)&&!inbound.Resolve(true));
 NeoCourseDirection wrong;wrong.Consider({0,.75f,5},{0,0,-1},{0,0,0},{0,0,10});assert(wrong.Resolve(false));
 otherDeck.Consider({0,20,5},{0,0,1},{0,0,0},{0,0,10});assert(otherDeck.Resolve(true)&&!otherDeck.Resolve(false));
 // A measured free-fall from the submitted course must remain recoverable.
 // Horizontal extent remains bounded; every destination still needs its
 // own supported body rectangle and onward corridor on the authored deck.
 assert(NeoRespawnDistanceAllowed({-2791.84f,-3053.97f,604.10f},{-2800,179,600}));
 assert(!NeoRespawnDistanceAllowed({0,-3000,0},{221,0,0}));
 assert(!NeoRespawnDistanceAllowed({0,NAN,0},{0,0,0}));
 CollisionWorld wide;Floor(wide,-20,20,-10,170);Prepare(wide);YellowRacingLine wavy;
 mwneo::Vehicle fallen;fallen.position={0,-3000,50};fallen.halfWidth=1;fallen.halfLength=2.5f;
 assert(NeoRespawnDistanceAllowed(fallen.position,{0,.75f,50}));
 assert(NeoRespawnPoseClear(wide,fallen,{0,.75f,50},{0,0,1}));
 assert(!NeoRespawnPoseClear(wide,fallen,{0,10,50},{0,0,1}));
 assert(!NeoRespawnPoseClear(wide,fallen,{0,.75f,200},{0,0,1}));
 for(unsigned i=0;i<=20;++i)wavy.points.push_back({{i==0||i==20?0.f:i%2?3.f:-3.f,.75f,float(i*8)},float(i*8)});
 assert(Valid(wide,wavy));float before=NeoCorridorEnergy(wavy.points);
 NeoSmoothLine(wide,wavy,1,nullptr,0,false,60,nullptr,0,0,true);
 assert(Valid(wide,wavy)&&NeoCorridorEnergy(wavy.points)<before*.3f);
 std::vector<YellowRecoveryPoint> mapWave;YellowRacingLine prior,rolling;
 for(unsigned i=0;i<=20;++i){mapWave.push_back({{i%2?1.f:-1.f,.75f,float(i*8)},float(i*8)});prior.points.push_back({{0,.75f,float(i*8)},float(i*8)});}
 float position[]={0,.75f,0},heading[]={0,0,1};
 assert(YellowOptimizeLine(wide,mapWave,0,position,heading,60,1,1.5f,&prior,rolling,nullptr,0,false,60,false,21,.03f,0,true,false));
 for(const auto& p:rolling.points)assert(std::abs(p.p[0])<.2f);
 // A narrow floor and solid median remain authoritative, including when a
 // smoother geometric shortcut exists on the other side of a road join.
 CollisionWorld l;Floor(l,-6,6,-10,90);Floor(l,-6,90,78,90);Prepare(l);YellowRacingLine bend;
 for(unsigned i=0;i<=10;++i)bend.points.push_back({{0,.75f,float(i*8)},float(i*8)});
 for(unsigned i=1;i<=10;++i)bend.points.push_back({{float(i*8),.75f,80},float(80+i*8)});
 assert(Valid(l,bend));NeoSmoothLine(l,bend,1,nullptr,0,false,40,nullptr,0,0,true);assert(Valid(l,bend));
 YellowRacingLine gate;for(unsigned i=0;i<=20;++i)gate.points.push_back({{i>7&&i<13?3.f:i%2?3.f:-3.f,.75f,float(i*8)},float(i*8)});
 gate.passage={{3,.75f,80},{0,0,1},2,2,1,true};assert(Valid(wide,gate));
 NeoSmoothLine(wide,gate,1,nullptr,0,false,50,nullptr,0,0,true);assert(Valid(wide,gate));
 // The floor on another deck cannot satisfy a missing lower-road support.
 CollisionWorld stacked;Floor(stacked,-6,6,-10,70);Floor(stacked,-6,6,90,170);Floor(stacked,-20,20,60,100,8);Prepare(stacked);
 assert(!YellowLineSupported(stacked,{0,.75f,65},{0,.75f,95},1.35f,.75f));
 barriers={{{0,0,40},{0,3,120}}};YellowRacingLine wall;
 for(unsigned i=0;i<=20;++i)wall.points.push_back({{i%2?4.f:2.f,.75f,float(i*8)},float(i*8)});
 assert(Valid(wide,wall));NeoSmoothLine(wide,wall,1,nullptr,0,false,60,nullptr,0,0,true);assert(Valid(wide,wall));
 barriers.clear();
 // An out-and-back CP's plane lies alongside an earlier curved approach.
 // The near-plane funnel must apply only to its final authored approach,
 // while an offset crossing on that final approach is still refused.
 std::vector<YellowRecoveryPoint> returning={{{-30,.75f,-10},0},{{-30,.75f,5},15},{{-30,.75f,30},40},{{0,.75f,30},70},{{0,.75f,0},100}};
 YellowGatePassage scoped={{0,.75f,0},{0,0,1},20,20,2,true};
 scoped.beginStation=NeoGateApproachBegin(returning,scoped);
 assert(scoped.beginStation>=80&&scoped.beginStation<100);
 assert(!YellowGateEdgeClear(scoped,{-30,.75f,-10},{-30,.75f,5}));
 assert(YellowGateEdgeClear(scoped,{-30,.75f,-10},{-30,.75f,5},15));
 assert(!YellowGateEdgeClear(scoped,{-30,.75f,-10},{-30,.75f,5},99));
 assert(YellowGateEdgeClear(scoped,{0,.75f,5},{0,.75f,-5},100));
 // An oblique narrow CP cannot constrain the preview after its crossing.
 scoped.beginStation=90;scoped.endStation=100;scoped.halfWidth=3;scoped.bodyReserve=1;
 assert(YellowGateEdgeClear(scoped,{0,.75f,0},{20,.75f,2},120,100));
 assert(!YellowGateEdgeClear(scoped,{4,.75f,-1},{4,.75f,1},99,95));
 assert(YellowGateEdgeClear(scoped,{20,.75f,2},{30,.75f,3},130,120));
 // Repeat-lap route boundary on a road crest. A long chord fails although
 // each short, supported floor segment has the full vehicle-width corridor.
 CollisionWorld crest;
 for(unsigned i=0;i<80;++i){float z0=float(i),z1=float(i+1);auto height=[](float z){return 5.f*std::sin(z*3.14159265f/80);};float y0=height(z0),y1=height(z1);
  crest.triangles.push_back({{{-8,y0,z0},{8,y0,z0},{8,y1,z1}},0,0});crest.triangles.push_back({{{-8,y0,z0},{8,y1,z1},{-8,y1,z1}},0,0});
 }
 Prepare(crest);mwel_grid::Launch connector;std::vector<YellowRecoveryPoint> prefix;unsigned failure=0;
 assert(!YellowLineSupported(crest,{0,.75f,0},{0,.75f,80},1.35f,.75f));
 assert(NeoSurfaceApproach(crest,{0,.75f,0},{0,0,80},{0,0,90},1,connector,prefix,failure));
 assert(prefix.size()>20&&prefix[10].p[1]>5&&prefix.back().station==connector.length);
 assert(!NeoSurfaceApproach(stacked,{0,.75f,65},{0,0,110},{0,0,120},1,connector,prefix,failure));
 barriers={{{-8,0,40},{8,0,40}}};
 unsigned blockedReference=0;
 assert(NeoSurfaceApproach(crest,{0,.75f,0},{0,0,80},{0,0,90},1,connector,prefix,failure,&blockedReference)&&blockedReference>0);
 // A reference may cross a median; only an actually checked lane can drive.
 // A barrier spanning the whole supported road still rejects executable plans.
 float start[]={0,.75f,0},forward[]={0,0,1};YellowRacingLine blockedPlan;
 assert(!YellowOptimizeLine(crest,prefix,0,start,forward,25,1,0,nullptr,blockedPlan,nullptr,0,false,25,false,42));
 puts("NEO_CORRIDOR_GEOMETRY_PASS wide floor flattened; narrow L road, gate aperture, wall and stacked missing deck retained");
}
