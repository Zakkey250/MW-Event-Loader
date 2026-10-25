#pragma once
// A short route boundary can climb/crest between its endpoints. Follow the
// current floor in bounded steps; a single endpoint-to-endpoint 3D chord may
// intersect that road or float above it. Never bridge missing support/decks.
static bool NeoSurfaceApproach(const CollisionWorld& world,mwneo::Point body,mwneo::Point first,mwneo::Point second,float halfWidth,
 mwel_grid::Launch& launch,std::vector<YellowRecoveryPoint>& prefix,unsigned& failure,unsigned* referenceObstructions=nullptr){
 if(referenceObstructions)*referenceObstructions=0;
 failure=0;mwel_grid::Launch candidate;
 if(!candidate.Initialize(body,first,second,NeoLaneExtent)){failure=1;return false;}
 RayHit prior;if(!world.Ray({body.x,body.y+1,body.z},{body.x,body.y-3,body.z},prior,true)){failure=2;return false;}
 const float clearance=std::clamp(body.y-prior.point.y,.15f,1.f),width=std::clamp(halfWidth,.8f,1.5f)+.35f;
 const float horizontal=std::hypot(first.x-body.x,first.z-body.z);
 std::vector<YellowRecoveryPoint> path{{{body.x,body.y,body.z},0}};
 float cursor=0;
 while(horizontal-cursor>.001f){
  float step=std::min(4.f,horizontal-cursor);RayHit floor;Vec3 target{};bool supported=false;
  if(!std::isfinite(prior.normal.y)||std::abs(prior.normal.y)<.4f){failure=3;return false;}
  for(unsigned attempt=0;attempt<3;++attempt){
   const float t=(cursor+step)/horizontal;
   target={body.x+(first.x-body.x)*t,0,body.z+(first.z-body.z)*t};
   const float expected=prior.point.y-(prior.normal.x*(target.x-prior.point.x)+prior.normal.z*(target.z-prior.point.z))/prior.normal.y;
   if(std::isfinite(expected)&&world.Ray({target.x,expected+1,target.z},{target.x,expected-1,target.z},floor,true)&&std::abs(floor.point.y-expected)<=.9f){supported=true;break;}
   step*=.5f;
  }
  if(!supported){failure=4;return false;}
  target.y=floor.point.y+clearance;const auto& previous=path.back();
  // This is the map reference, not an executable steering trajectory. The
  // live lane/arc planner must choose the supported vehicle-width path around
  // a curb/median just as on any other authored leg. Rejecting the reference
  // chord here bypasses that planner and freezes only at a route boundary.
  if(referenceObstructions&&!YellowLineSupported(world,{previous.p[0],previous.p[1],previous.p[2]},target,width,clearance,path.size()==1))++*referenceObstructions;
  cursor+=step;path.push_back({{target.x,target.y,target.z},candidate.length*cursor/horizontal});prior=floor;
 }
 // Keep the authored target deck authoritative. Following a different floor
 // must not silently connect to the road above/below the actual first leg.
 if(std::abs(prior.point.y-first.y)>2.5f){failure=5;return false;}
 if(path.size()<2){failure=6;return false;}
 launch=candidate;prefix=std::move(path);return true;
}

// The body may cross a wide finish gate in a different lane. Its diagonal
// chord to the grid can cut across a median even though the authored boundary
// runs along a continuous street. Build the reference from that boundary,
// keeping the actual body on the same floor; live steering/width/wall checks
// still decide how to join it. This does not grant any CP or lap progress.
static bool NeoBoundarySurfaceApproach(const CollisionWorld& world,mwneo::Point body,mwneo::Point boundary,mwneo::Point first,mwneo::Point second,float halfWidth,
 mwel_grid::Launch& launch,std::vector<YellowRecoveryPoint>& prefix,unsigned& failure){
 mwel_grid::Launch actual;
 if(!actual.Initialize(body,first,second,NeoLaneExtent)||!mwneo::Finite(boundary)||mwneo::Distance(body,boundary)>NeoLaneExtent+12.f){failure=7;return false;}
 RayHit a,b;
 if(!world.Ray({body.x,body.y+1,body.z},{body.x,body.y-3,body.z},a,true)||
    !world.Ray({boundary.x,boundary.y+1,boundary.z},{boundary.x,boundary.y-3,boundary.z},b,true)||std::abs(a.normal.y)<.4f){failure=8;return false;}
 float expected=a.point.y-(a.normal.x*(boundary.x-body.x)+a.normal.z*(boundary.z-body.z))/a.normal.y;
 if(!std::isfinite(expected)||std::abs(b.point.y-expected)>2.5f){failure=9;return false;}
 return NeoSurfaceApproach(world,boundary,first,second,halfWidth,launch,prefix,failure);
}
