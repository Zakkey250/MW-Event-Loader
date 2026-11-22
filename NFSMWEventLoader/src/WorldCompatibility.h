#pragma once
#include "WorldProviderModule.h"
#include "EventRoadCompatibility.h"
#include <optional>
// Included after EventCore. Cache hashes retain strict extraction integrity;
// race portability intentionally ignores cars, saves, language and textures.
namespace world_compatibility {
using mwel::Json;using mwel::fs::path;
std::string FileDigest(const path& path){
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char bytes[32]{};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("World hash unavailable");
 bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;std::ifstream stream(path,std::ios::binary);char buffer[65536];
 if(!stream)ok=false;while(ok&&stream){stream.read(buffer,sizeof(buffer));const auto count=stream.gcount();if(count&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer),ULONG(count),0)<0)ok=false;}
 ok=ok&&stream.eof()&&BCryptFinishHash(hash,bytes,sizeof(bytes),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(!ok)throw std::runtime_error("World source unreadable");
 std::string result;for(auto byte:bytes){constexpr char hex[]="0123456789ABCDEF";result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
inline bool Same(const Json& a,const Json& b){return a.is_object()&&b.is_object()&&a.value("schema",0)==1&&b.value("schema",0)==1&&a.value("kind",std::string())==b.value("kind",std::string())&&a.value("id",std::string())==b.value("id",std::string())&&mwel::Sha256(a.value("id",std::string()));}
inline Json PublicWorld(const std::string& master) {
 return {{"schema",1},{"kind","w2c"},{"id",mwel::Digest("MWEL_WORLD_V1\nw2c\ntracks/l2ra.bun:"+master+"\n")}};
}
inline bool KeepsEvent(const mwel_event_roads::Changes& roads,const Json& event) {
 const auto edgeMatches=[&](const Json& id){
  return id.is_number_integer() && roads.KeepsEdge(id.get<size_t>());
 };
 for(const auto* field:{"mapEdges","branchMapEdges","shortcutMapEdges","discouragedMapEdges"}){
  const auto ids=event.find(field);
  if(ids!=event.end()&&(!ids->is_array()||!std::all_of(ids->begin(),ids->end(),edgeMatches)))return false;
 }
 const auto pointMatches=[&](const Json& p){
  if(roads.changedRoads.empty())return true;
  if(!p.is_array()||p.size()!=3)return false;
  mwel_event_roads::Point point{};
  for(size_t i=0;i<point.size();++i){
   if(!p[i].is_number())return false;
   point[i]=p[i].get<float>();
   if(!std::isfinite(point[i]))return false;
  }
  return roads.KeepsPosition(point);
 };
 for(const auto* field:{"gates","shortcuts","extras","barriers","aiObstacles"}){
  const auto entries=event.find(field);
  if(entries==event.end())continue;
  if(!entries->is_array())return false;
  for(const auto& entry:*entries){
   const auto p=entry.find("position");
   if(p!=entry.end()&&!pointMatches(*p))return false;
  }
 }
 const auto routes=event.find("greenRoutes");
 if(routes==event.end())return true;
 if(!routes->is_array())return false;
 for(const auto& route:*routes){
  for(const auto* side:{"start","end"}){
   const auto anchor=route.find(side);
   if(anchor!=route.end()){
    const auto edge=anchor->find("edgeId");
    if(edge!=anchor->end()&&!edge->is_null()&&!edgeMatches(*edge))return false;
   }
  }
  const auto points=route.find("points");
  if(points!=route.end()&&(!points->is_array()||!std::all_of(points->begin(),points->end(),pointMatches)))return false;
 }
 return true;
}
Json Current(const path& game){
 const bool custom=mwel::fs::is_regular_file(game/L"NativeWorld"/L"index.txt")&&(mwel_world_provider::Module()!=nullptr);
 Json files=Json::object();std::string kind=custom?"w2c":"rockport";
 if(custom)for(const auto& entry:mwel::fs::directory_iterator(game/L"NativeWorld")){
  auto name=entry.path().filename().string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
  if(entry.is_symlink())throw std::runtime_error("Linked world source refused");
  if(entry.is_regular_file()&&name.size()>4&&name.substr(name.size()-4)==".wmc")files["nativeworld/"+name]=FileDigest(entry.path());
 }
 if(custom&&files.empty())throw std::runtime_error("W2C collision identity absent");
 std::string material="MWEL_WORLD_V1\n"+kind+"\n";for(const auto& item:files.items())material+=item.key()+":"+item.value().get<std::string>()+"\n";
 return Json{{"schema",1},{"kind",kind},{"files",files},{"id",mwel::Digest(material)}};
}
class Context {
 Json receiving,rockport,publicW2C,originalW2C;
 std::optional<mwel_event_roads::Changes> changedRoads;
 std::map<std::string,std::optional<mwel_event_roads::Changes>> history;
 path historyDirectory;
 std::string currentRoadMaster;
 static constexpr size_t MaxRoadMasterBytes=64*1024*1024;
 bool publicProvider=false;

 bool AllowsHistorical(const Json& identity,const Json& event){
  if(!identity.is_object()||identity.value("schema",0)!=1||identity.value("kind",std::string())!="w2c")return false;
  const auto id=identity.value("id",std::string());
  if(!mwel::Sha256(id))return false;
  auto [entry,firstCheck]=history.try_emplace(id);
  if(firstCheck)try{
   const auto reference=historyDirectory/path(id+".bun");
   if(mwel::fs::is_regular_file(reference)&&!mwel::fs::is_symlink(reference)){
    const auto before=mwel::Read(reference,MaxRoadMasterBytes);
    mwel_event_roads::Changes comparison;
    if(Same(identity,PublicWorld(mwel::Digest(before)))&&comparison.Compare(before,currentRoadMaster))
     entry->second=std::move(comparison);
   }
  }catch(const std::exception&){}
  return entry->second&&KeepsEvent(*entry->second,event);
 }
 public:
 explicit Context(const path& game):receiving(Current(game)),
   historyDirectory(mwel_paths::Root(game)/L"RoadCompatibility"){
  const auto installation=game/L"W2CMWPublic"/L"installation.json";
  if(!mwel_world_provider::SourceModule()||!mwel::fs::is_regular_file(installation))return;
  publicProvider=true;
  try{
   currentRoadMaster=mwel::Read(game/L"TRACKS"/L"L2RA.BUN",MaxRoadMasterBytes);
   const auto master=mwel::Digest(currentRoadMaster);
   publicW2C=PublicWorld(master);
   const auto nativeRockport=Json{{"schema",1},{"kind","rockport"},{"id",mwel::Digest("MWEL_WORLD_V1\nrockport\n")}};
   auto declaredMaster=std::string{};
   // Legacy Rockport V1 exports omit the master digest. The converter's
   // receiving baseline identifies their original Rockport layout.
   const auto config=Json::parse(mwel::Read(installation,64*1024),nullptr,false);
   if(config.is_object()&&config.value("packageId",std::string{})=="W2CMWPublic"){
    const auto core=config.find("targetCore");
    if(core!=config.end()&&core->is_object()){
     declaredMaster=core->value("TRACKS/L2RA.BUN",std::string{});
     std::transform(declaredMaster.begin(),declaredMaster.end(),declaredMaster.begin(),[](unsigned char c){return char(std::toupper(c));});
    }
   }
   if(master==declaredMaster)rockport=nativeRockport;
   const auto reference=game/L"W2CMWPublic"/L"originals"/L"TRACKS"/L"L2RA.BUN";
   if(!mwel::fs::is_regular_file(reference)||mwel::fs::is_symlink(reference))return;
   const auto original=mwel::Read(reference,MaxRoadMasterBytes);
   const auto source=mwel::Digest(original);
   if(source==master)return;
   mwel_event_roads::Changes comparison;
   if(!comparison.Compare(original,currentRoadMaster))return;
   originalW2C=PublicWorld(source);
   changedRoads=std::move(comparison);
   if(source==declaredMaster)rockport=nativeRockport;
  }catch(const std::exception&){
   // An unavailable reference only disables compatibility for that reference.
  }
 }
 bool Allows(const mwel::EventPackage& package){
  const auto found=package.authoring.find("worldCompatibility");
  if(found==package.authoring.end())return false;
  if(Same(*found,publicW2C))return true;
  if(Same(*found,originalW2C)||Same(*found,rockport))
   return !changedRoads||KeepsEvent(*changedRoads,package.authoring);
  if((!publicProvider||receiving.value("kind",std::string())!="rockport")&&Same(*found,receiving))return true;
  return !currentRoadMaster.empty()&&AllowsHistorical(*found,package.authoring);
 }
 void RememberCurrent()const{
  if(currentRoadMaster.empty()||!publicW2C.is_object())return;
  const auto id=publicW2C.at("id").get<std::string>();
  const auto snapshot=historyDirectory/path(id+".bun");
  if(mwel::fs::is_symlink(historyDirectory)||mwel::fs::is_symlink(snapshot))return;
  if(mwel::fs::is_regular_file(snapshot)&&mwel::Read(snapshot,MaxRoadMasterBytes)==currentRoadMaster)return;
  mwel_event_roads::Master master;
  mwel_event_roads::Graph road;
  if(master.Parse(currentRoadMaster)&&road.Parse(master.road))mwel::AtomicWrite(snapshot,currentRoadMaster);
 }
};
}
