#include "EventCore.h"
#include "../../World2MW/src/NativeLoader/CollisionWorld.h"
#include "../../integration/MWEventGeometryApi.h"
#include "NativeGroupPolicy.h"
#include "NativeCollisionBuild.h"
#include "NeoDriverBridge.h"
#include "NeoProviderBridge.h"
#include <MinHook.h>

extern void MWELNeoLog(const char* text);
extern bool MWELNeoLoggingEnabled();
namespace native_event_geometry {
using namespace w2mw;
CollisionWorld world;
bool tried=false,loaded=false;
std::vector<std::uint16_t> groups;
mwel_native_groups::Flags groupFlags{};
bool groupScope=false;
bool Snapshot(){__try{memcpy(groupFlags.data(),reinterpret_cast<void*>(0x902160),groupFlags.size());return true;}__except(EXCEPTION_EXECUTE_HANDLER){groupFlags.fill(0);return false;}}
bool Enabled(const void*,unsigned index){return index<groups.size()&&mwel_native_groups::Enabled(groups[index],groupFlags);}
bool W2CActive(){
 auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");
 auto query=module?reinterpret_cast<unsigned(__cdecl*)(mwelgeo::Api*,unsigned)>(GetProcAddress(module,"W2MW_GeometryApiV1")):nullptr;
 mwelgeo::Api api{};
 return query&&query(&api,sizeof(api))&&api.size==sizeof(api)&&api.version==mwelgeo::Version&&api.worldKind==mwelgeo::W2CWorld&&api.available&&api.available();
}
std::string FileHash(const mwel::fs::path& path){
 if(!mwel::fs::is_regular_file(path)||mwel::fs::is_symlink(path))throw std::runtime_error("World source is not an ordinary file");
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("Source hash unavailable");
 std::string result;bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 unsigned char digest[32]{};std::array<char,65536> buffer{};std::ifstream stream(path,std::ios::binary);
 if(!stream)ok=false;
 while(ok&&stream){stream.read(buffer.data(),buffer.size());auto bytes=stream.gcount();if(bytes&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),ULONG(bytes),0)<0)ok=false;}
 ok=ok&&stream.eof()&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 if(!ok)throw std::runtime_error("Source hash failed");
 for(auto v:digest){constexpr char hex[]="0123456789ABCDEF";result+=hex[v>>4];result+=hex[v&15];}return result;
}
void LoadCache(const mwel::fs::path& game,const mwel::fs::path& folder){
  auto manifest=mwel::Json::parse(mwel::Read(folder/L"source.json"));
  if(manifest.at("schema")!=2||manifest.at("worldKind")!="native")throw std::runtime_error("Native world manifest needs live group metadata");
  const auto& source=manifest.at("source");if(source.size()!=2)throw std::runtime_error("Native source set unsupported");
  for(const char* name:{"TRACKS/L2RA.BUN","TRACKS/STREAML2RA.BUN"}){
   auto expected=source.at(name).get<std::string>();if(!mwel::Sha256(expected)||FileHash(game/mwel::fs::u8path(name))!=expected)throw std::runtime_error("Native world revision mismatch");
  }
  auto expected=manifest.at("cacheSha256").get<std::string>();
  if(!mwel::Sha256(expected)||FileHash(folder/L"geometry.wmc")!=expected)throw std::runtime_error("Native query cache checksum mismatch");
  unsigned count=manifest.at("triangles").get<unsigned>();if(!count||count>4000000||!world.Load((folder/L"geometry.wmc").string(),count))throw std::runtime_error("Native collision cache invalid");
  expected=manifest.at("groupSha256").get<std::string>();if(!mwel::Sha256(expected)||FileHash(folder/L"groups.bin")!=expected)throw std::runtime_error("Native group metadata checksum mismatch");
  auto bytes=mwel::Read(folder/L"groups.bin",12+8000000);unsigned h[3]{};
  if(bytes.size()!=12+count*2)throw std::runtime_error("Native group metadata size");memcpy(h,bytes.data(),12);
  if(h[0]!=0x31474D57||h[1]!=1||h[2]!=count)throw std::runtime_error("Native group metadata identity");
  groups.resize(count);memcpy(groups.data(),bytes.data()+12,count*2);
  for(auto group:groups)if(group>=mwel_native_groups::Count)throw std::runtime_error("Native group metadata bounds");
}
bool Load(){
 if(tried)return loaded;tried=true;
 try{
  const BYTE statusGuard[]={0x8B,0x44,0x24,0x04,0x0F,0xBE,0x80,0x60,0x21,0x90,0x00,0xC3};
  if(memcmp(reinterpret_cast<void*>(0x722D70),statusGuard,sizeof(statusGuard)))throw std::runtime_error("Native scenery status ABI mismatch");
  wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))return false;
  auto game=mwel::fs::path(executable).parent_path();bool rebuilt=false;
  try{LoadCache(game,mwel_paths::Geometry(game));}
  catch(const std::exception& error){
   if(MWELNeoLoggingEnabled()){char text[512]{};sprintf_s(text,"EVENT_NATIVE_CACHE_REBUILD reason=%s receiving_installation=1",error.what());MWELNeoLog(text);}
   // A portable event's cache may come from another Rockport revision.
   // Rebuild from this installation instead of discarding its properties or
   // trusting mismatched collision/group records. No game files are written.
   world=CollisionWorld{};groups.clear();native_collision_build::Build(game,world,groups);rebuilt=true;
  }
  if(!Snapshot())throw std::runtime_error("Native group status unavailable");
  world.triangleEnabled=Enabled;loaded=true;
  if(MWELNeoLoggingEnabled()){char text[256]{};sprintf_s(text,"EVENT_NATIVE_GEOMETRY_LOADED triangles=%u rebuilt=%u native_group_activation=1 original_track_modified=0",unsigned(world.triangles.size()),unsigned(rebuilt));MWELNeoLog(text);}
 }catch(const std::exception& error){if(MWELNeoLoggingEnabled()){char text[512]{};sprintf_s(text,"EVENT_NATIVE_GEOMETRY_REFUSED reason=%s",error.what());MWELNeoLog(text);}loaded=false;world=CollisionWorld{};groups.clear();}
 return loaded;
}
unsigned __cdecl Available(){
 // A bound query freezes the world backend and scenery group snapshot.
 // The native provider cannot change to W2C halfway through that query.
 if(groupScope)return loaded&&!world.nodes.empty();
 return !W2CActive()&&Load()&&!world.nodes.empty();
}
bool Finite(mwelgeo::Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&fabs(p.x)<100000&&fabs(p.y)<100000&&fabs(p.z)<100000;}
Vec3 V(mwelgeo::Point p){return {p.x,p.y,p.z};}
unsigned __cdecl Begin(){if(!Available())return 2;auto previous=world.BeginQuery();if(!previous){groupScope=true;Snapshot();}return previous;}
void __cdecl End(unsigned previous){if(previous<2){world.EndQuery(previous);if(!previous)groupScope=false;}}
unsigned __cdecl Generation(){return world.QueryGeneration();}
unsigned __cdecl Ray(mwelgeo::Point from,mwelgeo::Point to,mwelgeo::Hit* out,unsigned flags){
 if(!out||flags>3||!Available()||!Finite(from)||!Finite(to))return 0;
 if(!groupScope)Snapshot();RayHit hit;bool found=world.Ray(V(from),V(to),hit,(flags&1)!=0,(flags&2)!=0);memcpy(out,&hit,sizeof(hit));return unsigned(found);
}
unsigned __cdecl Box(mwelgeo::Point low,mwelgeo::Point high,unsigned walls,mwelgeo::IndexSink sink,void* context){
 if(!sink||walls>1||!Available()||!Finite(low)||!Finite(high)||low.x>high.x||low.y>high.y||low.z>high.z)return 0;
 if(!groupScope)Snapshot();static thread_local std::vector<unsigned> found;world.QueryBox(V(low),V(high),found,walls!=0);for(auto index:found)sink(context,index);return 1;
}
unsigned __cdecl Triangle(unsigned index,mwelgeo::Triangle* out){if(!out||!Available()||index>=world.triangles.size())return 0;if(!groupScope)Snapshot();if(!Enabled(nullptr,index))return 0;memcpy(out,&world.triangles[index],sizeof(*out));return 1;}
unsigned __cdecl Overlaps(unsigned index,mwelgeo::Point low,mwelgeo::Point high){if(!Available()||!Finite(low)||!Finite(high)||index>=world.triangles.size())return 0;if(!groupScope)Snapshot();return world.TriangleOverlaps(index,V(low),V(high));}
mwelgeo::Point __cdecl Padding(){auto p=world.WallRayPadding();return {p.x,p.y,p.z};}
unsigned __cdecl Candidates(mwelgeo::Point from,mwelgeo::Point to,mwelgeo::Hit* out,const unsigned* values,unsigned count,unsigned complete){
 if(!out||(!values&&count)||count>world.triangles.size()||complete>1||!Available()||!Finite(from)||!Finite(to))return 0;
 std::span<const unsigned> indices(values,count);for(auto index:indices)if(index>=world.triangles.size())return 0;
 if(!groupScope)Snapshot();RayHit hit;bool found=world.RayCandidatesSpan(V(from),V(to),hit,indices,complete!=0);memcpy(out,&hit,sizeof(hit));return unsigned(found);
}
}
#pragma comment(linker,"/EXPORT:MWEL_NativeGeometryApiV1=_MWEL_NativeGeometryApiV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NativeGeometryApiV1(mwelgeo::Api* out,unsigned size){
 if(!out||size!=sizeof(*out))return 0;
 using namespace native_event_geometry;
 *out={sizeof(*out),mwelgeo::Version,mwelgeo::NativeWorld,Available,Begin,End,Generation,Ray,Box,Triangle,Overlaps,Padding,Candidates};return 1;
}
bool MWELNativeGeometryAudit(){
 using namespace native_event_geometry;if(!Available())return false;
 unsigned floors=0,walls=0;auto previous=Begin();
 unsigned index=0,disabled=0;
 for(const auto& triangle:world.triangles){
  if(!Enabled(nullptr,index++)){++disabled;continue;}
  if((triangle.wall&&walls>=16)||(!triangle.wall&&floors>=16))continue;
  auto n=Cross(Sub(triangle.p[1],triangle.p[0]),Sub(triangle.p[2],triangle.p[0]));float length=std::sqrt(Dot(n,n));if(length<1e-5f)continue;
  n={n.x/length,n.y/length,n.z/length};Vec3 center{(triangle.p[0].x+triangle.p[1].x+triangle.p[2].x)/3,(triangle.p[0].y+triangle.p[1].y+triangle.p[2].y)/3,(triangle.p[0].z+triangle.p[1].z+triangle.p[2].z)/3};
  mwelgeo::Hit hit{};if(!Ray({center.x+n.x,center.y+n.y,center.z+n.z},{center.x-n.x,center.y-n.y,center.z-n.z},&hit,triangle.wall?2:1)){End(previous);return false;}
  if(triangle.wall)++walls;else ++floors;if(floors==16&&walls==16)break;
 }
 End(previous);if(floors!=16||walls!=16)return false;
 char text[256]{};sprintf_s(text,"EVENT_NATIVE_GEOMETRY_AUDIT_PASS floors=16 walls=16 inactive_triangles_skipped=%u native_group_status_observed=1 W2C_required=0 vanilla_race_verified=0",disabled);MWELNeoLog(text);return true;
}
bool MWELNativeGeometryWarmup(){return native_event_geometry::Available()!=0;}
#include "NeoNativeGroundFallback.inl"
