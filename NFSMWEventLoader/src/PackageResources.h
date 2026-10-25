#pragma once
#include "RadarTransition.h"
#include "MapResourceIdentity.h"
// Native resource-loader adapter for separately owned package textures.
// No copies into TRACKS and no TexWizard configuration edits are required.
namespace package_resources {
bool enabled=false;
struct Map {std::string requested,radar,preview,prefix;unsigned eventKey=0,alias=0,texture=0;void* previewResource=nullptr;void* radarResource=nullptr;bool previewObserved=false;};
std::vector<std::unique_ptr<Map>> maps;
void*(__cdecl* originalCreate)(const char*,int,int,int,int);
void*(__cdecl* originalTexture)(unsigned,int,int);
bool(__cdecl* originalMapLoad)(int,char*,int,char*,int);
bool roamingRestorePending=false;
unsigned PlayMode(){BYTE* race=nullptr;unsigned mode=~0u;if(Copy(reinterpret_cast<void*>(0x91E000),&race,4)&&race)Copy(race+0x1960,&mode,4);return mode;}
unsigned TextureHash(const std::string& text){unsigned hash=~0u;for(unsigned char c:text)hash=hash*33+c;return hash;}
std::string ResourceName(const char* pointer){
 std::string result;if(!pointer)return result;
 for(unsigned i=0;i<1024;++i){char c=0;if(!Copy(pointer+i,&c,1))return {};if(!c)return result;
  if(c=='/')c='\\';result+=char(std::tolower(static_cast<unsigned char>(c)));
 }return {};
}
void* __cdecl Create(const char* name,int type,int a,int b,int c){
 auto requested=ResourceName(name);
 for(const auto& map:maps)if(requested==map->requested){
  Log("EVENT_PACKAGE_RADAR_RESOURCE requested=%s package_path=%s native_track_written=0",requested.c_str(),map->radar.c_str());
  return originalCreate(map->radar.c_str(),type,a,b,c);
 }
 return originalCreate(name,type,a,b,c);
}
void* __cdecl Texture(unsigned key,int fallback,int unloaded){
 for(const auto& map:maps)if(key==map->alias)return originalTexture(map->texture,fallback,unloaded);
 return originalTexture(key,fallback,unloaded);
}
bool __cdecl MapLoad(int mode,char* prefix,int prefixSize,char* path,int pathSize){
 auto key=MWELNeoCurrentKey();
 for(const auto& map:maps)if(event_radar::UseCourse(mode,PlayMode(),key)&&map->eventKey==key){
  if(!prefix||!path||prefixSize<=0||pathSize<=0||map->prefix.size()>=unsigned(prefixSize)||map->requested.size()>=unsigned(pathSize))return false;
  // The native output path is only 64 bytes. Keep its short logical name;
  // Create maps it to the full UUID package path at the resource boundary.
  memcpy(prefix,map->prefix.c_str(),map->prefix.size()+1);memcpy(path,map->requested.c_str(),map->requested.size()+1);
  auto result=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x65F7A0)(map->radar.c_str());
  Log("EVENT_PACKAGE_NATIVE_RADAR event=%08X prefix=%s resource=%s result=%08X",key,prefix,path,result);
  return result!=0;
 }
 return originalMapLoad(mode,prefix,prefixSize,path,pathSize);
}
struct NativeRadar {
 BYTE* manager;
 void Trace(const char* boundary){
  if(!worldLogEnabled)return;
  unsigned owner=0,engine=0,provider=0,resource=0,chunks=0;Copy(reinterpret_cast<void*>(0x91CADC),&owner,4);if(owner)Copy(reinterpret_cast<void*>(owner),&engine,4);if(engine)Copy(reinterpret_cast<void*>(engine+0x108),&provider,4);Copy(reinterpret_cast<void*>(0x91CF1C),&resource,4);Copy(manager,&chunks,4);
  Log("EVENT_RADAR_RESTORE_OWNER boundary=%s manager=%08X resource=%08X fe_owner=%08X fe_engine=%08X fe_provider=%08X registered_chunks=%u",boundary,unsigned(manager),resource,owner,engine,provider,chunks);
 }
 bool Query(std::string& prefix,std::string& path){Trace("before_query");char p[32]{},f[64]{};if(!originalMapLoad(0,p,sizeof(p),f,sizeof(f)))return false;prefix=p;path=f;Trace("after_query");return !prefix.empty()&&!path.empty();}
 void ClearTiles(){Trace("before_clear");reinterpret_cast<void(__thiscall*)(void*,void*,int)>(0x5672F0)(manager,nullptr,0);Trace("after_clear");}
 void Release(){auto owner=reinterpret_cast<void**>(0x91CF1C);auto resource=*owner;if(resource){reinterpret_cast<void(__cdecl*)(void*)>(0x663F00)(resource);*owner=nullptr;}Trace("after_release");}
 void ResetIndex(){
  // Native 005674D0 appends at +48h using the cursor at +00h; it does
  // not check the 64-entry bound. Reset only after the old resource and
  // decompressed tiles have been detached, before queueing their replacement.
  event_radar::ResetCompressedIndex(manager);Trace("after_index_reset");
 }
 void SetPrefix(const std::string& prefix){memset(manager+8,0,64);memcpy(manager+8,prefix.c_str(),prefix.size()+1);}
 bool Load(const std::string& path){auto resource=reinterpret_cast<void*(__cdecl*)(const char*,int,int,int,int,int,int)>(0x661760)(path.c_str(),4,0,0,0,0,0);*reinterpret_cast<void**>(0x91CF1C)=resource;Trace("after_load");return resource!=nullptr;}
};
void RestoreRoamingRadar(){
 if(!enabled||PlayMode()!=0)return;
 BYTE* manager=nullptr;char previous[65]{};
 if(!Copy(reinterpret_cast<void*>(0x91CEFC),&manager,4)||!manager||!Copy(manager+8,previous,64))return;
 // A safehouse/world reload owns this transition already. Its new HUD has
 // not assigned pMiniMapTexture yet; a second LoadResourceFile here races
 // that native request and appends two 64-tile banks into one 64-slot table.
 // Do not latch a retry: re-evaluate the native prefix on the next frame.
 void* liveResource=nullptr;if(!Copy(reinterpret_cast<void*>(0x91CF1C),&liveResource,4)||!liveResource)return;
 bool owned=roamingRestorePending;for(const auto& map:maps)owned|=map->prefix==previous;
 NativeRadar native{manager};auto result=event_radar::Restore(native,true,owned);
 if(result==event_radar::RestoreResult::Skipped)return;
 roamingRestorePending=result!=event_radar::RestoreResult::Queued;
 if(!roamingRestorePending)Log("EVENT_MAP_ROAMING_RESOURCE_RESTORED old_prefix=%s native_prefix=%s streamed_owner=1",previous,manager+8);
 else Log("EVENT_MAP_ROAMING_RESOURCE_PENDING old_prefix=%s",previous);
}
bool Install(){
 auto marker=game/L"MWEventLoader.package-resources-test";if(!productionRuntime&&!mwel::fs::exists(marker))return true;
 if(!productionRuntime&&mwel::Read(marker,256)!="MW_EVENT_LOADER_PACKAGE_RESOURCES_EXPERIMENT_V1\n")return false;
 std::set<unsigned> aliases;
 for(const auto& package:packages){
  if(!mwel::Feature(package.authoring,"replaceMap")){
   if(mwel::Correction(package.authoring))continue; // Preserve the native event's map and calibration.
   auto source=package.authoring.value("sourceEventId",std::string{});
   if(!std::regex_match(source,std::regex("[A-Za-z0-9_.-]{1,9}")))throw std::runtime_error("Native map source identity absent");
   auto token=package.nativeEventId;std::replace(token.begin(),token.end(),'.','_');std::replace(source.begin(),source.end(),'.','_');
   auto map=std::make_unique<Map>();map->eventKey=registration_experiment::Hash(package.nativeNamespace.c_str());
   map->requested=ResourceName(("TRACKS\\L2RA\\MINI_MAP_"+token+".BIN").c_str());
   map->radar="TRACKS\\L2RA\\MINI_MAP_"+source+".BIN";map->prefix=event_resource::RadarPrefix(source);
   map->alias=TextureHash("TRACK_MAP_"+token);map->texture=TextureHash("TRACK_MAP_"+source);
   const bool preview=package.assets.contains("maps/preview/textures.bin"),metadata=package.assets.contains("maps/preview/meta.json");
   if(preview!=metadata)throw std::runtime_error("Inherited preview resource incomplete");
   if(preview){
    auto meta=mwel::Json::parse(mwel::Read(package.directory/L"maps"/L"preview"/L"meta.json",16384));
    char own[32]{};sprintf_s(own,"W2ME_MAP_%08X",map->eventKey);
    if(meta.at("textures")!=mwel::Json::array({mwel::Json::array({"TRACK_MAP_"+token,std::string(own)})})||!meta.at("links").empty())throw std::runtime_error("Inherited preview mapping differs");
    map->preview=(package.directory/"maps"/"preview"/"textures.bin").lexically_relative(game).string();map->texture=TextureHash(own);
   }
   if(!aliases.insert(map->alias).second)throw std::runtime_error("Package preview alias collision");
   maps.push_back(std::move(map));continue;
  }
  if(!package.assets.contains("maps/map.json"))throw std::runtime_error("Enabled package map resource absent");
  auto binding=mwel::Json::parse(mwel::Read(package.directory/L"maps"/L"map.json",16384));
  if(binding.at("schema")!=1||binding.at("eventId")!=package.id||binding.at("nativeEventId")!=package.nativeEventId)throw std::runtime_error("Package map identity mismatch");
  for(const char* name:{"maps/radar.bin","maps/preview/textures.bin","maps/preview/meta.json"})if(!package.assets.contains(name))throw std::runtime_error("Package map resource absent");
  auto map=std::make_unique<Map>();auto token=package.nativeEventId;std::replace(token.begin(),token.end(),'.','_');
  map->requested=ResourceName(("TRACKS\\L2RA\\MINI_MAP_"+token+".BIN").c_str());
  map->radar=(package.directory/"maps"/"radar.bin").lexically_relative(game).string();
  map->preview=(package.directory/"maps"/"preview"/"textures.bin").lexically_relative(game).string();
  auto meta=mwel::Json::parse(mwel::Read(package.directory/L"maps"/L"preview"/L"meta.json",16384));
  auto nativeKey=mwel::Correction(package.authoring)?std::stoul(package.authoring.at("correctionTarget").at("hash").get<std::string>(),nullptr,16):registration_experiment::Hash(package.nativeNamespace.c_str());
  map->eventKey=unsigned(nativeKey);map->prefix=event_resource::RadarPrefix(package.nativeEventId);
  char ownTexture[32]{};sprintf_s(ownTexture,"W2ME_MAP_%08X",unsigned(nativeKey));
  std::string alias="TRACK_MAP_"+token;
  if(meta.at("textures")!=mwel::Json::array({mwel::Json::array({alias,std::string(ownTexture)})})||!meta.at("links").empty())throw std::runtime_error("Package preview mapping differs");
  map->alias=TextureHash(alias);map->texture=TextureHash(ownTexture);
  if(!aliases.insert(map->alias).second)throw std::runtime_error("Package preview alias collision");
  maps.push_back(std::move(map));
 }
 // Keep native entry bytes intact while other ASIs resolve startup patterns.
 // Activation occurs on the game thread at the database boundary, before any
 // package preview/radar load. No sleep, polling worker or third-party patch.
 return true;
}
bool Activate(){
 if(enabled||maps.empty())return true;
 const BYTE createGuard[]={0x6A,0xFF,0x68,0x5B,0xA1,0x87,0};
 const BYTE textureGuard[]={0x53,0x55,0x56,0x57,0x8B,0x7C,0x24,0x14};
 const BYTE beginGuard[]={0x8B,0x44,0x24,0x04,0x56,0x8B,0xF1};
 const BYTE mapGuard[]={0x83,0x7C,0x24,0x04,0x02,0x0F,0x84,0x31,0x01,0,0};
 const BYTE clearGuard[]={0x51,0x53,0x55,0x56,0x8B,0xE9};
 const BYTE releaseGuard[]={0x56,0x8B,0x74,0x24,0x08,0x85,0xF6};
 const BYTE streamGuard[]={0x8B,0x44,0x24,0x1C,0x8B,0x4C,0x24,0x18};
 if(memcmp(reinterpret_cast<void*>(0x5672F0),clearGuard,sizeof(clearGuard))||memcmp(reinterpret_cast<void*>(0x663F00),releaseGuard,sizeof(releaseGuard))||memcmp(reinterpret_cast<void*>(0x661760),streamGuard,sizeof(streamGuard)))return false;
 if(memcmp(reinterpret_cast<void*>(0x65FD30),createGuard,sizeof(createGuard))||memcmp(reinterpret_cast<void*>(0x503400),textureGuard,sizeof(textureGuard))||memcmp(reinterpret_cast<void*>(0x6616F0),beginGuard,sizeof(beginGuard))||memcmp(reinterpret_cast<void*>(0x57BEC0),mapGuard,sizeof(mapGuard)))return false;
 auto factory=reinterpret_cast<void*>(0x65FD30),texture=reinterpret_cast<void*>(0x503400),mapLoader=reinterpret_cast<void*>(0x57BEC0);
 if(MH_CreateHook(factory,Create,reinterpret_cast<void**>(&originalCreate))!=MH_OK)return false;
 if(MH_CreateHook(texture,Texture,reinterpret_cast<void**>(&originalTexture))!=MH_OK){MH_RemoveHook(factory);return false;}
 if(MH_CreateHook(mapLoader,MapLoad,reinterpret_cast<void**>(&originalMapLoad))!=MH_OK){MH_RemoveHook(factory);MH_RemoveHook(texture);return false;}
 if(MH_QueueEnableHook(factory)!=MH_OK||MH_QueueEnableHook(texture)!=MH_OK||MH_QueueEnableHook(mapLoader)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_DisableHook(factory);MH_DisableHook(texture);MH_DisableHook(mapLoader);MH_RemoveHook(factory);MH_RemoveHook(texture);MH_RemoveHook(mapLoader);return false;}
 enabled=true;Log("EVENT_PACKAGE_RESOURCES_ACTIVE maps=%u TRACKS_writes=0 TexWizard_required=0 lifetime_acceptance_pending=1",unsigned(maps.size()));return true;
}
void LoadPreviews(){
 if(!enabled)return;
 for(auto& map:maps)if(!map->preview.empty()&&!map->previewResource){
  auto resource=originalCreate(map->preview.c_str(),1,0,0,0);
  if(!resource){Log("EXPERIMENT_RACE_ABORT package_preview_resource_unavailable");continue;}
  map->previewResource=resource;reinterpret_cast<int(__thiscall*)(void*,int,int)>(0x6616F0)(resource,0,0);
  Log("EVENT_PACKAGE_PREVIEW_RESOURCE queued=1 package_path=%s native_frontend_written=0",map->preview.c_str());
 }
 // Radar is a streamed map resource (native load type 4), not an ordinary
 // preview texture pack. Its native manager owns loading and unloading.
 // Prefetching the same wrapped chunks as type 1 would create a second owner.
}
void AuditPreviews(){
 if(!worldLogEnabled||!enabled)return;
 static ULONGLONG next=0;const auto now=GetTickCount64();if(now<next)return;next=now+1000;
 for(auto& map:maps)if(!map->preview.empty()&&map->previewResource&&!map->previewObserved){
  auto info=static_cast<BYTE*>(Texture(map->alias,0,0));unsigned actual=0;
  if(info&&Copy(info+0x24,&actual,4)&&actual==map->texture){
   map->previewObserved=true;Log("EVENT_PACKAGE_PREVIEW_LOOKUP event=%08X alias=%08X texture=%08X native_resource_loaded=1",map->eventKey,map->alias,actual);
  }
 }
}
}
