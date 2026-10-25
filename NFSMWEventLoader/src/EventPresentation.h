#pragma once
#include "CombatUiAsset.h"
#include "GauntletUiAsset.h"
#include "HotPursuitUiAsset.h"
#include "EventPresentationPolicy.h"
namespace event_presentation {
using Package=mwel::EventPackage;
thread_local const Package* pending=nullptr;
void* resource=nullptr;void* gauntletResource=nullptr;void* hotPursuitResource=nullptr;
bool needed=false,armsNeeded=false,gauntletNeeded=false,hotPursuitNeeded=false;
bool Supported(const Package& p){return event_presentation_policy::Supported(p.authoring.value("combatMode",std::string("none")));}
std::map<std::string,unsigned> typeKeys;
unsigned Hash(const std::string& text){unsigned h=~0u;for(unsigned char c:text)h=h*33+c;return h;}
const Package* PackageFor(void* parameters){
 // Frontend indexed parameters can have no materialized attribute record.
 // GetEventId handles both the native index and the lazily loaded record.
 // Never infer an owned event from its underlying Sprint/Circuit race type.
 const auto token=reinterpret_cast<const char*(__thiscall*)(void*)>(0x5FBA70)(parameters);
 std::string eventId;
 if(token)for(unsigned i=0;i<64;++i){char c=0;if(!Copy(token+i,&c,1))return nullptr;if(!c)break;if(i==63)return nullptr;eventId+=c;}
 if(!eventId.empty())for(const auto& p:packages)if(p.nativeEventId==eventId&&Supported(p))return &p;
 auto read=[](const void* p,unsigned offset){unsigned out=0;if(p)Copy(static_cast<const BYTE*>(p)+offset,&out,4);return out;};
 unsigned record=read(parameters,8),collection=read(reinterpret_cast<void*>(record),4);unsigned seen[16]{};
 for(unsigned i=0;collection&&i<16;++i){for(unsigned j=0;j<i;++j)if(seen[j]==collection)return nullptr;seen[i]=collection;
  const auto key=read(reinterpret_cast<void*>(collection),0x20);auto clone=registration_experiment::Find(key);
  if(clone&&clone->race)for(const auto& p:packages)if(p.nativeNamespace==clone->name&&Supported(p))return &p;
  collection=read(reinterpret_cast<void*>(collection),0x10);
 }return nullptr;
}
std::string Type(const Package& p){return p.authoring.value("combatMode",std::string("none"))+":"+p.authoring.at("mode").get<std::string>();}
unsigned TypeKey(const Package* p,unsigned fallback){if(!p)return fallback;auto found=typeKeys.find(Type(*p));return found==typeKeys.end()?fallback:found->second;}
unsigned Icon(const Package* p,unsigned fallback){
 if(!p)return fallback;
 const auto mode=p->authoring.value("combatMode",std::string("none"));
 unsigned texture=0;void* loaded=nullptr;
 if(mode=="armsrace"){texture=combat_ui_asset::Texture;loaded=resource;}
 else if(mode=="gauntlet"){texture=gauntlet_ui_asset::Texture;loaded=gauntletResource;}
 else if(mode=="hotpursuit"){texture=hot_pursuit_ui_asset::Texture;loaded=hotPursuitResource;}
 if(!loaded)return fallback;
 return reinterpret_cast<void*(__cdecl*)(unsigned,int,int)>(0x503400)(texture,0,0)?texture:fallback;
}
int __fastcall Capture(void* parameters,void*){
 const auto type=reinterpret_cast<int(__thiscall*)(void*)>(0x5FAA20)(parameters);
 pending=PackageFor(parameters);
 if(pending&&worldLogEnabled){static std::set<std::string> reported;if(reported.insert(pending->id).second)Log("EVENT_COMBAT_PRESENTATION_CONTEXT id=%s native_type=%d indexed_or_materialized=1",pending->id.c_str(),type);}
 return type;
}
unsigned __stdcall TypeName(unsigned type){
 const auto p=pending;pending=nullptr;
 const auto native=reinterpret_cast<unsigned(__thiscall*)(void*,unsigned)>(0x56E010)(*reinterpret_cast<void**>(0x91CF90),type);
 return TypeKey(p,native);
}
unsigned __stdcall TypeIcon(unsigned type){
 const auto p=pending;pending=nullptr;
 const auto native=reinterpret_cast<unsigned(__thiscall*)(void*,unsigned)>(0x56E090)(*reinterpret_cast<void**>(0x91CF90),type);
 return Icon(p,native);
}
// Two career panels select type keys inline instead of using the getters.
// Their own GetRaceType call captures context; consume it at the type label.
void __cdecl CardName(const char* packageName,unsigned object,unsigned key){
 const auto p=pending;pending=nullptr;
 if(const auto icon=Icon(p,0))reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x527EE0)(packageName,0xCEB82625,icon);
 reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x525220)(packageName,object,TypeKey(p,key));
}
void LoadResource(bool required,void*& loaded,const char* digest,const unsigned char* embedded,size_t size,unsigned texture,const char* mode){
 if(!required||loaded)return;
 try{
  const auto relative=mwel::fs::path("scripts")/"EventLoader"/"UI"/(std::string("icons-")+digest+".bin");
  const auto path=game/relative;const std::string bytes(reinterpret_cast<const char*>(embedded),size);
  mwel::fs::create_directories(path.parent_path());
  if(mwel::fs::exists(path)){if(mwel::Read(path,1024*1024)!=bytes)throw std::runtime_error("Owned icon cache content mismatch");}
  else mwel::AtomicWrite(path,bytes);
  loaded=reinterpret_cast<void*(__cdecl*)(const char*,int,int,int,int)>(0x65FD30)(relative.string().c_str(),1,0,0,0);
  if(loaded)reinterpret_cast<int(__thiscall*)(void*,int,int)>(0x6616F0)(loaded,0,0);
  Log("EVENT_COMBAT_UI_RESOURCE mode=%s queued=%u texture=%08X embedded_asset=1 native_banks_modified=0",mode,unsigned(loaded!=nullptr),texture);
 }catch(const std::exception& e){Log("EVENT_COMBAT_UI_RESOURCE_FAILED mode=%s reason=%s",mode,e.what());}
}
void Load(){
 if(!needed)return;
 LoadResource(armsNeeded,resource,combat_ui_asset::Digest,combat_ui_asset::Bytes,sizeof(combat_ui_asset::Bytes),combat_ui_asset::Texture,"armsrace");
 LoadResource(gauntletNeeded,gauntletResource,gauntlet_ui_asset::Digest,gauntlet_ui_asset::Bytes,sizeof(gauntlet_ui_asset::Bytes),gauntlet_ui_asset::Texture,"gauntlet");
 LoadResource(hotPursuitNeeded,hotPursuitResource,hot_pursuit_ui_asset::Digest,hot_pursuit_ui_asset::Bytes,sizeof(hot_pursuit_ui_asset::Bytes),hot_pursuit_ui_asset::Texture,"hotpursuit");
}
bool Install(){
 for(const auto& p:packages)if(Supported(p)){
  const auto type=Type(p);if(typeKeys.count(type))continue;
  const auto mode=p.authoring.at("mode").get<std::string>();
  const auto combat=p.authoring.value("combatMode",std::string("none"));
  const auto en=event_presentation_policy::Name(combat,mode,false),ja=event_presentation_policy::Name(combat,mode,true);
  if(en.empty()||ja.empty())continue;
  if(combat=="gauntlet")gauntletNeeded=true;
  else if(combat=="hotpursuit")hotPursuitNeeded=true;
  else armsNeeded=true;
  const unsigned key=Hash("MWEL_COMBAT_TYPE_"+type);
  if(!event_titles::titles.emplace(key,event_titles::Title{en,type,{},ja}).second)return false;
  typeKeys.emplace(type,key);needed=true;
 }
 if(!needed)return true;
 struct Call {unsigned site,target;void* hook;BYTE old[5];};
 Call calls[]={
  {0x51F185,0x5FAA20,Capture,{}},{0x51F191,0x56E010,TypeName,{}},
  {0x51F19A,0x5FAA20,Capture,{}},{0x51F1A6,0x56E090,TypeIcon,{}},
  {0x52F966,0x5FAA20,Capture,{}},{0x52F972,0x56E090,TypeIcon,{}},
  {0x52F9B9,0x5FAA20,Capture,{}},{0x52F9C5,0x56E010,TypeName,{}},
  {0x535E85,0x5FAA20,Capture,{}},{0x535EE4,0x525220,CardName,{}},
  {0x53955D,0x5FAA20,Capture,{}},{0x5395F2,0x525220,CardName,{}},
  {0x547BD9,0x5FAA20,Capture,{}},{0x547BE5,0x56E090,TypeIcon,{}},
  {0x57BDBC,0x5FAA20,Capture,{}},{0x57BDC8,0x56E090,TypeIcon,{}},
  {0x597E04,0x5FAA20,Capture,{}},{0x597E10,0x56E090,TypeIcon,{}},
  {0x597E32,0x5FAA20,Capture,{}},{0x597E3E,0x56E010,TypeName,{}},
  {0x7B4378,0x5FAA20,Capture,{}},{0x7B4384,0x56E010,TypeName,{}},
  {0x7B439D,0x5FAA20,Capture,{}},{0x7B43A9,0x56E090,TypeIcon,{}}
 };
 for(auto& call:calls){auto site=reinterpret_cast<BYTE*>(call.site);int offset=0;
  if(!Copy(site,call.old,5)||call.old[0]!=0xE8)return false;memcpy(&offset,call.old+1,4);
  if(call.site+5+offset!=call.target)return false;
 }
 unsigned changed=0;
 for(auto& call:calls){auto site=reinterpret_cast<BYTE*>(call.site);DWORD old=0;
  if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old))break;
  *reinterpret_cast<int*>(site+1)=int(unsigned(call.hook)-call.site-5);
  DWORD ignored=0;VirtualProtect(site,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),site,5);++changed;
 }
 if(changed!=std::size(calls)){
  for(unsigned i=0;i<changed;++i){auto site=reinterpret_cast<BYTE*>(calls[i].site);DWORD old=0;if(VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)){memcpy(site,calls[i].old,5);DWORD ignored=0;VirtualProtect(site,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),site,5);}}
  return false;
 }
 Log("EVENT_COMBAT_PRESENTATION_ACTIVE types=%u callsites=%u native_race_type_unchanged=1",unsigned(typeKeys.size()),changed);return true;
}
}
