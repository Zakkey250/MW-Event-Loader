#pragma once
#include "../../World2MW/src/NativeLoader/YellowTraversableStep.h"
// Dedicated descent clearance. The accepted race planner keeps its own policy.
static bool NeoDownstepWorldWallCorridor(const CollisionWorld& world,Vec3 from,Vec3 to,float halfWidth,bool allowExistingContact,float padding,const Vec3* bodyCenter,YellowWallDiagnostic* diagnostic){
 if(diagnostic)*diagnostic={};auto fail=[&](unsigned reason,unsigned triangle=~0u){if(diagnostic)*diagnostic={reason,triangle,from,to};return false;};
 float dx=to.x-from.x,dz=to.z-from.z,length=mwel_math::Hypot(dx,dz);if(length<.01f)return false;
 float nx=-dz/length,nz=dx/length;RayHit hit;
 RayHit ground;float clearance=.5f;if(world.Ray({from.x,from.y+1,from.z},{from.x,from.y-3,from.z},ground,true))clearance=std::clamp(from.y-ground.point.y,.15f,1.f);
 // One broad phase supplies all nine body-width/height rays. Preserve the
 // original ray/triangle intersection and source leaf order.
 static thread_local std::vector<unsigned> rayTriangles;const auto tolerance=world.WallRayPadding();
 const float queryWidth=halfWidth+padding+.001f;
 const Vec3 footprintLow{std::min(from.x,to.x)-queryWidth,std::min(from.y,to.y)-clearance,std::min(from.z,to.z)-queryWidth},footprintHigh{std::max(from.x,to.x)+queryWidth,std::max(from.y,to.y)+2,std::max(from.z,to.z)+queryWidth};
 const Vec3 rayLow{std::min(from.x,to.x)-halfWidth-tolerance.x,std::min(from.y,to.y)-clearance-tolerance.y,std::min(from.z,to.z)-halfWidth-tolerance.z},rayHigh{std::max(from.x,to.x)+halfWidth+tolerance.x,std::max(from.y,to.y)+2+tolerance.y,std::max(from.z,to.z)+halfWidth+tolerance.z};
 world.QueryBox({std::min(rayLow.x,footprintLow.x),std::min(rayLow.y,footprintLow.y),std::min(rayLow.z,footprintLow.z)},{std::max(rayHigh.x,footprintHigh.x),std::max(rayHigh.y,footprintHigh.y),std::max(rayHigh.z,footprintHigh.z)},rayTriangles,true);
 // Complete static broad phase; live editor barriers were checked outside.
 if(rayTriangles.empty())return true;
 std::vector<signed char> stepMemo;
 if(yellowTraversableSteps)stepMemo.assign(rayTriangles.size(),-1);
 auto traversableStep=[&](unsigned index){
  if(!yellowTraversableSteps)return false;
  auto found=std::find(rayTriangles.begin(),rayTriangles.end(),index);if(found==rayTriangles.end())return false;
  auto& memo=stepMemo[found-rayTriangles.begin()];
  if(memo<0)memo=YellowSupportedStepFace(world,index,from,to,clearance)?1:0;
  return memo==1;
 };
 for(float height:{.4f-clearance,.8f-clearance,.5f})for(float side:{-halfWidth,0.f,halfWidth}){
  Vec3 a{from.x+nx*side,from.y+height,from.z+nz*side},b{to.x+nx*side,to.y+height,to.z+nz*side};
  bool struck=world.RayCandidates(a,b,hit,rayTriangles,true);
  // A skipped step must not conceal another wall behind it. Filter only
  // proven step faces; subset rays are deliberately not inserted into the
  // complete-wall-broadphase cache, whose key does not include candidates.
  while(struck&&traversableStep(hit.triangle)){
   std::vector<unsigned> remaining;remaining.reserve(rayTriangles.size());
   for(unsigned i=0;i<rayTriangles.size();++i)if(stepMemo[i]!=1)remaining.push_back(rayTriangles[i]);
   struck=world.RayCandidates(a,b,hit,remaining,false);
  }
  if(struck){
   if(!allowExistingContact)return fail(2,hit.triangle);
   const auto& t=world.triangles[hit.triangle];std::array<std::pair<Vec3,Vec3>,3> edges;
   for(unsigned j=0;j<3;++j)edges[j]={t.p[j],t.p[(j+1)%3]};
   if(!YellowBarrierSegmentsClear(edges,from,to,halfWidth,true,padding,bodyCenter,true))return fail(2,hit.triangle);
  }
 }
 // Thin posts can fit between individual rays. Test wall-triangle footprints
 // against the swept car width, ignoring low curbs and overhead geometry.
 for(unsigned index:rayTriangles){if(!world.TriangleOverlaps(index,footprintLow,footprintHigh))continue;const auto& triangle=world.triangles[index];
  Vec3 center{(triangle.p[0].x+triangle.p[1].x+triangle.p[2].x)/3,0,(triangle.p[0].z+triangle.p[1].z+triangle.p[2].z)/3};
  float t=std::clamp(((center.x-from.x)*dx+(center.z-from.z)*dz)/(length*length),0.f,1.f),road=from.y+(to.y-from.y)*t-clearance;
  float top=std::max({triangle.p[0].y,triangle.p[1].y,triangle.p[2].y}),bottom=std::min({triangle.p[0].y,triangle.p[1].y,triangle.p[2].y});
  if(top<road+.35f||bottom>road+1.8f)continue;
  if(traversableStep(index))continue;
  std::array<std::pair<Vec3,Vec3>,3> sides;for(unsigned i=0;i<3;++i)sides[i]={triangle.p[i],triangle.p[(i+1)%3]};
  if(!YellowBarrierSegmentsClear(sides,from,to,halfWidth,allowExistingContact,padding,bodyCenter,true))return fail(3,index);
 }return true;
}

static bool NeoDownstepWallCorridor(const CollisionWorld& world,Vec3 from,Vec3 to,float halfWidth,bool allowExistingContact=false,float padding=.2f,const Vec3* bodyCenter=nullptr,YellowWallDiagnostic* diagnostic=nullptr){
 if(diagnostic)*diagnostic={};
 if(mwel_math::Hypot(to.x-from.x,to.z-from.z)<.01f)return false;
 // Event barriers remain live. Only immutable world geometry is memoized.
 if(!YellowEditorWallCorridor(from,to,halfWidth,allowExistingContact,padding,bodyCenter)){if(diagnostic)*diagnostic={1,~0u,from,to};return false;}
 const unsigned generation=world.QueryGeneration();
 if(!generation)return NeoDownstepWorldWallCorridor(world,from,to,halfWidth,allowExistingContact,padding,bodyCenter,diagnostic);
 struct Entry{std::array<unsigned,12> key{};const CollisionWorld* world=nullptr;unsigned generation=0;bool clear=false;YellowWallDiagnostic diagnostic;};
 static thread_local std::vector<Entry> cache(4096);
 std::array<unsigned,12> key{};memcpy(key.data(),&from,12);memcpy(key.data()+3,&to,12);memcpy(key.data()+6,&halfWidth,4);memcpy(key.data()+7,&padding,4);key[8]=unsigned(allowExistingContact)|(unsigned(bodyCenter!=nullptr)<<1)|(unsigned(yellowTraversableSteps)<<2)|(unsigned(yellowTerrainDrops)<<3)|(yellowTerrainDrops?unsigned(std::ceil(yellowDropFloorRange))<<4:0u);if(bodyCenter)memcpy(key.data()+9,bodyCenter,12);
 unsigned hash=2166136261u;for(auto bits:key)hash=(hash^bits)*16777619u;
 // Mix high coordinate bits into the table index. Grid-aligned float keys
 // otherwise collide despite describing distant, unrelated world queries.
 hash^=hash>>16;hash*=0x7FEB352Du;hash^=hash>>15;hash*=0x846CA68Bu;hash^=hash>>16;
 for(unsigned probe=0,index=hash&4095;probe<32;++probe,index=(index+1)&4095){auto& entry=cache[index];
  if(entry.world!=&world||entry.generation!=generation){YellowWallDiagnostic result;bool clear=NeoDownstepWorldWallCorridor(world,from,to,halfWidth,allowExistingContact,padding,bodyCenter,&result);entry={key,&world,generation,clear,result};if(diagnostic)*diagnostic=result;return clear;}
  if(entry.key==key){if(diagnostic)*diagnostic=entry.diagnostic;return entry.clear;}
 }
 return NeoDownstepWorldWallCorridor(world,from,to,halfWidth,allowExistingContact,padding,bodyCenter,diagnostic);
}

static bool NeoDownstepAimGround(const CollisionWorld& world,Vec3 p,float expected,float clearance,Vec3& result){
 RayHit hit;if(!world.Ray({p.x,expected+2,p.z},{p.x,expected-3,p.z},hit,true))return false;
 if(fabs(hit.point.y+clearance-expected)>2.5f)return false;
 result={p.x,hit.point.y+clearance,p.z};return true;
}

static bool NeoDownstepLineSupported(const CollisionWorld& world,Vec3 from,Vec3 to,float width,float clearance,bool allowExistingContact=false,unsigned* failure=nullptr,float padding=.2f){
 if(failure)*failure=0;
 float dx=to.x-from.x,dz=to.z-from.z,length=mwel_math::Hypot(dx,dz);if(length<.1f)return false;
 if(!NeoDownstepWallCorridor(world,from,to,width,allowExistingContact,padding)){if(failure)*failure=1;return false;}
 const unsigned steps=unsigned(std::ceil(length/3.f));float nx=-dz/length,nz=dx/length;
 for(unsigned i=1;i<=steps;++i){float t=float(i)/steps;
  for(float side:{-width,0.f,width}){Vec3 p{from.x+dx*t+nx*side,from.y+(to.y-from.y)*t,from.z+dz*t+nz*side},surface;
   if(!NeoDownstepAimGround(world,p,p.y,clearance,surface)){if(failure)*failure=2;return false;}
  }
 }return true;
}

