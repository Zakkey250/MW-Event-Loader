#include "../../World2MW/src/NeoAI/Driver.h"
#include "../../World2MW/src/NeoAI/JamWatch.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include "NeoExecutionPreference.h"
#include "NeoBranchRejoin.h"
#include "NeoBranchRecovery.h"
#include "NeoLaneProgress.h"
#include "NeoPlanShift.h"
#include "../../World2MW/src/NativeLoader/NeoRouteProjection.h"
#include "NeoRaceRoutePolicy.h"
#include "NeoExecutionReserve.h"
struct Vec3 {float x,y,z;};
#include "../../World2MW/src/NativeLoader/YellowBarrierCorridor.h"
#include "../../World2MW/src/NativeLoader/YellowReferencePolicy.h"
#include "../../World2MW/src/NativeLoader/NeoCorridorRelax.h"
using namespace mwneo;
static void Require(bool condition,const char* label){if(!condition)throw std::runtime_error(label);}
static Guidance Straight(){Guidance g;g.status=1;g.confidence=1;g.count=13;g.aim={0,0,20};g.forward={0,0,1};
 for(unsigned i=0;i<g.count;++i){auto& s=g.samples[i];s.position={0,0,float(i*5)};s.distance=float(i*5);s.widthLeft=s.widthRight=6;s.roadCurvature=0;s.flags=Main|CheckedExecution|LocalCorrection;}return g;}
int main(){try{
 Require(mwel_branch::RepeatedJam(2,260,263,0),"repeated native blockage did not allow checked main-fork recovery");
 Require(!mwel_branch::RepeatedJam(1,260,263,0)&&!mwel_branch::RepeatedJam(2,260,291,0)&&!mwel_branch::RepeatedJam(2,260,263,20),"moving or genuinely advancing branch was abandoned");
 {auto old=[](float s,Point& p){if(s>56)return false;p={0,3,s};return true;};
  auto same=[](float s,Point& p){p={0,3,s};return true;};
  auto shifted=[](float s,Point& p){p={3,3,s};return true;};
  auto none=[](float,Point&){return false;};
  auto a=mwel_plan_shift::Measure(16,old,same),b=mwel_plan_shift::Measure(16,old,shifted),c=mwel_plan_shift::Measure(16,old,none);
  Require(a.samples==5&&a.mean==0&&a.maximum==0,"plan shift must use overlapping authored stations");
  Require(b.samples==5&&b.mean==3&&b.maximum==3,"plan shift must measure actual horizontal movement");
  Require(b.nearSamples==3&&b.nearMean==3&&b.nearMaximum==3,"near-line measurements must cover actual overlap 8/16/24 metres ahead");
  Require(c.samples==0,"missing plan overlap is unknown rather than stable");}
 {Require(mwel_lane_progress::BoundedAdvance(50,56,3,.1f,40),"checked-lane progress must accept physical forward movement");
  Require(!mwel_lane_progress::BoundedAdvance(50,90,3,.1f,40),"a nearby future crossing cannot cause a large station jump");
  Require(!mwel_lane_progress::BoundedAdvance(50,56,3,1,40),"stale motion cannot authorize line progress");
  struct Knot{float p[3];float station;};std::vector<Knot> lane={{{32,0,50},50},{{32,0,100},100}};
  auto original=[](float s,Point& p){p={0,0,s};return true;};
  Require(!mwel_lane_progress::HasForwardJoin(lane,50,original),"a self-rooted line on a parallel street is not route evidence");
  lane.push_back({{0,0,140},140});Require(mwel_lane_progress::HasForwardJoin(lane,50,original),"a checked continuation returning to the authored corridor must remain usable");
  lane.back().p[1]=8;Require(!mwel_lane_progress::HasForwardJoin(lane,50,original),"reconnection on a different authored deck must be refused");
 }
 {struct Knot{float p[3];float station;};std::vector<Knot> authored={{{0,0,0},0},{{0,0,200},200}},lane={{{32,0,0},0},{{32,0,200},200}};
  Vehicle car;car.position={32,0,70};car.heading={0,0,1};car.speed=40;float station=60,error=0;
  Require(!NeoProject(authored,car,60,station,error),"authored progress corridor must not accept an unrelated parallel road");
  Require(NeoProject(lane,car,60,station,error,3.f,1.5f)&&station==70,"actual checked wide-lane progress must advance in its canonical station coordinates");
  car.position={38,0,70};Require(!NeoProject(lane,car,60,station,error,3.f,1.5f),"departure from the checked lane must not advance progress");
  car.position={32,6,70};Require(!NeoProject(lane,car,60,station,error,3.f,1.5f),"a different deck must not project onto the checked lane");
  car.position={32,0,190};Require(!NeoProject(lane,car,60,station,error,3.f,1.5f),"checked lane projection must retain the bounded forward window");
  car.position={32,0,55};station=60;Require(NeoProject(lane,car,60,station,error,3.f,1.5f)&&station==60,"same-road reverse movement must not rewrite committed progress");
 }
 {struct Knot{float p[3];float station;};std::vector<Knot> main={{{0,0,0},0},{{0,0,100},100},{{0,0,200},200}};
  auto fork=mwel_branch::FindRecoveryFork(main,{0,0,100},{20,0,150},1,10000,9000);
  Require(fork.valid&&fork.station==100,"branch recovery must map the shared departure node into main coordinates");
  Require(!mwel_branch::FindRecoveryFork(main,{0,0,100},{20,0,150},0,10000,9000).valid,"first jam must retain ordinary recovery");
  Require(!mwel_branch::FindRecoveryFork(main,{0,0,100},{20,0,150},1,10000,0).valid,"traffic-only blockage cannot exclude a branch");
  Require(!mwel_branch::FindRecoveryFork(main,{0,0,100},{20,0,150},1,10000,4000).valid,"stale static obstruction cannot justify a branch reset");
  Require(!mwel_branch::FindRecoveryFork(main,{0,8,100},{20,0,150},1,10000,9000).valid,"overlapping roads on another deck cannot establish fork identity");
  Require(!mwel_branch::FindRecoveryFork(main,{0,0,100},{300,0,150},1,10000,9000).valid,"branch recovery must remain within the existing bounded physical placement radius");
  main.push_back({{0,0,100},300});
  Require(!mwel_branch::FindRecoveryFork(main,{0,0,100},{20,0,150},1,10000,9000).valid,"ambiguous repeated departure nodes must retain the old road");
 }
 {struct Knot{float p[3];float station;};std::vector<Knot> main={{{0,0,0},0},{{0,0,100},100}};
  auto far=mwel_branch::FindRejoin(main,{300,0,50},42,[](Point){return true;});
  Require(!far.valid,"distant main road must not inherit a branch station coordinate");
  auto blocked=mwel_branch::FindRejoin(main,{10,0,50},42,[](Point){return false;});
  Require(!blocked.valid,"a nearby but physically disconnected main road must retain the branch");
  auto reachable=mwel_branch::FindRejoin(main,{10,0,50},42,[](Point){return true;});
  Require(reachable.valid&&reachable.station==50&&reachable.distance==10,"reachable rejoin must use the main road's own station");
 }
 {Vehicle car;car.speed=70;std::vector<std::pair<Vec3,Vec3>> post{{{0,0,0},{0,2,.5f}}};
  Vec3 center{1,0,-2};
  Require(YellowBarrierSegmentsClear(post,{.3f,0,-2},{.3f,0,2},.05f,true,.025f,&center),"legacy corner fixture must remain a thin clearance");
  Require(!YellowBarrierSegmentsClear(post,{.3f,0,-2},{.3f,0,2},.05f,true,NeoExecutionCornerReserve(car),&center),"fast body corner must reserve clearance around a short fixed post");
  Require(YellowBarrierSegmentsClear(post,{1.5f,0,-2},{1.5f,0,2},.05f,true,NeoExecutionCornerReserve(car),&center),"reserve must leave a genuinely clear lane open");}
 {Vehicle car;car.speed=70;Vec3 center{1,0,-2};std::vector<std::pair<Vec3,Vec3>> wall{{{0,0,0},{0,2,20}}};
  Require(!YellowBarrierSegmentsClear(wall,{.3f,0,-2},{.3f,0,2},.05f,true,NeoExecutionCornerReserve(car),&center),"approaching a finite wall end is not existing parallel contact");
  center={1,0,1};Require(YellowBarrierSegmentsClear(wall,{.03f,0,1},{.03f,0,3},.05f,true,NeoExecutionCornerReserve(car),&center),"real contact along a continuous wall must remain escapable");
  std::vector<std::pair<Vec3,Vec3>> post{{{0,0,0},{0,2,.5f}}};center={1,0,.1f};
  Require(YellowBarrierSegmentsClear(post,{.03f,0,.1f},{1,0,.1f},.05f,true,NeoExecutionCornerReserve(car),&center),"actual post contact must allow outward escape");}
 {Graph graph;auto a=graph.AddNode({0,0,0}),b=graph.AddNode({0,0,100}),c=graph.AddNode({100,0,0});graph.start=a;graph.finish=c;
  graph.AddEdge(a,b,Main);graph.AddEdge(b,c,Main);graph.AddEdge(a,c,Branch,1);Require(graph.Validate(),"forward race graph fixture invalid");
  auto costs=mwel_race_route::Costs(graph,60,[](unsigned){return false;});
  for(unsigned seed=0;seed<20;++seed)Require(mwel_race_route::Choose(graph,costs,a,~0u,seed,60)==2,"slower random detour must not beat the fast route");
  costs=mwel_race_route::Costs(graph,60,[](unsigned branch){return branch==1;});
  Require(mwel_race_route::Choose(graph,costs,a,~0u,0,60)==0,"fixed blocked alternative must retain the forward main route");}
 {mwel_execution::Preference memory;
  memory.Observe({.2f,0,1},1000,40);
  Require(memory.Penalty({.2f,0,1},1100,45)==0,"same checked exit direction must retain its preference");
  Require(memory.Penalty({-.2f,0,1},1100,45)>4,"opposite near-equal fork aim must pay a soft continuity cost");
  Require(memory.Penalty({-.2f,0,1},1800,45)==0&&memory.Penalty({-.2f,0,1},900,45)==0,"stale or future execution memory must expire");
  memory.Clear();Require(memory.Penalty({-.2f,0,1},1100,45)==0,"retired route must not retain its old turn preference");
 }
 {struct Knot{float p[3];float station;};std::vector<Knot> wavy;
  for(unsigned i=0;i<=20;++i)wavy.push_back({{i%2?3.f:-3.f,0,float(i*8)},float(i*8)});
  wavy.front().p[0]=wavy.back().p[0]=0;auto original=wavy;float before=NeoCorridorEnergy(wavy);
  NeoRelaxCorridor(wavy,[](unsigned,float*){return true;});float after=NeoCorridorEnergy(wavy);
  Require(after<before*.3f,"wide supported corridor must flatten needless centreline oscillations");
  Require(wavy.front().p[0]==0&&wavy.back().p[0]==0,"corridor endpoints must remain anchored");
  for(unsigned i=0;i<wavy.size();++i)Require(wavy[i].station==original[i].station&&(!i||wavy[i].p[2]>wavy[i-1].p[2]),"corridor relaxation must retain forward station order");
  auto moved=original;unsigned knot=8;float allBefore=NeoCorridorEnergy(moved),localBefore=NeoCorridorEnergyRange(moved,knot,knot+2);moved[knot].p[0]+=.5f;
  Require(std::abs((NeoCorridorEnergy(moved)-allBefore)-(NeoCorridorEnergyRange(moved,knot,knot+2)-localBefore))<.001f,"local energy delta must include every affected bend");
  auto rejected=original;NeoRelaxCorridor(rejected,[](unsigned,float*){return false;});
  for(unsigned i=0;i<rejected.size();++i)Require(rejected[i].p[0]==original[i].p[0],"unsupported or blocked moves must preserve the checked fallback");
  auto narrow=original;NeoRelaxCorridor(narrow,[&](unsigned i,float* p){return p[0]*original[i].p[0]>0&&std::abs(p[0])>=2.75f;});
  for(unsigned i=1;i+1<narrow.size();++i)Require(std::abs(narrow[i].p[0])>=2.75f,"a narrow or gated corridor must not be cut for smoothness");
  std::printf("NEO_CORRIDOR_PASS energy %.3f -> %.3f checked fallback and forward stations retained\n",before,after);
 }
 {Vehicle car;car.position={0,0,0};car.heading={0,0,1};car.velocity={0,0,60};car.speed=60;
  Require(PredictSteeringCurve(car,{0,0,70},0,0)==0,"straight response prediction must remain straight");
  auto left=PredictSteeringCurve(car,{0,0,70},.5f,0);
  auto right=PredictSteeringCurve(car,{0,0,70},-.5f,0);
  Require(left<0&&right>0&&std::abs(left+right)<.00001f,"measured turn momentum must unwind symmetrically");
  car.velocity={40,0,0};Require(PredictSteeringCurve(car,{0,0,70},.5f,.01f)==.01f,"large skid must not extrapolate an untrusted heading");
  car.velocity={0,0,60};Require(PredictSteeringCurve(car,{0,0,5},.5f,.01f)==.01f,"close recovery targets must retain ordinary steering");}
 Profile p;Vehicle v;v.heading={0,0,1};v.speed=70;
 YellowTrafficPoint box{};box.heading[2]=1;box.halfWidth=2;box.halfLength=10;box.halfHeight=5;box.indestructible=true;
 Require(!YellowPredictedTrafficClear({-2.8f,0,10.8f},{-2.8f,0,10.8f},box,1,0,0),"fixed OBB corner must not inherit rounded car capsule");
 Require(!YellowPredictedTrafficClear({-5,0,0},{5,0,0},box,1,0,0),"fixed body crossing must be rejected");
 Require(!YellowPredictedTrafficClear({-3.3f,0,-12},{-3.3f,0,12},box,1,0,0),"fixed volume requires rotating-body reserve");
 Require(YellowPredictedTrafficClear({-3.6f,0,-12},{-3.6f,0,12},box,1,0,0),"clear reserved corridor must remain eligible");
 Require(YellowPredictedTrafficClear({-2.8f,0,0},{-5,0,0},box,1,0,0),"pre-existing contact must permit outward escape");
 Require(!YellowPredictedTrafficClear({-2.8f,0,0},{5,0,0},box,1,0,0),"contact must not permit cutting through fixed body");
 Require(YellowPredictedTrafficClear({-5,8,0},{5,8,0},box,1,0,0),"stacked road above box must remain eligible");
 Require(YellowTrafficPenalty(box)>=1e29f,"indestructible collision must never be a finite traffic preference");
 box.indestructible=false;Require(YellowPredictedTrafficClear({-2.8f,0,10.8f},{-2.8f,0,10.8f},box,1,0,0)&&YellowTrafficPenalty(box)==220,"moving-car capsule and soft cost changed");
 box.indestructible=true;box.p[2]=8;box.halfWidth=1;box.halfLength=2;box.halfHeight=2;
 float hillCar[]={0,10,-10};Require(YellowFixedObstacleNearby(hillCar,box.p,100),"downhill obstacle must not be filtered by current car height");
 Require(!YellowPredictedTrafficClear({0,10,-10},{0,0,10},box,1,0,0),"descending road must detect fixed obstacle before reaching its height");
 Require(YellowPredictedTrafficClear({0,10,-10},{0,10,10},box,1,0,0),"upper stacked road must remain clear of lower obstacle");
 auto g=Straight();g.aim={5,0,10};g.predictiveAvoidance=1;
 auto moving=PlanSpeed(g,v,p);Require(moving.valid&&moving.target==p.maxSpeed,"moving-car passing policy changed");
 g.predictiveAvoidance=2;Require(PlanSpeed(g,v,p).target==p.maxSpeed,"parallel-wall correction changed");
 g.predictiveAvoidance=AuthoredObstacleAvoidance;
 auto fixed=PlanSpeed(g,v,p);float expected=std::sqrt(p.lateralAccel/.08f);
 Require(fixed.valid&&std::abs(fixed.target-expected)<.001f&&fixed.constraint==1,"fixed support steering arc must respect lateral grip");
 g.predictiveAvoidance|=1;Require(std::abs(PlanSpeed(g,v,p).target-expected)<.001f,"moving car must not cancel fixed support limit");
 Driver driver;driver.effort=.8f;auto controls=driver.Update(v,g,p);
 Require(controls.valid&&controls.throttle==0&&controls.brake>0,"overspeed fixed detour must cut stale throttle");
 g=Straight();g.samples[3].position.x=8;g.samples[4].position.x=8;g.predictiveAvoidance=AuthoredObstacleAvoidance;
 auto detour=PlanSpeed(g,v,p);Require(detour.valid&&detour.target<p.maxSpeed&&detour.maxCurvature>.005f,"checked straight road must retain actual fixed detour curvature");
 g.predictiveAvoidance=1;Require(PlanSpeed(g,v,p).target==p.maxSpeed,"temporary moving passing shape must retain existing policy");
 g.status=2;g.predictiveAvoidance|=AuthoredObstacleAvoidance;Require(PlanSpeed(g,v,p).target==0,"blocked corridor must still stop");
 // A real path end/fixed stop needs braking now, rather than another third
 // of a second of stale throttle. This is opt-in to the owned event driver;
 // ordinary passing and below-envelope acceleration retain their policy.
 {auto end=Straight();end.samples[10].flags|=PlanEnd;Driver d;d.gripGuard=true;d.effort=.8f;
  auto out=d.Update(v,end,p);Require(out.valid&&out.targetSpeed<v.speed-5&&out.throttle==0&&out.brake>0,"overspeed path end must cut stale throttle");
  Driver legacy;legacy.effort=.8f;Require(legacy.Update(v,end,p).throttle>0,"legacy driver must retain its prior path-end pedal policy");
  Vehicle slow=v;slow.speed=10;Driver launch;launch.gripGuard=true;auto below=launch.Update(slow,end,p);
  Require(below.valid&&below.throttle>0&&below.brake==0,"path-end envelope must permit below-target acceleration");
  auto fixedStop=Straight();fixedStop.obstacleDistance=8;Driver stop;stop.gripGuard=true;stop.effort=.8f;
  auto braking=stop.Update(v,fixedStop,p);Require(braking.throttle==0&&braking.brake>0,"fixed obstacle overspeed must cut stale throttle");
  fixedStop.leadVehicle=1;fixedStop.leadSpeed=60;Driver passing;passing.gripGuard=true;passing.effort=.8f;
  Require(passing.Update(v,fixedStop,p).throttle>0,"vehicle spacing must retain existing passing pedal ramp");}
 g=Straight();g.samples[10].roadCurvature=.04f;g.predictiveAvoidance=1;Require(PlanSpeed(g,v,p).target<p.maxSpeed,"moving passing must retain road braking preview");
 // Grip learning: a straight road must never move the learned lateral ceiling.
 {Driver d;d.gripGuard=true;Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.0083f;auto road=Straight();
  for(int i=0;i<240;++i)d.Update(car,road,p);
  Require(d.gripCeiling==p.lateralAccel,"straight running must not change the learned grip ceiling");}
 // Steering saturated while the chassis does not rotate: tyres are past grip.
 // The ceiling must drop (never below the 65% floor), throttle must lift.
 {Driver d;d.gripGuard=true;Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.0083f;auto road=Straight();road.aim={30,0,40};
  Controls last{};for(int i=0;i<240;++i){car.steering=d.steering;last=d.Update(car,road,p);}
  Require(d.gripCeiling<p.lateralAccel*.9f&&d.gripCeiling>=p.lateralAccel*.65f-.001f,"understeer must lower the learned grip ceiling within its floor");
  Require(last.valid&&last.throttle==0,"saturated understeer must lift the throttle");
  float learned=d.gripCeiling;d.Reset();Require(d.gripCeiling==learned&&d.gripGuard,"respawn reset must keep the learned grip ceiling");}
 {Driver off;Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.0083f;auto road=Straight();road.aim={30,0,40};
  for(int i=0;i<240;++i){car.steering=off.steering;off.Update(car,road,p);}
  Require(off.gripCeiling==p.lateralAccel,"guard off (NFRR) must never touch the grip ceiling");}
 // A deficit window has already demonstrated useful yaw response. A later
 // transient dip must not snap its learned ceiling to the minimum.
 {Driver d;d.gripGuard=true;d.gripCeiling=p.lateralAccel;d.understeerTime=.295f;d.understeerPeak=p.lateralAccel*.9f;
  d.initialized=true;d.lastSpeed=50;d.lastHeading={0,0,1};d.yawRate=p.lateralAccel*.4f/50;
  d.steering=SteeringLimit(50,p,d.response.gain);
  Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.01f;auto road=Straight();road.aim={30,0,40};
  d.Update(car,road,p);Require(d.gripCeiling>p.lateralAccel*.9f&&d.gripCeiling<p.lateralAccel,"transient yaw dip must retain demonstrated window grip");
  d.Reset();Require(d.understeerPeak==0,"respawn must clear the old yaw response window");}
 // The provider can approve an arc using configured grip while the learned
 // speed ceiling is lower. The driver must brake for that ceiling and still
 // retain enough wheel authority to follow the approved arc during braking.
 {Driver d;d.gripGuard=true;d.gripCeiling=p.lateralAccel*.65f;d.steering=.75f;
  Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.0083f;auto road=Straight();road.aim={30,0,40};road.predictiveAvoidance=AuthoredObstacleAvoidance;
  auto learned=p;learned.lateralAccel=d.gripCeiling;auto safe=PlanSpeed(road,car,learned);auto ordinary=PlanSpeed(road,car,p);
  auto out=d.Update(car,road,p);
  Require(out.valid&&safe.target<ordinary.target&&std::abs(out.targetSpeed-safe.target)<.001f,"learned grip must retain its lower speed envelope");
  Require(std::abs(out.steering)>SteeringLimit(car.speed,learned,d.response.gain)+.001f,"learned speed must not remove configured steering authority");}
 // Uncapped races must accelerate beyond both former limits (330/400),
 {Vehicle car;car.heading={0,0,1};car.speed=70;car.dt=.01f;auto road=Straight();road.aim={15,0,30};road.predictiveAvoidance=2;
  Driver d;d.gripGuard=true;d.gripCeiling=p.lateralAccel*.65f;d.effort=.8f;
  auto out=d.Update(car,road,p);float k=2*15.f/(15*15+30*30);
  Require(out.valid&&out.targetSpeed<=std::sqrt(d.gripCeiling/k)+.001f&&out.throttle==0&&out.brake>0,"learned grip must constrain the checked local avoidance arc");
  Driver legacy;legacy.effort=.8f;Require(legacy.Update(car,road,p).targetSpeed==p.maxSpeed,"guard off must retain passing policy");
  road.aim={0,0,30};Driver straight;straight.gripGuard=true;straight.gripCeiling=p.lateralAccel*.65f;
  Require(straight.Update(car,road,p).targetSpeed==p.maxSpeed,"learned grip must not impose a straight-line cap");
  road.aim={15,0,30};road.predictiveAvoidance=SupportedAirborne;Driver flight;flight.gripGuard=true;flight.gripCeiling=p.lateralAccel*.65f;
  Require(flight.Update(car,road,p).targetSpeed==p.maxSpeed,"airborne support must not reintroduce the unsupported-corner brake");}
 // Uncapped races must accelerate beyond both former limits (330/400),
 {Driver d;d.gripGuard=true;d.gripCeiling=p.lateralAccel;Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.01f;
  auto road=Straight();road.aim={30,0,40};road.predictiveAvoidance=SupportedAirborne;
  for(unsigned i=0;i<240;++i){car.steering=d.steering;d.Update(car,road,p);}
  Require(d.gripCeiling==p.lateralAccel&&d.understeerTime==0,"airborne low yaw must not train a false tyre grip loss");}
 // A stronger steering profile must not raise the minimum measured tyre
 // grip. Default-50 keeps its existing 32.5 floor; NFRR's guard is opt-in.
 {Profile strong=p;strong.lateralAccel=60;Driver d;d.gripGuard=true;
  Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.01f;auto road=Straight();road.aim={30,0,40};
  for(unsigned i=0;i<240;++i){car.steering=d.steering;d.Update(car,road,strong);}
  Require(d.gripCeiling>=32.5f&&d.gripCeiling<39,"stronger steering must not force a higher measured tyre floor");}
 // Suspension settling after a supported flight cannot train grip, braking
 // or steering response, and the hold uses physics dt rather than wall time.
 {Driver d;d.gripGuard=true;d.gripCeiling=p.lateralAccel*.8f;
  Vehicle car;car.heading={0,0,1};car.speed=50;car.dt=.01f;auto road=Straight();road.predictiveAvoidance=SupportedAirborne;
  d.Update(car,road,p);float learned=d.gripCeiling;road.predictiveAvoidance=0;road.aim={30,0,40};
  for(unsigned i=0;i<25;++i){d.understeerTime=.4f;d.understeerPeak=0;car.steering=d.steering;d.Update(car,road,p);}
  Require(d.gripCeiling==learned&&d.understeerTime==0&&d.response.time==0&&d.brakes.time==0,"landing hold must not learn suspension/yaw transients");
  Require(d.landingLearningHold>0,"landing hold must survive 250 ms physics time");
  for(unsigned i=0;i<12;++i)d.Update(car,road,p);
  Require(d.landingLearningHold==0,"landing hold must expire in physics time");
  d.Reset();Require(d.landingLearningHold==0,"respawn must clear old landing hold");}
 // Uncapped races must accelerate beyond both former limits (330/400),
 // while every actual road/obstacle constraint remains binding.
 {Profile unlimited=p;unlimited.maxSpeed=0;
  for(float kmh:{0.f,330.f,400.f,450.f,600.f,900.f}){
   Vehicle fast=v;fast.speed=kmh/3.6f;fast.dt=1.f/120;fast.velocity={0,0,fast.speed};
   auto road=Straight();auto plan=PlanSpeed(road,fast,unlimited);
   Driver d;d.gripGuard=true;auto out=d.Update(fast,road,unlimited);
   Require(plan.valid&&std::isfinite(plan.target)&&plan.target>fast.speed+2&&plan.constraint==0,"uncapped straight target stopped rising");
   Require(out.valid&&out.throttle==1&&out.brake==0,"uncapped straight must use full engine output");
   road.samples[10].roadCurvature=.04f;auto corner=PlanSpeed(road,fast,unlimited);
   Require(corner.valid&&(kmh==0||corner.constraint==1&&corner.target<plan.target),"uncapped mode lost corner braking");
   road=Straight();road.obstacleDistance=8;auto obstacle=PlanSpeed(road,fast,unlimited);
   Require(obstacle.valid&&obstacle.constraint==6&&obstacle.target<plan.target,"uncapped mode lost fixed obstacle braking");
   road=Straight();road.status=2;Require(PlanSpeed(road,fast,unlimited).target==0,"uncapped blocked road must stop");
  }
  unlimited.maxSpeed=-1;Require(!PlanSpeed(Straight(),v,unlimited).valid,"negative speed policy must fail closed");
 }

 std::puts("PASS speed policy: uncapped 0/330/400/450/600/900 km/h, legacy caps, fixed obstacles, road curves, recovery and grip");return 0;
 }catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
