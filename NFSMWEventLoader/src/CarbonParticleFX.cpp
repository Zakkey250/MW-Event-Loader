#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "CarbonParticleFX.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace carbon_fx {
using U=std::uint32_t;
constexpr U EmitterClass=0xB30B18AF,GroupClass=0xABA86E60;
struct Node{U key,value,flags;};
struct Collection{Node* nodes;U capacity,count;unsigned short worst,keyShift;Collection* parent;void* klass;void* layout;U references,key;void* source;const char** name;};
static_assert(sizeof(Collection)==44&&sizeof(Node)==12);
struct Memory{BYTE* data=nullptr;~Memory(){if(data)VirtualFree(data,0,MEM_RELEASE);}bool Create(std::size_t size){data=static_cast<BYTE*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));return data!=nullptr;}};
struct Owned{Collection value{};Node empty{};Memory layout;std::string name;const char* namePointer=nullptr;};
struct Patch{unsigned short offset,size;std::array<BYTE,16> bytes{};};
struct Definition{U texture=0;std::vector<Patch> patches;};
struct Anchor{alignas(16) std::array<BYTE,80> record{};};
// Collections can remain referenced by the native attribute cache for process life.
static auto& owned=*new std::array<std::unique_ptr<Owned>,4>;
static std::array<Anchor,2> anchors;
static MWELCarbonNativeCollection original=nullptr;
static bool collectionsReady=false,sceneReady=false;
static std::string status="inactive";
static bool diagnostics=false;static MWELCarbonParticleStats stats{};
static bool Read(const void* from,void* out,std::size_t size){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),from,out,size,&got)!=FALSE&&got==size;}
template<class T>static bool ReadFile(std::ifstream& stream,T* value,std::size_t count=1){return bool(stream.read(reinterpret_cast<char*>(value),sizeof(T)*count));}
static U Hash(const char* value){return reinterpret_cast<U(__cdecl*)(const char*)>(0x4519D0)(value);}
static bool Bytes(U address,const BYTE* expected,unsigned size){BYTE bytes[16]{};return size<=sizeof(bytes)&&Read(reinterpret_cast<void*>(address),bytes,size)&&memcmp(bytes,expected,size)==0;}
static bool Guards(){
 const BYTE create[]={0xA0,0xEC,0x86,0x8F,0,0x84,0xC0,0x56,0x8B,0xF1};
 const BYTE matrix[]={0x53,0x8B,0x5C,0x24,8,0x56,0x57,0x8B,0xF1};
 const BYTE destroy[]={0x6A,0xFF,0x68,0x2B,0xDF,0x86,0};
 const BYTE freeGroup[]={0x8B,0x44,0x24,4,0x8B,0x48,0x18,0x8B,0x54,0x24,8,0x56};
 const BYTE clear[]={0x8B,0x44,0x24,4,0xC7,0x40,0xC,0,0,0,0,0xC3};
 return Bytes(0x50CC20,create,sizeof(create))&&Bytes(0x503BC0,matrix,sizeof(matrix))&&Bytes(0x5094C0,destroy,sizeof(destroy))&&Bytes(0x463CA0,freeGroup,sizeof(freeGroup))&&Bytes(0x4FDFD0,clear,sizeof(clear));
}
static bool Allowed(unsigned offset,unsigned size){
 if(offset<=0x80&&offset%16==0&&offset!=0x20)return size==16;
 if(offset==0x90||offset==0x98)return size==8;
 if(offset>=0xA4&&offset<=0x10C&&offset%4==0)return size==4;
 return offset>=0x112&&offset<=0x114&&size==1;
}
static bool ValuesValid(const Patch& p){
 if(p.offset<=0x80){for(unsigned i=0;i<4;++i){float value=0;memcpy(&value,p.bytes.data()+i*4,4);if(!std::isfinite(value)||fabs(value)>1000000)return false;}return true;}
 if(p.offset>=0x112)return p.bytes[0]<=1;
 if(p.offset==0x90){U kind=0;memcpy(&kind,p.bytes.data(),4);return kind<=4&&p.bytes[5]<=1;}
 if(p.offset==0x98||p.offset==0xAC||p.offset==0xB0||p.offset==0xB4||p.offset==0xE4||p.offset==0xC8||p.offset==0xE0||p.offset==0xFC||p.offset==0x10C)return true;
 float value=0;memcpy(&value,p.bytes.data(),4);return std::isfinite(value)&&fabs(value)<=1000000;
}
static bool Definitions(const char* path,std::array<Definition,3>& definitions){
 std::ifstream stream(path,std::ios::binary);U h[4]{};
 if(!ReadFile(stream,h,4)||h[0]!=0x58465443||h[1]!=1||h[2]!=3||h[3]!=0x118)return false;
 constexpr U textures[]={2172051775u,3930962155u,2191626924u};
 for(unsigned i=0;i<3;++i){U count=0;if(!ReadFile(stream,&definitions[i].texture)||definitions[i].texture!=textures[i]||!ReadFile(stream,&count)||count!=40)return false;
  std::array<bool,0x118> used{};for(unsigned n=0;n<count;++n){Patch patch;if(!ReadFile(stream,&patch.offset)||!ReadFile(stream,&patch.size)||!Allowed(patch.offset,patch.size)||unsigned(patch.offset)+patch.size>used.size()||!ReadFile(stream,patch.bytes.data(),patch.size)||!ValuesValid(patch))return false;
   for(unsigned b=patch.offset;b<unsigned(patch.offset)+patch.size;++b){if(used[b])return false;used[b]=true;}definitions[i].patches.push_back(patch);
  }
 }
 char extra;return !stream.read(&extra,1);
}
static std::unique_ptr<Owned> Clone(U classKey,const char* templateName,const char* name,unsigned layoutBytes){
 auto source=static_cast<Collection*>(original(classKey,Hash(templateName)));Collection current{};U sourceClass=0;
 if(!source||!Read(source,&current,sizeof(current))||!current.klass||!Read(current.klass,&sourceClass,4)||sourceClass!=classKey||!current.layout)return {};
 auto result=std::make_unique<Owned>();result->name=name;result->namePointer=result->name.c_str();result->value=current;
 if(!result->layout.Create(layoutBytes)||!Read(current.layout,result->layout.data,layoutBytes))return {};
 result->value.key=Hash(name);if(original(classKey,result->value.key))return {};
 result->value.parent=source;result->value.references=1;result->value.layout=result->layout.data;result->value.name=&result->namePointer;
 result->empty.value=U(&result->empty);result->value.nodes=&result->empty;result->value.capacity=1;result->value.count=0;result->value.keyShift=0;result->value.worst=0;
 return result;
}
static bool InitCollections(const char* path,MWELCarbonNativeCollection getter){
 if(collectionsReady)return true;if(!getter||!Guards()){status="native particle function guard failed";return false;}original=getter;
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&MWELCarbonParticlePrepare),&pinned)){status="particle module pin failed";return false;}
 std::array<Definition,3> definitions;if(!Definitions(path,definitions)){status="native torch definitions invalid";return false;}
 constexpr const char* names[]={"mwel_carbon_torch_core","mwel_carbon_torch_fire","mwel_carbon_torch_smoke"};
 constexpr const char* textureTemplates[]={"emcar_coplightred","fxgame_flare_sparks","fxgame_flare_smoke"};
 std::array<std::unique_ptr<Owned>,4> staged;
 for(unsigned i=0;i<3;++i){
  staged[i]=Clone(EmitterClass,"default",names[i],0x118);auto textureSource=static_cast<Collection*>(original(EmitterClass,Hash(textureTemplates[i])));Collection textureCollection{};U textureRecord[2]{};
  if(!staged[i]||!textureSource||!Read(textureSource,&textureCollection,sizeof(textureCollection))||!textureCollection.layout||!Read(static_cast<BYTE*>(textureCollection.layout)+0x98,textureRecord,8)||textureRecord[0]!=definitions[i].texture||textureRecord[1]>=512){status="native emitter template or texture identity mismatch";return false;}
  for(const auto& p:definitions[i].patches)memcpy(staged[i]->layout.data+p.offset,p.bytes.data(),p.size);
  memcpy(staged[i]->layout.data+0x98,textureRecord,8);auto namePointer=staged[i]->namePointer;memcpy(staged[i]->layout.data+0xA0,&namePointer,4);
 }
 staged[3]=Clone(GroupClass,"fxgame_flare_red","mwel_carbon_fxnis_girltorch",0x88);
 if(!staged[3]){status="native emitter-group template unavailable";return false;}
 auto header=reinterpret_cast<unsigned short*>(staged[3]->layout.data);
 if(header[0]<3||header[0]>6||header[1]>header[0]||header[2]!=12||(header[3]&0x8000)){status="native emitter-group inline array layout mismatch";return false;}
 header[1]=3;memset(staged[3]->layout.data+8,0,72);
 for(unsigned i=0;i<3;++i){U ref[]={EmitterClass,staged[i]->value.key,0};memcpy(staged[3]->layout.data+8+i*12,ref,12);}
 auto namePointer=staged[3]->namePointer;memcpy(staged[3]->layout.data+0x80,&namePointer,4);const float farClip=200;memcpy(staged[3]->layout.data+0x84,&farClip,4);
 owned=std::move(staged);collectionsReady=true;status="native-compatible source definitions ready; Carbon-only variance/rotation not reproduced";return true;
}
static BYTE* Group(Anchor& a){BYTE* group=nullptr;memcpy(&group,a.record.data()+12,4);return group;}
static bool OwnedGroup(Anchor& a,BYTE* group){void* owner=nullptr;U callback=0;return group&&Read(group+0x60,&owner,4)&&owner==a.record.data()&&Read(group+0x6C,&callback,4)&&callback==0x4FDFD0;}
static bool Create(Anchor& a,const float* matrix){
 unsigned groups=0,emitters=0;BYTE enabled=0;void* pool=nullptr;
 if(!Read(reinterpret_cast<void*>(0x8F86EC),&enabled,1)||!enabled||!Read(reinterpret_cast<void*>(0x916068),&pool,4)||!pool||!Read(reinterpret_cast<void*>(0x916A50),&groups,4)||groups>136||!Read(reinterpret_cast<void*>(0x916A54),&emitters,4)||emitters>400){if(diagnostics)++stats.createRefused;status="native particles disabled, allocator unavailable or safety budget exceeded";return false;}
 a.record.fill(0);U key=owned[3]->value.key;memcpy(a.record.data(),&key,4);const unsigned short section=2600;memcpy(a.record.data()+8,&section,2);memcpy(a.record.data()+16,matrix,64);
 // Direct stock entry avoids W2C's ambient chunk interceptor. No new hook.
 auto group=static_cast<BYTE*>(reinterpret_cast<void*(__thiscall*)(void*,void*,U)>(0x50CC20)(reinterpret_cast<void*>(0x9166C0),a.record.data(),0x08000000));if(!group){if(diagnostics)++stats.createRefused;status="native particle allocation refused";return false;}
 auto owner=a.record.data();U callback=0x4FDFD0;memcpy(group+0x60,&owner,4);memcpy(group+0x6C,&callback,4);memcpy(a.record.data()+12,&group,4);
 reinterpret_cast<void(__thiscall*)(void*,const float*)>(0x503BC0)(group,matrix);U flags=0;memcpy(&flags,group+0x18,4);flags|=1;memcpy(group+0x18,&flags,4);if(diagnostics)++stats.created;return true;
}
}

void* MWELCarbonEffectCollection(std::uint32_t classKey,std::uint32_t key){
 using namespace carbon_fx;if(!collectionsReady||(classKey!=GroupClass&&classKey!=EmitterClass))return nullptr;for(unsigned i=0;i<owned.size();++i)if(classKey==(i==3?GroupClass:EmitterClass)&&owned[i]->value.key==key)return &owned[i]->value;return nullptr;
}
void MWELCarbonParticleRelease(){
 using namespace carbon_fx;
 for(auto& a:anchors){auto group=Group(a);if(!group)continue;if(!OwnedGroup(a,group)||!Guards()){status="owned native particle release refused";if(diagnostics)++stats.releaseRefused;continue;}
  void* pool=nullptr;if(!Read(reinterpret_cast<void*>(0x916068),&pool,4)||!pool){status="native particle allocator unavailable";if(diagnostics)++stats.releaseRefused;continue;}
  reinterpret_cast<void(__thiscall*)(void*)>(0x5094C0)(group);
  // Native destruction invokes clear callback before the arena frees memory.
  if(Group(a)){status="native particle destructor did not clear owner";if(diagnostics)++stats.releaseRefused;continue;}
  reinterpret_cast<void(__cdecl*)(void*,void*)>(0x463CA0)(pool,group);if(diagnostics)++stats.destroyed;
 }
 sceneReady=false;
}
bool MWELCarbonParticlePrepare(const char* definitions,MWELCarbonNativeCollection getter){
 using namespace carbon_fx;if(!definitions)return false;
 try{return InitCollections(definitions,getter);}catch(...){status="FX OOM";return false;}
}
bool MWELCarbonParticleBegin(){
 using namespace carbon_fx;MWELCarbonParticleRelease();
 for(auto& a:anchors)if(Group(a))return false;
 sceneReady=collectionsReady;return sceneReady;
}
bool MWELCarbonParticleUpdate(const float positions[2][3]){
 using namespace carbon_fx;if(!sceneReady||!positions)return false;
 for(unsigned i=0;i<2;++i)for(unsigned j=0;j<3;++j)if(!std::isfinite(positions[i][j])||fabs(positions[i][j])>1000000)return false;
 bool ready=true;
 if(diagnostics)memcpy(stats.requested,positions,sizeof(stats.requested));
 for(unsigned i=0;i<2;++i){auto& a=anchors[i];alignas(16) float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,positions[i][0],positions[i][1],positions[i][2],1};
  auto group=Group(a);if(group){if(!OwnedGroup(a,group)){status="native particle ownership changed";ready=false;continue;}reinterpret_cast<void(__thiscall*)(void*,const float*)>(0x503BC0)(group,matrix);}else ready=Create(a,matrix)&&ready;
 }
 return ready;
}
const char* MWELCarbonParticleStatus(){return carbon_fx::status.c_str();}

void MWELCarbonParticleDiagnosticMode(bool enabled){carbon_fx::diagnostics=enabled;}
MWELCarbonParticleStats MWELCarbonParticleDiagnostics(){
 using namespace carbon_fx;if(!diagnostics)return {};auto result=stats;
 for(unsigned i=0;i<anchors.size();++i)if(auto group=Group(anchors[i])){++result.live;
  if(Read(group+0x50,result.native[i],12))for(unsigned axis=0;axis<3;++axis)result.positionError=std::max(result.positionError,std::abs(result.native[i][axis]-result.requested[i][axis]));
 }
 Read(reinterpret_cast<void*>(0x916A50),&result.nativeGroups,4);Read(reinterpret_cast<void*>(0x916A54),&result.nativeEmitters,4);return result;
}
