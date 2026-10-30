// Provider, graph choices and race state owned by NFSMWEventLoader.

#include "../../World2MW/src/NativeLoader/../NeoAI/RouteGraph.h"
#include "../../World2MW/src/NativeLoader/NeoBody.h"
#include "../../World2MW/src/NativeLoader/NeoRouteProjection.h"
#include "NeoManeuverPlannerOwned.h"
#include "NeoNativeGravity.h"
#include "../../World2MW/src/NativeLoader/NeoArcPreference.h"
#include "../../World2MW/src/NativeLoader/../NeoAI/RespawnHistory.h"
#include "../../World2MW/src/NativeLoader/../NeoAI/RecoveryAnchor.h"
// Independent race/progress/choice state. Only immutable authored geometry and
// collision query primitives are shared with CustomAI.
#include "NeoObstacleFile.h"
#include "RepeatLapRoad.h"
#include "NeoRaceRoutePolicy.h"
#include "NeoExecutionReserve.h"
#include "NeoExecutionPreference.h"
#include "NeoBranchRejoin.h"
#include "NeoBranchRecovery.h"
#include "NeoLaneProgress.h"
#include "NeoRecoveryObservation.h"
#include "DriftLinePolicy.h"
using NeoAuthoredObstacle=event_neo_obstacles::Obstacle;
struct NeoCourse {unsigned hash=0;std::vector<mwneo::Graph> legs;std::vector<NeoAuthoredObstacle> obstacles;};
static NeoCourse neoCourse;
static std::set<std::pair<int,unsigned>> neoRejectedBranches;
struct NeoBranchSpan {unsigned branch;float begin,end;};
struct NeoRoad {std::vector<YellowRecoveryPoint> points;std::vector<unsigned> flags;std::vector<NeoBranchSpan> branchSpans;bool branched=false;};
struct NeoProviderState {
 mwel_grid::Launch launch{};bool launchConsidered=false;ULONGLONG approachRetryAt=0;std::vector<YellowRecoveryPoint> launchSurface;
 unsigned key=0,hash=0,lap=~0u;void* race=nullptr;int leg=-1;float station=0;unsigned seed=0,routeRevision=0;
 std::map<int,NeoRoad> roads;std::vector<YellowRecoveryPoint> through,grounded;YellowRacingLine line;ULONGLONG planned=0,logged=0,snapshotAt=0,asyncRequestedAt=0;float buildMs=0;
 std::vector<YellowRecoveryPoint> driftShape;std::vector<mwel_drift_line::Bend> driftBends,driftWholeBends;std::vector<float> driftOrigins;unsigned driftRevision=~0u;int driftLeg=-1;ULONGLONG driftLineLogged=0;
 ULONGLONG lineAcceptedAt=0;
 double queryTotal=0,queryMax=0;unsigned queryCount=0;
 NeoManeuverCache reverseCache{},forwardCache{};
 NeoEscapeCommit escape{};
 mwneo::RespawnHistory respawnHistory{};
 mwneo::RecoveryAnchor recoveryAnchor{};
 YellowGatePassage passage{};
 std::set<int> mainFallbackLegs;
 ULONGLONG blockedSince=0,fallbackRetryAt=0;
 unsigned blockedStaticBranch=0;
 std::map<unsigned,ULONGLONG> staticBranchFailures;
 ULONGLONG lastClearAt=0,seamLogged=0,bridgeLogged=0;
 float lastClearSimulation=0,lastProjectionSimulation=0,lastFlightSimulation=0,flightStartedSimulation=0;
 bool flightLimitReported=false;
 mwel_execution::Preference executionPreference;
 void ForgetSupport(){lastClearAt=blockedSince=fallbackRetryAt=0;lastClearSimulation=lastProjectionSimulation=lastFlightSimulation=flightStartedSimulation=0;flightLimitReported=false;executionPreference.Clear();}
 mwneo::Point observedPosition{};ULONGLONG observedAt=0;float observedSpeed=0;
 ULONGLONG verifiedPlacementAt=0;mwneo::Point verifiedPlacementPosition{},verifiedPlacementHeading{};unsigned lastGuidanceStatus=0;
 unsigned curveFits=0,curveRejects=0;
};
static NeoProviderState neoProviders[16];
static const std::vector<std::pair<Vec3,Vec3>>& YellowBarrierSnapshot();
#include "../../World2MW/src/NativeLoader/NeoLinePlanner.h"
#include "NeoDriftLine.h"
#include "NeoAirborneRoad.h"
#include "NeoNativeGravity.h"
#include "NeoResetObservation.h"
#include "NeoFlightLearning.h"
#include "NeoDeparturePrediction.h"
#include "NeoPositionWatch.h"
#include "NeoTerrainFlight.h"
struct NeoFlightState {void* race=nullptr;void* vehicle=nullptr;unsigned handle=0,body=0;float mass=0,gravity=0;NeoFlightLearning model;NeoDepartureLearning departure;NeoPositionWatch positionWatch;ULONGLONG logged=0,proofLogged=0,departureLogged=0;};
static NeoFlightState neoFlightStates[16];
#include "NeoTerrainContinuation.h"
#include "NeoObservedPlacement.h"
#include "../../World2MW/src/NativeLoader/NeoGateExit.h"
#include "../../World2MW/src/NativeLoader/NeoPendingGate.h"
#include "NeoGateApproach.h"
#include "NeoSurfaceApproach.h"
static float NeoVehicleForwardSpeed(BYTE* vehicle,mwneo::Point direction){
 NeoBodyMotion body;if(!NeoReadBody(vehicle,body))return 0;
 return mwneo::DotXZ({body.velocity[0],body.velocity[1],body.velocity[2]},mwneo::UnitXZ(direction));
}
static bool NeoLoadCourse(unsigned hash){
 if(neoCourse.hash==hash)return !neoCourse.legs.empty();
 if(!LoadYellowNavigation(hash)||!LoadYellowRecoveryRoute(hash))return false;
 NeoCourse candidate;candidate.hash=hash;
 for(unsigned leg=0;leg<yellowRecoveryLegs.size();++leg){
  mwneo::Graph graph;const auto& main=yellowRecoveryLegs[leg];if(main.size()<2)return false;
  std::vector<unsigned> ids;ids.reserve(main.size());
  for(const auto& p:main){mwneo::Point point{p.p[0],p.p[1],p.p[2]};
   if(!ids.empty()&&mwneo::Distance(graph.nodes[ids.back()].position,point)<.01f)ids.push_back(ids.back());
   else ids.push_back(graph.AddNode(point,mwneo::Main|mwneo::ReverseForbidden));
  }
  for(unsigned i=1;i<main.size();++i)if(ids[i-1]!=ids[i]&&!graph.AddEdge(ids[i-1],ids[i],mwneo::Main|mwneo::ReverseForbidden,0))return false;
  if(leg<yellowLocalBranchLegs.size())for(unsigned a=0;a<yellowLocalBranchLegs[leg].size();++a){
   const auto& alternative=yellowLocalBranchLegs[leg][a];
   // Legacy sidecars can still contain discouraged alternatives. They are
   // exclusions for event-owned NeoAI, never candidates for faster routing.
   if(alternative.priority)continue;
   unsigned prior=ids[alternative.start];
   for(unsigned j=1;j<alternative.points.size();++j){auto p=alternative.points[j];
    if(mwneo::Distance(graph.nodes[prior].position,{p[0],p[1],p[2]})<.01f&&j+1<alternative.points.size())continue;
    if(j+1<alternative.points.size()&&mwneo::Distance(graph.nodes[ids[alternative.end]].position,{p[0],p[1],p[2]})<.01f)continue;
    unsigned next=j+1==alternative.points.size()?ids[alternative.end]:graph.AddNode({p[0],p[1],p[2]},mwneo::Branch|mwneo::ReverseForbidden);
    if(prior==next)continue;
    if(!graph.AddEdge(prior,next,mwneo::Branch|mwneo::ReverseForbidden,a+1))return false;prior=next;
   }
  }
  graph.finish=ids.back();if(!graph.Validate())return false;candidate.legs.push_back(std::move(graph));
 }
 char obstacleFile[MAX_PATH]{};
 // Exporters omit this optional asset when no fixed obstacles were authored.
 // A declared but unreadable/malformed asset must still reject the course.
 const bool declared=CourseFile(hash,"ai-obstacles.bin",obstacleFile,sizeof(obstacleFile));
 if(!event_neo_obstacles::Load(declared?obstacleFile:nullptr,hash,candidate.obstacles)){
  Log("MW_NEOAI_OBSTACLES_REFUSED event=%08X",hash);return false;
 }
 neoCourse=std::move(candidate);neoRejectedBranches.clear();
 // NeoAI deliberately plans against NativeWorld/collision.wmc and authored
 // event obstacles only. NativeProps/collision.bun contains smashable props
 // and must never become a steering or braking obstacle source.
 Log("MW_NEOAI_GRAPH event=%08X CP_legs=%u directed=1 validated_acyclic=1 authored_obstacles=%u destructible_props_avoided=0",hash,unsigned(neoCourse.legs.size()),unsigned(neoCourse.obstacles.size()));return true;
}
static const NeoRoad& NeoCommittedRoad(NeoProviderState& state,int leg,bool report=true){
 auto found=state.roads.find(leg);if(found!=state.roads.end())return found->second;
 NeoRoad road;const auto& graph=neoCourse.legs[leg];unsigned node=graph.start,steps=0,previousEdge=~0u;
 auto commit=[&]() -> const NeoRoad& {
  if(leg==0&&state.lap>0&&!yellowRecoveryLegs.empty()&&!yellowRecoveryLegs.back().empty()){
   float before=road.points.empty()?0.f:road.points.back().station;
   bool trimmed=mwel_lap::TrimApproach(road.points,road.flags,yellowRecoveryLegs.back().back().p);
   if(trimmed){float removed=before-road.points.back().station;for(auto& span:road.branchSpans){span.begin-=removed;span.end-=removed;}}
   if(trimmed)road.branched=std::any_of(road.flags.begin(),road.flags.end(),[](unsigned f){return (f&mwneo::Branch)!=0;});
   if(report)Log("MW_NEOAI_REPEAT_LAP_ROAD key=%u lap=%u trimmed=%u approach_removed_m=%.3f CP_writes=0",state.key,state.lap,unsigned(trimmed),trimmed?before-road.points.back().station:0.f);
  }
  return state.roads.emplace(leg,std::move(road)).first->second;
 };
 auto append=[&](unsigned id,unsigned flags){auto p=graph.nodes[id].position;YellowRecoveryPoint point{{p.x,p.y,p.z},0};
  if(!road.points.empty()){const auto& b=road.points.back();point.station=b.station+std::sqrt((p.x-b.p[0])*(p.x-b.p[0])+(p.y-b.p[1])*(p.y-b.p[1])+(p.z-b.p[2])*(p.z-b.p[2]));}
  road.points.push_back(point);road.flags.push_back(flags);
 };
 if(state.mainFallbackLegs.count(leg)){
  for(const auto& source:yellowRecoveryLegs[leg]){YellowRecoveryPoint point=source;if(!road.points.empty()){const auto& b=road.points.back();point.station=b.station+std::sqrt((point.p[0]-b.p[0])*(point.p[0]-b.p[0])+(point.p[1]-b.p[1])*(point.p[1]-b.p[1])+(point.p[2]-b.p[2])*(point.p[2]-b.p[2]));}else point.station=0;road.points.push_back(point);road.flags.push_back(mwneo::Main);}
  if(report)Log("MW_NEOAI_MAIN_FALLBACK_ROAD key=%u CP=%d points=%u blocked_branch_abandoned=1",state.key,leg,unsigned(road.points.size()));
  return commit();
 }
 append(node,mwneo::Main);
 auto costs=mwel_race_route::Costs(graph,neoSettings.profile.lateralAccel,[&](unsigned branch){return branch&&neoRejectedBranches.count({leg,branch})!=0;});
 while(!graph.nodes[node].successors.empty()&&++steps<=graph.edges.size()){
  const auto& choices=graph.nodes[node].successors;
  unsigned mix=state.seed^(unsigned(leg)*0x9E3779B9u)^(node*0x85EBCA6Bu);mix^=mix>>16;mix*=0x7FEB352Du;mix^=mix>>15;
  auto selected=mwel_race_route::Choose(graph,costs,node,previousEdge,mix,neoSettings.profile.lateralAccel);
  if(selected>=graph.edges.size())throw std::runtime_error("No forward race route remains");
  const auto& edge=graph.edges[selected];
  if(choices.size()>1)Log("MW_NEOAI_BRANCH key=%u CP=%d node=%u choice=%u options=%u commit_until_merge=1",state.key,leg,node,edge.branch,unsigned(choices.size()));
  float begin=road.points.back().station;
  if(edge.branch)road.branched=true;node=edge.to;append(node,edge.flags);previousEdge=selected;
  if(edge.branch){float end=road.points.back().station;
   if(!road.branchSpans.empty()&&road.branchSpans.back().branch==edge.branch&&fabs(road.branchSpans.back().end-begin)<.01f)road.branchSpans.back().end=end;
   else road.branchSpans.push_back({edge.branch,begin,end});}
 }
 Log("MW_NEOAI_COMMITTED_ROAD key=%u CP=%d points=%u length_m=%.3f directed_branch_selection=1",state.key,leg,unsigned(road.points.size()),road.points.empty()?0.f:road.points.back().station);
 return commit();
}
static void NeoPrepareDriftShape(NeoProviderState& state,int leg){
 if(!MWELNeoDriftMode())return;
 if(state.driftRevision!=state.routeRevision||state.driftOrigins.size()!=neoCourse.legs.size()){
  std::vector<YellowRecoveryPoint> shape;state.driftOrigins.clear();
  for(unsigned l=0;l<neoCourse.legs.size();++l){
   const auto& points=NeoCommittedRoad(state,int(l)).points;if(points.empty())return;
   float base=shape.empty()?0.f:shape.back().station+mwneo::Distance({shape.back().p[0],shape.back().p[1],shape.back().p[2]},{points.front().p[0],points.front().p[1],points.front().p[2]});
   base-=points.front().station;state.driftOrigins.push_back(base);
   for(auto p:points){p.station+=base;if(!shape.empty()&&p.station<=shape.back().station+.001f)continue;shape.push_back(p);}
  }
  state.driftWholeBends=mwel_drift_line::Bends(shape,MWELNeoDriftMode()==2);state.driftShape=std::move(shape);state.driftRevision=state.routeRevision;state.driftLeg=-1;
  Log("DRIFT_LINE_COURSE handle=%u legs=%u bends=%u CP_independent=1",state.key,unsigned(state.driftOrigins.size()),unsigned(state.driftWholeBends.size()));
  if(MWELNeoDetailedLoggingEnabled()){
   for(const auto& b:state.driftWholeBends)Log("DRIFT_LINE_BEND handle=%u begin=%.6f apex=%.6f end=%.6f side=%d turn=%.6f linked=%u",state.key,b.begin,b.apex,b.end,b.side,b.turn,unsigned(b.linked));
   // Once per route revision. These are diagnostic geometry observations,
   // never a second live reference or a per-frame export. OFF skips the loops.
   for(unsigned i=0;i<state.driftOrigins.size();++i)Log("DRIFT_LINE_ORIGIN handle=%u CP=%u station=%.6f",state.key,i,state.driftOrigins[i]);
   for(unsigned i=0;i<state.driftShape.size();++i){const auto& q=state.driftShape[i];Log("DRIFT_LINE_POINT handle=%u index=%u station=%.6f position=%.6f,%.6f,%.6f",state.key,i,q.station,q.p[0],q.p[1],q.p[2]);}
  }
 }
 if(state.driftLeg!=leg&&leg>=0&&unsigned(leg)<state.driftOrigins.size()){
  state.driftBends=state.driftWholeBends;const auto origin=state.driftOrigins[leg];
  for(auto& b:state.driftBends){b.begin-=origin;b.end-=origin;b.apex-=origin;}state.driftLeg=leg;
 }
}
#include "NeoPlanningAsync.inl"
static unsigned NeoGuidance(const mwneo::Vehicle& v,mwneo::Guidance& g){
 if(!neoSettings.enabled||neoSettings.controller!=2||editorStock||!EventGeometryAvailable()||!mwneo::Finite(v.position)||!mwneo::Finite(v.heading))return 0;
 const bool diagnosticTiming=MWELNeoDetailedLoggingEnabled();double queryStart=diagnosticTiming?Clock():0;
 RenderQueryTimer frameQuery;
 NeoPhaseTrace phaseTrace;phaseTrace.previous=renderWorkProfiling?queryStart:0;collision.profileEnabled=renderWorkProfiling;const auto geometryBefore=collision.profile;
 CollisionWorld::RayQueryScope geometryCache(collision);
 YellowTraversableStepScope stepPolicy;
 unsigned hash=EditorRouteCourseHash();if(!hash||!NeoLoadCourse(hash))return 0;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race)return 0;unsigned count=*reinterpret_cast<unsigned*>(race+0x1958);if(count>16)return 0;
 unsigned slot=16;for(unsigned i=0;i<count;++i)if(*reinterpret_cast<unsigned*>(race+0x18+i*0x194)==v.handle){slot=i;break;}if(slot==16)return 0;
 auto record=race+0x18+slot*0x194;int leg=*reinterpret_cast<int*>(record+0x2C);unsigned lap=*reinterpret_cast<unsigned*>(record+0x28);
 if(leg<0||unsigned(leg)>=neoCourse.legs.size()||record[0x21]||*reinterpret_cast<unsigned*>(record+0x1C))return 0;
 auto& state=neoProviders[slot];
 if(MWELNeoLoggingEnabled()&&(state.key!=v.handle||state.lap!=lap||state.leg!=leg))Log("EVENT_NEOAI_ROUTE_BOUNDARY handle=%u old_lap=%u old_CP=%d lap=%u CP=%d approach_considered=%u approach_metres=%.3f position=%.3f,%.3f,%.3f",v.handle,state.lap,state.leg,lap,leg,unsigned(state.launchConsidered),state.launch.length,v.position.x,v.position.y,v.position.z);
 if(state.hash!=hash||state.key!=v.handle||state.race!=race||state.lap!=lap){DiscardPlan(slot,state);state={};state.hash=hash;state.key=v.handle;state.race=race;state.lap=lap;state.seed=GetTickCount()^v.handle^(lap*1234567);}
 state.lastGuidanceStatus=0;
 NeoNativeDynamics dynamics;MWELNeoMotionState driveMotion;MWELNeoReadMotion(v.handle,driveMotion);BYTE* flightVehicle=nullptr;auto entries=*reinterpret_cast<BYTE***>(0x92CD1C);unsigned entriesCount=*reinterpret_cast<unsigned*>(0x92CD24);
 if(entries&&entriesCount<=128)for(unsigned i=0;i<entriesCount;++i)if(entries[i]&&*reinterpret_cast<unsigned*>(entries[i])==0x8AA828&&*reinterpret_cast<unsigned*>(entries[i]-0x78)==v.handle){flightVehicle=entries[i];break;}
 bool dynamicsRead=NeoReadNativeGravity(flightVehicle,dynamics);auto& flight=neoFlightStates[slot];
 if(flight.race!=race||flight.vehicle!=flightVehicle||flight.handle!=v.handle||flight.body!=dynamics.body||!dynamicsRead||std::abs(flight.mass-dynamics.mass)>1.f||std::abs(flight.gravity-dynamics.gravity)>.01f){flight.positionWatch.Invalidate();flight={};flight.race=race;flight.vehicle=flightVehicle;flight.handle=v.handle;flight.body=dynamics.body;flight.mass=dynamics.mass;flight.gravity=dynamics.gravity;}
 NeoFlightLearning::Observation observation;observation.time=SimulationSeconds();observation.pitch=std::asin(std::clamp(v.heading.y,-1.f,1.f));observation.gravity=dynamics.gravity;observation.contacts=dynamics.contacts;observation.collision=dynamics.collisionFlags;
 observation.eligible=dynamicsRead&&dynamics.forceRead&&v.recovery==mwneo::Recovery::Normal&&dynamics.contacts==0;
 observation.p[0]=v.position.x;observation.p[1]=v.position.y;observation.p[2]=v.position.z;observation.v[0]=v.velocity.x;observation.v[1]=v.velocity.y;observation.v[2]=v.velocity.z;
 NeoDepartureLearning::Observation ground;ground.time=observation.time;ground.contacts=dynamics.contacts;ground.collision=dynamics.collisionFlags;
 ground.known=dynamicsRead&&dynamics.forceRead&&v.recovery==mwneo::Recovery::Normal&&driveMotion.known&&driveMotion.nosRead;
 memcpy(ground.p,observation.p,12);memcpy(ground.v,observation.v,12);ground.pitch=observation.pitch;ground.power=driveMotion.powerScale;ground.filteredAcceleration=driveMotion.acceleration;
 ground.throttle=driveMotion.throttle;ground.brake=driveMotion.brake;ground.steering=driveMotion.steering;ground.mode=unsigned(driveMotion.nosEquipped)+2u*unsigned(driveMotion.nosRequested)+4u*unsigned(driveMotion.nosEngaged);
 bool departureSample=flight.departure.Observe(ground);
 bool flightSample=flight.model.Observe(observation);float flightLow=0,flightHigh=0;float pitchLow=0,pitchHigh=0;bool flightKnown=flight.model.Range(std::abs(v.speed),flightLow,flightHigh)&&flight.model.PitchRange(observation.pitch,pitchLow,pitchHigh)&&dynamics.halfHeight>0;
 if(MWELNeoDetailedLoggingEnabled()&&neoSettings.testMode&&neoSettings.telemetry)flight.positionWatch.Observe(flight.model.previous.p[0],flight.model.previous.time,dynamicsRead&&std::isfinite(dynamics.position[0])&&std::abs(dynamics.position[0]-v.position.x)<.001f);
 if(MWELNeoDetailedLoggingEnabled()&&neoSettings.testMode&&neoSettings.telemetry&&GetTickCount64()>=flight.logged){
  flight.logged=GetTickCount64()+100;
#ifdef MWEL_NEO_OBSERVE_ONLY
  NeoResetObservation resetObservation;bool resetRead=NeoReadResetObservation(flightVehicle,dynamics,resetObservation);
  const auto& r=resetObservation;
  Log("MW_NEOAI_RESET_STATE handle=%u CP=%d lap=%u native_seconds=%.6f read=%u body_pointer=%u reset_pointer=%u driver_class=%u native_up=%.6f,%.6f,%.6f ground_normal=%.6f,%.6f,%.6f,%.6f angular_velocity=%.6f,%.6f,%.6f flipped_seconds=%.6f wheels_on_ground=%d wheel_contacts=%d,%d,%d,%d compression=%.6f,%.6f,%.6f,%.6f wheel_surfaces=%u,%u,%u,%u world_flags=%u world_surface=%u native_null_surface=%u native_default_surface=%u cookie_count=%u cookie_last=%u cookie_position=%.6f,%.6f,%.6f cookie_time=%.6f",v.handle,leg,lap,observation.time,unsigned(resetRead),dynamics.body,r.primary,r.driver,r.up[0],r.up[1],r.up[2],r.normal[0],r.normal[1],r.normal[2],r.normal[3],r.angular[0],r.angular[1],r.angular[2],r.flipped,r.wheels,r.wheelContact[0],r.wheelContact[1],r.wheelContact[2],r.wheelContact[3],r.compression[0],r.compression[1],r.compression[2],r.compression[3],r.wheelSurface[0],r.wheelSurface[1],r.wheelSurface[2],r.wheelSurface[3],r.worldFlags,r.worldSurface,r.nullSurface,r.defaultSurface,r.cookieCount,r.cookieLast,r.cookie[0],r.cookie[1],r.cookie[2],r.cookieTime);
#endif
  Log("MW_NEOAI_FLIGHT_MODEL handle=%u CP=%d lap=%u read=%u contacts=%d collision=%u accepted=%u samples=%u known=%u accel_low=%.6f accel_high=%.6f raw_gravity=%.6f mass=%.3f body_pointer=%u physics_mode=%d vehicle_pointer=%u watch_reference=%u watch_bounds=%u watch_epoch=%u watch_handle=%u watch_body=%u watch_reference_time=%u watch_reference_velocity=%u position_pointer=%u position_getter=%u position_match=%.6f half_height=%.6f pitch=%.6f driver_acceleration=%.6f throttle=%.6f brake=%.6f steering=%.6f power_scale=%.6f motion_known=%u recovery=%u departure_accepted=%u departure_samples=%u nos_allowed=%u nos_read=%u nos_equipped=%u nos_requested=%u nos_engaged=%u nos_tank=%.6f velocity=%.6f,%.6f,%.6f position=%.6f,%.6f,%.6f native_seconds=%.6f",v.handle,leg,lap,unsigned(dynamicsRead),dynamics.contacts,dynamics.collisionFlags,unsigned(flightSample),flight.model.count,unsigned(flightKnown),flightLow,flightHigh,dynamics.gravity,dynamics.mass,dynamics.body,dynamics.physicsMode,unsigned(flightVehicle),unsigned(flight.model.previous.p),unsigned(&flight.positionWatch),unsigned(&flight.race),unsigned(&flight.handle),unsigned(&flight.body),unsigned(&flight.model.previous.time),unsigned(flight.model.previous.v),dynamics.positionPointer,dynamics.positionGetter,std::hypot(std::hypot(v.position.x-dynamics.position[0],v.position.z-dynamics.position[2]),v.position.y-dynamics.position[1]),dynamics.halfHeight,observation.pitch,driveMotion.acceleration,driveMotion.throttle,driveMotion.brake,driveMotion.steering,driveMotion.powerScale,unsigned(driveMotion.known),unsigned(v.recovery),unsigned(departureSample),flight.departure.count,unsigned(driveMotion.nosAllowed),unsigned(driveMotion.nosRead),unsigned(driveMotion.nosEquipped),unsigned(driveMotion.nosRequested),unsigned(driveMotion.nosEngaged),driveMotion.nosTank,v.velocity.x,v.velocity.y,v.velocity.z,v.position.x,v.position.y,v.position.z,observation.time);
 }
 if(state.leg!=leg){DiscardPlan(slot,state);state.leg=leg;state.launchConsidered=false;state.launch={};state.launchSurface.clear();state.verifiedPlacementAt=0;state.lastGuidanceStatus=0;state.ForgetSupport();state.respawnHistory={};state.recoveryAnchor={};state.staticBranchFailures.clear();state.station=0;state.line={};state.through.clear();state.grounded.clear();state.planned=0;state.reverseCache={};state.forwardCache={};state.escape={};state.observedAt=0;if(!MWELNeoDriftMode()){for(auto it=state.roads.begin();it!=state.roads.end();)if(it->first<leg-1||it->first>leg+1)it=state.roads.erase(it);else ++it;}}
 ULONGLONG now=GetTickCount64();
 // A committed shortcut can be geometrically valid yet blocked by fixed
 // construction scenery. After a real stationary blockage, abandon only the
 // current CP's branch and rebuild on its main road. Do not classify or wait
 // for smashable props here: they are intentionally absent from this planner.
 auto committedBeforeFallback=state.roads.find(leg);
 if(state.blockedSince&&now-state.blockedSince>=1200&&now>=state.fallbackRetryAt&&std::abs(v.speed)<2&&!state.mainFallbackLegs.count(leg)&&committedBeforeFallback!=state.roads.end()&&committedBeforeFallback->second.branched){
  state.fallbackRetryAt=now+1000;
  const Vec3 body{v.position.x,v.position.y,v.position.z};RayHit bodyFloor;
  const bool supported=collision.Ray({body.x,body.y+1,body.z},{body.x,body.y-3,body.z},bodyFloor,true);
  auto rejoin=mwel_branch::FindRejoin(yellowRecoveryLegs[leg],v.position,NeoLaneExtent,[&](mwneo::Point point){
   if(!supported||std::abs(bodyFloor.normal.y)<.4f)return false;
   float expected=bodyFloor.point.y-(bodyFloor.normal.x*(point.x-body.x)+bodyFloor.normal.z*(point.z-body.z))/bodyFloor.normal.y;
   RayHit floor;if(!collision.Ray({point.x,expected+1,point.z},{point.x,expected-2,point.z},floor,true)||std::abs(floor.point.y-expected)>1.5f)return false;
   float clearance=std::clamp(body.y-bodyFloor.point.y,.15f,1.f);Vec3 target{point.x,floor.point.y+clearance,point.z};
   return YellowGateEdgeClear(state.passage,body,target)&&YellowLineSupported(collision,body,target,std::clamp(v.halfWidth,.8f,3.5f)+.35f,clearance);
  });
  if(rejoin.valid){
   if(state.blockedStaticBranch&&neoRejectedBranches.insert({leg,state.blockedStaticBranch}).second)
    Log("MW_NEOAI_BRANCH_LEARNED_BLOCKED CP=%d branch=%u shared_this_race=1 reason=stationary_static_wall",leg,state.blockedStaticBranch);
   DiscardPlan(slot,state);state.mainFallbackLegs.insert(leg);state.roads.erase(leg);NeoCommittedRoad(state,leg);
   state.through.clear();state.grounded.clear();state.line={};state.planned=0;state.escape={};state.recoveryAnchor={};state.respawnHistory={};state.station=rejoin.station;++state.routeRevision;state.ForgetSupport();
   Log("MW_NEOAI_BRANCH_ABORT key=%u CP=%d reason=fixed_path_blocked fallback=main reachable_join_m=%.3f main_station=%.3f destructible_props_ignored=1",v.handle,leg,rejoin.distance,rejoin.station);
  }else Log("MW_NEOAI_BRANCH_FALLBACK_DEFERRED key=%u CP=%d old_branch_retained=1 main_join_not_physically_reachable=1",v.handle,leg);
 }
 const auto& committed=NeoCommittedRoad(state,leg);const auto& road=committed.points;if(road.size()<2)return 0;
 NeoPrepareDriftShape(state,leg);
 const float observedElapsed=state.observedAt&&now>=state.observedAt?float(now-state.observedAt)*.001f:0;
 const float observedMoved=state.observedAt?mwneo::Distance(v.position,state.observedPosition):0;
 if(state.observedAt&&now-state.observedAt<500){
  float elapsed=float(now-state.observedAt)*.001f;
  float distance=mwneo::Distance(v.position,state.observedPosition);
  if(distance>45.f+std::max(fabs(v.speed),state.observedSpeed)*elapsed){
   float cursor=state.station;
   bool acquired=NeoObservedPlacement(collision,road,{v.position.x,v.position.y,v.position.z},state.station,cursor);
   if(acquired){DiscardPlan(slot,state);state.ForgetSupport();state.recoveryAnchor.Arm(cursor);state.line={};state.grounded.clear();state.through.clear();state.planned=0;state.escape={};state.reverseCache={};state.forwardCache={};}
   Log("MW_NEOAI_OBSERVED_NATIVE_PLACEMENT key=%u CP=%d displacement=%.3f committed=%.3f physical_cursor=%.3f acquired=%u native_CP_writes=0",v.handle,leg,distance,state.station,cursor,unsigned(acquired));
  }
 }
 state.observedPosition=v.position;state.observedSpeed=fabs(v.speed);state.observedAt=now;
 // Wide/deep CP triggers can advance natively before the body reaches the
 // next canonical road, especially after a collision. Approach that road
 // physically rather than refusing recovery forever at station zero.
 if(!state.launch.length&&state.station<.1f&&now>=state.approachRetryAt)state.launchConsidered=false;
 if(mwel_grid::BeginApproach(state.launchConsidered,count,lap,leg,state.station,v.speed)){
  state.approachRetryAt=now+1000;
  {
   const auto& a=road.front();const auto& b=road[1];
   const mwneo::Point first{a.p[0],a.p[1],a.p[2]},second{b.p[0],b.p[1],b.p[2]};
   unsigned failure=0,referenceObstructions=0;
   bool initialized=leg>0||lap>0?NeoSurfaceApproach(collision,v.position,first,second,v.halfWidth,state.launch,state.launchSurface,failure,MWELNeoDetailedLoggingEnabled()?&referenceObstructions:nullptr):state.launch.Initialize(v.position,first,second);
   bool boundaryReference=false;
   unsigned preceding=leg>0?unsigned(leg-1):unsigned(yellowRecoveryLegs.size()-1);
   if((leg>0||lap>0)&&!initialized&&!yellowRecoveryLegs.empty()&&preceding<yellowRecoveryLegs.size()&&!yellowRecoveryLegs[preceding].empty()){
    const auto& end=yellowRecoveryLegs[preceding].back();
    boundaryReference=initialized=NeoBoundarySurfaceApproach(collision,v.position,{end.p[0],end.p[1],end.p[2]},first,second,v.halfWidth,state.launch,state.launchSurface,failure);
   }
   if(MWELNeoLoggingEnabled()&&leg>0)Log("EVENT_NEOAI_CP_REFERENCE handle=%u CP=%d canonical_boundary=%u accepted=%u failure=%u native_CP_writes=0",v.handle,leg,unsigned(boundaryReference),unsigned(initialized),failure);
   if(MWELNeoLoggingEnabled()&&leg==0&&lap>0)Log("EVENT_NEOAI_LAP_REFERENCE handle=%u lap=%u canonical_boundary=%u accepted=%u failure=%u",v.handle,lap,unsigned(boundaryReference),unsigned(initialized),failure);
   if(leg==0&&lap>0){
    unsigned chordFailure=0;bool chordClear=false;
    if(MWELNeoDetailedLoggingEnabled()&&!state.launchSurface.empty()){
     RayHit floor;if(collision.Ray({v.position.x,v.position.y+1,v.position.z},{v.position.x,v.position.y-3,v.position.z},floor,true)){
      const auto& target=state.launchSurface.back();chordClear=YellowLineSupported(collision,{v.position.x,v.position.y,v.position.z},{target.p[0],target.p[1],target.p[2]},std::clamp(v.halfWidth,.8f,3.5f)+.35f,std::clamp(v.position.y-floor.point.y,.15f,1.f),true,&chordFailure);
     }
    }
    Log("EVENT_NEOAI_LAP_SURFACE_APPROACH handle=%u lap=%u accepted=%u failure=%u samples=%u legacy_chord_clear=%u legacy_chord_failure=%u reference_obstructions=%u reference_only=1 live_lane_arc_wall_checks_retained=1 native_CP_writes=0",v.handle,lap,unsigned(initialized),failure,unsigned(state.launchSurface.size()),unsigned(chordClear),chordFailure,referenceObstructions);
   }
   if(initialized)
    Log("EVENT_NEOAI_GRID_APPROACH handle=%u metres=%.3f canonical_progress=0 CP_writes=0",v.handle,state.launch.length);
   if(state.launch.length&&leg==0&&lap>0)Log("EVENT_NEOAI_REPEAT_LAP_APPROACH handle=%u lap=%u metres=%.3f native_lap_observed=1 CP_writes=0 physical_corridor_checks_retained=1",v.handle,lap,state.launch.length);
  }
 }
 const bool gridApproach=state.station<.1f&&state.launch.Active(v.position);
 if(state.launch.length&&!gridApproach){DiscardPlan(slot,state);state.launch={};state.launchSurface.clear();state.line={};state.grounded.clear();state.through.clear();state.planned=0;}
 g={};g.status=1;g.event=hash;g.lap=lap;g.edge=leg;g.confidence=1;
 float error=0;bool projected=gridApproach||NeoProject(road,v,state.station,state.station,error);
 // Wide drivable lanes can be farther than the authored progress corridor.
 // Track the already checked, fresh same-leg line only when the actual body
 // is within 3 m of it, on the same floor, with no intervening wall. Merely
 // widening authored projection would accept unrelated parallel roads.
 if(!projected&&state.lineAcceptedAt&&now>=state.lineAcceptedAt&&now-state.lineAcceptedAt<=750&&state.line.points.size()>1){
  const float prior=state.station;float laneStation=prior,laneError=0;Vec3 nearest{};RayHit bodyFloor,laneFloor;
  if(NeoProject(state.line.points,v,state.station,laneStation,laneError,3.f,1.5f)&&
     mwel_lane_progress::BoundedAdvance(prior,laneStation,observedMoved,observedElapsed,std::abs(v.speed))&&
     mwel_lane_progress::HasForwardJoin(state.line.points,laneStation,[&](float s,mwneo::Point& out){Vec3 point;if(!YellowLineSample(road,s,point))return false;out={point.x,point.y,point.z};return true;})&&
     YellowLineSample(state.line.points,laneStation,nearest)&&
     collision.Ray({v.position.x,v.position.y+1,v.position.z},{v.position.x,v.position.y-3,v.position.z},bodyFloor,true)&&
     collision.Ray({nearest.x,nearest.y+1,nearest.z},{nearest.x,nearest.y-3,nearest.z},laneFloor,true)&&std::abs(bodyFloor.normal.y)>.4f){
   float expected=bodyFloor.point.y-(bodyFloor.normal.x*(nearest.x-v.position.x)+bodyFloor.normal.z*(nearest.z-v.position.z))/bodyFloor.normal.y;
   Vec3 body{v.position.x,v.position.y,v.position.z};
   if(std::abs(laneFloor.point.y-expected)<=.75f&&(mwel_math::Hypot(body.x-nearest.x,body.z-nearest.z)<.01f||YellowWallCorridor(collision,body,nearest,.2f))){
    state.station=laneStation;error=laneError;projected=true;
    if(now>=state.logged)Log("MW_NEOAI_CHECKED_LANE_PROGRESS key=%u CP=%d prior=%.3f station=%.3f body_moved=%.3f elapsed=%.4f line_age_ms=%llu line_distance=%.3f same_floor=1 authored_reconnection=1 native_CP_writes=0",v.handle,leg,prior,laneStation,observedMoved,observedElapsed,now-state.lineAcceptedAt,laneError);
   }
  }
 }
 if(!projected){g.status=2;g.confidence=.2f;}
 g.progress=state.station;g.lateralError=error;
 float gatePosition[3]{},gateDimensions[3]{},gateRotation=0;
 if(!MWELNeoGate(hash,unsigned(leg),gatePosition,gateDimensions,&gateRotation))return 0;
 float gateAngle=gateRotation*3.14159265358979323846f/180.f;
 state.passage={{gatePosition[0],gatePosition[1],gatePosition[2]},{-std::sin(gateAngle),0,std::cos(gateAngle)},gateDimensions[1]*.5f,gateDimensions[2]*.5f,v.halfWidth+1.f,true};
 state.passage.beginStation=NeoGateApproachBegin(road,state.passage);
  state.passage.endStation=road.back().station;
  if(gridApproach){state.passage.beginStation+=state.launch.length;state.passage.endStation+=state.launch.length;}
 if(NeoPendingGatePassed(road,state.passage,v,state.station)){
  DiscardPlan(slot,state);
  if(!mwneo::SetBlockedPreview(g,v))return 0;
  g.predictiveAvoidance|=mwneo::PendingGateReacquire;
  return 1;
 }
 const bool hadThrough=!state.through.empty();
 if(state.through.empty()&&unsigned(leg+1)<neoCourse.legs.size()&&road.back().station-state.station<320)YellowCustomThroughCP(road,NeoCommittedRoad(state,leg+1).points,state.station,state.through);
 if(state.through.empty()&&unsigned(leg+1)==neoCourse.legs.size()&&road.back().station-state.station<320)NeoFinalGateExit(road,state.passage,state.through);
 if(!hadThrough&&!state.through.empty())DiscardPlan(slot,state);
 std::vector<YellowRecoveryPoint> launchTrajectory;
 if(gridApproach){
  // Geometry/speed preview receives an actual approach path. Canonical race
  // progress remains zero until the body reaches the authored start plane.
  const auto& suffix=state.through.empty()?road:state.through;const float length=state.launch.length;
  launchTrajectory=mwel_grid::ApproachPrefix(state.launch,state.launchSurface);
  mwel_grid::AppendCanonical(suffix,length,launchTrajectory);
 }
 const auto& authoredTrajectory=gridApproach?launchTrajectory:state.through.empty()?road:state.through;float p[]={v.position.x,v.position.y,v.position.z},h[]={v.heading.x,v.heading.y,v.heading.z};
 const float geometricReference=gridApproach?state.launch.Cursor(v.position):state.recoveryAnchor.Reference(state.station);
 float geometricStation=geometricReference;
 // Match the ordinary progress projector's bounded forward window. A fast
 // contact/deceleration can leave progress lagging the physical car; a fixed
 // +16 m geometric window otherwise rejects that same committed road.
 bool geometricProjected=NeoProjectRoad(authoredTrajectory,{p[0],p[1],p[2]},geometricReference,geometricStation,40,state.recoveryAnchor.Active()?8.f:4.f,std::clamp(std::abs(v.speed)*.7f+20,40.f,90.f));
 // A collision/backoff can put the physical car farther behind the committed
 // station. Recover a local cursor on this same CP road; never rewind progress
 // or search a future overlapping loop. Ground support is still checked below.
 if(!geometricProjected&&v.recovery!=mwneo::Recovery::Normal)
  geometricProjected=NeoProjectRoad(authoredTrajectory,{p[0],p[1],p[2]},geometricReference,geometricStation,100,8,90);
 // Green curves carry interpolated authored heights, not sampled hill
 // elevations. A verified local road deck may correct that height error.
 if(!geometricProjected){
  float ahead=std::clamp(std::abs(v.speed)*.7f+20,40.f,90.f);
  geometricProjected=NeoProjectSupportedRoad(collision,state.grounded,{p[0],p[1],p[2]},geometricReference,geometricStation,ahead,&authoredTrajectory);
  if(!geometricProjected){auto measured=NeoGroundTrajectory(collision,authoredTrajectory,geometricReference,p,v.speed,16.f,true);
   if(NeoProjectSupportedRoad(collision,measured,{p[0],p[1],p[2]},geometricReference,geometricStation,ahead,&authoredTrajectory)){state.grounded=std::move(measured);geometricProjected=true;}
  }
  if(geometricProjected&&!projected){state.station=std::max(state.station,std::min(road.back().station,geometricStation));g.progress=state.station;}
 }
 const float simulationNow=SimulationSeconds(),supportAge=simulationNow-state.lastClearSimulation;
 bool supportedFlight=false,supportedLanding=false;
 if(!gridApproach&&v.recovery==mwneo::Recovery::Normal&&state.lastClearSimulation>0&&state.lastProjectionSimulation>0&&
    NeoAirborneRoadProjection(collision,authoredTrajectory,{p[0],p[1],p[2]},v.speed,supportAge,simulationNow-state.lastProjectionSimulation,geometricReference,geometricStation,state.grounded)){
  if(!state.flightStartedSimulation)state.flightStartedSimulation=simulationNow;
  if(simulationNow-state.flightStartedSimulation<=3.f){
   supportedFlight=true;state.lastFlightSimulation=simulationNow;geometricProjected=true;state.station=std::max(state.station,std::min(road.back().station,geometricStation));g.progress=state.station;
  }else if(!state.flightLimitReported){state.flightLimitReported=true;Log("MW_NEOAI_FLIGHT_LIMIT handle=%u CP=%d native_seconds=%.3f",v.handle,leg,simulationNow-state.flightStartedSimulation);}
 }
 if(!geometricProjected&&!gridApproach&&v.recovery==mwneo::Recovery::Normal&&state.lastFlightSimulation>0&&state.lastProjectionSimulation>0&&
    NeoLandingRoadProjection(collision,authoredTrajectory,{p[0],p[1],p[2]},v.speed,simulationNow-state.lastFlightSimulation,simulationNow-state.lastProjectionSimulation,geometricReference,geometricStation,state.grounded)){
  supportedLanding=true;geometricProjected=true;state.station=std::max(state.station,std::min(road.back().station,geometricStation));g.progress=state.station;
 }
 if(!supportedFlight&&state.flightStartedSimulation){RayHit floor;if(collision.Ray({p[0],p[1]+1,p[2]},{p[0],p[1]-1.5f,p[2]},floor,true)&&p[1]-floor.point.y>=0){state.flightStartedSimulation=0;state.flightLimitReported=false;}}
 if(geometricProjected)state.lastProjectionSimulation=simulationNow;
 if(geometricProjected&&!gridApproach)state.recoveryAnchor.Observe(geometricStation,state.station);
 YellowTrafficPoint traffic[128]{};unsigned trafficCount=0;
 // These are explicit editor volumes for indestructible supports. They never
 // come from NativeProps, so smashable boxes, signs and street furniture stay
 // legal contact-racing objects rather than causing avoidance or braking.
 for(unsigned i=0;i<neoCourse.obstacles.size()&&trafficCount<128;++i){const auto& source=neoCourse.obstacles[i];if(!YellowFixedObstacleNearby(p,source.p,mwneo::ObstacleHorizon(v.speed)))continue;auto& obstacle=traffic[trafficCount++];memcpy(obstacle.p,source.p,12);memcpy(obstacle.heading,source.heading,12);obstacle.halfWidth=source.halfWidth;obstacle.halfLength=source.halfLength;obstacle.handle=0xF0000000u+i;obstacle.indestructible=true;obstacle.halfHeight=source.halfHeight;}
 const unsigned authoredTrafficCount=trafficCount;
 auto list=*reinterpret_cast<BYTE***>(0x92CD1C);unsigned vehicles=*reinterpret_cast<unsigned*>(0x92CD24);
 if(list&&vehicles<=128)for(unsigned i=0;i<vehicles&&trafficCount<128;++i){auto other=list[i];if(!other||*reinterpret_cast<unsigned*>(other)!=0x8AA828||*reinterpret_cast<unsigned*>(other-0x78)==v.handle)continue;
  NeoBodyMotion body;if(!NeoReadBody(other,body))continue;auto q=body.position;
  mwneo::Point delta{q[0]-p[0],q[1]-p[1],q[2]-p[2]};float distance=mwneo::LengthXZ(delta);
  if(!mwneo::Finite(delta)||std::abs(delta.y)>4||distance>mwneo::ObstacleHorizon(v.speed))continue;
  // A fast oncoming car is a closing obstacle, not a faster lead car.
  float speed=mwneo::DotXZ({body.velocity[0],body.velocity[1],body.velocity[2]},mwneo::UnitXZ(v.heading));if(mwneo::DotXZ(delta,v.heading)>1&&speed>std::abs(v.speed)+5)continue;
  auto& obstacle=traffic[trafficCount++];memcpy(obstacle.p,q,12);memcpy(obstacle.heading,body.heading,12);memcpy(obstacle.velocity,body.velocity,12);obstacle.halfWidth=body.halfWidth;obstacle.halfLength=body.halfLength;obstacle.handle=*reinterpret_cast<unsigned*>(other-0x78);
  for(unsigned r=0;r<count;++r)if(*reinterpret_cast<unsigned*>(race+0x18+r*0x194)==obstacle.handle){obstacle.raceOpponent=true;break;}
 }
 phaseTrace.Mark(0);BeginYellowBarrierQuery({p[0],p[1],p[2]});static ULONGLONG nextBuild=0;
 const bool committedRecovery=state.escape.active&&v.recovery>=mwneo::Recovery::Reverse&&std::abs(v.speed)<5;
 if(committedRecovery)DiscardPlan(slot,state);else ApplyPlan(slot,state,v,now);
 if(!committedRecovery&&now>=nextBuild&&(!state.planned||now-state.planned>400)){
  if(SubmitPlan(slot,state,v,authoredTrajectory,geometricStation,traffic,trafficCount,now)){
   state.planned=now;nextBuild=now+50;
  }else{
  const double planStart=diagnosticTiming?Clock():0;YellowRacingLine planned;
  // Authored map lines can lie below the actual road surface. Reproject the
  // local preview using the measured road-height offset, without changing X/Z
  // or topological progress. Narrow rays preserve the current stacked road.
  state.grounded=NeoGroundTrajectory(collision,authoredTrajectory,geometricStation,p,v.speed,16.f,true);
  const auto& trajectory=state.grounded;
  Vec3 laneOrigin{},laneAhead{};float preferred=0;
  if(YellowLineSample(trajectory,geometricStation,laneOrigin)&&YellowLineSample(trajectory,std::min(trajectory.back().station,geometricStation+4),laneAhead)){
   auto tangent=mwneo::UnitXZ({laneAhead.x-laneOrigin.x,laneAhead.y-laneOrigin.y,laneAhead.z-laneOrigin.z});
   preferred=NeoPreferredLane(slot,mwneo::CrossXZ(tangent,{p[0]-laneOrigin.x,p[1]-laneOrigin.y,p[2]-laneOrigin.z}));
  }
  const bool driftPlan=MWELNeoDriftMode()!=0;
  if(driftPlan?NeoBuildDriftLine(collision,trajectory,state.driftBends,geometricStation,v,planned,state.passage):YellowOptimizeLine(collision,trajectory,geometricStation,p,h,v.speed,v.halfWidth,preferred,&state.line,planned,traffic,trafficCount,true,v.speed,true,NeoLaneExtent,NeoInitialCurveLimit(v,neoSettings.profile),0.f,true,false,&state.passage)){
   auto checkedPlan=planned;
   if(!driftPlan){NeoSmoothLine(collision,planned,v.halfWidth,traffic,trafficCount,true,v.speed,h,NeoInitialCurveLimit(v,neoSettings.profile),std::clamp(std::abs(v.speed)*.012f,.2f,.8f),true);
   if(NeoCurveLine(collision,planned,v.halfWidth,traffic,trafficCount,true,v.speed,h,NeoInitialCurveLimit(v,neoSettings.profile)))++state.curveFits;else ++state.curveRejects;}
   planned.passage=state.passage;if(!YellowGateLineClear(planned))planned=std::move(checkedPlan);
   state.line=std::move(planned);state.lineAcceptedAt=now;
  }
  state.planned=now;nextBuild=GetTickCount64()+50;state.buildMs=diagnosticTiming?float(Clock()-planStart):0;
  }
 }
 phaseTrace.Mark(1);const auto& trajectory=state.grounded.empty()?authoredTrajectory:state.grounded;
 // An expired plan must never veto a valid arc on the committed route. Select
 // and validate one geometry first, then use that same geometry for speed.
 YellowRacingLine referenceLine;referenceLine.points=trajectory;referenceLine.passage=state.passage;
 const YellowRacingLine* selectedLine=&referenceLine;
 float selectedCursor=geometricStation;
 Vec3 aim{};bool clear=false;YellowArcDiagnostic lastFailure{};unsigned failureMask=0,arcAttempts=0;bool selectedPlan=false;
 float selectedTrafficCost=1e30f,selectedScore=1e30f,selectedLook=0;
 bool originUnsupported=false;
 auto selectArc=[&](){
 for(const auto* candidate:{&state.line,&referenceLine}){
  float cursor=geometricStation;
  if(candidate->points.size()<2||!NeoProjectLine(*candidate,{p[0],p[1],p[2]},geometricStation,cursor)||cursor+6>candidate->points.back().station)continue;
  const unsigned driftMode=MWELNeoDriftMode();
  const float wanted=driftMode?mwel_drift_line::SteeringLook(v.speed,driftMode==2):mwneo::SteeringHorizon(v.speed,neoSettings.profile);
  float look=std::min(wanted,candidate->points.back().station-cursor);
  // Native yaw-request control has a different limit from the grip driver's
  // wheel-lock geometry. A close return target can be clear but unexecutable
  // at this speed, causing the actuator to brake for its own correction arc.
  // Try a longer REAL point on the same line before accepting that slowdown.
  // Every alternative receives the identical body/ground/gate/traffic sweep;
  // ordinary/Arcade paths and the original short-look fallback are unchanged.
  Vec3 nearby{};
  if(driftMode==2&&YellowLineSample(candidate->points,cursor+look,nearby)&&
     std::abs(mwel_drift_line::NativeAimRate(v,{nearby.x,nearby.y,nearby.z}))>mcd::NativeDriftTurnLimit){
   const float maximum=std::min(candidate->points.back().station-cursor,std::clamp(v.speed*1.2f,24.f,40.f));
   for(float extended=look+4;extended<=maximum+.001f;extended+=4){
    Vec3 target{};
    if(!YellowLineSample(candidate->points,cursor+extended,target)||
       !NeoArcFeasible(v,neoSettings.profile,{target.x,target.y,target.z}))continue;
    const float rate=mwel_drift_line::NativeAimRate(v,{target.x,target.y,target.z});
    if(!std::isfinite(rate)||std::abs(rate)>mcd::NativeDriftTurnLimit)continue;
    YellowArcDiagnostic failure;++arcAttempts;
    if(!YellowLineArc(collision,*candidate,cursor,p,h,target,v.halfWidth,nullptr,0,v.speed,
       std::clamp(std::abs(v.speed)*.75f,10.f,55.f),&failure,candidate==&state.line?3.5f:NeoRouteTolerance,
       v.halfLength,NeoExecutionCornerReserve(v),true)){
     lastFailure=failure;failureMask|=1u<<failure.reason;
     if(failure.reason==4){originUnsupported=true;return;}continue;
    }
    const float risk=YellowArcTrafficCost(p,h,target,v.speed,v.halfWidth,traffic,trafficCount);
    if(risk>=1e29f){failureMask|=1u<<9;continue;}
    const float score=NeoArcPreference(extended,wanted,candidate==&state.line,risk,12.f)+
      state.executionPreference.Penalty({target.x-p[0],0,target.z-p[2]},now,geometricStation);
    if(score<selectedScore){clear=true;selectedLine=candidate;selectedCursor=cursor;selectedPlan=candidate==&state.line;
     selectedTrafficCost=risk;selectedScore=score;selectedLook=extended;g.aim={target.x,target.y,target.z};}
    if(risk==0){aim={g.aim.x,g.aim.y,g.aim.z};return;}
    break;
   }
  }
  for(;;){
   // Steering is recomputed every physics tick. Validate the upcoming
   // execution, not a fixed circle all the way to a distant preview point.
   // Otherwise S bends force preview down to six metres and cause weaving.
   // Fixed supports and median pillars need to be seen before the last-second
   // braking zone. Smashable props are in a separate collision domain, so
   // increasing this static-world horizon cannot make NeoAI avoid small props.
   float execution=std::clamp(std::abs(v.speed)*.75f,10.f,55.f);
   if(YellowLineSample(candidate->points,cursor+look,aim)&&NeoArcFeasible(v,neoSettings.profile,{aim.x,aim.y,aim.z})){
    YellowArcDiagnostic failure;++arcAttempts;
    if(YellowLineArc(collision,*candidate,cursor,p,h,aim,v.halfWidth,nullptr,0,v.speed,execution,&failure,candidate==&state.line?3.5f:NeoRouteTolerance,v.halfLength,NeoExecutionCornerReserve(v),true)){
     float risk=YellowArcTrafficCost(p,h,aim,v.speed,v.halfWidth,traffic,trafficCount),score=NeoArcPreference(look,wanted,candidate==&state.line,risk,12.f)+state.executionPreference.Penalty({aim.x-p[0],0,aim.z-p[2]},now,geometricStation);
     if(risk>=1e29f){failureMask|=1u<<9;if(look<=6)break;look=std::max(6.f,look-4);continue;}
     if(score<selectedScore){clear=true;selectedLine=candidate;selectedCursor=cursor;selectedPlan=candidate==&state.line;selectedTrafficCost=risk;selectedScore=score;selectedLook=look;g.aim={aim.x,aim.y,aim.z};}
     if(risk==0&&look>=wanted-.01f){aim={g.aim.x,g.aim.y,g.aim.z};return;}
     break;
    }
    lastFailure=failure;failureMask|=1u<<failure.reason;
    // Every candidate shares this exact origin. Repeating its failed floor
    // ray dozens of times cannot find another supported steering arc.
    if(failure.reason==4){originUnsupported=true;return;}
   }
   if(look<=6)break;look=std::max(6.f,look-4);
  }
 }
 if(clear)aim={g.aim.x,g.aim.y,g.aim.z};
 };
 if(!committedRecovery)selectArc();phaseTrace.Mark(2);
 // A line built before the car moved can have a clear polyline but no safe
 // current steering arc. Replan at the actual pose before requesting a stop.
 if(!committedRecovery&&!clear&&!originUnsupported&&now>=nextBuild&&now-state.planned>=100){
  // Emergency planning uses the same CPU-only snapshot worker. Native pose,
  // live traffic and immediate full-body sweeps stay on the game thread.
  // Until a fresh result passes its ownership/age checks, keep the existing
  // live-validated fallback or the ordinary blocked/recovery controls below.
  // Failure to capture/start the optional worker retains synchronous safety.
  if(SubmitPlan(slot,state,v,authoredTrajectory,geometricStation,traffic,trafficCount,now)){
   state.planned=now;nextBuild=now+50;
  }else{
  DiscardPlan(slot,state);
  Vec3 origin{},ahead{};float preferred=0;
  if(YellowLineSample(trajectory,geometricStation,origin)&&YellowLineSample(trajectory,std::min(trajectory.back().station,geometricStation+4),ahead)){
   auto tangent=mwneo::UnitXZ({ahead.x-origin.x,ahead.y-origin.y,ahead.z-origin.z});preferred=NeoPreferredLane(slot,mwneo::CrossXZ(tangent,{p[0]-origin.x,p[1]-origin.y,p[2]-origin.z}));
  }
  YellowRacingLine replanned;
  const bool driftPlan=MWELNeoDriftMode()!=0;
  if(driftPlan?NeoBuildDriftLine(collision,trajectory,state.driftBends,geometricStation,v,replanned,state.passage):YellowOptimizeLine(collision,trajectory,geometricStation,p,h,v.speed,v.halfWidth,preferred,&state.line,replanned,traffic,trafficCount,true,v.speed,true,NeoLaneExtent,NeoInitialCurveLimit(v,neoSettings.profile),0.f,true,false,&state.passage)){
   auto checkedPlan=replanned;
   if(!driftPlan){NeoSmoothLine(collision,replanned,v.halfWidth,traffic,trafficCount,true,v.speed,h,NeoInitialCurveLimit(v,neoSettings.profile),std::clamp(std::abs(v.speed)*.012f,.2f,.8f),true);
   if(NeoCurveLine(collision,replanned,v.halfWidth,traffic,trafficCount,true,v.speed,h,NeoInitialCurveLimit(v,neoSettings.profile)))++state.curveFits;else ++state.curveRejects;}
   replanned.passage=state.passage;if(!YellowGateLineClear(replanned))replanned=std::move(checkedPlan);
   state.line=std::move(replanned);state.lineAcceptedAt=now;selectArc();
  }
  state.planned=now;nextBuild=GetTickCount64()+50;
  }
 }
 phaseTrace.Mark(3);
 // The drawn route is a direction reference. A safe nearby driving arc need
 // not aim at a point embedded in a wall or occupied by a lead vehicle.
 if(!committedRecovery&&!originUnsupported&&(!clear||selectedTrafficCost>0||selectedLook<mwneo::SteeringHorizon(v.speed,neoSettings.profile)*.55f)){
  const float look=std::clamp(mwneo::SteeringHorizon(v.speed,neoSettings.profile),10.f,50.f);
  float best=selectedScore;Vec3 chosen=aim;const YellowRacingLine* chosenLine=clear?selectedLine:nullptr;float chosenCursor=selectedCursor;
  struct ArcChoice {const YellowRacingLine* line;float cursor,score;Vec3 target;};
  std::array<ArcChoice,22> choices{};unsigned choiceCount=0;
  for(const auto* candidate:{&state.line,&referenceLine}){
   float cursor=geometricStation;if(candidate->points.size()<2||!NeoProjectLine(*candidate,{p[0],p[1],p[2]},geometricStation,cursor))continue;
   Vec3 desired{};if(!YellowLineSample(candidate->points,std::min(candidate->points.back().station,cursor+look),desired))continue;
   auto route=mwneo::UnitXZ({desired.x-p[0],0,desired.z-p[2]});float intended=std::atan2(mwneo::CrossXZ(v.heading,route),mwneo::DotXZ(v.heading,route));
   // A nearby road behind the car is not a valid shortcut entry. Keep the
   // free corridor directed toward this committed CP/branch.
   if(mwneo::DotXZ(mwneo::UnitXZ(v.heading),route)<.15f)continue;
   for(float angle:{0.f,-.08f,.08f,-.16f,.16f,-.24f,.24f,-.36f,.36f,-.5f,.5f}){
    auto forward=mwneo::UnitXZ(v.heading);float c=std::cos(angle),s=std::sin(angle);Vec3 target{p[0]+look*(forward.x*c-forward.z*s),desired.y,p[2]+look*(forward.z*c+forward.x*s)};
    if(mwneo::DotXZ(route,{forward.x*c-forward.z*s,0,forward.z*c+forward.x*s})<.2f)continue;
    float risk=YellowArcTrafficCost(p,h,target,v.speed,v.halfWidth,traffic,trafficCount);
    float score=risk+20*std::abs(angle-intended)+4*std::abs(angle)+(candidate==&referenceLine?1.f:0)+state.executionPreference.Penalty({target.x-p[0],0,target.z-p[2]},now,geometricStation);
    if(score<best)choices[choiceCount++]={candidate,cursor,score,target};
   }
  }
  // Rank inexpensive dynamic predictions first. Once the best supported
  // candidate passes the complete body sweep, lower-ranked ones cannot win.
  std::stable_sort(choices.begin(),choices.begin()+choiceCount,[](const auto& a,const auto& b){return a.score<b.score;});
  for(unsigned i=0;i<choiceCount;++i){const auto& option=choices[i];YellowArcDiagnostic failure;++arcAttempts;
   if(!NeoArcFeasible(v,neoSettings.profile,{option.target.x,option.target.y,option.target.z}))continue;
   if(!YellowLineArc(collision,*option.line,option.cursor,p,h,option.target,v.halfWidth,nullptr,0,v.speed,std::clamp(std::abs(v.speed)*.75f,10.f,55.f),&failure,option.line==&state.line?5.f:NeoRouteTolerance,v.halfLength,NeoExecutionCornerReserve(v),true)){lastFailure=failure;failureMask|=1u<<failure.reason;continue;}
   best=option.score;chosen=option.target;chosenLine=option.line;chosenCursor=option.cursor;break;
  }
  if(chosenLine){clear=true;aim=chosen;selectedLine=chosenLine;selectedCursor=chosenCursor;selectedPlan=chosenLine==&state.line;}
 }
 // Collision floor rays can briefly miss at streaming seams or while a fast
 // car is airborne.  If route projection is still close and a full arc was
 // recently verified, retain route-directed acceleration for that short gap
 // instead of issuing an unexplained full brake on an otherwise clear road.
 // A persistent miss still becomes blocked and enters normal recovery.
 if(!clear&&MWELNeoDetailedLoggingEnabled()&&neoSettings.testMode&&std::abs(v.speed)<25&&now>=state.seamLogged){
  state.seamLogged=now+1000;
  for(float d:{0.f,2.f,4.f,6.f,8.f,10.f,15.f,20.f,30.f}){
   Vec3 q;RayHit floor;const bool sample=YellowLineSample(trajectory,std::min(trajectory.back().station,geometricStation+d),q);
   const bool supported=sample&&collision.Ray({q.x,p[1]+2,q.z},{q.x,p[1]-20,q.z},floor,true);
   Log("MW_NEOAI_SEAM_PROBE handle=%u CP=%d cursor=%.3f d=%.1f point=%.3f,%.3f,%.3f supported=%u floor=%.3f normal=%.3f,%.3f,%.3f reject=%u",v.handle,leg,geometricStation,d,q.x,q.y,q.z,unsigned(supported),supported?floor.point.y:0,supported?floor.normal.x:0,supported?floor.normal.y:0,supported?floor.normal.z:0,lastFailure.reason);
  }
 }
 bool unsupportedCruise=false;
 // Even if an ordinary arc still passes near takeoff/landing, the measured
 // body is above its supported deck. Do not train tyre response in that pose.
 if(supportedFlight)g.predictiveAvoidance|=mwneo::SupportedAirborne;
 if(clear){state.lastClearAt=now;state.lastClearSimulation=simulationNow;if(!committedRecovery&&std::abs(v.speed)>2)state.executionPreference.Observe({aim.x-p[0],0,aim.z-p[2]},now,geometricStation);}
 else {
  const bool transientSupportGap=originUnsupported||lastFailure.reason==5||lastFailure.reason==8||(arcAttempts==0&&geometricProjected);
  const bool closeProjection=geometricProjected&&(!projected||error<12);
  if((transientSupportGap||supportedFlight)&&closeProjection&&std::abs(v.speed)>15&&state.lastClearAt&&(now-state.lastClearAt<=750||supportedFlight)){
  Vec3 referenceAim{};float look=std::clamp(mwneo::SteeringHorizon(v.speed,neoSettings.profile),12.f,32.f);
  if(YellowLineSample(trajectory,std::min(trajectory.back().station,geometricStation+look),referenceAim)){
   const Vec3 origin{p[0],p[1],p[2]},airAhead{referenceAim.x,p[1],referenceAim.z};float width=std::clamp(v.halfWidth,.8f,3.5f)+.35f;
   if(!supportedFlight||(YellowWallCorridor(collision,origin,airAhead,width)&&YellowEditorWallCorridor(origin,airAhead,width))){
    selectedLine=&referenceLine;selectedCursor=geometricStation;aim=referenceAim;unsupportedCruise=true;
   }
  }
  }
 }
 phaseTrace.Mark(4);auto sample=[&](float station,Vec3& point){return NeoPreviewPoint(*selectedLine,trajectory,station,point);};
 // A clear instantaneous steering arc is not proof that the car can follow
 // the next road bend. Runs 75-77 increased recovery demand; retain the
 // road-curvature envelope until an execution rollout proves the override.
 float checkedPrefix=0;
 float end=std::min(trajectory.back().station,selectedCursor+mwneo::SpeedHorizon(v.speed)),distanceAlong=0;
 // A truncated lattice is a checked cul-de-sac, not a lane that can be
 // extrapolated indefinitely. Replan as speed drops before its last point.
 bool planEnd=selectedPlan&&selectedLine->truncated;
 YellowRacingLine terrainContinuation;
 bool flightContinuation=false;
#if defined(MWEL_NEO_TERRAIN_D_EXPERIMENT) && !defined(MWEL_NEO_OBSERVE_ONLY)
#error D experiment is local observational build only
#endif
#ifdef MWEL_NEO_TERRAIN_D_EXPERIMENT
 if(planEnd&&clear&&flightKnown){
  YellowRacingLine fastLanding,slowLanding;NeoFlightPrediction slowPrediction,fastPrediction;
  YellowArcDiagnostic slowFailure,fastFailure,envelopeFailure;
  bool slowOK=NeoTerrainFlightContinuation(collision,*selectedLine,trajectory,selectedCursor,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,slowLanding,&slowFailure,flightLow,&slowPrediction);
  bool fastOK=slowOK&&NeoTerrainFlightContinuation(collision,*selectedLine,trajectory,selectedCursor,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,fastLanding,&fastFailure,flightHigh,&fastPrediction);
  bool envelopeOK=fastOK&&NeoFlightEnvelopeClear(collision,slowLanding,fastLanding,selectedLine->points.back().station-8,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,&envelopeFailure);
  if(envelopeOK){
   terrainContinuation=std::move(slowLanding);flightContinuation=true;
   if(MWELNeoDetailedLoggingEnabled()&&now>=flight.proofLogged){
    flight.proofLogged=now+500;
    Log("MW_NEOAI_TERRAIN_D_EXPERIMENT handle=%u CP=%d lap=%u native_seconds=%.6f source_station=%.3f speed=%.6f acceleration=%.6f,%.6f takeoff=%.6f,%.6f,%.6f launch_velocity=%.6f,%.6f,%.6f slow_landing=%.6f,%.6f,%.6f slow_time=%.6f fast_landing=%.6f,%.6f,%.6f fast_time=%.6f envelope_checked=1",v.handle,leg,lap,observation.time,selectedCursor,v.speed,flightLow,flightHigh,slowPrediction.takeoff.x,slowPrediction.takeoff.y,slowPrediction.takeoff.z,slowPrediction.velocity.x,slowPrediction.velocity.y,slowPrediction.velocity.z,slowPrediction.landing.x,slowPrediction.landing.y,slowPrediction.landing.z,slowPrediction.time,fastPrediction.landing.x,fastPrediction.landing.y,fastPrediction.landing.z,fastPrediction.time);
   }
  }else if(MWELNeoDetailedLoggingEnabled()&&now>=flight.proofLogged){
   flight.proofLogged=now+500;auto failure=!slowOK?slowFailure:!fastOK?fastFailure:envelopeFailure;
   Log("MW_NEOAI_TERRAIN_DECLINED handle=%u CP=%d lap=%u native_seconds=%.6f station=%.3f plan_end=%.3f speed=%.6f accel=%.6f,%.6f stage=%s reason=%u at=%.6f,%.6f,%.6f",v.handle,leg,lap,observation.time,selectedCursor,selectedLine->points.back().station,v.speed,flightLow,flightHigh,!slowOK?"slow":!fastOK?"fast":"envelope",failure.reason,failure.position.x,failure.position.y,failure.position.z);
  }
 }
#endif
 // Expanded flight execution remains shadow-only while launch bounds are
 // validated. The actual driver's a-policy below is unchanged.
 if(planEnd&&clear&&MWELNeoDetailedLoggingEnabled()&&now>=flight.departureLogged){
  flight.departureLogged=now+500;
  YellowRacingLine discoveryLine;NeoFlightPrediction discovery;YellowArcDiagnostic failure;
  NeoTerrainFlightContinuation(collision,*selectedLine,trajectory,selectedCursor,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,discoveryLine,&failure,9.81f,&discovery);
  NeoDeparturePrediction departure;
  bool departureKnown=discovery.approachDistance>0&&NeoPredictDeparture(flight.departure,ground,std::hypot(v.velocity.x,v.velocity.z),discovery.approachDistance,driveMotion.nosAllowed&&driveMotion.nosEquipped,departure);
  float netLow=1e30f,netHigh=0;
  bool boundsKnown=departureKnown&&flight.model.PitchRange(observation.pitch,pitchLow,pitchHigh)&&dynamics.halfHeight>0;
  if(boundsKnown)for(float launchSpeed:{float(departure.arrival.minimumSpeed),float((departure.arrival.minimumSpeed+departure.arrival.maximumSpeed)*.5),float(departure.arrival.maximumSpeed)}){
   float a=0,b=0;if(!flight.model.Range(launchSpeed,a,b)){boundsKnown=false;break;}netLow=std::min(netLow,a);netHigh=std::max(netHigh,b);
  }
  bool lowOK=false,highOK=false,envelopeOK=false;YellowRacingLine lowLine,highLine;NeoFlightPrediction lowPrediction,highPrediction;YellowArcDiagnostic lowFailure,highFailure,envelopeFailure;
  if(boundsKnown){
   NeoDepartureKinematics lower{float(departure.arrival.minimumSpeed),float(departure.arrival.latestSeconds)},upper{float(departure.arrival.maximumSpeed),float(departure.arrival.earliestSeconds)};
   lowOK=NeoTerrainFlightContinuation(collision,*selectedLine,trajectory,selectedCursor,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,lowLine,&lowFailure,netHigh,&lowPrediction,&lower);
   highOK=lowOK&&NeoTerrainFlightContinuation(collision,*selectedLine,trajectory,selectedCursor,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,highLine,&highFailure,netLow,&highPrediction,&upper);
   envelopeOK=highOK&&NeoFlightEnvelopeClear(collision,lowLine,highLine,selectedLine->points.back().station-8,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,&envelopeFailure,dynamics.halfHeight,std::min(pitchLow,std::atan2(lowPrediction.velocity.y,std::hypot(lowPrediction.velocity.x,lowPrediction.velocity.z))),std::max(pitchHigh,std::atan2(highPrediction.velocity.y,std::hypot(highPrediction.velocity.x,highPrediction.velocity.z))));
  }
  Log("MW_NEOAI_DEPARTURE_SHADOW handle=%u CP=%d lap=%u native_seconds=%.6f source_station=%.6f source_speed=%.6f source_throttle=%.6f source_brake=%.6f power=%.6f nos_mode=%u nos_possible=%u approach_distance=%.6f departure_known=%u departure_samples=%u departure_reason=%u missing_mode=%u ground_acceleration=%.6f,%.6f launch_speed=%.6f,%.6f launch_seconds=%.6f,%.6f flight_known=%u flight_acceleration=%.6f,%.6f takeoff=%.6f,%.6f,%.6f low_ok=%u high_ok=%u envelope=%u low_landing=%.6f,%.6f,%.6f low_time=%.6f high_landing=%.6f,%.6f,%.6f high_time=%.6f low_velocity=%.6f,%.6f,%.6f high_velocity=%.6f,%.6f,%.6f reject=%u reject_station=%.6f",v.handle,leg,lap,observation.time,selectedCursor,std::hypot(v.velocity.x,v.velocity.z),driveMotion.throttle,driveMotion.brake,driveMotion.powerScale,ground.mode,unsigned(driveMotion.nosAllowed&&driveMotion.nosEquipped),discovery.approachDistance,unsigned(departureKnown),departure.samples,departure.reason,departure.missingMode,departure.accelerationLow,departure.accelerationHigh,departure.arrival.minimumSpeed,departure.arrival.maximumSpeed,departure.arrival.earliestSeconds,departure.arrival.latestSeconds,unsigned(boundsKnown),netLow,netHigh,discovery.takeoff.x,discovery.takeoff.y,discovery.takeoff.z,unsigned(lowOK),unsigned(highOK),unsigned(envelopeOK),lowPrediction.landing.x,lowPrediction.landing.y,lowPrediction.landing.z,lowPrediction.time,highPrediction.landing.x,highPrediction.landing.y,highPrediction.landing.z,highPrediction.time,lowPrediction.velocity.x,lowPrediction.velocity.y,lowPrediction.velocity.z,highPrediction.velocity.x,highPrediction.velocity.y,highPrediction.velocity.z,(!boundsKnown?0:!lowOK?lowFailure.reason:!highOK?highFailure.reason:envelopeFailure.reason),(!boundsKnown?0:!lowOK?lowFailure.deviation:!highOK?highFailure.deviation:envelopeFailure.deviation));
 }
 if(planEnd&&clear&&(flightContinuation||NeoTerrainContinuation(collision,*selectedLine,trajectory,selectedCursor,v.speed,v.halfWidth,v.halfLength,traffic,trafficCount,terrainContinuation))){
  selectedLine=&terrainContinuation;planEnd=false;
 }
 if(planEnd)end=std::min(end,selectedLine->points.back().station);
 Vec3 previous{};
 for(float s=selectedCursor;g.count<mwneo::MaxSamples;s=std::min(s+6,end)){
  Vec3 point;if(!sample(s,point))break;if(g.count)distanceAlong+=mwel_math::Hypot(point.x-previous.x,point.z-previous.z);previous=point;
  auto& out=g.samples[g.count++];out.position={point.x,point.y,point.z};out.distance=distanceAlong;out.widthLeft=out.widthRight=5;
  out.roadCurvature=NeoDrivingCurvature(*selectedLine,trajectory,s);
  if(planEnd&&s>=selectedLine->points.back().station-.001f)out.flags|=mwneo::PlanEnd;
  if(out.roadCurvature>=0&&out.roadCurvature<.006f)out.flags|=mwneo::LocalCorrection;
  if(s>=end)break;
 }
 if(g.count<2){
  if(!mwneo::SetBlockedPreview(g,v))return 0;
  clear=false;unsupportedCruise=false;selectedPlan=false;aim={g.aim.x,g.aim.y,g.aim.z};
 }
 g.forward=mwneo::UnitXZ(mwneo::Sub(g.samples[1].position,g.samples[0].position));
 if(!clear&&!unsupportedCruise){g.status=2;aim={p[0]+h[0]*10,p[1],p[2]+h[2]*10};}
 else if(geometricProjected){g.status=1;g.confidence=1;}
 if(g.status==2){if(!state.blockedSince)state.blockedSince=now;}else state.blockedSince=0;
 state.blockedStaticBranch=0;
 if(g.status==2&&trafficCount==authoredTrafficCount&&(lastFailure.reason==7||lastFailure.reason==8))for(const auto& span:committed.branchSpans)
  if(geometricStation>=span.begin&&geometricStation<=span.end){state.blockedStaticBranch=span.branch;state.staticBranchFailures[span.branch]=now;break;}
 g.aim={aim.x,aim.y,aim.z};
 // Keep fixed-volume speed feasibility active through the whole detour.
 // Turning away clears the current heading ray before the post is passed;
 // the original committed road still identifies why that detour is needed.
 if(clear&&authoredTrafficCount){
  float end=std::min(trajectory.back().station,geometricStation+std::clamp(std::abs(v.speed)*1.4f+12,18.f,100.f));
  Vec3 previous;if(YellowLineSample(trajectory,geometricStation,previous))for(float station=std::min(geometricStation+4,end);station>geometricStation;station=std::min(station+4,end)){
   Vec3 next;if(!YellowLineSample(trajectory,station,next))break;
   for(unsigned i=0;i<authoredTrafficCount;++i)if(!YellowFixedVolumeClear(previous,next,traffic[i],v.halfWidth+.35f))g.predictiveAvoidance|=mwneo::AuthoredObstacleAvoidance;
   previous=next;if(station>=end)break;
  }
 }
 // Wall clearance reserve or shallow side contact must not become a speed
 // limit for the small steering correction away from that wall. A frontal
 // obstruction still fails the contact-aware corridor and remains blocked;
 // the actual road curvature envelope remains active in PlanSpeed.
 if(clear){
  float distance=std::clamp(std::abs(v.speed)*.2f,4.f,12.f);
  Vec3 origin{p[0],p[1],p[2]},ahead{p[0]+h[0]*distance,p[1],p[2]+h[2]*distance};
  float width=std::clamp(v.halfWidth,.8f,3.5f)+.35f;
  if(!YellowWallCorridor(collision,origin,ahead,width)&&YellowWallCorridor(collision,origin,ahead,width,true))g.predictiveAvoidance|=2;
 }
 // Moving-car passing retains its contact-racing policy. A fixed authored
 // support instead needs enough lateral grip to execute the selected detour.
 if(clear&&mwneo::DotXZ(mwneo::UnitXZ(v.heading),g.forward)>.3f){
  auto forward=mwneo::UnitXZ(v.heading);float horizon=std::clamp(std::abs(v.speed)*1.4f+12,18.f,100.f);
  Vec3 origin{p[0],p[1],p[2]},end{p[0]+forward.x*horizon,p[1],p[2]+forward.z*horizon};
  for(unsigned i=0;i<trafficCount;++i)if(!YellowPredictedTrafficClear(origin,end,traffic[i],v.halfWidth+.35f,0,YellowTrafficArrival(horizon,v.speed)))g.predictiveAvoidance|=i<authoredTrafficCount?mwneo::AuthoredObstacleAvoidance:1;
 }
 // Moving vehicles constrain longitudinal spacing only if their projected
 // body intersects our selected trajectory. In contact racing this identifies
 // a jam for the one-second push/recovery timer, never a cruising speed cap.
 if(list&&vehicles<=128)for(unsigned i=0;i<vehicles;++i){auto other=list[i];if(!other||*reinterpret_cast<unsigned*>(other)!=0x8AA828||*reinterpret_cast<unsigned*>(other-0x78)==v.handle)continue;
  NeoBodyMotion body;if(!NeoReadBody(other,body))continue;auto q=body.position;
  mwneo::Point relative{q[0]-p[0],q[1]-p[1],q[2]-p[2]};float along=mwneo::DotXZ(relative,mwneo::UnitXZ(v.heading));
  if(fabs(relative.y)<3&&along>-(v.halfLength+.5f)&&along<v.halfLength+body.halfLength+1&&fabs(mwneo::CrossXZ(mwneo::UnitXZ(v.heading),relative))<v.halfWidth+body.halfWidth+.5f){
   g.obstacleDistance=std::min(g.obstacleDistance,std::max(0.f,along));g.leadVehicle=mwneo::RaceTrafficPush;g.leadSpeed=std::max(0.f,mwneo::DotXZ({body.velocity[0],body.velocity[1],body.velocity[2]},mwneo::UnitXZ(v.heading)));
  }
  for(unsigned j=1;j<g.count;++j){auto a=g.samples[j-1].position,b=g.samples[j].position;auto d=mwneo::Sub(b,a);float n=mwneo::DotXZ(d,d);if(n<.001f)continue;
   mwneo::Point car{q[0],q[1],q[2]};float t=std::clamp(mwneo::DotXZ(mwneo::Sub(car,a),d)/n,0.f,1.f);mwneo::Point nearest{a.x+t*d.x,a.y+t*d.y,a.z+t*d.z};
   float core=std::max(0.f,body.halfLength-body.halfWidth);
   if(std::abs(car.y-nearest.y)>3||YellowSegmentDistance(a.x,a.z,b.x,b.z,q[0]-body.heading[0]*core,q[2]-body.heading[2]*core,q[0]+body.heading[0]*core,q[2]+body.heading[2]*core)>v.halfWidth+body.halfWidth)continue;
   float distance=g.samples[j-1].distance+t*(g.samples[j].distance-g.samples[j-1].distance);if(distance<2||distance>=g.obstacleDistance)continue;
   float speed=mwneo::DotXZ({body.velocity[0],body.velocity[1],body.velocity[2]},mwneo::UnitXZ(d));g.obstacleDistance=distance;g.leadSpeed=std::max(0.f,speed);g.leadVehicle=mwneo::RaceTrafficPush;
  }
 }
 phaseTrace.Mark(5);
 // A wall eight metres away must not veto a shorter, usable backoff.
 // Reserve rear body length and braking clearance in the driver's target.
 for(float distance:{8.f,6.f,4.f,3.f}){
  Vec3 rear{p[0]-h[0]*distance,p[1],p[2]-h[2]*distance};
  if(YellowLineSupported(collision,{p[0],p[1],p[2]},rear,std::clamp(v.halfWidth,.8f,3.5f)+.35f,.5f,true)){g.rearDistance=distance;break;}
 }
 // Traffic contact is allowed during backoff too. Only static walls and
 // missing road support veto reversing; nearby racers cannot freeze a jam.
 g.rearClear=g.rearDistance>=3;
 phaseTrace.Mark(6);bool contactJam=g.leadVehicle==mwneo::RaceTrafficPush&&g.obstacleDistance<12;
 // A timed-out forward escape starts a fresh recovery. Reusing its already
 // completed reverse distance would immediately cancel the next backoff.
 if(state.escape.Expired(v.recovery))state.escape.active=false;
 if(state.escape.active&&state.escape.previous==mwneo::Recovery::Reverse&&v.recovery!=mwneo::Recovery::Reverse)state.escape.forwardOrigin=v.position;
 if(!originUnsupported&&(g.status==2||v.recovery==mwneo::Recovery::Reverse||contactJam)&&std::abs(v.speed)<5){
  if(!state.escape.active){
  auto reverse=NeoCachedManeuver(state.reverseCache,now,collision,v,neoSettings.profile,trajectory,geometricStation,g.forward,true,g.leadVehicle==mwneo::RaceTrafficPush);
  if(!state.escape.active&&reverse.forwardDistance>0&&v.recovery<=mwneo::Recovery::Reverse){
   state.escape.active=true;state.escape.origin=v.position;state.escape.backSteering=reverse.steering;state.escape.backDistance=reverse.distance;
   state.escape.forwardSteering=reverse.forwardSteering;state.escape.forwardDistance=reverse.forwardDistance;
   Log("MW_NEOAI_ESCAPE_COMMIT key=%u CP=%d back_steering=%.3f back_distance=%.2f forward_steering=%.3f forward_distance=%.2f checked_net_advance=1",v.handle,leg,reverse.steering,reverse.distance,reverse.forwardSteering,reverse.forwardDistance);
  }
  if(reverse.clear){g.rearClear=1;g.rearDistance=reverse.distance+2.8f;g.reverseSteering=reverse.steering;}
  if(g.status==2||contactJam){auto forward=NeoCachedManeuver(state.forwardCache,now,collision,v,neoSettings.profile,trajectory,geometricStation,g.forward,false);
   if(forward.clear){g.maneuverSteering=forward.steering;g.maneuverSpeed=2.5f;}
  }
  }
  if(state.escape.active&&v.recovery<=mwneo::Recovery::Reverse){
   float remaining=state.escape.backDistance-mwneo::Distance(v.position,state.escape.origin);Vec3 end;mwneo::Point heading;
   if(remaining>.15f&&NeoManeuverSweep(collision,v,neoSettings.profile,state.escape.backSteering,true,remaining,nullptr,0,end,heading)&&NeoManeuverLateral(trajectory,geometricStation,end)<=NeoRouteTolerance){
    g.rearClear=1;g.rearDistance=remaining+2.8f;g.reverseSteering=state.escape.backSteering;
   }else if(v.recovery==mwneo::Recovery::Reverse){g.rearClear=0;}
  }
  if(state.escape.active&&(v.recovery==mwneo::Recovery::Realign||v.recovery==mwneo::Recovery::Rejoin)&&(g.status==2||contactJam)){
   float remaining=state.escape.forwardDistance-mwneo::Distance(v.position,state.escape.forwardOrigin);Vec3 end;mwneo::Point heading;
   if(remaining>.5f&&NeoManeuverSweep(collision,v,neoSettings.profile,state.escape.forwardSteering,false,std::min(4.f,remaining),nullptr,0,end,heading)&&NeoManeuverLateral(trajectory,geometricStation,end)<=NeoRouteTolerance){
    g.maneuverSteering=state.escape.forwardSteering;g.maneuverSpeed=2.5f;
   }else state.escape.active=false;
  }
 }
 phaseTrace.Mark(7);state.escape.previous=v.recovery;
 if(now>=state.logged){
 if(supportedLanding)Log("MW_NEOAI_LANDING_REACQUIRED handle=%u CP=%d progress=%.3f native_CP_writes=0",v.handle,leg,g.progress);
 if(supportedFlight)Log("MW_NEOAI_AIRBORNE_SUPPORT handle=%u CP=%d simulation_age_s=%.3f progress=%.3f status=%u coast=%u native_physics_written=0",v.handle,leg,supportAge,g.progress,g.status,unsigned(unsupportedCruise));
 if(MWELNeoDetailedLoggingEnabled()&&!clear){
  Vec3 original{},grounded{},plan{};bool a=YellowLineSample(authoredTrajectory,geometricStation,original),b=YellowLineSample(trajectory,geometricStation,grounded),c=YellowLineSample(state.line.points,geometricStation,plan);
  Log("MW_NEOAI_BLOCKED_REFERENCE handle=%u CP=%d position=%.3f,%.3f,%.3f station=%.3f authored=%u,%.3f,%.3f,%.3f grounded=%u,%.3f,%.3f,%.3f plan=%u,%.3f,%.3f,%.3f plan_begin=%.3f plan_end=%.3f",v.handle,leg,p[0],p[1],p[2],geometricStation,unsigned(a),original.x,original.y,original.z,unsigned(b),grounded.x,grounded.y,grounded.z,unsigned(c),plan.x,plan.y,plan.z,state.line.points.empty()?0.f:state.line.points.front().station,state.line.points.empty()?0.f:state.line.points.back().station);
 }
 state.logged=now+1000;Log("MW_NEOAI_GUIDANCE key=%u event=%08X CP=%d progress=%.2f lateral=%.2f status=%u samples=%u plan_ms=%.3f aim=%.2f,%.2f,%.2f projection=%u geometric_station=%.2f selected_station=%.2f clear=%u unsupported_grace=%u selected_plan=%u plan_points=%u plan_age_ms=%llu traffic=%u arc_attempts=%u reject_mask=%u last_reject=%u reject_at=%.2f,%.2f,%.2f reject_distance=%.2f reject_deviation=%.2f checked_prefix=%.2f curve_fits=%u curve_rejects=%u",v.handle,hash,leg,g.progress,error,g.status,g.count,state.buildMs,aim.x,aim.y,aim.z,unsigned(projected),geometricStation,selectedCursor,unsigned(clear),unsigned(unsupportedCruise),unsigned(selectedPlan),unsigned(state.line.points.size()),now-state.planned,trafficCount,arcAttempts,failureMask,lastFailure.reason,lastFailure.position.x,lastFailure.position.y,lastFailure.position.z,lastFailure.distance,lastFailure.deviation,checkedPrefix,state.curveFits,state.curveRejects);}
 double queryMs=diagnosticTiming?Clock()-queryStart:0;
 if(renderWorkProfiling&&queryMs>3){const auto& c=collision.profile;const auto& b=geometryBefore;Log("MW_NEOAI_QUERY_PHASE key=%u total_ms=%.3f prepare_ms=%.3f plan_ms=%.3f arc_ms=%.3f replan_ms=%.3f fallback_ms=%.3f preview_ms=%.3f rear_ms=%.3f recovery_ms=%.3f rays=%u candidate_rays=%u box_queries=%u",v.handle,queryMs,phaseTrace.ms[0],phaseTrace.ms[1],phaseTrace.ms[2],phaseTrace.ms[3],phaseTrace.ms[4],phaseTrace.ms[5],phaseTrace.ms[6],phaseTrace.ms[7],c.rays-b.rays,c.candidateRays-b.candidateRays,c.boxes-b.boxes);}
 if(diagnosticTiming){state.queryTotal+=queryMs;state.queryMax=std::max(state.queryMax,queryMs);++state.queryCount;
  if(state.logged==now+1000){Log("MW_NEOAI_PERF key=%u query_ms=%.3f mean_ms=%.3f max_ms=%.3f calls=%u",v.handle,queryMs,state.queryTotal/state.queryCount,state.queryMax,state.queryCount);state.queryTotal=state.queryMax=0;state.queryCount=0;}
 }
 if((g.status==2||(g.leadVehicle==mwneo::RaceTrafficPush&&std::abs(v.speed)<1))&&neoSettings.testMode&&neoSettings.telemetry&&now>=state.snapshotAt){
  state.snapshotAt=now+5000;
  for(const auto& q:trajectory)Log("MW_NEOAI_BLOCK_PATH key=%u time=%llu type=reference station=%.6f position=%.6f,%.6f,%.6f",v.handle,now,q.station,q.p[0],q.p[1],q.p[2]);
  for(const auto& q:state.line.points)Log("MW_NEOAI_BLOCK_PATH key=%u time=%llu type=plan station=%.6f position=%.6f,%.6f,%.6f",v.handle,now,q.station,q.p[0],q.p[1],q.p[2]);
  Log("MW_NEOAI_BLOCK_SNAPSHOT key=%u time=%llu CP=%d station=%.6f speed=%.6f position=%.6f,%.6f,%.6f heading=%.6f,%.6f,%.6f half_width=%.6f half_length=%.6f steering=%.6f steering_gain=%.6f",v.handle,now,leg,geometricStation,v.speed,p[0],p[1],p[2],h[0],h[1],h[2],v.halfWidth,v.halfLength,v.steering,v.steeringGain);
  for(unsigned i=0;i<trafficCount;++i){const auto& t=traffic[i];Log("MW_NEOAI_BLOCK_TRAFFIC key=%u time=%llu other=%u position=%.6f,%.6f,%.6f heading=%.6f,%.6f,%.6f velocity=%.6f,%.6f,%.6f half_width=%.6f half_length=%.6f",v.handle,now,t.handle,t.p[0],t.p[1],t.p[2],t.heading[0],t.heading[1],t.heading[2],t.velocity[0],t.velocity[1],t.velocity[2],t.halfWidth,t.halfLength);}
  for(const auto& wall:YellowBarrierSnapshot())if(YellowSegmentDistance(p[0],p[2],p[0],p[2],wall.first.x,wall.first.z,wall.second.x,wall.second.z)<20)Log("MW_NEOAI_BLOCK_WALL key=%u time=%llu a=%.6f,%.6f,%.6f b=%.6f,%.6f,%.6f",v.handle,now,wall.first.x,wall.first.y,wall.first.z,wall.second.x,wall.second.y,wall.second.z);
 }
 state.lastGuidanceStatus=g.status;return g.status;
}
#include "NeoRespawnOwned.inl"
#pragma comment(linker,"/EXPORT:MWEL_NeoGuidanceV1=_MWEL_NeoGuidanceV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoGuidanceV1(const mwneo::Vehicle* v,mwneo::Guidance* g){
 if(!v||!g||v->size!=sizeof(*v)||v->version!=mwneo::AbiVersion||g->size!=sizeof(*g)||g->version!=mwneo::AbiVersion)return 0;
 __try{return NeoGuidance(*v,*g);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
