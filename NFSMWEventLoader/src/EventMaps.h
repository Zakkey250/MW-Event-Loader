// Independent event map adapter. Native event-ID resource loading owns the
// radar TPK lifetime; this adapter owns calibration and frontend bounds only.
#include "EventPreviewBounds.h"
#include "MapResourceIdentity.h"
namespace event_maps {
struct Calibration {float x,y,width;unsigned short rotation;int track;};
struct Bounds {float x0,y0,x1,y1;};
std::map<unsigned,Calibration> calibrations;
std::map<unsigned,Bounds> bounds;
std::map<NFSPluginSDK::MW05::TrackInfo*,Calibration> baseline,applied;
std::string nativeId;
std::map<unsigned,std::string> nativeIds;
bool active=false;
DWORD lastAudit=0;
unsigned Hash(const std::string& text){unsigned value=0xFFFFFFFF;for(unsigned char ch:text)value=value*33+ch;return value;}
bool Readable(const void* ptr,size_t bytes){
 if(!ptr)return false;MEMORY_BASIC_INFORMATION region{};
 if(!VirtualQuery(ptr,&region,sizeof(region))||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
 uintptr_t begin=reinterpret_cast<uintptr_t>(ptr),end=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize;
 return begin<=end&&bytes<=end-begin;
}
void(__thiscall* originalBounds)(void*,float*,float*)=nullptr;
void __fastcall PreviewBounds(void* self,void*,float* lower,float* upper){
 originalBounds(self,lower,upper);
 unsigned key=0;if(!Readable(self,0x18)||!Copy(static_cast<BYTE*>(self)+0x14,&key,4))return;
 auto found=bounds.find(key);if(found==bounds.end())return;
 auto v=found->second;auto native=event_preview::ToNative(v.x0,v.y0,v.x1,v.y1);
 lower[0]=native.lower[0];lower[1]=native.lower[1];upper[0]=native.upper[0];upper[1]=native.upper[1];
 if(worldLogEnabled){static unsigned last=0;if(last!=key){last=key;Log("EVENT_MAP_PREVIEW_BOUNDS texture=%08X bounds=%.4f,%.4f,%.4f,%.4f native=%.4f,%.4f,%.4f,%.4f width=%.4f height=%.4f manual_acceptance_pending=1",key,v.x0,v.y0,v.x1,v.y1,lower[0],lower[1],upper[0],upper[1],upper[0]-lower[0],lower[1]-upper[1]);}}
}
bool Install(){
 if(active)return true;
 auto marker=game/L"MWEventLoader.map-test";if(!mwel::fs::exists(marker)&&!package_resources::enabled)return true;
 std::vector<mwel::Json> bindings;
 if(package_resources::enabled){for(const auto& package:packages)if(mwel::Feature(package.authoring,"replaceMap")&&package.assets.contains("maps/map.json"))bindings.push_back(mwel::Json::parse(mwel::Read(package.directory/L"maps"/L"map.json",16384)));}
 else{if(packages.size()!=1)throw std::runtime_error("map proof requires one package");bindings.push_back(mwel::Json::parse(mwel::Read(marker,16384)));}
 for(const auto& binding:bindings){
 auto package=std::find_if(packages.begin(),packages.end(),[&](const auto& p){return p.id==binding.at("eventId");});
 if(package==packages.end()||binding.at("nativeEventId")!=package->nativeEventId)throw std::runtime_error("map package identity mismatch");
 auto token=binding.at("nativeEventId").get<std::string>();std::replace(token.begin(),token.end(),'.','_');
 unsigned key=0,rotation=0;Calibration v{};auto line=binding.at("calibrationLine").get<std::string>();
 if(sscanf_s(line.c_str(),"%x %f %f %f %u",&key,&v.x,&v.y,&v.width,&rotation)!=5||!key||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.width)||v.width<=1||v.width>=100000||rotation>65535)throw std::runtime_error("invalid map calibration");
 unsigned expected=mwel::Correction(package->authoring)?std::stoul(package->authoring.at("correctionTarget").at("hash").get<std::string>(),nullptr,16):registration_experiment::Hash(package->nativeNamespace.c_str());
 if(key!=expected)throw std::runtime_error("Map calibration native key differs");
 v.rotation=static_cast<unsigned short>(rotation);if(calibrations.count(key))throw std::runtime_error("Duplicate event map key");calibrations[key]=v;nativeIds[key]=event_resource::Token(package->nativeEventId);
 Bounds b{};line=binding.at("previewBounds").get<std::string>();
 if(sscanf_s(line.c_str(),"%x %f %f %f %f",&key,&b.x0,&b.y0,&b.x1,&b.y1)!=5||!key||!std::isfinite(b.x0)||!std::isfinite(b.y0)||!std::isfinite(b.x1)||!std::isfinite(b.y1)||b.x0<0||b.y0<0||b.x1>1||b.y1>1||b.x1<=b.x0||b.y1<=b.y0)throw std::runtime_error("invalid preview bounds");
 if(key!=Hash("TRACK_MAP_"+token))throw std::runtime_error("Preview native key differs");
 if(bounds.count(key))throw std::runtime_error("Duplicate preview map key");bounds[key]=b;
 // Resource aliases may expose either the requested TRACK_MAP hash or the
 // actual W2ME texture identity. Both must drive the same native camera.
 char ownPreview[32]{};sprintf_s(ownPreview,"W2ME_MAP_%08X",expected);auto ownKey=Hash(ownPreview);
 if(bounds.count(ownKey))throw std::runtime_error("Duplicate owned preview map key");bounds[ownKey]=b;
 }
 if(bindings.empty())return true;
 const BYTE guard[]={0x8B,0x49,0x0C,0x83,0xEC,0x10};auto address=reinterpret_cast<void*>(0x510C10);
 if(memcmp(address,guard,sizeof(guard))||MH_CreateHook(address,PreviewBounds,reinterpret_cast<void**>(&originalBounds))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);return false;}
 active=true;Log("EVENT_MAP_ADAPTER_ACTIVE maps=%u original_map_modified=0 preview_manual_acceptance_pending=1",unsigned(calibrations.size()));return true;
}
Calibration Current(NFSPluginSDK::MW05::TrackInfo* track){return {track->TrackMapCalibrationUpperLeft.x,track->TrackMapCalibrationUpperLeft.y,track->TrackMapCalibrationMapWidthMetres,track->TrackMapCalibrationRotation,track->TrackNumber};}
bool Same(Calibration a,Calibration b){return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.rotation==b.rotation&&a.track==b.track;}
void Write(NFSPluginSDK::MW05::TrackInfo* track,Calibration v){track->TrackMapCalibrationUpperLeft.x=v.x;track->TrackMapCalibrationUpperLeft.y=v.y;track->TrackMapCalibrationMapWidthMetres=v.width;track->TrackMapCalibrationRotation=v.rotation;}
void RestoreCalibrations(){
 for(const auto& item:applied){auto track=item.first;auto saved=baseline.find(track);if(Readable(track,sizeof(*track))&&saved!=baseline.end()&&Same(Current(track),item.second)){Write(track,saved->second);Log("EVENT_MAP_CALIBRATION_RESTORED track=%d teardown_boundary=1",saved->second.track);}}
 applied.clear();baseline.clear();
}
void AuditRoaming(NFSPluginSDK::MW05::Minimap* map){
 if(!MWELNeoLoggingEnabled())return;
 auto now=GetTickCount();if(now-lastAudit<5000)return;lastAudit=now;
 BYTE* manager=nullptr;char prefix[65]{};if(!Copy(reinterpret_cast<void*>(0x91CEFC),&manager,4)||!manager||!Copy(manager+8,prefix,64))return;
 unsigned native=0,course=0;for(unsigned image=0;image<4;++image){unsigned key=0;auto art=map->TrackmapArt[image];if(!Readable(art,0x28)||!Copy(reinterpret_cast<BYTE*>(art)+0x24,&key,4))continue;
  for(unsigned tile=0;tile<64;++tile){if(key==Hash(std::string(prefix)+"_CHOP"+std::to_string(tile)))++native;for(const auto& id:nativeIds)if(key==Hash("MINI_MAP_"+id.second+"_CHOP"+std::to_string(tile)))++course;}
 }
 auto c=Current(map->CurrentTrack);Log("EVENT_MAP_ROAMING_BINDING prefix=%s native_images=%u course_images=%u expected_images=4 calibration=%.4f,%.4f width=%.4f",prefix,native,course,c.x,c.y,c.width);
}
void FrameUnsafe(bool owned,unsigned eventKey){
 using namespace NFSPluginSDK::MW05;if(!active)return;
 auto count=*reinterpret_cast<unsigned*>(0x92D884);auto players=*reinterpret_cast<IPlayer***>(0x92D87C);
 if(!count||count>8||!Readable(players,count*4)||!Readable(players[0],4))return;
 auto hud=static_cast<FEngHud*>(players[0]->GetHud());if(!Readable(hud,sizeof(FEngHud)))return;
 auto map=static_cast<Minimap*>(hud->pMinimap);if(!Readable(map,sizeof(Minimap)))return;
 auto track=map->CurrentTrack;if(!Readable(track,sizeof(TrackInfo)))return;
 auto current=Current(track);auto old=applied.find(track);bool still=old!=applied.end()&&Same(current,old->second);
 auto found=calibrations.find(eventKey);
 if(!owned||found==calibrations.end()){
  if(still){auto saved=baseline.find(track);if(saved!=baseline.end()&&saved->second.track==current.track){Write(track,saved->second);Log("EVENT_MAP_CALIBRATION_RESTORED track=%d",current.track);}}
  applied.erase(track);baseline.erase(track);if(package_resources::PlayMode()==0)AuditRoaming(map);return;
 }
 if(!still)baseline[track]=current;
 auto v=found->second;v.track=current.track;
 nativeId=nativeIds.at(eventKey);
 if(!Same(current,v)){Write(track,v);Log("EVENT_MAP_CALIBRATED event=%08X track=%d upper_left=%.4f,%.4f width=%.4f rotation=%u",eventKey,v.track,v.x,v.y,v.width,v.rotation);}
 applied[track]=v;
 if(!MWELNeoLoggingEnabled())return;
 auto now=GetTickCount();if(now-lastAudit<5000)return;lastAudit=now;
 unsigned loaded=0;std::vector<unsigned> tileKeys;auto getTexture=reinterpret_cast<BYTE*(__cdecl*)(unsigned,int,int)>(0x503400);
 for(unsigned tile=0;tile<64;++tile){auto key=Hash("MINI_MAP_"+nativeId+"_CHOP"+std::to_string(tile));tileKeys.push_back(key);auto info=getTexture(key,0,0);unsigned actual=0;if(Readable(info,0x28)&&Copy(info+0x24,&actual,4)&&actual==key)++loaded;}
 auto previewKey=Hash("TRACK_MAP_"+nativeId);auto preview=getTexture(previewKey,0,0);unsigned actualPreview=0;
 char ownPreview[32]{};sprintf_s(ownPreview,"W2ME_MAP_%08X",eventKey);
 bool previewLoaded=Readable(preview,0x28)&&Copy(preview+0x24,&actualPreview,4)&&actualPreview==Hash(ownPreview);
 Log("EVENT_MAP_NATIVE_CONTEXT name=%.32s directory=%.32s region=%.8s track=%d",track->Name,track->TrackDirectory,track->RegionName,current.track);
 unsigned bound=0;
 for(unsigned i=0;i<4;++i){auto art=map->TrackmapArt[i];if(Readable(art,0x28)){unsigned key=0;Copy(reinterpret_cast<BYTE*>(art)+0x24,&key,4);bool ours=std::find(tileKeys.begin(),tileKeys.end(),key)!=tileKeys.end();bound+=ours;char textureName[25]{};auto info=getTexture(key,0,0);if(Readable(info,0x28))Copy(info+12,textureName,24);Log("EVENT_MAP_RADAR_BINDING image=%u texture=%08X name=%s own_tile=%u",i,key,textureName,unsigned(ours));}}
 Log("EVENT_MAP_TEXTURES event=%08X radar_tiles=%u available_tile_count=64 bound_images=%u expected_images=4 preview_loaded=%u preview_actual=%08X hud_visible=%u",eventKey,loaded,bound,unsigned(previewLoaded),actualPreview,unsigned(map->mCurrentlySetVisible));
 // Bounded ABI evidence before binding assertions: inspect native image
 // records without treating SDK's hTexture offsets as authoritative.
 static bool dumped=false;if(!dumped){dumped=true;for(unsigned i=0;i<4;++i){unsigned data[32]{};if(Readable(map->TrackmapArt[i],sizeof(data))&&Copy(map->TrackmapArt[i],data,sizeof(data)))for(unsigned word=0;word<32;++word)Log("EVENT_MAP_ART_WORD image=%u offset=%02X value=%08X",i,word*4,data[word]);}}
}
void Frame(bool owned,unsigned key){__try{FrameUnsafe(owned,key);}__except(EXCEPTION_EXECUTE_HANDLER){Log("EXPERIMENT_RACE_ABORT map_adapter_fault code=%08X",GetExceptionCode());}}
}
