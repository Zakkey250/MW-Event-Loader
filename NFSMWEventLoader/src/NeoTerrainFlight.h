#pragma once
// Only a complete, supported takeoff/landing on the committed lane may remove
// a lattice's false cul-de-sac. Follow the lane through curved descents rather
// than fitting one giant circle that can cut across a real bend. Native body,
// progress, timers and performance remain untouched.
struct NeoFlightPrediction {Vec3 takeoff{},velocity{},landing{};float time=0,station=0,acceleration=0;bool valid=false;float approachDistance=0,approachSeconds=0,takeoffStation=0;};
struct NeoDepartureKinematics {float speed=0,seconds=0;};
static bool NeoTerrainFlightContinuation(const CollisionWorld& world,const YellowRacingLine& plan,const std::vector<YellowRecoveryPoint>& road,
 float station,float speed,float halfWidth,float halfLength,const YellowTrafficPoint* cars,unsigned count,YellowRacingLine& output,YellowArcDiagnostic* failure=nullptr,float acceleration=9.81f,NeoFlightPrediction* prediction=nullptr,const NeoDepartureKinematics* departureMotion=nullptr){
 if(prediction)*prediction={};
 if(!yellowTraversableSteps||!plan.truncated||plan.points.size()<3||road.size()<2||std::abs(speed)<=15)return false;
 const float stop=plan.points.back().station;
#ifdef MWEL_NEO_TERRAIN_D_EXPERIMENT
 const float begin=std::max(plan.points.front().station,stop-8.f);
#else
 const float begin=std::max({plan.points.front().station,stop-8.f,station});
#endif
 const float end=std::min(road.back().station,begin+std::clamp(std::abs(speed)*3.f+40.f,90.f,380.f));
 if(end<=stop+12)return false;
 YellowTerrainDropScope terrain(true,48.f);
 YellowRacingLine continuation;continuation.passage=plan.passage;continuation.points=plan.points;
 Vec3 previous{},nextPoint{};if(!NeoPreviewPoint(plan,road,begin,previous)||!NeoPreviewPoint(plan,road,begin+2,nextPoint))return false;
 float approachPrefix=0;Vec3 prefix;
 if(!NeoPreviewPoint(plan,road,station,prefix))return false;
 for(float s=std::min(station+2,begin);s<=begin;){Vec3 p;if(!NeoPreviewPoint(plan,road,s,p))return false;approachPrefix+=std::hypot(p.x-prefix.x,p.z-prefix.z);prefix=p;if(s==begin)break;s=std::min(s+2,begin);}
 RayHit departure;if(!world.Ray({previous.x,previous.y+1,previous.z},{previous.x,previous.y-3,previous.z},departure,true))return false;
 const float clearance=std::clamp(previous.y-departure.point.y,.15f,1.f),width=std::clamp(halfWidth,.8f,3.5f)+.35f;
 auto previousForward=mwneo::UnitXZ({nextPoint.x-previous.x,0,nextPoint.z-previous.z});
 float travelled=0,takeoffAt=0,takeoffHeight=0,takeoffSlope=0,landedAt=0,departureFloor=departure.point.y,flightSpeed=std::abs(speed),approachSeconds=0;
 if(departureMotion&&(!std::isfinite(departureMotion->speed)||departureMotion->speed<=15||!std::isfinite(departureMotion->seconds)||departureMotion->seconds<0||departureMotion->seconds>.75f))return false;
 bool airborne=false,hadDrop=false,landed=false;Vec3 takeoffOrigin{},previousAuthored=previous;mwneo::Point takeoffForward{};
 auto refuse=[&](unsigned reason,Vec3 p){if(failure)*failure={reason,travelled,0,p};return false;};
 for(float s=begin+2;;s=std::min(s+2,end)){
  Vec3 authored{},after{};if(!NeoPreviewPoint(plan,road,s,authored)||!NeoPreviewPoint(plan,road,std::min(end,s+2),after))return false;
  auto forward=s<end?mwneo::UnitXZ({after.x-authored.x,0,after.z-authored.z}):previousForward;
  float step=std::hypot(authored.x-previousAuthored.x,authored.z-previousAuthored.z);if(step<.01f)return false;travelled+=step;
  bool justLanded=false;Vec3 surface{};bool supported=!airborne&&YellowAimGround(world,authored,previous.y,clearance,surface);
  if(supported&&!hadDrop){departureFloor=surface.y-clearance;RayHit floor;if(world.Ray({surface.x,surface.y+1,surface.z},{surface.x,surface.y-3,surface.z},floor,true))departure=floor;}
  if(!supported){
   if(landed)return refuse(5,authored);
   RayHit landing;bool lower=false;
   if(!YellowDropSupport(world,authored,-forward.z,forward.x,width,departureFloor,landing,lower)||(!airborne&&!lower))return refuse(5,authored);
   // A supported declared upper deck cannot be substituted by a lower road.
   RayHit declared;if(world.Ray({authored.x,authored.y+1,authored.z},{authored.x,authored.y-3,authored.z},declared,true)&&std::abs(declared.point.y-landing.point.y)>4.f)return refuse(6,authored);
   if(!airborne){takeoffOrigin=previous;takeoffForward=previousForward;airborne=hadDrop=true;takeoffAt=travelled-step;takeoffHeight=previous.y;takeoffSlope=std::clamp(-(departure.normal.x*previousForward.x+departure.normal.z*previousForward.z)/std::max(.4f,std::abs(departure.normal.y)),-.8f,.8f);
    approachSeconds=departureMotion?departureMotion->seconds:(approachPrefix+takeoffAt)/std::abs(speed);if(departureMotion)flightSpeed=departureMotion->speed;
    if(prediction){prediction->takeoff=takeoffOrigin;prediction->approachDistance=approachPrefix+takeoffAt;prediction->approachSeconds=approachSeconds;prediction->takeoffStation=s-step;prediction->velocity={takeoffForward.x*flightSpeed,takeoffSlope*flightSpeed,takeoffForward.z*flightSpeed};}
   }
   float distance=travelled-takeoffAt,time=distance/flightSpeed;if(time>3.f)return refuse(5,authored);
   surface={takeoffOrigin.x+takeoffForward.x*distance,takeoffHeight+takeoffSlope*distance-(.5f*acceleration)*time*time,takeoffOrigin.z+takeoffForward.z*distance};
   // Airborne velocity cannot steer along a curved authored line. Reject
   // that shortcut unless the actual straight flight remains over this road.
   if(std::hypot(surface.x-authored.x,surface.z-authored.z)>12||!YellowDropSupport(world,surface,-takeoffForward.z,takeoffForward.x,width,departureFloor,landing,lower))return refuse(6,surface);
   if(world.Ray({authored.x,authored.y+1,authored.z},{authored.x,authored.y-3,authored.z},declared,true)&&std::abs(declared.point.y-landing.point.y)>4.f)return refuse(6,surface);
   forward=takeoffForward;
   if(surface.y<=landing.point.y+clearance){surface.y=landing.point.y+clearance;airborne=false;landed=true;justLanded=true;landedAt=travelled;
    if(prediction){prediction->landing=surface;prediction->time=time;prediction->station=s;prediction->acceleration=acceleration;prediction->valid=true;}}
  }
  if(!YellowGateEdgeClear(plan.passage,previous,surface))return refuse(10,surface);
  if(!YellowWallCorridor(world,previous,surface,width,true))return refuse(7,surface);
  if(!airborne&&!justLanded&&!YellowLineSupported(world,previous,surface,width,clearance,true))return refuse(8,surface);
  if(!YellowEditorWallCorridor(previous,surface,width))return refuse(7,surface);
  float grade=airborne?takeoffSlope:(surface.y-previous.y)/step;
  for(float part:{-halfLength,halfLength})for(float side:{-halfWidth,0.f,halfWidth}){
   Vec3 a{previous.x+previousForward.x*part-previousForward.z*side,previous.y+grade*part,previous.z+previousForward.z*part+previousForward.x*side};
   Vec3 b{surface.x+forward.x*part-forward.z*side,surface.y+grade*part,surface.z+forward.z*part+forward.x*side};
   if(!YellowWallCorridor(world,a,b,.05f,true,.025f,&previous))return refuse(7,b);
  }
  float a[]={previous.x,previous.y,previous.z},b[]={surface.x,surface.y,surface.z};
#ifdef MWEL_NEO_TERRAIN_D_EXPERIMENT
  float arrival=std::min(2.5f,(std::max(0.f,begin-station)+travelled)/std::abs(speed));
#else
  float arrival=std::min(2.5f,hadDrop?approachSeconds+std::max(0.f,travelled-takeoffAt)/flightSpeed:(approachPrefix+travelled)/std::abs(speed));
#endif
  if(!YellowTrafficCorridor(a,b,cars,count,width,arrival))return refuse(9,surface);
  if(s>stop)continuation.points.push_back({{surface.x,surface.y,surface.z},s});
  previous=surface;previousAuthored=authored;previousForward=forward;
  if(s>=end)break;
 }
 if(!hadDrop||!landed||airborne||travelled-landedAt<12)return false;
 if(failure){*failure={};failure->terrainDrop=failure->terrainLanded=true;}
 continuation.truncated=false;output=std::move(continuation);return true;
}

// Once the body is airborne, use its measured velocity vector, not a road
// tangent or an average grade. This is a read-only ballistic safety rollout.
static bool NeoMeasuredFlightRange(const CollisionWorld& world,const std::vector<YellowRecoveryPoint>& road,const mwneo::Vehicle& v,
 float station,const YellowTrafficPoint* cars,unsigned count,Vec3& aim,float acceleration){
 const float horizontal=std::hypot(v.velocity.x,v.velocity.z);if(horizontal<=15||road.size()<2)return false;
 YellowTerrainDropScope terrain(true,48.f);
 Vec3 previous{v.position.x,v.position.y,v.position.z};
 const float hx=v.heading.x,hz=v.heading.z,width=std::clamp(v.halfWidth,.8f,3.5f)+.35f;
 for(unsigned i=1;i<=60;++i){float time=i*.05f;Vec3 p{v.position.x+v.velocity.x*time,v.position.y+v.velocity.y*time-(.5f*acceleration)*time*time,v.position.z+v.velocity.z*time};
  RayHit floor;if(!world.Ray({p.x,std::max(p.y,previous.y)+1,p.z},{p.x,p.y-48,p.z},floor,true)||std::abs(floor.normal.y)<.4f)return false;
  float nearest=1e30f;Vec3 declaredPoint{};
  for(unsigned j=1;j<road.size();++j){const auto& a=road[j-1];const auto& b=road[j];if(b.station<station-8||a.station>station+horizontal*3+24)continue;
   float dx=b.p[0]-a.p[0],dz=b.p[2]-a.p[2],n=dx*dx+dz*dz;if(n<.001f)continue;
   float t=std::clamp(((p.x-a.p[0])*dx+(p.z-a.p[2])*dz)/n,0.f,1.f);Vec3 q{a.p[0]+dx*t,a.p[1]+(b.p[1]-a.p[1])*t,a.p[2]+dz*t};
   float lateral=std::hypot(q.x-p.x,q.z-p.z);if(lateral<nearest){nearest=lateral;declaredPoint=q;}
  }
  if(nearest>12)return false;
  RayHit declared;if(world.Ray({declaredPoint.x,declaredPoint.y+1,declaredPoint.z},{declaredPoint.x,declaredPoint.y-3,declaredPoint.z},declared,true)&&std::abs(declared.point.y-floor.point.y)>4)return false;
  bool landed=p.y<=floor.point.y+.75f;if(landed)p.y=floor.point.y+.75f;
  if(!YellowWallCorridor(world,previous,p,width,true)||!YellowEditorWallCorridor(previous,p,width))return false;
  for(float part:{-v.halfLength,v.halfLength})for(float side:{-v.halfWidth,0.f,v.halfWidth}){
   Vec3 a{previous.x+hx*part-hz*side,previous.y,previous.z+hz*part+hx*side},b{p.x+hx*part-hz*side,p.y,p.z+hz*part+hx*side};
   if(!YellowWallCorridor(world,a,b,.05f,true,.025f,&previous))return false;
  }
  float a[]={previous.x,previous.y,previous.z},b[]={p.x,p.y,p.z};if(!YellowTrafficCorridor(a,b,cars,count,width,std::min(2.5f,time)))return false;
  if(landed){
   Vec3 next{p.x+v.velocity.x/horizontal*12,p.y,p.z+v.velocity.z/horizontal*12},surface;
   if(!YellowAimGround(world,next,p.y,.75f,surface)||!YellowLineSupported(world,p,surface,width,.75f,true))return false;
   aim=surface;return true;
  }
  previous=p;
 }return false;
}

// Conservative occupied volume between the two acceleration bounds. A thin
// obstacle between the outer trajectories must not be missed. Wall triangle
// AABBs deliberately overestimate the volume; uncertain geometry keeps PlanEnd.
static bool NeoFlightEnvelopeClear(const CollisionWorld& world,const YellowRacingLine& slow,const YellowRacingLine& fast,
 float begin,float speed,float halfWidth,float halfLength,const YellowTrafficPoint* cars,unsigned count,YellowArcDiagnostic* failure=nullptr,float halfHeight=1.f,float pitchLow=0,float pitchHigh=0){
 if(!std::isfinite(halfHeight)||halfHeight<=0||!std::isfinite(pitchLow)||!std::isfinite(pitchHigh)||pitchLow>pitchHigh)return false;
 float verticalReserve=1.2f,horizontalLength=halfLength;
 const float angle=std::atan2(halfLength,halfHeight),other=std::atan2(halfHeight,halfLength);
 for(float pitch:{pitchLow,pitchHigh,-angle,angle,-other,other,0.f})if(pitch>=pitchLow&&pitch<=pitchHigh){verticalReserve=std::max(verticalReserve,std::abs(std::sin(pitch))*halfLength+std::abs(std::cos(pitch))*halfHeight);horizontalLength=std::max(horizontalLength,std::abs(std::cos(pitch))*halfLength+std::abs(std::sin(pitch))*halfHeight);}
 if(slow.points.size()<2||slow.points.size()!=fast.points.size())return false;
 YellowTerrainDropScope terrain(true,48.f);std::vector<unsigned> candidates;
 const float reserve=halfWidth+halfLength+.2f;
 for(unsigned i=1;i<slow.points.size();++i){
  if(slow.points[i].station<begin)continue;
  auto a=slow.points[i-1].p,b=slow.points[i].p,c=fast.points[i-1].p,d=fast.points[i].p;
  float difference=0;for(unsigned axis=0;axis<3;++axis)difference+=std::abs(a[axis]-c[axis])+std::abs(b[axis]-d[axis]);
  // Coincident bounds were already checked by both complete body sweeps.
  if(difference<.0001f)continue;
  Vec3 from{(a[0]+c[0])*.5f,(a[1]+c[1])*.5f,(a[2]+c[2])*.5f},to{(b[0]+d[0])*.5f,(b[1]+d[1])*.5f,(b[2]+d[2])*.5f};
  float dx=to.x-from.x,dz=to.z-from.z,n=std::hypot(dx,dz);if(n<.01f)return false;
  float hx=dx/n,hz=dz/n;
  auto local=[&](Vec3 p){return Vec3{-hz*p.x+hx*p.z,p.y,hx*p.x+hz*p.z};};
  Vec3 localLow{1e30f,std::min({a[1],b[1],c[1],d[1]})-verticalReserve,1e30f},localHigh{-1e30f,std::max({a[1],b[1],c[1],d[1]})+verticalReserve,-1e30f};
  for(const auto* line:{&slow,&fast})for(unsigned index:{i-1,i}){
   const auto& q=line->points[index];unsigned left=index?index-1:0,right=std::min(index+1,unsigned(line->points.size()-1));
   float ex=line->points[right].p[0]-line->points[left].p[0],ez=line->points[right].p[2]-line->points[left].p[2],length=std::hypot(ex,ez);if(length<.01f)return false;ex/=length;ez/=length;
   for(float front:{-horizontalLength-.2f,horizontalLength+.2f})for(float side:{-halfWidth-.2f,halfWidth+.2f}){
    auto corner=local({q.p[0]+ex*front-ez*side,q.p[1],q.p[2]+ez*front+ex*side});
    localLow.x=std::min(localLow.x,corner.x);localHigh.x=std::max(localHigh.x,corner.x);localLow.z=std::min(localLow.z,corner.z);localHigh.z=std::max(localHigh.z,corner.z);
   }
  }
  Vec3 lo{1e30f,localLow.y,1e30f},hi{-1e30f,localHigh.y,-1e30f};
  for(float x:{localLow.x,localHigh.x})for(float z:{localLow.z,localHigh.z}){float px=-hz*x+hx*z,pz=hx*x+hz*z;lo.x=std::min(lo.x,px);hi.x=std::max(hi.x,px);lo.z=std::min(lo.z,pz);hi.z=std::max(hi.z,pz);}
  Vec3 center{(localLow.x+localHigh.x)*.5f,(localLow.y+localHigh.y)*.5f,(localLow.z+localHigh.z)*.5f};
  Vec3 extent{(localHigh.x-localLow.x)*.5f,(localHigh.y-localLow.y)*.5f,(localHigh.z-localLow.z)*.5f};
  auto overlaps=[&](unsigned index){
   const auto& triangle=world.triangles[index];Vec3 v[3];for(unsigned j=0;j<3;++j){v[j]=local(triangle.p[j]);v[j].x-=center.x;v[j].y-=center.y;v[j].z-=center.z;}
   auto separating=[&](Vec3 axis){float r=extent.x*std::abs(axis.x)+extent.y*std::abs(axis.y)+extent.z*std::abs(axis.z)+.001f;float a=(axis.x*v[0].x+axis.y*v[0].y+axis.z*v[0].z),b=(axis.x*v[1].x+axis.y*v[1].y+axis.z*v[1].z),c=(axis.x*v[2].x+axis.y*v[2].y+axis.z*v[2].z);return std::min({a,b,c})>r||std::max({a,b,c})<-r;};
   if(separating({1,0,0})||separating({0,1,0})||separating({0,0,1}))return false;
   auto sub=[](Vec3 a,Vec3 b){return Vec3{a.x-b.x,a.y-b.y,a.z-b.z};};auto cross=[](Vec3 a,Vec3 b){return Vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
   Vec3 edge[3]={sub(v[1],v[0]),sub(v[2],v[1]),sub(v[0],v[2])};
   if(separating(cross(edge[0],edge[1])))return false;
   for(Vec3 e:edge)for(Vec3 axis:{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}})if(separating(cross(e,axis)))return false;
   return true;
  };
  world.QueryBox(lo,hi,candidates,true);
  Vec3 probeFrom=from,probeTo=to;probeFrom.x-=hx*reserve;probeFrom.z-=hz*reserve;probeTo.x+=hx*reserve;probeTo.z+=hz*reserve;
  for(unsigned triangle:candidates)if(overlaps(triangle)&&!YellowSupportedStepFace(world,triangle,probeFrom,probeTo,.75f)){if(failure)*failure={7,float(triangle),float(slow.points[i].station),from};return false;}
  float spread=std::max(std::hypot(a[0]-c[0],a[2]-c[2]),std::hypot(b[0]-d[0],b[2]-d[2]))*.5f;
  float bottom=std::min({a[1],b[1],c[1],d[1]}),top=std::max({a[1],b[1],c[1],d[1]});
  for(float y=bottom;;y=std::min(y+.5f,top)){
   Vec3 p=from,q=to;p.y=q.y=y;
   if(!YellowEditorWallCorridor(p,q,halfWidth+spread+.35f))return false;
   float p0[]={p.x,p.y,p.z},p1[]={q.x,q.y,q.z};
   if(!YellowTrafficCorridor(p0,p1,cars,count,halfWidth+spread+.35f,std::min(2.5f,std::max(0.f,slow.points[i].station-begin)/std::abs(speed))))return false;
   if(y>=top)break;
  }
 }return true;
}
