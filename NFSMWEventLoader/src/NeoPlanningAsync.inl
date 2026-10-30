// Included after planner helpers. Native state is captured only by SubmitPlan.
struct NeoPlanInput {
 mwel_async_geometry::Snapshot geometry;
 std::vector<YellowRecoveryPoint> road;std::vector<mwel_drift_line::Bend> driftBends;
 std::vector<std::pair<Vec3,Vec3>> barriers;
 std::vector<YellowTrafficPoint> traffic;
 mwneo::Vehicle vehicle;mwneo::Profile profile;
 YellowRacingLine previous;YellowGatePassage passage;
 float station=0;unsigned slot=0;bool diagnosticTiming=false,drift=false;
};
#include "NeoPlanShift.h"
struct NeoPlanOutput {
 std::vector<YellowRecoveryPoint> grounded;YellowRacingLine line;
 mwneo::Point position{},heading{};float buildMs=0;bool optimized=false,curveFit=false;
 mwel_plan_shift::Result shift{};
};
using NeoPlanWorker=mwel_async::Worker<NeoPlanInput,NeoPlanOutput>;
// No static thread destructor under the loader lock. The pinned ASI owns this
// process-lifetime worker; race retirement invalidates jobs without waiting.
static NeoPlanWorker* planWorker=nullptr;
static bool planWorkerFailed=false;
static std::uint64_t planRaceGeneration=1;
static std::optional<NeoPlanOutput> ComputePlan(NeoPlanInput& input,const mwel_async::Cancel& cancel){
 if(cancel.Requested())return {};
 const double start=input.diagnosticTiming?Clock():0;
 mwel_async_geometry::World world(std::move(input.geometry));
 mwel_async_geometry::Binding binding(world);
 CollisionWorld local(mwel_async_geometry::Api());
 CollisionWorld::RayQueryScope query(local);
 YellowTraversableStepScope stepPolicy;
 nearbyWalls=input.barriers;
 const auto& v=input.vehicle;float p[]={v.position.x,v.position.y,v.position.z},h[]={v.heading.x,v.heading.y,v.heading.z};
 NeoPlanOutput out;out.position=v.position;out.heading=v.heading;
 out.grounded=NeoGroundTrajectory(local,input.road,input.station,p,v.speed,16.f,true);
 if(cancel.Requested())return {};
 Vec3 origin{},ahead{};float preferred=0;
 if(YellowLineSample(out.grounded,input.station,origin)&&YellowLineSample(out.grounded,std::min(out.grounded.back().station,input.station+4),ahead)){
  auto tangent=mwneo::UnitXZ({ahead.x-origin.x,ahead.y-origin.y,ahead.z-origin.z});
  preferred=NeoPreferredLane(input.slot,mwneo::CrossXZ(tangent,{p[0]-origin.x,p[1]-origin.y,p[2]-origin.z}));
 }
 auto traffic=input.traffic.data();unsigned count=unsigned(input.traffic.size());
 if(input.drift?NeoBuildDriftLine(local,out.grounded,input.driftBends,input.station,v,out.line,input.passage):YellowOptimizeLine(local,out.grounded,input.station,p,h,v.speed,v.halfWidth,preferred,&input.previous,out.line,traffic,count,true,v.speed,true,NeoLaneExtent,NeoInitialCurveLimit(v,input.profile),0.f,true,false,&input.passage)){
  out.optimized=true;auto checked=out.line;
  if(cancel.Requested())return {};
  if(!input.drift)NeoSmoothLine(local,out.line,v.halfWidth,traffic,count,true,v.speed,h,NeoInitialCurveLimit(v,input.profile),std::clamp(std::abs(v.speed)*.012f,.2f,.8f),true);
  if(cancel.Requested())return {};
  if(!input.drift)out.curveFit=NeoCurveLine(local,out.line,v.halfWidth,traffic,count,true,v.speed,h,NeoInitialCurveLimit(v,input.profile));
  out.line.passage=input.passage;if(!YellowGateLineClear(out.line))out.line=std::move(checked);
 }
 if(cancel.Requested()||!world.Complete())return {};
 if(input.diagnosticTiming&&out.optimized){
  auto sample=[](const auto& line,float station,mwneo::Point& point){Vec3 p;if(!YellowLineSample(line.points,station,p))return false;point={p.x,p.y,p.z};return true;};
  out.shift=mwel_plan_shift::Measure(input.station,[&](float s,mwneo::Point& p){return sample(input.previous,s,p);},[&](float s,mwneo::Point& p){return sample(out.line,s,p);});
 }
 out.buildMs=input.diagnosticTiming?float(Clock()-start):0;return out;
}
static bool EnsurePlanWorker(){
 if(planWorker)return true;if(planWorkerFailed||!neoSettings.asyncPlanning)return false;
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&EnsurePlanWorker),&pinned)){planWorkerFailed=true;return false;}
 auto worker=new NeoPlanWorker;
 // Keep the native game/render thread some headroom on small CPUs.
 DWORD_PTR processMask=0,systemMask=0;unsigned available=0;
 if(GetProcessAffinityMask(GetCurrentProcess(),&processMask,&systemMask))
  for(auto bits=processMask;bits;bits&=bits-1)++available;
 if(!available)available=std::thread::hardware_concurrency();
 const unsigned workers=mwel_async::PlanningWorkers(available);
 if(!worker->Start(ComputePlan,workers)){delete worker;planWorkerFailed=true;return false;}
 planWorker=worker;Log("EVENT_NEOAI_ASYNC_STARTED workers=%u available_logical=%u slots=16 native_queries_on_worker=0 module_pinned=1",workers,available);return true;
}
static mwel_async::Identity PlanIdentity(const NeoProviderState& state){
 return {planRaceGeneration,std::uint64_t(collision.BackendIdentity()),state.hash,state.key,state.lap,state.leg};
}
static void DiscardPlan(unsigned slot,NeoProviderState& state){if(planWorker)planWorker->Discard(slot);state.asyncRequestedAt=0;}
static bool SubmitPlan(unsigned slot,NeoProviderState& state,const mwneo::Vehicle& v,const std::vector<YellowRecoveryPoint>& road,float station,const YellowTrafficPoint* traffic,unsigned count,ULONGLONG now){
 if(!EnsurePlanWorker()||road.size()<2)return false;
 NeoPlanInput input;input.vehicle=v;input.profile=neoSettings.profile;input.road=road;input.previous=state.line;input.passage=state.passage;input.station=station;input.slot=slot;input.diagnosticTiming=MWELNeoLoggingEnabled();
 input.drift=MWELNeoDriftMode()!=0;if(input.drift)input.driftBends=state.driftBends;
 input.barriers=nearbyWalls;input.traffic.assign(traffic,traffic+count);
 Vec3 low{v.position.x,v.position.y,v.position.z},high=low;
 auto expand=[&](Vec3 p){low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};};
 const float first=std::max(road.front().station,station-16),last=std::min(road.back().station,station+mwneo::SpeedHorizon(v.speed)+24);
 Vec3 point;if(YellowLineSample(road,first,point))expand(point);if(YellowLineSample(road,last,point))expand(point);
 for(const auto& p:road)if(p.station>=first&&p.station<=last)expand({p.p[0],p.p[1],p.p[2]});
 low={low.x-64,low.y-8,low.z-64};high={high.x+64,high.y+8,high.z+64};
 if(!mwel_async_geometry::Capture(collision,{low,high},input.geometry))return false;
 if(!planWorker->Submit(slot,{PlanIdentity(state),now,std::move(input)}))return false;
 state.asyncRequestedAt=now;return true;
}
static bool ApplyPlan(unsigned slot,NeoProviderState& state,const mwneo::Vehicle& v,ULONGLONG now){
 if(!planWorker||!state.asyncRequestedAt)return false;
 NeoPlanOutput out;if(!planWorker->Take(slot,PlanIdentity(state),now,200,out))return false;
 const auto age=now-state.asyncRequestedAt;
 state.asyncRequestedAt=0;
 // Live arc/traffic checks still run after accepting this advisory plan.
 if(mwneo::Distance(out.position,v.position)>16||mwneo::DotXZ(mwneo::UnitXZ(out.heading),mwneo::UnitXZ(v.heading))<.85f)return false;
 state.grounded=std::move(out.grounded);state.buildMs=out.buildMs;
 Log("EVENT_NEOAI_ASYNC_APPLIED key=%u CP=%d age_ms=%llu worker_ms=%.3f optimized=%u line_shift_mean=%.3f line_shift_max=%.3f line_shift_samples=%u near_shift_mean=%.3f near_shift_max=%.3f near_shift_samples=%u",state.key,state.leg,age,out.buildMs,unsigned(out.optimized),out.shift.mean,out.shift.maximum,out.shift.samples,out.shift.nearMean,out.shift.nearMaximum,out.shift.nearSamples);
 if(out.optimized){state.line=std::move(out.line);state.lineAcceptedAt=now-age;if(out.curveFit)++state.curveFits;else ++state.curveRejects;}
 return true;
}
