#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <stdexcept>
#include "../../World2MW/src/NativeLoader/CollisionWorld.h"
using namespace w2mw;
static CollisionWorld collision;
static bool worldActive=true;
static bool loaderActive=true,propNativeReference=false;
unsigned W2MW_IsWorldActiveV1(){return unsigned(worldActive);}
#include "../../World2MW/src/NativeLoader/EventGeometryService.h"
#include "GeometryQueries.h"
#include "NativeGroupPolicy.h"
#include "GridSupport.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "../../World2MW/src/NativeLoader/YellowBarrierCorridor.h"
static mwel_native_groups::Flags testGroups{};
static bool GroupEnabled(const void*,unsigned index){return index<2||mwel_native_groups::Enabled(7,testGroups);}
static unsigned availabilityChecks=0;
static unsigned CountAvailable(){++availabilityChecks;return event_geometry_service::Available();}
int main(int argc,char** argv){
 collision.triangles={{{{-100,0,-100},{100,0,-100},{100,0,100}},0,0},{{{-100,0,-100},{100,0,100},{-100,0,100}},0,0},{{{5,0,-10},{5,6,-10},{5,6,10}},0,1},{{{5,0,-10},{5,6,10},{5,0,10}},0,1}};
 collision.nodes={{{-100,-1,-100},{100,7,100},0,0x80000004}};assert(collision.PrepareQueries());
 mwelgeo::Api api{};assert(!W2MW_GeometryApiV1(&api,sizeof(api)-1));assert(W2MW_GeometryApiV1(&api,sizeof(api)));
 auto countedApi=api;countedApi.available=CountAvailable;event_geometry::WorldQueries scopedAdapter(countedApi);
 {event_geometry::WorldQueries::RayQueryScope scope(scopedAdapter);unsigned checks=availabilityChecks;event_geometry::RayHit hit;
  for(unsigned i=0;i<100;++i)assert(scopedAdapter.Ray({0,2,0},{0,-2,0},hit,true));
  {event_geometry::WorldQueries::RayQueryScope nested(scopedAdapter);assert(scopedAdapter.Ray({0,2,0},{0,-2,0},hit,true));}
  assert(availabilityChecks==checks);loaderActive=false;assert(!scopedAdapter.Ray({0,2,0},{0,-2,0},hit,true));loaderActive=true;
 }
 unsigned checks=availabilityChecks;event_geometry::RayHit countedHit;assert(scopedAdapter.Ray({0,2,0},{0,-2,0},countedHit,true));assert(availabilityChecks>checks);
 event_geometry::WorldQueries adapter(api);assert(!adapter.nodes.empty());
 {event_geometry::WorldQueries::RayQueryScope query(adapter);auto generation=adapter.QueryGeneration();assert(generation);
  {event_geometry::WorldQueries::RayQueryScope nested(adapter);assert(adapter.QueryGeneration()==generation);}
  assert(adapter.QueryGeneration()==generation);
  w2mw::RayHit native;event_geometry::RayHit through;
  assert(collision.Ray({0,2,0},{0,-2,0},native,true));assert(adapter.Ray({0,2,0},{0,-2,0},through,true));assert(memcmp(&native,&through,sizeof(native))==0);
  std::vector<unsigned> direct,indirect;collision.QueryBox({4,0,-1},{6,3,1},direct,true);adapter.QueryBox({4,0,-1},{6,3,1},indirect,true);assert(direct==indirect&&direct.size()==2);
  assert(collision.RayCandidates({0,1,0},{10,1,0},native,direct,true));assert(adapter.RayCandidates({0,1,0},{10,1,0},through,indirect,true));assert(memcmp(&native,&through,sizeof(native))==0);
  auto triangle=adapter.triangles[native.triangle];assert(memcmp(&triangle,&collision.triangles[native.triangle],sizeof(triangle))==0);
  assert(adapter.TriangleOverlaps(native.triangle,{4,0,-1},{6,3,1}));
  assert(!adapter.Ray({NAN,0,0},{1,1,1},through));
 }
 // Cross-query reuse belongs only to immutable wall candidates. The filter
 // tests below must still observe every native group activation change.
 collision.profileEnabled=true;std::vector<unsigned> cachedWalls;
 {CollisionWorld::RayQueryScope query(collision);collision.QueryBox({4,0,-1},{6,3,1},cachedWalls,true);}
 unsigned hitsBefore=collision.profile.boxCacheHits;
 {CollisionWorld::RayQueryScope query(collision);collision.QueryBox({4,0,-1},{6,3,1},cachedWalls,true);assert(cachedWalls.size()==2);assert(collision.profile.boxCacheHits>hitsBefore);}
 collision.profileEnabled=false;
 assert(adapter.QueryGeneration()==0);worldActive=false;event_geometry::RayHit missing;
 // Race preparation precedes driving. A loaded W2C world must still answer.
 assert(!adapter.nodes.empty());assert(adapter.Ray({0,2,0},{0,-2,0},missing,true));
 propNativeReference=true;assert(adapter.nodes.empty());assert(!adapter.Ray({0,2,0},{0,-2,0},missing,true));assert(missing.triangle==~0u);
 propNativeReference=false;loaderActive=false;assert(adapter.nodes.empty());assert(!adapter.Ray({0,2,0},{0,-2,0},missing,true));
 worldActive=true;loaderActive=true;assert(!adapter.nodes.empty());assert(adapter.Ray({0,2,0},{0,-2,0},missing,true));
 auto invalid=api;invalid.size=0;event_geometry::WorldQueries rejected(invalid);assert(rejected.nodes.empty());
 invalid=api;invalid.triangle=nullptr;event_geometry::WorldQueries incomplete(invalid);assert(incomplete.nodes.empty());
 collision.triangleEnabled=GroupEnabled;
 {
  CollisionWorld::RayQueryScope scope(collision);RayHit hit;
  assert(!collision.Ray({0,1,0},{10,1,0},hit,false,true));
  assert(collision.Ray({0,2,0},{0,-2,0},hit,true));
  std::vector<unsigned> candidates;collision.QueryBox({4,0,-1},{6,3,1},candidates,true);assert(candidates.empty());
  assert(!collision.TriangleOverlaps(2,{4,0,-1},{6,3,1}));
  std::array<unsigned,2> all{2,3};assert(!collision.RayCandidatesSpan({0,1,0},{10,1,0},hit,all,true));
 }
 testGroups[7]=1;
 {
  CollisionWorld::RayQueryScope scope(collision);RayHit hit;
  assert(collision.Ray({0,1,0},{10,1,0},hit,false,true));
  std::array<unsigned,2> all{2,3};assert(collision.RayCandidatesSpan({0,1,0},{10,1,0},hit,all,true));
  std::vector<unsigned> candidates;collision.QueryBox({4,0,-1},{6,3,1},candidates,true);assert(candidates.size()==2);
 }
 testGroups[7]=4;
 {CollisionWorld::RayQueryScope scope(collision);RayHit hit;assert(collision.Ray({0,1,0},{10,1,0},hit,false,true));}
 testGroups[7]=0;
 {CollisionWorld::RayQueryScope scope(collision);RayHit hit;assert(!collision.Ray({0,1,0},{10,1,0},hit,false,true));}
 assert(mwel_native_groups::Enabled(0,testGroups)&&!mwel_native_groups::Enabled(65535,testGroups));
 collision.triangleEnabled=nullptr;
 puts("NATIVE_GROUP_POLICY_PASS disabled_geometry_excluded=1 native_any_nonzero_status=1 activation_and_teardown=1 query_cache_epoch=1 unconditional_floors_preserved=1");
 puts("GEOMETRY_API_PASS POD_ABI=1 scoped_cache=1 native_equivalence=1 preparation_before_driving=1 inactive_reference_world_rejected=1 invalid_table_rejected=1");
 if(argc==5){
  assert(collision.Load(argv[1],std::stoul(argv[2])));loaderActive=true;propNativeReference=false;worldActive=false;
  assert(!adapter.nodes.empty());
  std::ifstream eventFile(argv[3]);auto package=nlohmann::json::parse(eventFile);const auto& gate=package.at("authoring").at("gates").front();const auto& p=gate.at("position");
  mwel_grid::Position origin{-p.at(1).get<float>(),p.at(2).get<float>(),p.at(0).get<float>()};float rotation=gate.at("rotation").get<float>();
  std::ifstream wallFile(argv[4],std::ios::binary);unsigned header[3]{};wallFile.read(reinterpret_cast<char*>(header),sizeof(header));assert(header[0]==0x31434557&&header[2]<=2048);
  std::vector<std::pair<Vec3,Vec3>> walls;
  for(unsigned i=0;i<header[2];++i){
   unsigned bytes=0;Header mesh{};wallFile.read(reinterpret_cast<char*>(&bytes),4);wallFile.read(reinterpret_cast<char*>(&mesh),sizeof(mesh));assert(mesh.magic==Magic&&mesh.version==1&&mesh.vertices<=1024&&mesh.meshes<=16&&mesh.indices<=4096);
   assert(bytes==sizeof(mesh)+mesh.meshes*sizeof(Mesh)+mesh.vertices*sizeof(Vertex)+mesh.indices*4);
   std::vector<Mesh> materials(mesh.meshes);std::vector<Vertex> vertices(mesh.vertices);wallFile.read(reinterpret_cast<char*>(materials.data()),materials.size()*sizeof(Mesh));wallFile.read(reinterpret_cast<char*>(vertices.data()),vertices.size()*sizeof(Vertex));wallFile.seekg(mesh.indices*4,std::ios::cur);assert(wallFile);
   Vec3 a{},b{};float longest=-1;for(const auto& first:vertices)for(const auto& second:vertices){auto x=first.position,y=second.position;float length=std::hypot(x.x-y.x,x.y-y.y);if(length>longest){longest=length;a=x;b=y;}}
   walls.push_back({{-a.y,mesh.minimum.z,a.x},{-b.y,mesh.maximum.z,b.x}});
  }
  assert(wallFile.peek()==EOF);
  for(const auto& point:package.at("authoring").value("extras",nlohmann::json::array()))if(point.value("nativeTemplate","")=="speedtrap"){
   const auto& pos=point.at("position");Vec3 at{-pos.at(1).get<float>(),pos.at(2).get<float>(),pos.at(0).get<float>()};RayHit floor{};
   bool hit=collision.Ray({at.x,at.y+50,at.z},{at.x,at.y-100,at.z},floor,true);
   printf("SPEEDTRAP_FLOOR_REPLAY point=%s height=%.3f floor_read=%u floor_height=%.3f delta=%.3f\n",point.value("name","").c_str(),at.y,unsigned(hit),floor.point.y,hit?at.y-floor.point.y:0.f);
  }
  auto clear=[&](const mwel_grid::Position& p,float dx,float dz){std::vector<std::pair<Vec3,Vec3>> nearby;for(const auto& w:walls)if(p[1]+.75f>=w.first.y&&p[1]-1.25f<=w.second.y)nearby.push_back(w);return YellowBarrierSegmentsClear(nearby,Vec3{p[0]-dx*2.8f,p[1],p[2]-dz*2.8f},Vec3{p[0]+dx*2.8f,p[1],p[2]+dz*2.8f},1.2f);};
  for(unsigned count:{4u,6u,8u,16u}){auto layout=mwel_grid::Supported(count,origin,rotation,adapter,clear);assert(layout.valid&&layout.count==count);for(unsigned a=0;a<count;++a)for(unsigned b=a+1;b<count;++b)assert(std::hypot(layout.positions[a][0]-layout.positions[b][0],layout.positions[a][2]-layout.positions[b][2])>=2.99f);printf("AUTHORED_GRID_REPLAY_PASS count=%u columns=%u world_driving=0 floor_and_walls_checked=1\n",count,layout.columns);}
  loaderActive=false;assert(!mwel_grid::Supported(6,origin,rotation,adapter,clear).valid);loaderActive=true;
  auto bad=mwel_grid::Supported(6,{90000,0,90000},rotation,adapter,clear);assert(!bad.valid&&!bad.count);
  puts("GRID_PREFLIGHT_FAILURE_PASS unavailable_backend_and_unsupported_location_have_no_partial_layout");
 }
 if(argc==3){
  CollisionWorld native;unsigned expected=std::stoul(argv[2]);assert(native.Load(argv[1],expected));assert(native.triangles.size()==expected);
  unsigned floors=0,walls=0;
  for(const auto& t:native.triangles){
   if((t.wall&&walls>=16)||(!t.wall&&floors>=16))continue;
   auto n=Cross(Sub(t.p[1],t.p[0]),Sub(t.p[2],t.p[0]));float length=std::sqrt(Dot(n,n));if(length<1e-5f)continue;
   n={n.x/length,n.y/length,n.z/length};Vec3 center{(t.p[0].x+t.p[1].x+t.p[2].x)/3,(t.p[0].y+t.p[1].y+t.p[2].y)/3,(t.p[0].z+t.p[1].z+t.p[2].z)/3};
   RayHit hit;assert(native.Ray({center.x+n.x,center.y+n.y,center.z+n.z},{center.x-n.x,center.y-n.y,center.z-n.z},hit,!t.wall,t.wall!=0));
   if(t.wall)++walls;else ++floors;if(floors==16&&walls==16)break;
  }
  assert(floors==16&&walls==16);printf("NATIVE_COLLISION_CACHE_PASS triangles=%u sampled_floors=%u sampled_walls=%u\n",expected,floors,walls);
 }
}
