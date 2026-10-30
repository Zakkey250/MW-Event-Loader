#pragma once
#include <limits>
#include "DriftCurvePolicy.h"
#include "DriftSpeedPolicy.h"
#include "DriftAIAPI.h"
#include "NeoNativeTurnRecovery.h"
#include "../../MW-Arcade-Drift-public/src/NativeDriftSteering.h"
inline bool NeoDriftSteeringFeedback(bool identityValid,unsigned reason){
 // Pausing the yaw/velocity writer does not invalidate the current native
 // wheel input's reference frame. Unsafe or lost identities still fail closed.
 return identityValid&&(reason==mcdai::Applied||reason==mcdai::Grip||reason==mcdai::Paused||reason==mcdai::StaleCommand);
}
inline mwneo::Vehicle NeoDriftPlanningVehicle(const mwneo::Vehicle& v,bool active,bool entryEnvelope=false){
 auto out=v;if(!active||v.speed<=12||v.recovery!=mwneo::Recovery::Normal)return out;
 const auto facing=mwneo::UnitXZ(v.heading),travel=mwneo::UnitXZ(v.velocity);
 if(mwneo::LengthXZ(travel)<.9f)return out;
 const float cosine=std::abs(mwneo::DotXZ(facing,travel)),sine=std::abs(mwneo::CrossXZ(facing,travel));
 // Conservative projection of the real rotated chassis into travel axes.
 // Only the planner's reference changes; the actual body pose is untouched.
 out.heading=travel;out.halfWidth=v.halfWidth*cosine+v.halfLength*sine;
 out.halfLength=v.halfLength*cosine+v.halfWidth*sine;
 if(entryEnvelope){
  // Reserve clearance BEFORE the chassis rotates. Growing the footprint
  // only after entry can leave an apex path with no room for the rear swing.
  const float widthAngle=std::min(.65f,std::atan2(v.halfLength,v.halfWidth));
  const float lengthAngle=std::min(.65f,std::atan2(v.halfWidth,v.halfLength));
  out.halfWidth=std::max(out.halfWidth,v.halfWidth*std::cos(widthAngle)+v.halfLength*std::sin(widthAngle));
  out.halfLength=std::max(out.halfLength,v.halfLength*std::cos(lengthAngle)+v.halfWidth*std::sin(lengthAngle));
 }
 return out;
}
// Input planning for an explicitly selected event racer. This does not change
// the accepted Arcade/Special correction formulas or the player's profiles.
inline bool NeoDriftTrafficHazard(const mwneo::Vehicle& v,const mwneo::Guidance& g){
 if(g.leadVehicle!=mwneo::RaceTrafficPush)return g.obstacleDistance<std::max(15.f,v.speed*.7f);
 if(!std::isfinite(g.leadSpeed)||!std::isfinite(g.obstacleDistance))return true;
 // Same-pace tandem and side-by-side racers do not veto drift. Only a
 // stopped body or a rapidly closing collision needs the grip emergency.
 const float closing=std::max(0.f,v.speed-g.leadSpeed);
 const float reserve=2*v.halfLength+1;
 return (g.leadSpeed<2||closing>6)&&g.obstacleDistance<reserve+std::max(4.f,closing*.65f);
}
inline bool NeoDriftGripHandoff(bool travelPlanning,bool intentActive,bool steeringStateValid,unsigned rejection,
 const mwneo::Vehicle& v,float beta,const mwneo::Profile& profile,mwneo::Controls& controls){
 // The grip driver above also used the travel-axis planning proxy. Its
 // steering still needs conversion back to the real chassis after intent
 // is vetoed. Dropping that conversion drops countersteer during a slide.
 if(!travelPlanning||intentActive||!steeringStateValid||rejection==16||rejection==18||!controls.valid||controls.reverse||
    controls.recovery!=mwneo::Recovery::Normal||v.recovery!=mwneo::Recovery::Normal||v.speed<=12||
    !std::isfinite(beta)||std::abs(beta)>=.8f||!std::isfinite(profile.steeringLock)||profile.steeringLock<=.1f)return false;
 controls.steering=std::clamp(controls.steering+beta/profile.steeringLock,-1.f,1.f);
 return true;
}
// Native power-on re-entry is distinct from kicking the rear loose with
// another handbrake pulse. Require a fully clear, aligned current turn and
// keep both the native yaw ceiling and the existing tyre/speed envelope.
inline bool NeoDriftPowerEntry(unsigned mode,const mwneo::Vehicle& v,const mwneo::Guidance& g,
 float curve,float beta,float cornerSpeed,bool physicalSafe,float clearSeconds,float lineError){
 if(mode!=2||!physicalSafe||clearSeconds!=.8f||
    !std::isfinite(curve)||std::abs(curve)<=mwel_drift_curve::Minimum||std::abs(curve)>=.09f||
    !std::isfinite(beta)||std::abs(beta)>=.35f||!std::isfinite(cornerSpeed)||
    !std::isfinite(v.speed)||v.speed<=12||v.speed>cornerSpeed+.5f||
    !std::isfinite(lineError)||lineError<0||lineError>=1||g.status!=1||v.recovery!=mwneo::Recovery::Normal||
    !mwneo::Finite(v.position)||!mwneo::Finite(v.velocity)||!mwneo::Finite(g.forward))return false;
 const auto travel=mwneo::UnitXZ(v.velocity),forward=mwneo::UnitXZ(g.forward),aim=mwneo::Sub(g.aim,v.position);
 const float square=mwneo::DotXZ(aim,aim);
 if(!mwneo::Finite(aim)||square<=4||mwneo::LengthXZ(travel)<.9f||mwneo::DotXZ(travel,forward)<=.95f)return false;
 const float arc=2*mwneo::CrossXZ(travel,aim)/square;
 return arc*curve>0&&std::abs(arc)>mwel_drift_curve::Minimum&&std::abs(arc)*v.speed<=mcd::NativeDriftTurnLimit;
}
// Reacquire an ALREADY existing native slide after a temporary planner veto.
// This permits ordinary power/steering only; never another entry HB pulse.
inline bool NeoDriftNativeContinuation(unsigned mode,const mwneo::Vehicle& v,const mwneo::Guidance& g,
 bool physicalSafe,float clearSeconds,float beta,float curve,float bodyYaw,float roadSpeed,float lineError,float room){
 if(mode!=2||!physicalSafe||clearSeconds!=.8f||g.status!=1||v.recovery!=mwneo::Recovery::Normal||
    !std::isfinite(beta)||std::abs(beta)<=.15f||std::abs(beta)>=.5f||
    !std::isfinite(curve)||std::abs(curve)<=mwel_drift_curve::Minimum||std::abs(curve)>=.09f||curve*beta>=0||
    !std::isfinite(bodyYaw)||std::abs(bodyYaw)>=mcd::NativeDriftTurnLimit||
    !std::isfinite(roadSpeed)||roadSpeed<12||!std::isfinite(v.speed)||v.speed<=12||v.speed>roadSpeed+1.5f||
    !std::isfinite(lineError)||lineError<0||!std::isfinite(room)||room<=0||lineError>=room||
    !mwneo::Finite(v.velocity)||!mwneo::Finite(g.forward))return false;
 return mwneo::DotXZ(mwneo::UnitXZ(v.velocity),mwneo::UnitXZ(g.forward))>.95f;
}
struct NeoDriftIntent {
 NeoNativeTurnRecovery nativeRecovery;
 float steering=0,entry=0,settle=0,clear=0,roadCurve=0,targetSlip=0,pathYaw=0,attemptElapsed=0,oppositeAge=0,entrySpeed=0,unsafeAge=0,resumeGrace=0;
 float pathStrength=1.f,retryLineError=0,retryRoom=0;bool retryAligned=false,powerEntry=false;
 float previousPhysicalSlip=0,physicalSlipRate=0,physicalSlipIntegral=0,physicalSteerTrim=0;
 mwneo::Point previousTravel{};bool travelReady=false;float measuredTravelRate=0;
 float nativeSlideRelease=0;bool nativeSlide=false;
 float exitSlip=0,exitSide=0,exitSettled=0;bool nativeExiting=false;
 bool turning=false,seenSlide=false,awaitingRetry=false,nativeExitResumed=false,nativeRetryResumed=false;unsigned rejection=0,attempts=0,sectionEdge=UINT32_MAX,sectionLap=UINT32_MAX;
 int bendSide=0;unsigned powerEntries=0;
 void Reset(){*this={};}
 void StopAttempt(){
  awaitingRetry=attempts!=0;turning=seenSlide=false;
  steering=entry=settle=targetSlip=pathYaw=attemptElapsed=entrySpeed=unsafeAge=resumeGrace=0;
  previousPhysicalSlip=physicalSlipRate=physicalSlipIntegral=physicalSteerTrim=0;
  previousTravel={};travelReady=false;measuredTravelRate=0;
  nativeSlideRelease=0;nativeSlide=false;
  exitSlip=exitSide=exitSettled=0;nativeExiting=false;
 }
 bool Apply(unsigned mode,const mwneo::Vehicle& v,const mwneo::Guidance& g,
            const mwneo::Profile& profile,bool calibrated,bool sliding,float beta,
            mwneo::Controls& c,float& handbrake,float courseCurve=std::numeric_limits<float>::quiet_NaN(),bool physicalSafe=true,float upcomingPeak=0,bool entryPrepared=true,float clearSeconds=-1,float bodyYaw=std::numeric_limits<float>::quiet_NaN(),float roadSpeed=std::numeric_limits<float>::quiet_NaN()){
  handbrake=0;powerEntry=false;nativeExitResumed=false;nativeRetryResumed=false;
  const float dt=std::clamp(v.dt,.001f,.1f);
  // A single beta threshold made normal oscillation through 0.15 radians
  // cancel the attempt and enter retry wait. Native tyre sliding has a
  // separate release threshold and dwell; safety vetoes below still reset it.
  if(mode==2&&std::isfinite(beta)){
   if(std::abs(beta)>.15f){nativeSlide=true;nativeSlideRelease=0;}
   else if(nativeSlide){nativeSlideRelease=std::abs(beta)<.08f?nativeSlideRelease+dt:0;
    if(nativeSlideRelease>=.3f){nativeSlide=false;nativeSlideRelease=0;}}
   sliding=nativeSlide;
   // Release the entry handbrake when the native tyres have actually begun
   // sliding. Holding the rest of the timed pulse locks the rear wheels while
   // the driver is already trying to power out of the entry. Hysteresis above
   // keeps threshold noise from creating another entry pulse.
   if(nativeSlide&&std::abs(beta)>.3f)entry=0;
  }
  const float actualWidth=NeoDriftPlanningVehicle(v,true).halfWidth;
  const float corridor=g.count?std::max(0.f,std::min(g.samples[0].widthLeft,g.samples[0].widthRight)-actualWidth-.25f):2.5f;
  float lineError=std::abs(g.lateralError);
  const bool trafficHazard=NeoDriftTrafficHazard(v,g);
  if(g.count>=2&&mwneo::Finite(g.samples[0].position)&&mwneo::Finite(g.samples[1].position)){
   const auto direction=mwneo::UnitXZ(mwneo::Sub(g.samples[1].position,g.samples[0].position));
   if(mwneo::LengthXZ(direction)>.9f)lineError=std::abs(mwneo::CrossXZ(direction,mwneo::Sub(v.position,g.samples[0].position)));
  }
  const bool measuredClearance=mode==2&&std::isfinite(clearSeconds)&&clearSeconds>=0&&clearSeconds<=.8f;
  const unsigned blocked=!mode?1:!calibrated?2:!c.valid?3:c.reverse?4:c.recovery!=mwneo::Recovery::Normal?5:g.status!=1?6:
   !std::isfinite(beta)||std::abs(beta)>.78f?7:v.speed<12||v.speed>120?8:trafficHazard?9:
   (measuredClearance?clearSeconds<.8f:lineError>corridor*.6f)?10:(g.predictiveAvoidance&mwneo::SupportedAirborne)?11:0;
  // A native tyre slide can safely outlive the short hazard that started its
  // exit. The native wheel feedback already supplies countersteer. Do not
  // retain a half-throttle exit merely to settle the old wheel-only state.
  // Resume without another HB pulse or retry-budget reset; every live hazard,
  // current native speed envelope and the full .8-second body sweep still win.
  if(mode==2&&nativeExiting&&!blocked&&physicalSafe&&measuredClearance&&clearSeconds==.8f&&
     std::abs(beta)>.15f&&std::abs(beta)<.5f&&-exitSide*beta>.15f&&
     std::isfinite(bodyYaw)&&std::abs(bodyYaw)<mcd::NativeDriftTurnLimit&&
     std::isfinite(courseCurve)&&std::abs(courseCurve)>mwel_drift_curve::Minimum&&courseCurve*beta<0&&
     std::isfinite(roadSpeed)&&roadSpeed>=12&&v.speed<=roadSpeed+1.5f&&
     mwneo::DotXZ(mwneo::UnitXZ(v.velocity),mwneo::UnitXZ(g.forward))>.95f){
   nativeExiting=false;exitSlip=exitSide=exitSettled=0;
   turning=seenSlide=true;awaitingRetry=false;entry=0;resumeGrace=.25f;nativeExitResumed=true;
  }
  if(mode==2&&physicalSafe&&(blocked==10||(!blocked&&nativeExiting))&&
     (nativeExiting||std::abs(beta)>.12f)&&std::abs(beta)<.78f&&!(g.predictiveAvoidance&mwneo::SupportedAirborne)){
   // Preserve steering continuity while leaving a slide. Switching directly
   // to the grip planner at large beta can request a sudden opposite lock.
   if(!nativeExiting){nativeExiting=true;exitSide=-std::copysign(1.f,beta);exitSlip=std::abs(beta);previousPhysicalSlip=exitSlip;physicalSlipRate=0;exitSettled=0;}
   exitSlip=std::max(0.f,exitSlip-.6f*dt);
   // Preserve the original side through zero. abs(beta) and a newly chosen
   // side reverse the feedback while the chassis is still rotating.
   const float slip=-exitSide*beta,rate=std::clamp((slip-previousPhysicalSlip)/dt,-2.f,2.f);
   physicalSlipRate+=(rate-physicalSlipRate)*dt/(.15f+dt);previousPhysicalSlip=slip;
   const float desired=std::clamp(beta/profile.steeringLock+exitSide*std::clamp((exitSlip-slip)*1.2f-physicalSlipRate*.25f,-.4f,.4f),-1.f,1.f);
   steering=mwneo::Approach(steering,desired,2.5f*dt);c.steering=steering;
   c.throttle=std::min(c.throttle,measuredClearance&&clearSeconds<.3f?0.f:.5f);
   if(measuredClearance&&clearSeconds<.3f)c.brake=std::max(c.brake,.25f);
   exitSettled=std::abs(beta)<.08f&&std::abs(physicalSlipRate)<.2f&&exitSlip==0?exitSettled+dt:0;
   if(exitSettled>=.15f&&!blocked)StopAttempt();
   entry=0;rejection=18;return false;
  }
  if(nativeExiting&&!blocked)StopAttempt();
  if(blocked||!physicalSafe){
   // A brief crest can unload one axle without leaving the checked road.
   // Publish NO drift correction while unsafe, retain only the steering/entry
   // state briefly, then allow the safe controller to re-arm after landing.
   // A long flight, blocked path, obstacle, recovery or excess slip still ends
   // the attempt immediately through the ordinary safety branch below.
   if(!blocked&&!physicalSafe&&turning&&unsafeAge+dt<=.25f){
    unsafeAge+=dt;resumeGrace=.25f;rejection=13;c.steering=steering;
    handbrake=0;return false;
   }
   if(!mode)Reset();else StopAttempt();rejection=blocked?blocked:13;
   if(blocked==10){c.throttle=std::min(c.throttle,.35f);c.brake=std::max(c.brake,.25f);}
   // Yaw/velocity correction stays OFF beyond its slip envelope. Do not then
   // replace the existing countersteer with the grip driver's tiny wheel input:
   // a grounded, forward-moving car can unwind through native tyres alone.
   if(blocked==7&&physicalSafe&&std::abs(beta)<1.4f&&v.speed>12&&v.speed<120&&
      !trafficHazard&&lineError<=corridor&&!(g.predictiveAvoidance&mwneo::SupportedAirborne)){
    steering=std::copysign(1.f,beta);c.steering=steering;c.throttle=0;c.brake=0;rejection=16;
   }
   return false;
  }rejection=0;unsafeAge=0;resumeGrace=std::max(0.f,resumeGrace-dt);
  float sum=0,weight=0;
  for(unsigned i=2;i<g.count&&i<mwneo::MaxSamples;++i){
   if(g.samples[i].distance>std::max(30.f,v.speed*1.1f))break;
   auto a=g.samples[i-2].position,b=g.samples[i-1].position,d=g.samples[i].position;
   const auto ab=mwneo::Sub(b,a),bc=mwneo::Sub(d,b);
   const float k=mwneo::Curve(a,b,d),sign=mwneo::CrossXZ(ab,bc),w=mwneo::LengthXZ(bc);
   sum+=std::copysign(k,sign)*w;weight+=w;
  }
  const float observed=std::isfinite(courseCurve)?courseCurve:weight>0?sum/weight:c.curvature;
  roadCurve+=(observed-roadCurve)*dt/(.2f+dt);
  const float curve=roadCurve;
  bool bend=std::abs(curve)>mwel_drift_curve::Minimum&&std::abs(curve)<.09f;
  bool newSection=sectionEdge!=g.edge||sectionLap!=g.lap;
  sectionEdge=g.edge;sectionLap=g.lap;
  if(bend){
   const int side=curve>0?1:-1;
   if(!bendSide)bendSide=side;
   if(side!=bendSide){
    oppositeAge+=dt;
    if(oppositeAge>=.25f){newSection=true;bendSide=side;oppositeAge=0;previousPhysicalSlip=physicalSlipRate=physicalSlipIntegral=physicalSteerTrim=0;}
   }else oppositeAge=0;
  }else oppositeAge=0;
  if(newSection){
   powerEntries=0;
   // A completed checkpoint or sustained opposite bend is genuine progress.
   // Do not carry an exhausted retry budget into the next route section.
   // A pending safety reset still requires the same continuous safe settle.
   attempts=sliding||awaitingRetry?1u:0u;settle=0;
   if(!sliding){turning=seenSlide=false;entry=attemptElapsed=0;}
  }
  float peak=std::abs(curve);
  if(std::isfinite(upcomingPeak)&&upcomingPeak>0&&upcomingPeak<.09f)peak=std::max(peak,upcomingPeak);
  for(unsigned i=0;i<g.count;++i){
   if(g.samples[i].distance>std::max(60.f,v.speed*3.f))break;
   const auto k=g.samples[i].roadCurvature;if(std::isfinite(k)&&k>=0&&k<.09f)peak=std::max(peak,k);
  }
  if(mode==2){
   // The checked execution arc can be tighter than the road centreline
   // while rejoining an outside line. The CP2 trace requested 1.23 rad/s
   // at 24 m/s but accelerated toward the centreline's 30+ m/s envelope.
   // Bound ordinary pedal demand by the arc actually requested below.
   const auto direction=mwneo::UnitXZ(v.velocity),target=mwneo::Sub(g.aim,v.position);
   const float square=mwneo::DotXZ(target,target);
   if(mwneo::LengthXZ(direction)>.9f&&mwneo::Finite(target)&&square>4)
    peak=std::max(peak,std::abs(2*mwneo::CrossXZ(direction,target)/square));
  }
  float cornerSpeed=peak>mwel_drift_curve::Minimum?mwel_drift_speed::Corner(peak):mwel_drift_speed::Maximum;
  // Anticipate the native steering envelope before entry, not after the car
  // has reached the tightest arc. Slow tight corners can naturally re-grip.
  if(mode==2&&peak>mwel_drift_curve::Minimum)cornerSpeed=std::min(cornerSpeed,std::max(8.f,mcd::NativeDriftTurnLimit/peak));
  if(mode==2&&std::isfinite(roadSpeed)&&roadSpeed>=0&&roadSpeed<=mwel_drift_speed::Maximum){
   // A modest approach allowance avoids the full distance-preview speed
   // jump. The actual apex/checked arc limit still wins, and neither tyre
   // capacity nor native yaw authority is increased.
   cornerSpeed=std::min(roadSpeed,cornerSpeed+5.f);
   const auto direction=mwneo::UnitXZ(v.velocity),target=mwneo::Sub(g.aim,v.position);
   const float square=mwneo::DotXZ(target,target);
   if(mwneo::LengthXZ(direction)>.9f&&mwneo::Finite(target)&&square>4){
    const float local=std::abs(2*mwneo::CrossXZ(direction,target)/square);
    if(local>.001f)cornerSpeed=std::min(cornerSpeed,std::min(mwel_drift_speed::Corner(local),std::max(8.f,mcd::NativeDriftTurnLimit/local)));
   }
  }
  // Reapplying ordinary power through a freshly swept, aligned native turn
  // is not another handbrake kick. A long CP leg may contain several linked
  // slides: do not permanently veto their power-on entry after the first.
  // The two HB attempts, half-second safe rearm and all physical guards stay.
  powerEntry=(!entryPrepared||attempts>=2)&&NeoDriftPowerEntry(mode,v,g,curve,beta,cornerSpeed,physicalSafe,clearSeconds,lineError);
  // Start shedding excess entry speed on the preceding straight. Waiting
  // for bend=true relied on a strong velocity rotation to reach the apex.
  if(!turning&&!sliding&&v.speed>cornerSpeed+2){
   c.targetSpeed=std::min(c.targetSpeed,cornerSpeed);c.throttle=0;
   c.brake=std::clamp((v.speed-cornerSpeed-2)*.06f,0.f,.6f);rejection=12;return false;
  }
  if(bend&&!turning&&!sliding&&!entryPrepared&&!powerEntry){rejection=17;return false;}
  if(turning)attemptElapsed+=dt;
  if(turning&&!sliding&&resumeGrace<=0&&(seenSlide||(attemptElapsed>.5f&&entry<=0))){StopAttempt();}
  if(sliding)seenSlide=true;
  if(!bend){
   clear+=dt;
   if(clear>.35f){attempts=0;awaitingRetry=false;seenSlide=false;bendSide=0;}
  }else clear=0;
  if(awaitingRetry){
   // A physics safety reset ended the previous attempt. Require sustained
   // safe grip and room for the whole target-angle chassis before one retry.
   const float sweptWidth=v.halfWidth*std::cos(.65f)+v.halfLength*std::sin(.65f);
   const float room=g.count?std::min(g.samples[0].widthLeft,g.samples[0].widthRight)-sweptWidth-.4f:2.5f;
   const bool aligned=mwneo::DotXZ(mwneo::UnitXZ(v.velocity),mwneo::UnitXZ(g.forward))>.9f;
   retryLineError=lineError;retryRoom=room;retryAligned=aligned;
   const bool checkedContinuation=NeoDriftNativeContinuation(mode,v,g,physicalSafe,clearSeconds,beta,curve,bodyYaw,roadSpeed,lineError,room);
   const bool safeLine=(lineError<std::min(1.f,room)&&aligned)||checkedContinuation;
   // A native slide may outlive the planner's temporary safety veto.
   // Re-acquire normal wheel/pedal control after half a second of checked
   // clearance, without another handbrake pulse or a new entry attempt.
   const bool continuing=mode==2&&sliding&&std::abs(beta)<.65f&&bend;
   settle=safeLine&&(continuing||(!sliding&&std::abs(beta)<.35f))?settle+dt:0;
   if(!bend||(!continuing&&attempts>=2&&!powerEntry)||settle<(checkedContinuation?.15f:.5f)){rejection=14;return false;}
   nativeRetryResumed=checkedContinuation;
   awaitingRetry=false;seenSlide=continuing;turning=continuing;settle=0;
  }
  if(bend){
   // Keep the accepted drift-angle envelope. Natural momentum must come
   // from native steering and bounded path help, not from hiding the slide
   // by reducing its angle or changing the score formula.
   const float load=std::clamp(v.speed*v.speed*std::abs(curve)/mwel_drift_speed::LateralAcceleration,0.f,1.f);
   const float angle=.62f+.03f*load;
   targetSlip=mwneo::Approach(targetSlip,-std::copysign(angle,curve),.8f*dt);
  }
  else {
   if(clear>.35f){turning=false;entry=0;targetSlip=mwneo::Approach(targetSlip,0,.8f*dt);}
   if(std::abs(targetSlip)<.02f&&std::abs(beta)<.08f)return false;
  }
  // Pure pursuit includes the measured velocity-direction and lateral error;
  // the road's average curvature alone cannot bring a displaced slide back.
  const auto travel=mwneo::UnitXZ(v.velocity),aim=mwneo::Sub(g.aim,v.position);
  const float aimSquare=mwneo::DotXZ(aim,aim);
  if(mwneo::LengthXZ(travel)<.9f||!mwneo::Finite(aim)||aimSquare<4){StopAttempt();rejection=15;return false;}
  // Controls.curvature includes the grip driver's future chassis prediction.
  // The velocity writer must follow the same current execution arc that the
  // provider actually swept, rather than applying that prediction a second time.
  pathYaw=std::clamp(-2*mwneo::CrossXZ(travel,aim)/aimSquare*v.speed,-1.4f,1.4f);
  const float lateralDemand=std::clamp((lineError-.15f)/std::max(.35f,corridor*.35f),0.f,1.f);
  const float turnDemand=std::clamp(v.speed*v.speed*std::abs(curve)/mwel_drift_speed::LateralAcceleration,0.f,1.f);
  pathStrength=.6f+.4f*std::max(lateralDemand,turnDemand);
  if(g.leadVehicle!=mwneo::RaceTrafficPush&&g.obstacleDistance<std::max(40.f,v.speed*1.5f))pathStrength=1.f;
  // Enter through real steering and a short native handbrake pulse. Once
  // traction breaks, countersteer the measured slip; no map-angle body writer.
  if(bend&&calibrated&&!turning&&!sliding){turning=true;attemptElapsed=0;previousPhysicalSlip=physicalSlipRate=physicalSlipIntegral=physicalSteerTrim=0;entrySpeed=std::clamp(v.speed,mwel_drift_speed::Minimum,mwel_drift_speed::EntryMaximum);if(powerEntry){entry=0;++powerEntries;}else {++attempts;entry=mode==1?.12f:.4f;}}
  // Do not instantly straighten the front tyres along the travel vector at
  // the first hint of slip. Progressively add counter as the rear steps out.
  const float counterWeight=std::clamp((std::abs(beta)-.08f)/.42f,.35f,1.f);
  float desired=std::clamp(c.steering+counterWeight*beta/profile.steeringLock,-1.f,1.f);
  if(mode==2&&bend){
   const float side=curve>0?1.f:-1.f,slip=-side*beta;
   const float measured=turning?std::clamp((slip-previousPhysicalSlip)/dt,-2.f,2.f):0;
   physicalSlipRate+=(measured-physicalSlipRate)*dt/(.15f+dt);previousPhysicalSlip=slip;
   if(travelReady){const float rate=-std::atan2(mwneo::CrossXZ(previousTravel,travel),mwneo::DotXZ(previousTravel,travel))/dt;
    measuredTravelRate+=(std::clamp(rate,-2.f,2.f)-measuredTravelRate)*dt/(.2f+dt);}
   else measuredTravelRate=pathYaw;
   previousTravel=travel;travelReady=true;
   // The driver asks for a turn, not a fixed 30-degree chassis pose.
   // The same native wheel controller is available to the human Pad mode.
   const float yaw=std::isfinite(bodyYaw)?bodyYaw:measuredTravelRate-side*physicalSlipRate;
   desired=-mcd::nativeDriftSteering(pathYaw,yaw,beta);
  }
  if(turning&&attemptElapsed<.3f&&std::abs(beta)<.25f)
   desired=std::copysign(std::max(std::abs(c.steering),.35f),curve);
  // Choose the safety request before the single slew step. Applying a
  // second countersteer step after slewing inward cancelled itself each tick.
  if(std::abs(beta)>.72f)desired=std::copysign(1.f,beta);
  steering=mwneo::Approach(steering,desired,4.5f*dt);
  c.steering=steering;
  // The grip planner normally brakes precisely where drift entry is needed.
  // Only physical hazards above may veto entry, not that corner-speed pedal.
  if(entry>0){
   // The AWD CP7/8 traces began another HB kick at beta near zero but
   // actual yaw already above 1rad/s. Small slip during a direction change
   // is not settled grip. Release the rear brake at the existing native
   // turn-authority limit, including mid-pulse; ordinary steering and tyre
   // counterfeedback remain active. A missing yaw sample cannot allow a kick.
   if(mode==2&&(!std::isfinite(bodyYaw)||std::abs(bodyYaw)>=mcd::NativeDriftTurnLimit))entry=0;
   else {entry=std::max(0.f,entry-dt);handbrake=.7f;}
  }
  // Drift intent must break rear traction under power, rather than inherit
  // the grip driver's corner-speed brake. Obstacle/recovery guards above win.
  if(calibrated){
   c.throttle=1;c.brake=0;
   // Entry uses full power. Once the actual chassis is sliding, meter power
   // against the checked corner envelope instead of accelerating out of it.
   if(std::abs(beta)>.4f){
    // A coarse grip-plan chord may be straight inside a real authored bend.
    // Keep the SAME road-speed envelope after entry instead of accelerating
    // beyond the checked bend envelope while the chassis is still sideways.
    // Retain entry momentum only within the next checked bend's envelope.
    // A fast entry must not become a speed floor through a tightening bend.
    const float ceiling=std::min(mwel_drift_speed::Maximum,cornerSpeed+1.5f);
    const float goal=std::clamp(std::max(entrySpeed,cornerSpeed),mwel_drift_speed::Minimum,ceiling);
    c.targetSpeed=goal;c.throttle=std::clamp(.95f+(goal-v.speed)*.06f,.35f,1.f);
   }
   // A tightening next bend can require deceleration during the current
   // slide too. Native braking wins over the ordinary power-on request.
   if(v.speed>cornerSpeed+3){c.throttle=0;c.brake=std::clamp((v.speed-cornerSpeed-3)*.06f,0.f,.6f);}
  }
  if(std::abs(beta)>.72f){
   handbrake=0;entry=0;c.throttle=std::min(c.throttle,.55f);
  }
  return calibrated;
 }
};
