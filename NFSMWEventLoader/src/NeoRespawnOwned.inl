// Native physical placement; progress is never rewritten.

#include "../../World2MW/src/NativeLoader/NeoRespawnGeometry.h"
#include "NeoRecoveryPrefix.h"
static unsigned NeoRespawn(const mwneo::Vehicle& v,void* ai){
 if(!neoSettings.enabled||neoSettings.controller!=2||editorStock||!EventGeometryAvailable())return 0;
 auto vt=*reinterpret_cast<unsigned**>(ai);
 auto events=GetModuleHandleW(L"MWEventLoader.asi");auto local=events?reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(events,"MWEL_LocalRaceTestV1")):nullptr;
 bool localPlayer=(neoSettings.testMode&&neoSettings.autoStart)||(local&&local());
 if((vt!=reinterpret_cast<unsigned*>(0x892640)&&!(vt==reinterpret_cast<unsigned*>(0x8929F0)&&localPlayer))||
    vt[23]!=0x422730)return 0;
 auto primary=static_cast<BYTE*>(ai)-0x4C;auto sim=*reinterpret_cast<BYTE**>(primary+0x34);
 if(!sim||*reinterpret_cast<unsigned*>(sim+8)!=v.handle)return 0;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race||*reinterpret_cast<unsigned*>(race+0x1960)!=1||!race[0x198A]||!race[0x1978])return 0;
 unsigned count=*reinterpret_cast<unsigned*>(race+0x1958);if(count>16)return 0;
 for(unsigned i=0;i<count;++i){
  auto info=race+0x18+i*0x194;auto& state=neoProviders[i];
  if(*reinterpret_cast<unsigned*>(info)!=v.handle||state.key!=v.handle||state.race!=race||state.hash!=EditorRouteCourseHash()||info[0x21]||*reinterpret_cast<unsigned*>(info+0x1C))continue;
  int leg=*reinterpret_cast<int*>(info+0x2C);unsigned lap=*reinterpret_cast<unsigned*>(info+0x28);
  if(leg!=state.leg||lap!=state.lap)return 0;
  auto found=state.roads.find(leg);if(found==state.roads.end()||found->second.points.size()<2)return 0;
  const auto& committed=found->second.points;
  auto canonicalDirection=mwneo::UnitXZ({committed[1].p[0]-committed[0].p[0],0,committed[1].p[2]-committed[0].p[2]});
  float canonicalEntry=mwneo::DotXZ(mwneo::Sub(v.position,{committed.front().p[0],committed.front().p[1],committed.front().p[2]}),canonicalDirection);
  if(leg>0&&state.station<.1f&&!state.launch.length&&canonicalEntry < -std::max(2.f,v.halfLength)){
   const auto& previous=NeoCommittedRoad(state,leg-1).points;
   if(NeoRecoveryPrefix(previous,committed,state.launch,state.launchSurface)){
    DiscardPlan(i,state);state.line={};state.through.clear();state.grounded.clear();state.planned=0;
    Log("EVENT_NEOAI_RECOVERY_APPROACH key=%u CP=%d length=%.3f previous_authored_road=1 native_CP_lap_writes=0",v.handle,leg,state.launch.length);
   }
  }
  // A repeat-lap approach precedes canonical station zero. Recover on the
  // same checked prefix used by driving, in its nonnegative local coordinates,
  // rather than clamping the car forward to the original grid. Native and
  // canonical race progress remain unchanged.
  std::vector<YellowRecoveryPoint> approachRoad;
  const bool approaching=state.station<.1f&&state.launch.Active(v.position);
  float coordinateOffset=0;
  if(approaching){
   coordinateOffset=state.launch.length;approachRoad=mwel_grid::ApproachPrefix(state.launch,state.launchSurface);
   mwel_grid::AppendCanonical(committed,coordinateOffset,approachRoad);
  }
  const auto& authored=approaching?approachRoad:committed;
  auto entry=mwneo::UnitXZ({authored[1].p[0]-authored[0].p[0],0,authored[1].p[2]-authored[0].p[2]});
  float beforeEntry=mwneo::DotXZ(mwneo::Sub(v.position,{authored.front().p[0],authored.front().p[1],authored.front().p[2]}),entry);
  if(mwel_grid::BeforeRecoveryEntry(state.station,approaching,approaching?state.launch.Cursor(v.position):0,beforeEntry,v.halfLength)){
   Log("MW_NEOAI_RESPAWN_DEFERRED key=%u CP=%d before_reference_m=%.3f approach=%u forward_placement_prevented=1",v.handle,leg,-beforeEntry,unsigned(approaching));return 0;
  }
  float reference=approaching?state.launch.Cursor(v.position):state.recoveryAnchor.Reference(state.station),cursor=reference,p[]={v.position.x,v.position.y,v.position.z};
  if(!NeoProjectRoad(authored,{p[0],p[1],p[2]},reference,cursor,100,8,90)){
   auto measured=NeoGroundTrajectory(collision,authored,reference,p,0,16.f,true);
   // A wedged car may be outside the projection corridor entirely.  Recovery
   // must still search backward from its last committed same-leg station; all
   // destination poses are independently grounded, swept and continuation
   // checked below, and can never advance CP/lap progress.
   if(!NeoProjectSupportedRoad(collision,measured,{p[0],p[1],p[2]},reference,cursor,90,&authored)){
    cursor=std::min(reference,state.station+coordinateOffset);
    Log("MW_NEOAI_RESPAWN_OFF_ROUTE key=%u CP=%d station=%.3f search_from_committed=1",v.handle,leg,cursor);
   }
  }
  // Never place ahead of this car's current geometric position, its already
  // committed progress, or its pending CP. Retain the chosen branch and lap.
  cursor=std::min(cursor,approaching?reference:state.station);
  NeoRoad fallbackRoad;unsigned failedBranch=0;const float oldStation=state.station;
  const std::vector<YellowRecoveryPoint>* recoveryRoad=&authored;
  // A short clear continuation can still lead back into the same fixed
  // blockage. After one failed same-branch recovery, consider going behind
  // its common departure node. Prepare the alternate road locally; native
  // pose/continuation and unchanged CP/lap must pass before committing it.
  if(!approaching&&state.respawnHistory.count&&unsigned(leg)<yellowLocalBranchLegs.size()){
   for(const auto& span:found->second.branchSpans){
    if(state.station<span.begin||state.station>span.end||!span.branch||span.branch>yellowLocalBranchLegs[leg].size())continue;
    if(mwel_branch::RepeatedJam(state.respawnHistory.count,state.respawnHistory.anchor,state.station,v.speed)){
     state.staticBranchFailures[span.branch]=GetTickCount64();
     Log("MW_NEOAI_BRANCH_NATIVE_JAM key=%u CP=%d branch=%u attempts=%u station=%.3f anchor=%.3f actual_stationary_recovery=1 checked_main_fork_required=1",v.handle,leg,span.branch,state.respawnHistory.count,state.station,state.respawnHistory.anchor);
    }
    auto failure=state.staticBranchFailures.find(span.branch);if(failure==state.staticBranchFailures.end())continue;
    const auto& alternative=yellowLocalBranchLegs[leg][span.branch-1];
    if(alternative.start>=yellowRecoveryLegs[leg].size())continue;
    const auto& departure=yellowRecoveryLegs[leg][alternative.start];
    NeoProviderState pending=state;pending.roads.erase(leg);pending.mainFallbackLegs.insert(leg);
    auto proposed=NeoCommittedRoad(pending,leg,false);
    auto fork=mwel_branch::FindRecoveryFork(proposed.points,{departure.p[0],departure.p[1],departure.p[2]},v.position,state.respawnHistory.count,GetTickCount64(),failure->second);
    if(!fork.valid||fork.station<proposed.points.front().station+12)continue;
    fallbackRoad=std::move(proposed);recoveryRoad=&fallbackRoad.points;cursor=fork.station;failedBranch=span.branch;break;
   }
  }
  auto road=NeoGroundTrajectory(collision,*recoveryRoad,cursor,p,0,220.f,true);
  unsigned candidates=0,groundRejected=0,distanceRejected=0,poseRejected=0,historyRejected=0,occupiedRejected=0,continuationRejected=0,nativeAttempts=0;
  BeginYellowBarrierQuery({p[0],p[1],p[2]});
  // A multi-car pile can occupy every nearby recovery slot. Search farther
  // back on the same committed CP leg so each racer gets an independent,
  // verified pose instead of refusing recovery behind the first car.
  // Do not put the car back on the exact pose where the jam was detected.
  // Several authored lines pass through a fixed support or wall; a zero-metre
  // placement was valid as a static pose but immediately repeated the same
  // collision.  One car-length of run-up gives the freshly rebuilt guidance a
  // clear approach while remaining strictly behind committed race progress.
  for(float back:{6.f,12.f,20.f,32.f,48.f,64.f,96.f,128.f,160.f,192.f}){
   float station=std::max(road.front().station,cursor-back);Vec3 center{},a{},b{};
   if(!YellowLineSample(road,station,center)||!YellowLineSample(road,std::max(road.front().station,station-2),a)||!YellowLineSample(road,std::min(road.back().station,station+2),b))continue;
   auto direction=mwneo::UnitXZ({b.x-a.x,b.y-a.y,b.z-a.z});if(mwneo::LengthXZ(direction)<.9f)continue;
   float current=mwneo::CrossXZ(direction,{p[0]-center.x,0,p[2]-center.z});
   for(float offset:{0.f,current,current-2,current+2,current-4,current+4,current-6,current+6,-6.f,6.f,-12.f,12.f,-24.f,24.f,-36.f,36.f}){
    if(fabs(offset)>NeoLaneExtent)continue;
    Vec3 candidate{center.x-direction.z*offset,center.y,center.z+direction.x*offset},surface;
    // Independently validate the authored destination deck. A vertical fall
    // must not disable recovery after crossing an arbitrary height limit.
    ++candidates;
    if(!YellowAimGround(collision,candidate,center.y,.75f,surface)){++groundRejected;continue;}
    if(!NeoRespawnDistanceAllowed(v.position,surface)){++distanceRejected;continue;}
    if(!NeoRespawnPoseClear(collision,v,surface,direction)){++poseRejected;continue;}
    if(!state.respawnHistory.Allows({surface.x,surface.y,surface.z},state.station,std::max(2.f,2*v.halfWidth+1.f))){++historyRejected;continue;}
    auto list=*reinterpret_cast<BYTE***>(0x92CD1C);unsigned vehicles=*reinterpret_cast<unsigned*>(0x92CD24);bool occupied=false;
    if(!list||vehicles>128)return 0;
    for(unsigned j=0;j<vehicles;++j){auto other=list[j];NeoBodyMotion body;if(!other||*reinterpret_cast<unsigned*>(other)!=0x8AA828||*reinterpret_cast<unsigned*>(other-0x78)==v.handle||!NeoReadBody(other,body))continue;
     if(fabs(body.position[1]-surface.y)>3)continue;
     float ownCore=std::max(0.f,v.halfLength-v.halfWidth),otherCore=std::max(0.f,body.halfLength-body.halfWidth);
     if(YellowSegmentDistance(surface.x-direction.x*ownCore,surface.z-direction.z*ownCore,surface.x+direction.x*ownCore,surface.z+direction.z*ownCore,
        body.position[0]-body.heading[0]*otherCore,body.position[2]-body.heading[2]*otherCore,body.position[0]+body.heading[0]*otherCore,body.position[2]+body.heading[2]*otherCore)<v.halfWidth+body.halfWidth+.7f){occupied=true;break;}
    }if(occupied){++occupiedRejected;continue;}
    if(!NeoRespawnContinuation(collision,v,surface,direction,road,station,offset)){++continuationRejected;continue;}
    // A native rejection or a failed post-placement continuation must not
    // select this same requested pose indefinitely on the next recovery.
    state.respawnHistory.Record({surface.x,surface.y,surface.z},state.station);
    // Native tyre probes need to begin above the requested road surface,
    // especially on a sloping deck. The measured final pose must still
    // match the independently checked destination below.
    float position[]={surface.x,surface.y+2.f,surface.z},heading[]={direction.x,direction.y,direction.z};
    ++nativeAttempts;bool placed=event_neo_driver::MWELNeoPlaceVehicle(ai,position,heading);
    auto vehicle=*reinterpret_cast<BYTE**>(primary+0x48);NeoBodyMotion actual{};bool read=NeoReadBody(vehicle,actual);
    // A failed native ground query may move the body but leave its native
    // surface invalid. Require BOTH native support and measured geometry.
    bool verified=placed&&read&&mwneo::Distance({actual.position[0],actual.position[1],actual.position[2]},{surface.x,surface.y,surface.z})<3&&
       mwneo::DotXZ(direction,{actual.heading[0],actual.heading[1],actual.heading[2]})>.7f;
    if(verified){
     Vec3 actualPose{actual.position[0],actual.position[1],actual.position[2]};mwneo::Point actualHeading{actual.heading[0],actual.heading[1],actual.heading[2]};
     float actualOffset=mwneo::CrossXZ(direction,{actualPose.x-center.x,0,actualPose.z-center.z});
     verified=NeoRespawnPoseClear(collision,v,actualPose,actualHeading)&&NeoRespawnContinuation(collision,v,actualPose,actualHeading,road,station,actualOffset);
    }
    bool progressUnchanged=*reinterpret_cast<int*>(info+0x2C)==leg&&*reinterpret_cast<unsigned*>(info+0x28)==lap;
    if(verified&&progressUnchanged){
     state.verifiedPlacementAt=GetTickCount64();state.verifiedPlacementPosition={actual.position[0],actual.position[1],actual.position[2]};state.verifiedPlacementHeading={actual.heading[0],actual.heading[1],actual.heading[2]};state.lastGuidanceStatus=0;
     if(failedBranch){
      DiscardPlan(i,state);state.mainFallbackLegs.insert(leg);state.roads[leg]=std::move(fallbackRoad);
      state.station=station;++state.routeRevision;state.respawnHistory={};state.staticBranchFailures.clear();
      neoRejectedBranches.insert({leg,failedBranch});
      Log("MW_NEOAI_BRANCH_RECOVERY_COMMIT key=%u CP=%d branch=%u old_station=%.3f main_station=%.3f behind_departure_m=%.3f actual_pose_verified=1 native_CP_lap_unchanged=1 race_local_branch_exclusion=1",v.handle,leg,failedBranch,oldStation,station,cursor-station);
     }
     if(approaching)state.recoveryAnchor={}; // launch.Cursor owns prefix coordinates until the grid
     else state.recoveryAnchor.Arm(station);
    }
    Log("MW_NEOAI_RESPAWN key=%u CP=%d station_before=%.3f placement_station=%.3f behind_m=%.3f native_return=%u actual_verified=%u physical_CP_unchanged=%u chosen_branch_retained=%u continuation_checked=1 position_before=%.3f,%.3f,%.3f position_after=%.3f,%.3f,%.3f approach=%u coordinate_offset=%.3f",
       v.handle,leg,oldStation,station-coordinateOffset,cursor-station,unsigned(placed),unsigned(verified),unsigned(progressUnchanged),unsigned(!failedBranch||!verified||!progressUnchanged),p[0],p[1],p[2],actual.position[0],actual.position[1],actual.position[2],unsigned(approaching),coordinateOffset);
    DiscardPlan(i,state);state.ForgetSupport();state.line={};state.grounded.clear();state.through.clear();state.planned=0;state.escape={};state.reverseCache={};state.forwardCache={};
    // Same-road recovery retains monotonic station. A verified road change
    // adopts its own coordinate system. No timers/ranks/CP writes here.
    if(verified&&progressUnchanged)return 1u;
    // Try a different independently checked pose immediately, with a strict
    // work bound; do not count a failed native placement as recovery.
    if(!progressUnchanged||nativeAttempts>=8)return 2u;
   }
  }
  Log("MW_NEOAI_RESPAWN_NO_POSE key=%u CP=%d committed=%.3f cursor=%.3f candidates=%u ground=%u distance=%u pose=%u history=%u occupied=%u continuation=%u grounded_back_m=220",v.handle,leg,state.station,cursor,candidates,groundRejected,distanceRejected,poseRejected,historyRejected,occupiedRejected,continuationRejected);
  if(nativeAttempts)return 2u; // Requery the changed body before issuing inputs.
 }return 0;
}
#pragma comment(linker,"/EXPORT:MWEL_NeoRecoverV1=_MWEL_NeoRecoverV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoRecoverV1(const mwneo::Vehicle* v,void* ai){
 if(!v||!ai||v->size!=sizeof(*v)||v->version!=mwneo::AbiVersion||!mwneo::Finite(v->position)||!mwneo::Finite(v->heading))return 0;
 __try{return NeoRespawn(*v,ai);}__except(EXCEPTION_EXECUTE_HANDLER){Log("MW_NEOAI_RESPAWN_REFUSED fault=%08X",GetExceptionCode());return 0;}
}
