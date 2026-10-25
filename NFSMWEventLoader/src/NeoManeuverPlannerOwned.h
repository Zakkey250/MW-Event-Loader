#pragma once
#include "../../World2MW/src/NativeLoader/YellowRacingLine.h"
#include "../../World2MW/src/NativeLoader/NeoCorridorPolicy.h"
// Event Loader-only query-local optimization. Never retain clearance across
// queries/frames: the ordinary live reuse sweep below remains unchanged.
struct NeoManeuverPrefixPoint {bool clear=false;Vec3 endpoint{};mwneo::Point heading{};};
struct NeoManeuverPrefix {std::array<NeoManeuverPrefixPoint,9> points{};};
struct NeoManeuver {bool clear=false;float steering=0,distance=0,score=1e30f,forwardSteering=0,forwardDistance=0;};
struct NeoEscapeCommit {
 bool active=false;mwneo::Point origin{},forwardOrigin{};float backSteering=0,backDistance=0,forwardSteering=0,forwardDistance=0;
 mwneo::Recovery previous=mwneo::Recovery::Normal;
 bool Expired(mwneo::Recovery current)const {
  return active&&((current==mwneo::Recovery::Normal&&previous!=mwneo::Recovery::Normal)||
    (current==mwneo::Recovery::DetectingStuck&&previous>=mwneo::Recovery::Realign));
 }
};
struct NeoManeuverDiagnostic {unsigned reason=0;Vec3 position{};float part=0,side=0;YellowWallDiagnostic wall{};};
struct NeoManeuverCache {NeoManeuver result{};mwneo::Vehicle pose{};unsigned long long at=0;bool initialized=false,turnForTraffic=false;unsigned hits=0,searches=0;};
// Low-speed, short-horizon manoeuvres only. The complete front/centre/rear
// footprint is swept against walls, ground and traffic; no position writes.
static bool NeoManeuverSweep(const CollisionWorld& world,const mwneo::Vehicle& v,const mwneo::Profile& profile,float desired,bool reverse,float length,
 const YellowTrafficPoint* traffic,unsigned count,Vec3& endpoint,mwneo::Point& finalHeading,NeoManeuverDiagnostic* diagnostic=nullptr,NeoManeuverPrefix* prefix=nullptr){
 if(diagnostic)*diagnostic={};auto fail=[&](unsigned reason,Vec3 p,float part=0,float side=0){if(diagnostic)*diagnostic={reason,p,part,side};return false;};
 auto h=mwneo::UnitXZ(v.heading);Vec3 position{v.position.x,v.position.y,v.position.z};float yaw=std::atan2(h.z,h.x),steering=std::abs(v.speed)<.5f?desired:v.steering;
 // Ground support follows the tyre tracks, not the overhanging bumper.
 // Exact body corners below still check barriers at the full body width.
 const float width=std::clamp(v.halfWidth*.85f,.6f,1.4f);
 const unsigned steps=unsigned(std::ceil(length/.5f));const float advance=length/steps;
 if(prefix)*prefix={};
 const bool recordPrefix=prefix&&steps<=8&&advance==.5f;
 for(unsigned i=0;i<steps;++i){
  float signedAdvance=reverse?-advance:advance;
  float nextSteering=mwneo::Approach(steering,desired,advance/2.5f*2.6f);
  float curve=std::tan((steering+nextSteering)*.5f*profile.steeringLock)/profile.wheelbase;
  if(v.steeringGain!=1.f)curve*=std::clamp(v.steeringGain,.55f,1.4f);
  float nextYaw=yaw+curve*signedAdvance,mid=yaw+curve*signedAdvance*.5f;
  Vec3 next{position.x+std::cos(mid)*signedAdvance,position.y,position.z+std::sin(mid)*signedAdvance},ground;
  if(!YellowAimGround(world,next,next.y,.75f,ground))return fail(1,next);next=ground;
  if(!YellowLineSupported(world,position,next,width,.75f,true))return fail(2,next);
  for(float part:{-v.halfLength,0.f,v.halfLength})for(float side:{-v.halfWidth,0.f,v.halfWidth}){
   Vec3 a{position.x+std::cos(yaw)*part-std::sin(yaw)*side,position.y,position.z+std::sin(yaw)*part+std::cos(yaw)*side};
   Vec3 b{next.x+std::cos(nextYaw)*part-std::sin(nextYaw)*side,next.y,next.z+std::sin(nextYaw)*part+std::cos(nextYaw)*side};
   YellowWallDiagnostic wall;
   if(!YellowWallCorridor(world,a,b,.05f,true,.025f,&position,&wall)){fail(3,b,part,side);if(diagnostic)diagnostic->wall=wall;return false;}
   float from[]={a.x,a.y,a.z},to[]={b.x,b.y,b.z};
   if(!YellowTrafficCorridor(from,to,traffic,count,.05f))return fail(4,b,part,side);
  }
  position=next;yaw=nextYaw;steering=nextSteering;
  if(recordPrefix)prefix->points[i+1]={true,position,{std::cos(yaw),0,std::sin(yaw)}};
 }
 endpoint=position;finalHeading={std::cos(yaw),0,std::sin(yaw)};return true;
}
static float NeoManeuverLateral(const std::vector<YellowRecoveryPoint>& road,float station,Vec3 end){
 float lateral=1e30f;
 for(unsigned i=1;i<road.size();++i){const auto& a=road[i-1];const auto& b=road[i];if(b.station<station-16||a.station>station+20)continue;
  float dx=b.p[0]-a.p[0],dz=b.p[2]-a.p[2],square=dx*dx+dz*dz;if(square<.001f)continue;
  float t=std::clamp(((end.x-a.p[0])*dx+(end.z-a.p[2])*dz)/square,0.f,1.f);
  if(fabs(end.y-a.p[1]-t*(b.p[1]-a.p[1]))>4)continue;
  lateral=std::min(lateral,mwel_math::Hypot(end.x-a.p[0]-t*dx,end.z-a.p[2]-t*dz));
 }return lateral;
}
static NeoManeuver NeoChooseManeuver(const CollisionWorld& world,const mwneo::Vehicle& v,const mwneo::Profile& profile,const std::vector<YellowRecoveryPoint>& road,float station,
 mwneo::Point direction,bool reverse,const YellowTrafficPoint* traffic,unsigned count,bool turnForTraffic=false){
 struct Candidate {NeoManeuver maneuver{};Vec3 endpoint{};mwneo::Point heading{};};
 NeoManeuver best;direction=mwneo::UnitXZ(direction);std::array<Candidate,21> candidates{};unsigned candidateCount=0;
 for(float steering:{0.f,-.1f,.1f,-.45f,.45f,-.75f,.75f}){
  if(reverse&&turnForTraffic&&fabs(steering)<.01f)continue;
  NeoManeuverPrefix prefix;Vec3 ignored;mwneo::Point ignoredHeading;
  // All three original lengths use exactly 0.5m steps. A failed long sweep
  // keeps already fully checked shorter endpoints, without approving the
  // failed step or anything beyond it. Ground/body/wall/traffic tests above
  // are byte-for-byte the original conditions, in the original order.
  NeoManeuverSweep(world,v,profile,steering,reverse,4.f,traffic,count,ignored,ignoredHeading,nullptr,&prefix);
  for(float length:{4.f,2.5f,1.f}){
  auto mark=prefix.points[unsigned(length/.5f)];if(!mark.clear)continue;
  Vec3 end=mark.endpoint;mwneo::Point finalHeading=mark.heading;
  // Keep recovery on the committed road and near the current CP station.
  float lateral=NeoManeuverLateral(road,station,end);
  if(lateral>NeoRouteTolerance)continue;
  float alignment=mwneo::DotXZ(finalHeading,direction);
  // Prefer changing the car's heading over repeating the same back-and-forth.
  float score=12*(1-alignment)+.2f*lateral+.15f*fabs(steering)+.4f*fabs(steering-v.steering)-.05f*length;
  if(reverse&&turnForTraffic){float preferred=(v.handle&1)?1.f:-1.f;if(steering*preferred<0)score+=1.f;}
  candidates[candidateCount++]={{true,steering,length,score},end,finalHeading};
  if(score<best.score)best={true,steering,length,score};
 }}
 // A backoff is useful only if it opens a subsequent forward exit. Examine a
 // bounded shortlist while stationary; moving/cache reuse never repeats the
 // sequence search. Both halves sweep the full body and retain this CP road.
 if(reverse&&best.clear&&std::abs(v.speed)<.5f){
  std::stable_sort(candidates.begin(),candidates.begin()+candidateCount,[](const auto& a,const auto& b){return a.maneuver.score<b.maneuver.score;});
  unsigned checked=0;
  for(unsigned i=0;i<candidateCount&&checked<7;++i){auto candidate=candidates[i].maneuver;if(candidate.distance<2.5f)continue;++checked;
   // The same pose/profile/traffic and static geometry query has already
   // swept this exact backoff. Retain its end pose only within this call.
   Vec3 back=candidates[i].endpoint;mwneo::Point heading=candidates[i].heading;
   auto after=v;after.position={back.x,back.y,back.z};after.heading=heading;after.speed=0;after.steering=candidate.steering;
   for(float steering:{0.f,-.1f,.1f,-.45f,.45f,-.75f,.75f}){
    Vec3 end;mwneo::Point finalHeading;float length=candidate.distance+3;
    if(!NeoManeuverSweep(world,after,profile,steering,false,length,traffic,count,end,finalHeading))continue;
    if(mwneo::DotXZ(mwneo::Sub({end.x,end.y,end.z},v.position),direction)<1||mwneo::DotXZ(finalHeading,direction)<.3f||NeoManeuverLateral(road,station,end)>NeoRouteTolerance)continue;
    candidate.forwardSteering=steering;candidate.forwardDistance=length;return candidate;
   }
  }
 }
 if(!best.clear&&reverse&&turnForTraffic)return NeoChooseManeuver(world,v,profile,road,station,direction,reverse,traffic,count,false);
 return best;
}
// Reuse a short-lived choice, never its old clearance. Every successful reuse
// sweeps the complete body at the current pose before authorizing movement.
static NeoManeuver NeoCachedManeuver(NeoManeuverCache& cache,unsigned long long now,const CollisionWorld& world,const mwneo::Vehicle& v,const mwneo::Profile& profile,
 const std::vector<YellowRecoveryPoint>& road,float station,mwneo::Point direction,bool reverse,bool turnForTraffic=false,const YellowTrafficPoint* traffic=nullptr,unsigned count=0){
 if(cache.initialized&&now>=cache.at&&now-cache.at<200&&cache.turnForTraffic==turnForTraffic){
  if(cache.result.clear){Vec3 end;mwneo::Point heading;
   if(NeoManeuverSweep(world,v,profile,cache.result.steering,reverse,cache.result.distance,traffic,count,end,heading)&&NeoManeuverLateral(road,station,end)<=NeoRouteTolerance){++cache.hits;return cache.result;}
  }else if(!count&&mwneo::Distance(v.position,cache.pose.position)<.01f&&mwneo::Distance(v.heading,cache.pose.heading)<.001f&&fabs(v.speed-cache.pose.speed)<.1f&&fabs(v.steering-cache.pose.steering)<.005f){
   ++cache.hits;return cache.result; // at most 200 ms before another search.
  }
 }
 ++cache.searches;cache.result=NeoChooseManeuver(world,v,profile,road,station,direction,reverse,traffic,count,turnForTraffic);
 cache.at=now;cache.pose=v;cache.turnForTraffic=turnForTraffic;cache.initialized=true;return cache.result;
}
