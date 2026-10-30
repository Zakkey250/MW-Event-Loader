#pragma once
#include "NeoDownstepClearance.h"
// Event-owned descent proof. Ordinary steering retains its accepted native-world policy.
struct NeoDownstepArcDiagnostic {unsigned reason=0;float distance=0,deviation=0;Vec3 position{};bool terrainDrop=false,terrainLanded=false;};
static bool NeoDownstepArc(const CollisionWorld& world,const YellowRacingLine& line,float station,const float* position,const float* heading,
 Vec3 goal,float halfWidth,const YellowTrafficPoint* cars,unsigned carCount,float trafficSpeed=0,float sweepLimit=0,NeoDownstepArcDiagnostic* diagnostic=nullptr,float routeTolerance=3.5f,float halfLength=0,float cornerPadding=.025f,bool allowTerrainDrop=false){
 const bool slowDrop=true;
 YellowTerrainDropScope terrainPolicy(allowTerrainDrop&&yellowTraversableSteps&&(std::abs(trafficSpeed)>15.f||slowDrop),slowDrop?8.f:16.f);
 if(diagnostic)*diagnostic={};
 auto fail=[&](unsigned reason,Vec3 at,float distance=0,float deviation=0){if(diagnostic)*diagnostic={reason,distance,deviation,at};return false;};
 Vec3 from{position[0],position[1],position[2]};float hn=mwel_math::Hypot(heading[0],heading[2]);if(hn<.01f)return fail(1,from);
 float hx=heading[0]/hn,hz=heading[2]/hn,dx=goal.x-from.x,dz=goal.z-from.z;
 float forward=dx*hx+dz*hz,side=-dx*hz+dz*hx,square=dx*dx+dz*dz;if(forward<2||square<16)return fail(2,goal);
 float curvature=2*side/square,angle=2*std::atan2(side,forward),length=fabs(curvature)>.00001f?angle/curvature:std::sqrt(square);
 if(length>160||length<2)return fail(3,goal);
 RayHit ground;if(!world.Ray({from.x,from.y+1,from.z},{from.x,from.y-3,from.z},ground,true))return fail(4,from);
 float clearance=std::clamp(from.y-ground.point.y,.15f,1.f),width=std::clamp(halfWidth,.8f,3.5f)+.35f;
 float checked=sweepLimit>0?std::min(length,sweepLimit):length;
 Vec3 prior=from;unsigned steps=unsigned(std::ceil(checked/3));
 // The station window is identical for every sample of this arc. Determine
 // its enclosing index range once. Keep the original filter inside the range
 // so even non-monotonic or duplicate station input retains its semantics.
 unsigned routeBegin=unsigned(line.points.size()),routeEnd=0;
 for(unsigned j=1;j<line.points.size();++j){const auto& a=line.points[j-1];const auto& b=line.points[j];
  if(b.station<station-8||a.station>station+checked*1.7f+12)continue;
  routeBegin=std::min(routeBegin,j);routeEnd=j+1;
 }
 bool airborne=false,hadDrop=false;float departureFloor=ground.point.y,takeoffDistance=0,takeoffHeight=from.y,takeoffSlope=0;
 auto landingAgrees=[&](Vec3 probe,const RayHit& landing){
  float best=1e30f;Vec3 reference{};
  for(unsigned j=1;j<line.points.size();++j){const auto& a=line.points[j-1];const auto& b=line.points[j];if(b.station<station-8||a.station>station+checked*1.7f+12)continue;
   float dx=b.p[0]-a.p[0],dz=b.p[2]-a.p[2],square=dx*dx+dz*dz;if(square<.001f)continue;
   float t=std::clamp(((probe.x-a.p[0])*dx+(probe.z-a.p[2])*dz)/square,0.f,1.f);Vec3 point{a.p[0]+dx*t,a.p[1]+(b.p[1]-a.p[1])*t,a.p[2]+dz*t};
   float distance=std::hypot(point.x-probe.x,point.z-probe.z);if(distance<best){best=distance;reference=point;}
  }
  RayHit declared;
  return best<1e30f&&(!world.Ray({reference.x,reference.y+1,reference.z},{reference.x,reference.y-3,reference.z},declared,true)||std::abs(declared.point.y-landing.point.y)<=4.f);
 };
 for(unsigned i=1;i<=steps;++i){float distance=checked*i/steps,t=distance/length;
  float along=fabs(curvature)>.00001f?std::sin(curvature*distance)/curvature:distance;
  float across=fabs(curvature)>.00001f?(1-std::cos(curvature*distance))/curvature:0;
  Vec3 p{from.x+hx*along-hz*across,from.y+(goal.y-from.y)*t,from.z+hz*along+hx*across},surface;
  bool supported=!airborne&&NeoDownstepAimGround(world,p,yellowTerrainDrops?prior.y:p.y,clearance,surface);
  if(supported&&!hadDrop)departureFloor=surface.y-clearance;
  if(!supported){
   RayHit landing;bool lower=false;
   if(!yellowTerrainDrops||!YellowDropSupport(world,p,-hz,hx,width,departureFloor,landing,lower)||(!airborne&&!lower)||!landingAgrees(p,landing))return fail(5,p,distance);
   if(!airborne){
    const float previousDistance=checked*(i-1)/steps;
    takeoffDistance=previousDistance;takeoffHeight=prior.y;
    takeoffSlope=previousDistance>.1f?std::clamp((prior.y-from.y)/previousDistance,-.8f,.8f):std::clamp(-(ground.normal.x*hx+ground.normal.z*hz)/std::max(.4f,std::abs(ground.normal.y)),-.8f,.8f);
    airborne=hadDrop=true;
   }
   float travelled=distance-takeoffDistance,time=travelled/std::abs(trafficSpeed);
   if(time>3.f)return fail(5,p,distance);
   surface={p.x,takeoffHeight+takeoffSlope*travelled-4.905f*time*time,p.z};
   if(surface.y<=landing.point.y+clearance){surface.y=landing.point.y+clearance;airborne=false;}
  }
  if(!YellowGateEdgeClear(line.passage,prior,surface))return fail(10,surface,distance);
  // Stay near the committed optimized path, including around intersections.
  float nearest=1e30f;
  for(unsigned j=routeBegin;j<routeEnd;++j){const auto& a=line.points[j-1];const auto& b=line.points[j];if(b.station<station-8||a.station>station+checked*1.7f+12)continue;
   float ex=b.p[0]-a.p[0],ez=b.p[2]-a.p[2],den=ex*ex+ez*ez;if(den<.01f)continue;
   float u=std::clamp(((surface.x-a.p[0])*ex+(surface.z-a.p[2])*ez)/den,0.f,1.f);
   if(!hadDrop&&fabs(surface.y-a.p[1]-u*(b.p[1]-a.p[1]))>3)continue;
   nearest=std::min(nearest,mwel_math::Hypot(surface.x-a.p[0]-u*ex,surface.z-a.p[2]-u*ez));
  }
  if(nearest>routeTolerance)return fail(6,surface,distance,nearest);
  unsigned supportFailure=0;bool support=airborne?NeoDownstepWallCorridor(world,prior,surface,width,true):NeoDownstepLineSupported(world,prior,surface,width,clearance,true,&supportFailure);
  if(!support){
   if(airborne)supportFailure=1;
   RayHit landing;bool lower=false;
   const bool dropSupport=yellowTerrainDrops&&supportFailure==2&&YellowDropSupport(world,surface,-hz,hx,width,departureFloor,landing,lower)&&lower&&landingAgrees(surface,landing);
   // A parallel wall brushing the outside of the swept body is steering
   // feedback, not a race-speed stop.  Keep the arc when a narrow centre
   // corridor and its ground support remain open; a pillar or frontal wall
   // still blocks that centre corridor and is rejected normally.
   if(!dropSupport&&(supportFailure!=1||!(airborne?NeoDownstepWallCorridor(world,prior,surface,.2f,true,.025f):NeoDownstepLineSupported(world,prior,surface,.2f,clearance,true,nullptr,.025f))))
    return fail(supportFailure==1?7:8,surface,distance);
  }
  if(halfLength>0){
   float previousDistance=checked*(i-1)/steps,step=distance-previousDistance;
   float beforeYaw=curvature*previousDistance,afterYaw=curvature*distance;
   float beforeX=hx*std::cos(beforeYaw)-hz*std::sin(beforeYaw),beforeZ=hz*std::cos(beforeYaw)+hx*std::sin(beforeYaw);
   float afterX=hx*std::cos(afterYaw)-hz*std::sin(afterYaw),afterZ=hz*std::cos(afterYaw)+hx*std::sin(afterYaw);
   float grade=hadDrop?heading[1]/hn:(step>.01f?(surface.y-prior.y)/step:0);
   // Sweep oriented body corners. Applying the entire car width normal to
   // each end's motion inflates a rotating tail along the car's length.
   // The centre corridor above checks the interior; these point tracks add
   // the nose/tail beyond it without counting body width twice.
   for(float part:{-halfLength,halfLength})for(float side:{-halfWidth,0.f,halfWidth}){
    Vec3 a{prior.x+beforeX*part-beforeZ*side,prior.y+grade*part,prior.z+beforeZ*part+beforeX*side};
    Vec3 b{surface.x+afterX*part-afterZ*side,surface.y+grade*part,surface.z+afterZ*part+afterX*side};
    // Body overhang is not a wheel contact point; road support is checked by
    // the centre corridor. Permit only an already penetrating corner's exit.
    // A clear centre track cannot excuse a blocked nose or tail: it would
    // accept a frontal post just beyond the centre's execution horizon.
    // Contact-aware corner checks already permit parallel scrapes/exits.
    if(!NeoDownstepWallCorridor(world,a,b,.05f,true,cornerPadding,&prior))return fail(7,b,distance);
   }
  }
  float a[]={prior.x,prior.y,prior.z},b[]={surface.x,surface.y,surface.z};
  float arrival=trafficSpeed>0?std::min(2.5f,(distance-checked/steps*.5f)/std::max(4.f,trafficSpeed)):0;
  if(!YellowTrafficCorridor(a,b,cars,carCount,width,arrival))return fail(9,surface,distance);prior=surface;
 }if(slowDrop&&hadDrop&&airborne)return fail(5,prior,checked);
 if(diagnostic){diagnostic->terrainDrop=hadDrop;diagnostic->terrainLanded=hadDrop&&!airborne;}return true;
}
