#include "EventBarriers.h"
#include "NeoProviderBridge.h"
#include "NeoDriverBridge.h"
#include "../../integration/MWEventSceneApi.h"
#include "../../integration/MWRaceBarrierPolicy.h"
#include "../../integration/MWBarrierSpatialIndex.h"
#include "../../World2MW/src/NativeLoader/PackFormat.h"
#include <MinHook.h>
#include <d3d9.h>
#include <array>
#include <cstdio>
#include <memory>
#include <limits>
#include <future>
#include "NativeWallSolids.h"
#pragma comment(lib,"d3d9.lib")
namespace event_barriers {
using w2mw::Vec3;using w2mw::Header;using w2mw::Mesh;using w2mw::Vertex;
struct NativeModel {void* next=nullptr;void* previous=nullptr;unsigned hash=0;void* solid=nullptr;void* replacements=nullptr;unsigned replacementCount=0;};
static_assert(sizeof(NativeModel)==24);
struct Wall {unsigned model=0;D3DMATRIX matrix{};Header header{};std::vector<Mesh> meshes;std::vector<Vertex> vertices;std::vector<unsigned> indices;mwelscene::Barrier contact{};};
struct Course {unsigned key;std::vector<Wall> walls;std::map<unsigned,NativeModel> models;mw_barrier_index::Index contactIndex;};
std::vector<Course> courses;
bool active=false,nativeInstalled=false;
bool solidsAttempted=false;
std::future<std::vector<Course>> pendingCourses;
bool pendingStarted=false;
bool Read(const void* source,void* destination,size_t bytes){if(!source)return false;__try{memcpy(destination,source,bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
unsigned Hash(const std::string& text){return reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)(text.c_str());}
unsigned Current(){if(!active)return 0;unsigned state=0,mode=0;BYTE* race=nullptr;if(!Read(reinterpret_cast<void*>(0x925E90),&state,4)||state!=6||!Read(reinterpret_cast<void*>(0x91E000),&race,4)||!race||!Read(race+0x1960,&mode,4)||mode!=1)return 0;return MWELNeoCurrentKey();}
Course* Find(){auto key=Current();for(auto& course:courses)if(course.key==key)return &course;return nullptr;}
unsigned Contacts(const mwelscene::Point* center,float radius,mwelscene::Barrier* output,unsigned capacity){
 auto course=Find();if(!course||!center||!output||capacity>4096||!std::isfinite(radius)||radius<0)return 0;unsigned count=0;
 thread_local std::vector<unsigned> candidates;const bool indexed=course->contactIndex.Query(center->x,center->z,radius,candidates);
 for(unsigned n=0;n<(indexed?candidates.size():course->walls.size());++n){const auto& wall=course->walls[indexed?candidates[n]:n];const auto& b=wall.contact;float dx=b.b.x-b.a.x,dz=b.b.z-b.a.z,len=dx*dx+dz*dz;if(len<.001f||center->y+4<b.a.y||center->y-4>b.b.y)continue;
  float t=std::clamp(((center->x-b.a.x)*dx+(center->z-b.a.z)*dz)/len,0.f,1.f);float x=b.a.x+t*dx-center->x,z=b.a.z+t*dz-center->z;
  if(x*x+z*z>radius*radius)continue;if(count==capacity)break;output[count++]=b;
 }return count;
}
template<class Add> unsigned ForEachContact(const Course& course,const mwelscene::Point& center,float radius,Add add){
 unsigned count=0;const float radiusSquared=radius*radius;
 thread_local std::vector<unsigned> candidates;const bool indexed=course.contactIndex.Query(center.x,center.z,radius,candidates);
 for(unsigned n=0;n<(indexed?candidates.size():course.walls.size());++n){const auto& wall=course.walls[indexed?candidates[n]:n];const auto& b=wall.contact;float dx=b.b.x-b.a.x,dz=b.b.z-b.a.z,len=dx*dx+dz*dz;
  if(len<.001f||center.y+4<b.a.y||center.y-4>b.b.y)continue;
  float t=std::clamp(((center.x-b.a.x)*dx+(center.z-b.a.z)*dz)/len,0.f,1.f);
  float x=b.a.x+t*dx-center.x,z=b.a.z+t*dz-center.z;
  if(x*x+z*z>radiusSquared)continue;
  add(b);++count;
 }return count;
}
// Native eFindSolid and eView::Render preserve the active installation's
// model, effects, transparency, animated textures and MOD substitutions.
// No private DDS or fixed-function rendering is used.
void DrawUnsafe(void* view){
 auto course=Find();if(!course||!view)return;unsigned id=0;if(!Read(static_cast<BYTE*>(view)+4,&id,4)||(id!=1&&id!=3&&id!=4))return;
 BYTE* camera=nullptr;D3DMATRIX v{};if(!Read(view,&camera,4)||!camera||!Read(camera,&v,64))return;
 Vec3 eye{-(v._41*v._11+v._42*v._12+v._43*v._13),-(v._41*v._21+v._42*v._22+v._43*v._23),-(v._41*v._31+v._42*v._32+v._43*v._33)};
 // Reject out-of-view barriers before resolving solids or their textures.
 // The previous order queried every model on every camera pass, including
 // reflection and depth passes with no visible authored wall.
 constexpr float visibleDistance=300.f;
 thread_local std::vector<Wall*> visible;
 thread_local std::vector<unsigned> visibleModels;
 visible.clear();visibleModels.clear();visible.reserve(course->walls.size());visibleModels.reserve(course->models.size());
 for(auto& wall:course->walls){auto low=wall.header.minimum,high=wall.header.maximum;float dx=(low.x+high.x)*.5f-eye.x,dy=(low.y+high.y)*.5f-eye.y,dz=(low.z+high.z)*.5f-eye.z;if(dx*dx+dy*dy+dz*dz>visibleDistance*visibleDistance)continue;
  if(reinterpret_cast<unsigned char(__thiscall*)(void*,Vec3*,Vec3*,void*)>(0x6CF2B0)(view,&low,&high,nullptr)==0)continue;
  visible.push_back(&wall);visibleModels.push_back(wall.model);
 }
 std::sort(visibleModels.begin(),visibleModels.end());visibleModels.erase(std::unique(visibleModels.begin(),visibleModels.end()),visibleModels.end());
 for(auto hash:visibleModels){auto& model=course->models.at(hash);
  model.solid=reinterpret_cast<void*(__cdecl*)(unsigned)>(0x505340)(hash);
  // Direct starts can register the solid before its texture bank is resident.
  // Resolve the native packet table once all referenced textures are available.
  if(model.solid){auto solid=static_cast<BYTE*>(model.solid);auto table=*reinterpret_cast<unsigned**>(solid+0x2C);bool ready=true,changed=false;
   // Main's installed blue barrier textures share hashes with older gold
   // textures. A separate native pack gives the blue copies unique keys;
   // prefer them only when the registered copy has more image data. The
   // game's own material binder still handles translucency and animation.
   for(unsigned i=0;i<solid[0x19];++i){unsigned alias=table[i*2]==0xA711C606u?0xC909E57Au:table[i*2]==0x2C505700u?0x426DF63Cu:0;if(!alias)continue;
    auto original=reinterpret_cast<BYTE*(__cdecl*)(unsigned,int,int)>(0x503400)(table[i*2],0,0);
    auto replacement=reinterpret_cast<BYTE*(__cdecl*)(unsigned,int,int)>(0x503400)(alias,0,0);
    unsigned originalBytes=0,replacementBytes=0;if(original)Read(original+56,&originalBytes,4);if(replacement)Read(replacement+56,&replacementBytes,4);
    if(replacementBytes>originalBytes){table[i*2]=alias;changed=true;}
   }
   for(unsigned i=0;i<solid[0x19];++i){auto info=reinterpret_cast<void*(__cdecl*)(unsigned,int,int)>(0x503400)(table[i*2],0,0);if(!info)ready=false;else if(unsigned(info)!=table[i*2+1])changed=true;}
   if(ready&&changed)reinterpret_cast<void(__thiscall*)(void*)>(0x5051D0)(solid);
   if(MWELNeoLoggingEnabled()){static std::set<unsigned> observed;if(observed.insert(hash).second){for(unsigned i=0;i<solid[0x19];++i){auto info=reinterpret_cast<void*(__cdecl*)(unsigned,int,int)>(0x503400)(table[i*2],0,0);char text[180];sprintf_s(text,"EVENT_WALL_TEXTURE model=%08X hash=%08X lookup=%08X bound=%08X ready=%u",hash,table[i*2],unsigned(info),table[i*2+1],unsigned(ready));MWELNeoLog(text);}}}
  }
 }
 unsigned drawn=0,pending=0;
 // These translucent, emissive race markers do not belong in the native
 // shadow view (15). Limit the other views to the game's nearby race scene;
 // a 650 m sphere submitted distant markers hidden behind streets/tunnels.
 for(auto* wall:visible){auto& model=course->models.at(wall->model);if(!model.solid){++pending;continue;}
  reinterpret_cast<void(__thiscall*)(void*,NativeModel*,D3DMATRIX*,void*,int,void*)>(0x6DA9B0)(view,&model,&wall->matrix,nullptr,0,nullptr);++drawn;
 }
 if(MWELNeoLoggingEnabled()){static DWORD logged=0;auto now=GetTickCount();if(id==1&&now-logged>3000){logged=now;char text[200];sprintf_s(text,"EVENT_PACKAGE_NATIVE_WALL_DRAW key=%08X walls=%u draws=%u unresolved_models=%u native_material_animation=1 private_textures=0 owner=EventASI",course->key,unsigned(course->walls.size()),drawn,pending);MWELNeoLog(text);}}
}
// Vanilla hosts retain their native CARP, scenery and barrier allocator.
struct Cache {void* first=nullptr;void* end=nullptr;unsigned capacity=0;std::vector<mwelscene::Barrier> combined;};
std::map<void*,Cache> caches;
void(__thiscall* originalLists)(void*,unsigned)=nullptr;void(__thiscall* originalReserve)(void*,unsigned)=nullptr;void(__thiscall* originalReset)(void*,unsigned)=nullptr;void(__thiscall* originalClear)(void*)=nullptr;
void Detach(void* self){auto found=caches.find(self);if(found==caches.end())return;auto data=static_cast<BYTE*>(self);auto& c=found->second;if(*reinterpret_cast<void**>(data+80)==c.combined.data()){*reinterpret_cast<void**>(data+80)=c.first;*reinterpret_cast<void**>(data+84)=c.end;*reinterpret_cast<unsigned*>(data+88)=c.capacity;}}
void __fastcall Reserve(void* self,void*,unsigned flags){Detach(self);originalReserve(self,flags);}
void __fastcall Reset(void* self,void*,unsigned flags){Detach(self);originalReset(self,flags);}
void __fastcall Clear(void* self,void*){auto owner=static_cast<BYTE*>(self)-76;Detach(owner);originalClear(self);caches.erase(owner);}
void __fastcall Lists(void* self,void*,unsigned flags){
 Detach(self);originalLists(self,flags);MWELNeoCompleteGroundLists(self,flags);if(!(flags&4))return;auto data=static_cast<BYTE*>(self);auto first=*reinterpret_cast<mwelscene::Barrier**>(data+80),end=*reinterpret_cast<mwelscene::Barrier**>(data+84);
 if((!first&&end)||uintptr_t(end)<uintptr_t(first)||(uintptr_t(end)-uintptr_t(first))%sizeof(*first)||(uintptr_t(end)-uintptr_t(first))/sizeof(*first)>100000)return;
 mwelscene::Point center{};float radius=0;memcpy(&center,data+32,12);memcpy(&radius,data+44,4);if(!std::isfinite(radius))return;
 auto course=Find();if(!course)return;
 const auto query=mw_race_barriers::ForCollision(self);
 if(query.driver==1||query.driver==2)return;
 // Collision lists are queried many times per physics frame. Reuse storage
 // instead of zeroing an 80 KiB stack array on every query.
 thread_local std::vector<mwelscene::Barrier> extra;extra.clear();
 ForEachContact(*course,center,std::clamp(radius+5,15.f,100.f),[&](const auto& wall){extra.push_back(wall);});
 if(extra.empty())return;
 unsigned added=0;
 auto& c=caches[self];c.first=first;c.end=end;c.capacity=*reinterpret_cast<unsigned*>(data+88);c.combined.clear();
 const auto baseCount=first?size_t(end-first):0;c.combined.reserve(baseCount+extra.size()*2);if(first)c.combined.insert(c.combined.end(),first,end);
 for(const auto& wall:extra){if(!mw_race_barriers::Collides(wall.flags,query))continue;++added;c.combined.push_back(wall);c.combined.push_back({{wall.b.x,wall.a.y,wall.b.z},wall.flags,{wall.a.x,wall.b.y,wall.a.z},wall.inverseLength,{0,0}});}
 static unsigned seen=0;if(query.driver<7&&!(seen&(1u<<query.driver))){seen|=1u<<query.driver;char report[220];sprintf_s(report,"EVENT_WALL_COLLISION_POLICY driver=%u world_id=%08X exclusions=%08X guards=%u collidable=%u native_world_preserved=1",query.driver,query.worldId,query.exclusions,unsigned(extra.size()),added);MWELNeoLog(report);}
 if(!added)return;
 *reinterpret_cast<void**>(data+80)=c.combined.data();*reinterpret_cast<void**>(data+84)=c.combined.data()+c.combined.size();
}
void(__thiscall* originalDraw)(void*,void*,void*)=nullptr;
void __fastcall NativeDraw(void* self,void*,void* view,void* flags){DrawUnsafe(view);originalDraw(self,view,flags);}
bool InstallNative(){
 struct Hook {unsigned address;void* detour;void** original;const char* guard;unsigned size;};
 const Hook hooks[]={{0x789320,Lists,reinterpret_cast<void**>(&originalLists),"\x83\xEC\x08\x53\x8A\x5C\x24\x10\xF6",9},{0x77C6E0,Clear,reinterpret_cast<void**>(&originalClear),"\x56\x8B\xF1\x57\x8B\x7E\x04\x85\xFF",9},{0x781670,Reserve,reinterpret_cast<void**>(&originalReserve),"\x53\x8A\x5C\x24\x08\xF6\xC3\x08\x56",9},{0x782AB0,Reset,reinterpret_cast<void**>(&originalReset),"\x53\x8A\x5C\x24\x08\xF6\xC3\x08\x56",9},{0x723FA0,NativeDraw,reinterpret_cast<void**>(&originalDraw),"\x83\xEC\x14\x53\x8B\x5C\x24\x20\x56",9}};
 for(const auto& hook:hooks)if(memcmp(reinterpret_cast<void*>(hook.address),hook.guard,hook.size))return false;
 std::vector<void*> installed;auto rollback=[&](){for(auto site:installed){MH_DisableHook(site);MH_RemoveHook(site);}return false;};
 for(const auto& hook:hooks){auto site=reinterpret_cast<void*>(hook.address);if(MH_CreateHook(site,hook.detour,hook.original)!=MH_OK)return rollback();installed.push_back(site);}
 for(auto site:installed)if(MH_QueueEnableHook(site)!=MH_OK)return rollback();if(MH_ApplyQueued()!=MH_OK)return rollback();
 nativeInstalled=true;MWELNeoLog("EVENT_PACKAGE_WALL_NATIVE_BRIDGE native_floor_and_walls_preserved=1 owner=EventASI");return true;
}
}
bool MWELPrepareBarriers(const std::vector<mwel::EventPackage>& packages,bool production){
 if(!production)return true;using namespace event_barriers;
 const BYTE findGuard[]={0x8B,0x44,0x24,0x04,0x6A,0x00,0x50};const BYTE renderGuard[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x81,0xEC};
 if(memcmp(reinterpret_cast<void*>(0x505340),findGuard,sizeof(findGuard))||memcmp(reinterpret_cast<void*>(0x6DA9B0),renderGuard,sizeof(renderGuard)))throw std::runtime_error("Native wall renderer ABI mismatch");
 struct Source {mwel::fs::path directory;unsigned key;bool hasModels;};
 std::vector<Source> sources;sources.reserve(packages.size());
 for(const auto& package:packages)sources.push_back({package.directory,mwel::Correction(package.authoring)?std::stoul(package.authoring.at("correctionTarget").at("hash").get<std::string>(),nullptr,16):Hash(package.nativeNamespace),package.assets.contains("course/wall-models.json")});
 // This task touches package bytes and CPU-only geometry. It neither invokes
 // the game nor registers graphics resources. The database hook joins it
 // before registration can inspect grid/barrier state.
 pendingCourses=std::async(std::launch::async,[sources=std::move(sources)](){
 std::vector<Course> prepared;prepared.reserve(sources.size());
 for(const auto& source:sources){Course course{};course.key=source.key;
  auto raw=mwel::Read(source.directory/L"course"/L"walls.wmw",128*1024*1024);size_t cursor=0;auto read=[&](void* destination,size_t n){if(n>raw.size()-cursor)throw std::runtime_error("Truncated package wall geometry");memcpy(destination,raw.data()+cursor,n);cursor+=n;};
  unsigned header[3]{};read(header,12);if(header[0]!=0x31434557||header[1]!=course.key||header[2]>2048)throw std::runtime_error("Package wall identity/count");
  for(unsigned i=0;i<header[2];++i){Wall wall{};unsigned bytes=0;read(&bytes,4);read(&wall.header,sizeof(Header));const auto& h=wall.header;
   if(h.magic!=w2mw::Magic||h.version!=w2mw::Version||h.meshes>16||h.vertices>1024||h.indices>4096||h.indices%3||bytes!=sizeof(Header)+sizeof(Mesh)*h.meshes+sizeof(Vertex)*h.vertices+4*h.indices)throw std::runtime_error("Invalid package wall mesh");
   wall.meshes.resize(h.meshes);wall.vertices.resize(h.vertices);wall.indices.resize(h.indices);read(wall.meshes.data(),wall.meshes.size()*sizeof(Mesh));read(wall.vertices.data(),wall.vertices.size()*sizeof(Vertex));read(wall.indices.data(),wall.indices.size()*4);
   for(const auto& m:wall.meshes){if(uint64_t(m.firstVertex)+m.vertexCount>h.vertices||uint64_t(m.firstIndex)+m.indexCount>h.indices||m.indexCount%3)throw std::runtime_error("Wall mesh range");for(unsigned j=0;j<m.indexCount;++j)if(wall.indices[m.firstIndex+j]>=m.vertexCount)throw std::runtime_error("Wall vertex index");}
   float longest=-1;Vec3 a{},b{};for(const auto& v:wall.vertices){for(float n:{v.position.x,v.position.y,v.position.z})if(!std::isfinite(n)||fabs(n)>100000)throw std::runtime_error("Wall vertex invalid");for(const auto& w:wall.vertices){float dx=v.position.x-w.position.x,dy=v.position.y-w.position.y,d=dx*dx+dy*dy;if(d>longest){longest=d;a=v.position;b=w.position;}}}
   if(!std::isfinite(h.minimum.z)||!std::isfinite(h.maximum.z)||h.minimum.z>h.maximum.z)throw std::runtime_error("Wall height invalid");
   wall.contact={{-a.y,h.minimum.z,a.x},512,{-b.y,h.maximum.z,b.x},longest>.001f?1/std::sqrt(longest):0,{0,0}};course.walls.push_back(std::move(wall));
  }
  if(cursor!=raw.size())throw std::runtime_error("Trailing wall payload");
  if(!source.hasModels)throw std::runtime_error("Native wall model references absent; export with current editor");
  auto models=mwel::Json::parse(mwel::Read(source.directory/L"course"/L"wall-models.json",4*1024*1024));
  if(models.at("schema")!=1||std::stoul(models.at("eventHash").get<std::string>(),nullptr,16)!=course.key||!models.at("walls").is_array()||models.at("walls").size()!=course.walls.size())throw std::runtime_error("Native wall model identity/count");
  for(size_t i=0;i<course.walls.size();++i){auto& wall=course.walls[i];const auto& row=models.at("walls").at(i);const auto text=row.at("model").get<std::string>();if(text.size()!=8||text.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::runtime_error("Invalid native wall model hash");wall.model=std::stoul(text,nullptr,16);if(!wall.model||!row.at("matrix").is_array()||row.at("matrix").size()!=16)throw std::runtime_error("Invalid native wall model/matrix");
   auto values=reinterpret_cast<float*>(&wall.matrix);for(unsigned j=0;j<16;++j){values[j]=row.at("matrix").at(j).get<float>();if(!std::isfinite(values[j])||fabs(values[j])>100000)throw std::runtime_error("Invalid native wall transform");}
   if(values[3]!=0||values[7]!=0||values[11]!=0||values[15]!=1)throw std::runtime_error("Native wall transform is not affine");
   auto& native=course.models[wall.model];native.hash=wall.model;
  }
  course.contactIndex.Build(course.walls.size(),[&](unsigned i){const auto& b=course.walls[i].contact;return mw_barrier_index::Bounds{std::min(b.a.x,b.b.x),std::min(b.a.z,b.b.z),std::max(b.a.x,b.b.x),std::max(b.a.z,b.b.z)};});
  prepared.push_back(std::move(course));
 }return prepared;
 });pendingStarted=true;return true;
}
bool MWELCompleteBarriers(){
 using namespace event_barriers;
 if(active)return true;
 if(!pendingStarted||!pendingCourses.valid())return false;
 auto prepared=pendingCourses.get();
 for(const auto& course:prepared){if(MWELNeoLoggingEnabled()){char report[160];sprintf_s(report,"EVENT_PACKAGE_WALLS_PREPARED key=%08X walls=%u owner=EventASI CourseOverrides_written=0",course.key,unsigned(course.walls.size()));MWELNeoLog(report);}}
 courses=std::move(prepared);pendingStarted=false;active=true;return true;
}
bool MWELBarrierSegments(unsigned key,std::vector<MWELBarrierSegment>& output){
 using namespace event_barriers;
 if(!active)return false;
 for(const auto& course:courses)if(course.key==key){output.clear();output.reserve(course.walls.size());
  for(const auto& wall:course.walls){const auto& b=wall.contact;output.push_back({b.a.x,b.a.y,b.a.z,b.b.x,b.b.y,b.b.z});}
  return true;
 }return false;
}
bool MWELBarrierFrame(){
 if(!event_barriers::active)return true;
 unsigned state=0;if(!event_barriers::Read(reinterpret_cast<void*>(0x925E90),&state,4))return true;
 if(state!=6){event_barriers::solidsAttempted=false;return true;}
 if(!event_barriers::solidsAttempted){
  event_barriers::solidsAttempted=true;std::set<unsigned> missing;for(const auto& course:event_barriers::courses)for(const auto& model:course.models)if(!reinterpret_cast<void*(__cdecl*)(unsigned)>(0x505340)(model.first))missing.insert(model.first);
  try{if(!missing.empty()&&!native_wall_solids::Load(missing)){MWELNeoLog("EVENT_NATIVE_WALL_RESOURCE_REFUSED missing_installed_model=1");return false;}MWELNeoLog("EVENT_NATIVE_WALL_RESOURCES_REGISTERED source=current_game byte_preserved=1 native_loader=005075A0 private_DDS=0");}
  catch(const std::exception& error){MWELNeoLog(error.what());return false;}
 }
 if(event_barriers::nativeInstalled)return true;
 auto world=GetModuleHandleW(L"World2MWNativeLoader.asi");using Active=unsigned(__cdecl*)();auto active=world?reinterpret_cast<Active>(GetProcAddress(world,"W2MW_IsWorldActiveV1")):nullptr;
 if(active&&active())return true;return event_barriers::InstallNative();
}
#pragma comment(linker,"/EXPORT:MWEL_BarrierContactsV1=_MWEL_BarrierContactsV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_BarrierContactsV1(const mwelscene::Point* center,float radius,mwelscene::Barrier* output,unsigned capacity){__try{return event_barriers::Contacts(center,radius,output,capacity);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
#pragma comment(linker,"/EXPORT:MWEL_DrawEventV1=_MWEL_DrawEventV1")
extern "C" __declspec(dllexport) void __cdecl MWEL_DrawEventV1(void* view){__try{event_barriers::DrawUnsafe(view);}__except(EXCEPTION_EXECUTE_HANDLER){MWELNeoLog("EVENT_PACKAGE_WALL_DRAW_FAULT");}}
