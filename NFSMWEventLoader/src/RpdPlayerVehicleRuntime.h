#pragma once
// Owned native racer/player archetypes retain the selected patrol model and
// physical data, with genuine player mechanics, sound ownership and NOS.
namespace rpd_player_vehicle {
using namespace registration_experiment;
struct Specification {std::unique_ptr<Clone> attributes;std::string model,loanPreset;unsigned source=0,presetKey=0;bool stockReady=false;std::array<unsigned,20> layout{};std::array<BYTE,0x28C> preset{};};
std::mutex mutex;
std::map<unsigned,std::unique_ptr<Specification>> specifications;
void*(__cdecl* nativePreset)(unsigned)=nullptr;
bool presetAttempted=false;
bool StockPreset(Specification& spec);
void* __cdecl FindPreset(unsigned key){
 Specification* found=nullptr;
 {std::lock_guard<std::mutex> lock(mutex);for(auto& entry:specifications)if(entry.second->presetKey==key){found=entry.second.get();break;}}
 if(found)return StockPreset(*found)?found->preset.data():nullptr;
 return nativePreset(key);
}
bool InstallPreset(){
 if(nativePreset)return true;if(presetAttempted)return false;presetAttempted=true;
 const BYTE expected[]={0x56,0x8B,0x35,0xF4,0x2B,0x9B,0};auto site=reinterpret_cast<void*>(0x748130);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,FindPreset,reinterpret_cast<void**>(&nativePreset))!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);nativePreset=nullptr;return false;}return true;
}
bool Name(const char* pointer,std::string& output){
 output.clear();if(!pointer)return false;
 for(unsigned i=0;i<32;++i){char c=0;if(!Copy(pointer+i,&c,1))return false;if(!c)return !output.empty();
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;output+=c;
 }return false;
}
bool Field(Collection* source,const char* name,Node& output,Collection** owner=nullptr){
 const auto key=Hash(name);std::set<Collection*> seen;
 for(unsigned depth=0;source&&depth<32;++depth){
  Collection c{};if(!seen.insert(source).second||!Copy(source,&c,sizeof(c))||c.capacity>4096)return false;
  for(unsigned i=0;i<c.capacity;++i){Node n{};if(!Copy(c.nodes+i,&n,sizeof(n)))return false;if(n.key==key){output=n;if(owner)*owner=source;return true;}}
  source=c.parent;
 }return false;
}
bool Police(Collection* source){
 std::set<Collection*> seen;const auto cops=Hash("cops");
 for(unsigned i=0;source&&i<32;++i){Collection c{};if(!seen.insert(source).second||!Copy(source,&c,sizeof(c)))return false;if(c.key==cops)return true;source=c.parent;}
 return false; // Helicopter possession is intentionally not enabled here.
}
struct VanillaPatrol {const char* model;const char* handling;};
const std::map<std::string,VanillaPatrol> vanillaPatrols={
 {"copsuv",{"COPSUV","mustanggt"}},{"copsuvpatrol",{"COPSUV","mustanggt"}},{"copsuvl",{"COPSUVL","mustanggt"}},
 {"copsport",{"COPSPORT","corvette"}},{"copsportghost",{"COPSPORTGHOST","corvette"}},{"copsporthench",{"COPSPORTHENCH","corvette"}},{"copcross",{"COPSPORT","corvette"}},
 {"copmidsize",{"COPMIDSIZE","viper"}},{"copmidsize_nis_ld",{"COPMIDSIZE","viper"}},{"copmidsize_nis",{"COPMIDSIZEINT","viper"}},{"copghost",{"COPGHOST","viper"}},
 {"copgto",{"COPGTO","gto"}},{"copgtoghost",{"COPGTOGHOST","gto"}},{"copheli",{"COPHELI",nullptr}}
};
const VanillaPatrol* StockPatrol(const std::string& collection,const std::string& model){
 const auto found=vanillaPatrols.find(collection);
 return found!=vanillaPatrols.end()&&model==found->second.model?&found->second:nullptr;
}
bool PlayerMechanic(Clone& clone,Fields& fields,Collection* donor,const char* name){
 Node node{};if(!Field(donor,name,node))return false;
 if((node.flags>>24)==0x20){OwnNodeData(clone,node);fields[node.key]=node;return true;}
 // Native behaviour identifiers are StringKey objects, with both hashes and
 // a string pointer. Preserve that type rather than converting them to text.
 const auto data=reinterpret_cast<void*(__thiscall*)(Collection*,unsigned,unsigned)>(0x454190)(donor,node.key,0);
 NFSPluginSDK::MW05::Attrib::StringKey value;std::string text;
 if((node.flags&0x02000000)||!data||!Copy(data,&value,sizeof(value))||!Name(value.mString,text)){
  Log("RPD_PLAYER_MECHANIC_UNAVAILABLE field=%s flags=%08X",name,node.flags);return false;
 }
 value.mString=OwnString(clone,value.mString);
 auto owned=std::make_unique<std::vector<BYTE>>(sizeof(value));memcpy(owned->data(),&value,sizeof(value));
 node.value=unsigned(owned->data());node.flags&=0x00ffffff;
 clone.arrays.push_back(std::move(owned));fields[node.key]=node;
 Log("RPD_PLAYER_MECHANIC field=%s native_type=%04X value=%s",name,node.flags&0xffff,text.c_str());return true;
}
Specification* Prepare(unsigned sourceKey){
 const auto classKey=Hash("pvehicle");auto source=findCollection(classKey,sourceKey);
 if(!source){Log("RPD_PLAYER_SPECIFICATION_REFUSED source=%08X reason=pvehicle_missing class=%08X",sourceKey,classKey);return nullptr;}
 if(!Police(source)){Log("RPD_PLAYER_SPECIFICATION_REFUSED source=%08X reason=police_parent_missing",sourceKey);return nullptr;}
 if(!InstallPreset()){Log("RPD_PLAYER_SPECIFICATION_REFUSED source=%08X reason=preset_lookup_hook_unavailable",sourceKey);return nullptr;}
 {
  std::lock_guard<std::mutex> lock(mutex);
  for(auto& entry:specifications)if(entry.second->source==sourceKey)return entry.second.get();
 }
 std::string modelName;NFSPluginSDK::MW05::Attrib::StringKey model;
 const auto data=[](Collection* c,const char* field){return reinterpret_cast<void*(__thiscall*)(Collection*,unsigned,unsigned)>(0x454190)(c,Hash(field),0);};
 const auto modelData=data(source,"MODEL");
 if(!modelData||!Copy(modelData,&model,sizeof(model))||!Name(model.mString,modelName)){
  Log("RPD_PLAYER_SPECIFICATION_REFUSED source=%08X reason=model_layout_unavailable",sourceKey);return nullptr;
 }
 auto donor=findCollection(classKey,Hash("cs_gto_copgto"));if(!donor)return nullptr;
 // Move the matched stock chassis, tires and brakes together. Keep the
 // patrol powertrain, body, and mass, and leave add-on handling authored.
 const char* collectionName=nullptr;std::string collection;
 const auto stock=Copy(data(source,"CollectionName"),&collectionName,4)&&Name(collectionName,collection)?StockPatrol(collection,modelName):nullptr;
 const auto handling=stock&&stock->handling?findCollection(classKey,Hash(stock->handling)):nullptr;
 const auto hasHandling=[&](Collection* candidate){
  for(const auto field:{"tires","chassis","brakes"}){
   Node node{};unsigned reference[3]{};
   if(!Field(candidate,field,node)||(node.flags&0xffff)!=0x14||
      !Copy(data(candidate,field),reference,sizeof(reference))||reference[0]!=Hash(field)||!reference[1])return false;
   if((node.flags>>24)&2){
    unsigned short header[4]{};
    if((node.flags&0x10000000)||!Copy(reinterpret_cast<void*>(node.value),header,sizeof(header))||
       !header[1]||header[1]>header[0]||header[0]>16||header[2]!=12||(header[3]&0x7fff)!=0x14)return false;
   }
  }
  return true;
 };
 const bool playerHandling=handling&&!Police(handling)&&hasHandling(handling);
 auto result=std::make_unique<Specification>();result->source=sourceKey;result->model=modelName;result->attributes=std::make_unique<Clone>();
 auto& c=*result->attributes;
 // Gen::pvehicle's native constructor allocates a 0x50-byte layout (004E4EF0).
 // The gameplay clone's two-word layout is not the pvehicle layout contract.
 if(!Copy(source,&c.collection,sizeof(c.collection))||!c.collection.layout||!Copy(c.collection.layout,result->layout.data(),sizeof(result->layout)))return nullptr;
 char name[32];sprintf_s(name,"mwel_officer_%08X",sourceKey);c.name=name;c.namePointer=c.name.c_str();
 const auto key=Hash(c.name.c_str());if(!key||findCollection(classKey,key))return nullptr;
 c.collection.key=key;c.collection.parent=donor;c.collection.references=1;
 c.collection.name=&c.namePointer;c.collection.layout=result->layout.data();
 Fields fields;
 // This is a native racer/player archetype with the selected police model
 // and its actual physical data. Keeping the NPC cops parent made native
 // opponent initialization reject the field before creating its suspect.
 for(const char* field:{"engine","transmission","brakes","tires","chassis","induction","rigidbodyspecs","damagespecs","engineaudio","ShiftSND"}){
  auto physical=playerHandling&&(!strcmp(field,"tires")||!strcmp(field,"chassis")||!strcmp(field,"brakes"))?handling:source;
  Node node{};if(!Field(physical,field,node))continue;
  if((node.flags>>24)&2){
   OwnNodeData(c,node);
   if(physical==handling&&playerHandling){
    auto bytes=reinterpret_cast<BYTE*>(node.value);const auto header=reinterpret_cast<const unsigned short*>(bytes);
    const auto offset=8+((header[3]&0x8000)?8:0);
    for(unsigned i=0;i<header[1];++i)*reinterpret_cast<unsigned*>(bytes+offset+i*12+8)=0;
   }
   fields[node.key]=node;continue;
  }
  if((node.flags&0xffff)==0x14){
   unsigned reference[3]{};const auto native=data(physical,field);if(!native||!Copy(native,reference,sizeof(reference)))return nullptr;
   reference[2]=0;auto owned=std::make_unique<std::vector<BYTE>>(sizeof(reference));memcpy(owned->data(),reference,sizeof(reference));
   fields[node.key]={node.key,unsigned(owned->data()),0x14};c.arrays.push_back(std::move(owned));
  }
 }
 PreserveNativeField(c,fields,donor,"nos");
 auto upgrade=data(donor,"nos_upgrades");const auto offset=unsigned(upgrade)-unsigned(donor->layout);unsigned value=0;
 if(!fields.count(Hash("nos"))||offset>sizeof(result->layout)-4||!Copy(upgrade,&value,4)){
  Log("RPD_PLAYER_SPECIFICATION_REFUSED source=%08X reason=nos_layout fields=%u offset=%u",sourceKey,unsigned(fields.size()),offset);return nullptr;
 }
 memcpy(reinterpret_cast<BYTE*>(result->layout.data())+offset,&value,4);
 // Behaviour names are native typed attributes, not ordinary text fields.
 // Retain this patrol's physical/model data, using the real Human archetype's
 // player-capable mechanics (including RBVehicle and the player sound owner).
 for(const char* field:{"BEHAVIOR_MECHANIC_RIGIDBODY","BEHAVIOR_MECHANIC_SUSPENSION","BEHAVIOR_MECHANIC_ENGINE","BEHAVIOR_MECHANIC_DRAW","BEHAVIOR_MECHANIC_DAMAGE","BEHAVIOR_MECHANIC_AUDIO","BEHAVIOR_MECHANIC_RESET","EventSequencer"})
  PlayerMechanic(c,fields,donor,field);
 if(!fields.count(Hash("BEHAVIOR_MECHANIC_AUDIO"))||!fields.count(Hash("BEHAVIOR_MECHANIC_RIGIDBODY")))return nullptr;
 Scalar(fields,"PlayerUsable",1,6);Scalar(fields,"RandomOpponent",0,6);
 // NPC cops inherit PlayerUsable=false. Keep native character setup aware
 // that this private child is a real local-player loan, not a dispatch slot.
 for(const char* field:{"frontend","aivehicle"}){
  unsigned reference[3]{};const auto native=data(donor,field);
  if(!native||!Copy(native,reference,sizeof(reference))||reference[0]!=Hash(field))return nullptr;
  // Frontend vaults may load later. Preserve the genuine reference and let
  // native resolution fill its cache. The officer role is attached at GO;
  // initial Human/racer setup must not inherit the dispatched NPC's brain.
  reference[2]=0;auto owned=std::make_unique<std::vector<BYTE>>(sizeof(reference));memcpy(owned->data(),reference,sizeof(reference));
  const auto fieldKey=Hash(field);fields[fieldKey]={fieldKey,unsigned(owned->data()),0x14};c.arrays.push_back(std::move(owned));
 }
 Rebuild(c,fields);
 memcpy(result->preset.data()+8,modelName.c_str(),modelName.size()+1);
 char presetName[32];sprintf_s(presetName,"MWEL_COP_%08X",sourceKey);
 result->loanPreset=presetName;
 memcpy(result->preset.data()+0x28,presetName,strlen(presetName)+1);
 result->presetKey=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x460BC0)(presetName);
 if(nativePreset(result->presetKey))return nullptr;
 memcpy(result->preset.data()+0x50,&key,4);
 auto ready=result.get();{
  std::lock_guard<std::mutex> lock(mutex);if(specifications.count(key))return nullptr;specifications.emplace(key,std::move(result));
 }
 Log("RPD_PLAYER_SPECIFICATION source=%08X owned=%08X model=%s native_racer_parent=1 native_patrol_engine=1 native_player_tires=%u native_player_chassis=%u handling_donor=%08X native_player_nos=1 original_collection_written=0 native_player_brakes=%u handling_model=%s",sourceKey,key,modelName.c_str(),unsigned(playerHandling),unsigned(playerHandling),playerHandling?handling->key:0,unsigned(playerHandling),playerHandling?stock->handling:"patrol");
 return ready;
}
int ModelType(const std::string& model){
 const auto hash=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x460BF0)(model.c_str());
 const auto entry=reinterpret_cast<const BYTE*(__cdecl*)(unsigned)>(0x739940)(hash);
 if(!entry)return -1;char name[16]{};unsigned type=0;
 if(!Copy(entry,name,sizeof(name))||!memchr(name,0,sizeof(name))||_stricmp(name,model.c_str())||!Copy(entry+0x90,&type,4)||type>=1024)return -1;
 // Use the native/Unlimiter accessors, not the obsolete 84-entry table.
 return reinterpret_cast<const BYTE*(__cdecl*)(unsigned)>(0x739920)(type)==entry?int(type):-1;
}
bool StockPreset(Specification& spec){
 if(spec.stockReady)return true;
 const auto type=ModelType(spec.model);if(type<0)return false;
 NFSPluginSDK::MW05::RideInfo ride(static_cast<NFSPluginSDK::MW05::CarType>(type));ride.SetStockParts();
 NFSPluginSDK::MW05::FECustomizationRecord record;record.WriteRideIntoRecord(&ride);
 unsigned installed=0;
 for(unsigned i=0;i<139;++i){
  const auto part=ride.GetPart(static_cast<NFSPluginSDK::MW05::CarSlotId>(i));unsigned hash=0;
  if(part&&!Copy(part,&hash,4))return false;
  memcpy(spec.preset.data()+0x60+i*4,&hash,4);installed+=record.mInstalledParts[i]!=-1;
 }
 if(!installed||record.GetInstalledPart(NFSPluginSDK::MW05::CarSlotId::Body)==-1)return false;
 spec.stockReady=true;
 Log("RPD_PLAYER_PRESET_STOCK model=%s type=%d installed_parts=%u",spec.model.c_str(),type,installed);return true;
}
}
void* MWELRpdPlayerCollection(unsigned classKey,unsigned key){
 // Hash lookup is deliberately separate from gameplay enumeration and scope.
 if(classKey!=0x4A97EC8F)return nullptr;
 std::lock_guard<std::mutex> lock(rpd_player_vehicle::mutex);
 auto it=rpd_player_vehicle::specifications.find(key);
 return it==rpd_player_vehicle::specifications.end()?nullptr:&it->second->attributes->collection;
}
bool MWELRpdPlayerSpecification(unsigned sourceKey,void* customization,unsigned* playerKey){
 if(!customization||!playerKey)return false;
 try{
  auto spec=rpd_player_vehicle::Prepare(sourceKey);if(!spec)return false;
  if(!rpd_player_vehicle::StockPreset(*spec))return false;
  // The native preset owns real stock hashes for this model. Zero hashes
  // leave parts uninstalled and must never stand in for a stock car.
  reinterpret_cast<void(__thiscall*)(void*,void*)>(0x56F340)(customization,spec->preset.data());
  const auto type=rpd_player_vehicle::ModelType(spec->model);if(type<0)return false;
  // An empty FE preset means uninstalled parts, not a stock vehicle. Use
  // native stock selection for this model and serialize its real part IDs.
  NFSPluginSDK::MW05::RideInfo ride(static_cast<NFSPluginSDK::MW05::CarType>(type));
  ride.SetStockParts();
  auto record=static_cast<NFSPluginSDK::MW05::FECustomizationRecord*>(customization);
  record->WriteRideIntoRecord(&ride);
  unsigned installed=0;for(auto part:record->mInstalledParts)installed+=part>=0;
  Log("RPD_PLAYER_APPEARANCE model=%s type=%u installed_parts=%u native_stock_selection=1",spec->model.c_str(),type,installed);
  if(!installed)return false;
  *playerKey=spec->attributes->collection.key;return true;
 }catch(const std::exception& error){Log("RPD_PLAYER_SPECIFICATION_REFUSED source=%08X reason=%s",sourceKey,error.what());return false;}
}
