#pragma once
#include "GauntletVehiclePolicy.h"
#include "RpdFleetPolicy.h"
// Functional authoring inventory. This is independent of diagnostic logging.
namespace rpd_vehicle_catalog {
using namespace rpd_player_vehicle;
struct Entry {unsigned source=0,mask=0,heat=0,regulatedHeats=0;bool helicopter=false,available=false,suv=false;std::string name,model,preset,origin,heatSource;};
std::vector<Entry> entries;
std::array<std::vector<unsigned>,11> dispatchRoster;
bool prepared=false,exported=false;
void* Data(Collection* c,const char* field,unsigned index=0){return reinterpret_cast<void*(__thiscall*)(Collection*,unsigned,unsigned)>(0x454190)(c,Hash(field),index);}
bool Descendant(Collection* c,const char* ancestor){
 const auto key=Hash(ancestor);std::set<Collection*> seen;
 for(unsigned i=0;c&&i<32;++i){Collection value{};if(!seen.insert(c).second||!Copy(c,&value,sizeof(value)))return false;if(value.key==key)return true;c=value.parent;}return false;
}
unsigned Detachment(Collection* c){
 unsigned ref[3]{},mask=0;
 if(!Copy(Data(c,"aivehicle"),ref,sizeof(ref))||ref[0]!=Hash("aivehicle"))return 0;
 const auto ai=findCollection(ref[0],ref[1]);
 return ai&&Copy(Data(ai,"DetachmentID"),&mask,4)?mask:0;
}
std::map<unsigned,unsigned> HeatAssignments(){
 std::map<unsigned,unsigned> assigned;
 for(unsigned tier=1;tier<=10;++tier){char name[16];sprintf_s(name,"heat_%02u",tier);
  auto level=findCollection(Hash("pursuitlevels"),Hash(name));Node node{};Collection* owner=nullptr;
  // The cops array is a class-layout attribute, so it may have no per-row
  // table node. Native GetAttribute resolves that definition and its owner.
  const auto nativeNode=level?reinterpret_cast<Node*(__thiscall*)(Collection*,unsigned,Collection**)>(0x454030)(level,Hash("cops"),&owner):nullptr;
  const bool field=nativeNode&&Copy(nativeNode,&node,sizeof(node));
  if(!field||!(node.flags&0x02000000)||(node.flags&0xffff)!=0x27)continue;
  unsigned short header[4]{};const auto base=(node.flags&0x10000000)?unsigned(owner->layout)+node.value:node.value;
  if(!Copy(reinterpret_cast<void*>(base),header,sizeof(header)))continue;
  if(header[1]>header[0]||header[0]>64||header[2]!=24)continue;
  for(unsigned i=0;i<header[1];++i){
   struct CopCount {NFSPluginSDK::MW05::Attrib::StringKey type;unsigned count;float chance;};
   static_assert(sizeof(CopCount)==24);CopCount value;auto item=Data(level,"cops",i);
   if(!item||!Copy(item,&value,sizeof(value)))continue;
   // Exact EN1.3 CopCount: typed StringKey, Count, Chance. The native
   // StringKey's 32-bit key identifies the installed pvehicle collection.
   for(auto& e:entries)if(value.type.mHash32==e.source){
    assigned.try_emplace(e.source,tier);
    if(value.count&&value.chance>0)e.regulatedHeats|=1u<<tier;
    if(value.count&&value.chance>0&&!e.helicopter&&!gauntlet_vehicle::SUV(e.model.c_str()))dispatchRoster[tier].push_back(e.source);
   }
  }
 }
 return assigned;
}
const Entry* DispatchVehicle(unsigned heat,unsigned* rosterHeat=nullptr){
 if(rosterHeat)*rosterHeat=0;
 if(heat<1||heat>=dispatchRoster.size())return nullptr;
 // Some native high tiers only contain support SUVs, zero-count entries,
 // or have no patrol row at all. Use the nearest enabled chase roster
 // without changing the event heat. Prefer the lower tier on a tie.
 const auto from=[&](unsigned tier)->const Entry*{
  for(const auto source:dispatchRoster[tier])for(const auto& entry:entries)if(entry.source==source){
   if(rosterHeat)*rosterHeat=tier;return &entry;
  }
  return nullptr;
 };
 for(unsigned gap=0;gap<dispatchRoster.size()-1;++gap){
  if(gap<heat)if(const auto entry=from(heat-gap))return entry;
  if(gap&&heat+gap<dispatchRoster.size())if(const auto entry=from(heat+gap))return entry;
 }
 return nullptr;
}
void Prepare(){
 if(prepared)return;
 auto seed=findCollection(Hash("pvehicle"),Hash("copgto"));if(!seed)return;
 prepared=true;
 unsigned key=firstCollection(seed->klass);std::set<unsigned> seen;
 while(key&&seen.size()<1024&&seen.insert(key).second){
  auto c=getCollection(seed->klass,key);
  const bool police=c&&Police(c),heli=c&&Descendant(c,"choppers");
  if(police||heli){
   Entry e;e.source=key;e.helicopter=heli;
   NFSPluginSDK::MW05::Attrib::StringKey model;const char* name=nullptr;
   if(Copy(Data(c,"MODEL"),&model,sizeof(model))&&Name(model.mString,e.model)&&
      Copy(Data(c,"CollectionName"),&name,4)&&Name(name,e.name)){
    e.origin=StockPatrol(e.name,e.model)?"vanilla":"addon";e.mask=police?Detachment(c):0;
    char preset[32];sprintf_s(preset,"MWEL_COP_%08X",key);e.preset=e.name=="copgto"?"CS_CAR_COPGTO":preset;
    if(police)try{e.available=rpd_player_vehicle::Prepare(key)!=nullptr;}catch(const std::exception& error){Log("RPD_CATALOG_VEHICLE_UNAVAILABLE key=%08X reason=%s",key,error.what());}
    entries.push_back(std::move(e));
   }
  }
  key=nextCollection(seed->klass,key);
 }
 const auto assigned=HeatAssignments();
 for(auto& e:entries){
  auto c=findCollection(Hash("pvehicle"),e.source);std::set<Collection*> parents;
  for(unsigned i=0;c&&i<32&&parents.insert(c).second;++i){
   auto tier=assigned.find(c->key);if(tier!=assigned.end()){e.heat=tier->second;e.heatSource=c->key==e.source?"pursuitlevels":"pvehicle-parent";break;}c=c->parent;
  }
 }
 for(auto& e:entries)if(!e.heat&&e.mask){
  for(const auto& other:entries)if(other.mask==e.mask&&other.heat&&(!e.heat||other.heat<e.heat)){e.heat=other.heat;e.heatSource="native-detachment-class";}
 }
 // Derive the native SUV detachment from the installed stock definitions,
 // then recognize add-on SUVs sharing that class (regardless of model name).
 std::set<unsigned> suvMasks;
 for(const auto& e:entries)if(e.origin=="vanilla"&&e.mask&&gauntlet_vehicle::SUV(e.model))suvMasks.insert(e.mask);
 for(auto& e:entries)e.suv=gauntlet_vehicle::SUV(e.model)||(e.mask&&suvMasks.count(e.mask));
}
struct Fleet {std::vector<const Entry*> models,patrol,suv;std::set<unsigned> sources;unsigned next=0,patrolNext=0,suvNext=0;};
std::array<Fleet,11> fleets;
bool fleetsPrepared=false;
void PrepareFleets(){
 if(fleetsPrepared||!prepared)return;
 fleetsPrepared=true;
 for(unsigned heat=1;heat<=10;++heat){auto& fleet=fleets[heat];rpd_fleet_policy::Inventory inventory;
  for(const auto& e:entries){
   if(e.helicopter||e.name=="copmidsize_nis"||e.name=="copmidsize_nis_ld"||
      !rpd_fleet_policy::AtHeat(heat,e.heat,e.regulatedHeats))continue;
   // AI eligibility is independent of whether a player loan preset can be built.
   std::error_code ec;
   if(ModelType(e.model)<0||!std::filesystem::is_regular_file(game/L"CARS"/e.model/L"GEOMETRY.BIN",ec))continue;
   const bool unique=inventory.Add(e.source,e.model);fleet.sources.insert(e.source);
   if(!unique)continue;
   fleet.models.push_back(&e);(e.suv?fleet.suv:fleet.patrol).push_back(&e);
  }
 }
}
bool LimitBreakerReady(){
 static bool ready=false;if(ready)return true;
 auto module=GetModuleHandleW(L"NFSMWLimitBreaker.asi");
 auto query=module?reinterpret_cast<nfsmw_limitbreaker::QueryResourceLimitsV1>(GetProcAddress(module,"NFSMWLB_QueryResourceLimitsV1")):nullptr;
 nfsmw_limitbreaker::ResourceLimitsV1 status{};
 if(!query)return false;
 query(&status,sizeof(status));
 return ready=status.structSize==sizeof(status)&&status.abiVersion==nfsmw_limitbreaker::kResourceLimitsAbiV1&&status.logicalVehicleLimit>=20;
}
bool FleetEnabled(unsigned heat){
 PrepareFleets();return heat<fleets.size()&&rpd_fleet_policy::Enabled(LimitBreakerReady(),heat,unsigned(fleets[heat].models.size()));
}
bool FleetAccepts(unsigned heat,unsigned source){return FleetEnabled(heat)&&fleets[heat].sources.count(source);}
const Entry* FleetVehicle(unsigned heat,int role=-1){
 if(!FleetEnabled(heat))return nullptr;
 auto& f=fleets[heat];auto& pool=role==0?f.patrol:role==1?f.suv:f.models;
 auto& cursor=role==0?f.patrolNext:role==1?f.suvNext:f.next;
 return pool.empty()?nullptr:pool[(cursor++)%pool.size()];
}
Entry* Find(const std::string& preset){for(auto& e:entries)if(e.preset==preset)return &e;return nullptr;}
void Export(){
 if(exported||!prepared||entries.empty())return;
 unsigned table=0;if(!Copy(reinterpret_cast<void*>(0x9B09D8),&table,4)||!table)return;
 mwel::Json rows=mwel::Json::array();
 rows.push_back({{"preset",mwel_rpd::UndercoverPreset},{"label",mwel_rpd::UndercoverLabel},{"origin","undercover"},{"role","ground"},{"available",true},{"piloting",true},{"verified",true},{"heat",0}});
 for(auto& e:entries){
  if(e.available){auto spec=rpd_player_vehicle::Prepare(e.source);e.available=spec&&StockPreset(*spec);}
  auto label=e.model;if(label.rfind("COP",0)==0)label.erase(0,3);
  if(e.name=="copmidsize_nis")label+=" (NIS interior)";
  else if(e.name=="copmidsize_nis_ld")label+=" (NIS low detail)";
  else if(e.name=="copcross")label+=" (Cross)";
  else if(e.name=="copsuvpatrol")label+=" (patrol)";
  rows.push_back({{"sourceKey",e.source},{"collection",e.name},{"model",e.model},{"preset",e.preset},{"label",label},{"origin",e.origin},{"role",e.helicopter?"helicopter":"ground"},{"available",e.available},{"piloting",!e.helicopter&&e.available},{"verified",true},{"heat",e.heat},{"heatSource",e.heatSource},{"detachmentMask",e.mask}});
 }
 HMODULE self=nullptr;wchar_t path[32768]{};
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Export),&self)||!GetModuleFileNameW(self,path,32768))return;
 mwel::Json banks=mwel::Json::object();
 for(const char* name:{"attributes.bin","fe_attrib.bin","gameplay.bin"})banks[name]=mwel::Digest(mwel::Read(game/L"GLOBAL"/name));
 const mwel::Json catalog={{"schema",1},{"capability","MWEL_RpdAuthoringV1"},{"moduleSha256",mwel::Digest(mwel::Read(path))},{"globalBanks",banks},{"missions",mwel::Json::array({"interceptor"})},{"vehicles",rows},{"helicopterPiloting",false}};
 mwel::AtomicWrite(mwel_paths::Root(game)/L"Runtime"/L"police-vehicles.json",catalog.dump(2)+"\n");exported=true;
}
}
void MWELRpdPrepareVehicles(){rpd_vehicle_catalog::Prepare();}
bool MWELRpdPresetAvailable(const std::string& preset){if(preset==mwel_rpd::UndercoverPreset||preset==mwel_rpd::InGameSelectedPreset)return true;auto e=rpd_vehicle_catalog::Find(preset);if(!e||!e->available||e->helicopter)return false;auto spec=rpd_player_vehicle::Prepare(e->source);return spec&&rpd_player_vehicle::StockPreset(*spec);}
std::string MWELRpdLoanPreset(const std::string& preset){
 const auto e=rpd_vehicle_catalog::Find(preset);if(!e)return {};
 const auto spec=rpd_player_vehicle::Prepare(e->source);
 return spec&&rpd_player_vehicle::StockPreset(*spec)?spec->loanPreset:std::string{};
}
unsigned MWELRpdPresetHeat(const std::string& preset){auto e=rpd_vehicle_catalog::Find(preset);return e?e->heat:0;}
unsigned MWELRpdAuthoredHeat(const mwel::Json& event){
 const auto preset=event.value("policeVehicle",std::string());
 const auto e=rpd_vehicle_catalog::Find(preset);
 if(preset!=mwel_rpd::UndercoverPreset&&preset!=mwel_rpd::InGameSelectedPreset&&(!e||e->origin!="addon"||e->helicopter))return 0;
 const auto heat=event.find("startHeatLevel");
 if(heat!=event.end()&&heat->is_number_integer()&&*heat>=1&&*heat<=10)return heat->get<unsigned>();
 return e&&e->heat>=1&&e->heat<=10?e->heat:1u;
}
unsigned MWELRpdVehicleHeat(unsigned vehicleKey){
 unsigned source=vehicleKey;{
  std::lock_guard<std::mutex> lock(rpd_player_vehicle::mutex);
  auto spec=rpd_player_vehicle::specifications.find(vehicleKey);if(spec!=rpd_player_vehicle::specifications.end())source=spec->second->source;
 }
 for(const auto& e:rpd_vehicle_catalog::entries)if(e.source==source)return e.heat;return 0;
}
std::string MWELRpdPresetLabel(const std::string& preset){if(preset==mwel_rpd::UndercoverPreset)return mwel_rpd::UndercoverLabel;if(preset==mwel_rpd::InGameSelectedPreset)return mwel_rpd::InGameSelectedLabel;auto e=rpd_vehicle_catalog::Find(preset);if(!e)return {};auto name=e->model;if(name.rfind("COP",0)==0)name.erase(0,3);return name;}
bool MWELRpdUsesOwnCar(const mwel::EventPackage& package){
 if(mwel_rpd::Undercover(package.authoring))return true;
 if(!mwel_rpd::InGameSelected(package.authoring))return false;
 const auto clone=registration_experiment::Find(registration_experiment::Hash(package.nativeNamespace.c_str()));
 return clone&&clone->rpdUsesOwnCar;
}
