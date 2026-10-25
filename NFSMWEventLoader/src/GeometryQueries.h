#pragma once
#include "../../integration/MWEventGeometryApi.h"
#include "../../World2MW/src/NativeLoader/PackFormat.h"
namespace event_geometry {
using w2mw::Vec3;
struct RayHit {unsigned triangle=~0u;float fraction=1;Vec3 point{},normal{};};
struct CollisionTriangle {Vec3 p[3];unsigned surface,wall;};
static_assert(sizeof(RayHit)==sizeof(mwelgeo::Hit)&&sizeof(CollisionTriangle)==sizeof(mwelgeo::Triangle));
inline mwelgeo::Point P(Vec3 p){return {p.x,p.y,p.z};}
class WorldQueries {
 mutable mwelgeo::Api api{};
 mutable unsigned queryDepth=0;
 static bool Valid(const mwelgeo::Api& value){return value.size==sizeof(value)&&value.version==mwelgeo::Version&&(value.worldKind==mwelgeo::NativeWorld||value.worldKind==mwelgeo::W2CWorld)&&value.available&&value.begin&&value.end&&value.generation&&value.ray&&value.box&&value.triangle&&value.overlaps&&value.padding&&value.candidates;}
 bool Connect()const{
  // A validated provider stays bound until the synchronous query ends.
  // Its callbacks retain their own availability and argument checks.
  if(queryDepth)return true;
  if(Valid(api)&&api.available())return true;
  // World activation belongs to W2C. A previously selected native backend
  // becomes unavailable when that loader activates; retry both providers
  // instead of leaving the event loader pinned to its earlier selection.
  auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");
  using Get=unsigned(__cdecl*)(mwelgeo::Api*,unsigned);
  auto get=module?reinterpret_cast<Get>(GetProcAddress(module,"W2MW_GeometryApiV1")):nullptr;
  mwelgeo::Api candidate{};
  if(!get||!get(&candidate,sizeof(candidate))||!Valid(candidate)||!candidate.available()){
   auto events=GetModuleHandleW(L"MWEventLoader.asi");
   auto native=events?reinterpret_cast<Get>(GetProcAddress(events,"MWEL_NativeGeometryApiV1")):nullptr;
   if(!native||!native(&candidate,sizeof(candidate))||!Valid(candidate)||!candidate.available())return false;
  }
  api=candidate;return api.available()!=0;
 }
 static void __cdecl Append(void* context,unsigned index){static_cast<std::vector<unsigned>*>(context)->push_back(index);}
public:
 WorldQueries()=default;
 explicit WorldQueries(const mwelgeo::Api& backend):api(backend){}
 struct ProfileCounters {unsigned lookups=0,hits=0,fallbacks=0,probes=0,rays=0,triangles=0,boxes=0,candidateRays=0;};
 mutable ProfileCounters profile{};bool profileEnabled=false;
 struct Nodes {const WorldQueries* world;bool empty()const{return !world->Connect();}} nodes{this};
 struct Triangles {const WorldQueries* world;CollisionTriangle operator[](unsigned index)const{mwelgeo::Triangle raw{};if(!world->Connect()||!world->api.triangle(index,&raw))throw std::runtime_error("Immutable world triangle unavailable");CollisionTriangle value{};memcpy(&value,&raw,sizeof(value));return value;}} triangles{this};
 struct RayQueryScope {const WorldQueries& world;unsigned previous=2;explicit RayQueryScope(const WorldQueries& value):world(value){if(world.Connect()){previous=world.api.begin();if(previous<2)++world.queryDepth;}}~RayQueryScope(){if(previous<2){world.api.end(previous);--world.queryDepth;}}RayQueryScope(const RayQueryScope&)=delete;};
 unsigned QueryGeneration()const{return Connect()?api.generation():0;}
 std::uintptr_t BackendIdentity()const{return Connect()?reinterpret_cast<std::uintptr_t>(api.ray):0;}
 Vec3 WallRayPadding()const{if(!Connect())return {.001f,.001f,.001f};auto p=api.padding();return {p.x,p.y,p.z};}
 bool TriangleOverlaps(unsigned index,Vec3 low,Vec3 high)const{return Connect()&&api.overlaps(index,P(low),P(high));}
 void QueryBox(Vec3 low,Vec3 high,std::vector<unsigned>& result,bool walls)const{result.clear();if(profileEnabled)++profile.boxes;if(Connect())api.box(P(low),P(high),unsigned(walls),Append,&result);}
 bool Ray(Vec3 from,Vec3 to,RayHit& hit,bool floors=false,bool walls=false)const{mwelgeo::Hit raw{~0u,1,{},{}};if(profileEnabled)++profile.rays;bool found=Connect()&&api.ray(P(from),P(to),&raw,unsigned(floors)|(unsigned(walls)<<1));memcpy(&hit,&raw,sizeof(hit));return found;}
 bool RayCandidates(Vec3 from,Vec3 to,RayHit& hit,const std::vector<unsigned>& candidates,bool complete=false)const{mwelgeo::Hit raw{~0u,1,{},{}};if(profileEnabled)++profile.candidateRays;bool found=Connect()&&api.candidates(P(from),P(to),&raw,candidates.data(),unsigned(candidates.size()),unsigned(complete));memcpy(&hit,&raw,sizeof(hit));return found;}
};
}
