// Native registration shared by production and isolated validation fixtures.
// Native game files remain unchanged; collections belong to this process.
#include "CollectionLookupCache.h"
#include "OpponentRosterPolicy.h"
#include "CarbonParticleFX.h"
namespace registration_experiment {
struct Node { unsigned key,value,flags; };
struct Collection {
 Node* nodes;unsigned capacity,count;unsigned short worstCollision,keyShift;
 Collection* parent;void* klass;void* layout;unsigned references,key;void* source;const char** name;
};
static_assert(sizeof(Collection)==44&&offsetof(Collection,key)==0x20&&sizeof(Node)==12);
struct Clone {
 Collection collection{};
 std::vector<Node> nodes;
 unsigned layout[2]{};
 std::string name;
 std::string eventId;
 const char* namePointer=nullptr;
 unsigned templateKey=0;
 unsigned ownerKey=0;
 bool race=true;
 bool correction=false;
 bool bin=false;
 bool placement=false;
 std::vector<unsigned> opponentKeys;
 bool character=false;
 bool gridValidated=false;
 bool gauntlet=false,hotPursuit=false;
 std::vector<unsigned> gridPositions;
 std::vector<std::unique_ptr<std::string>> strings;
 std::vector<std::unique_ptr<std::vector<BYTE>>> arrays;
 const mwel::EventPackage* package=nullptr;
};
static_assert(offsetof(Clone,collection)==0,"Native collection must remain the first member");
std::vector<std::unique_ptr<Clone>> clones;
bool enabled=false;
bool enumerateAdditional=true;
bool authoredGeometry=false;
Collection*(__thiscall* getCollection)(void*,unsigned);
Collection*(__cdecl* findCollection)(unsigned,unsigned);
unsigned(__thiscall* firstCollection)(void*);
unsigned(__thiscall* nextCollection)(void*,unsigned);
unsigned(__thiscall* countCollections)(void*);
void*(__thiscall* findVault)(void*,unsigned);
void*(__cdecl* originalEventLookup)(unsigned);
void*(__thiscall* originalParameterEvent)(void*);
void(__thiscall* originalActivateEvent)(void*,unsigned);
void(__thiscall* originalActivityActivate)(void*);
void*(__thiscall* originalParameterLookup)(void*,unsigned);
void(__cdecl* originalInitRacers)(void*);
void(__cdecl* originalWarpPlayerToTrigger)(void*);
void*(__thiscall* originalAttribute)(void*,unsigned,unsigned);
thread_local bool activatingOwned=false,initializingOwnedGrid=false;
thread_local Clone* initializingGridOwner=nullptr;
thread_local Clone* activatingOwner=nullptr;
bool(__thiscall* originalScopeContains)(void*,void*);
bool(__thiscall* originalScopeCommand)(void*,unsigned,void*);
void(__thiscall* originalSetParent)(Collection*,unsigned);
// Bounded lab objects live until process teardown. A release loader must add
// vault unload/retry ownership before this mechanism can be enabled normally.
std::map<unsigned,void*> geometryObjects;
Clone* Find(unsigned key);
bool PrepareExtendedGrid(Clone& clone);
void* __cdecl EventLookup(unsigned key){
 auto result=originalEventLookup(key);
 if(!worldLogEnabled)return result; // The remaining work is diagnostic only.
 bool own=false;for(const auto& clone:clones)if(clone->race&&!clone->correction&&!clone->eventId.empty()&&reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)(clone->eventId.c_str())==key)own=true;
 static unsigned queries=0;if(queries++<64&&mwel::fs::exists(game/L"MWEventLoader.runtime-validation-test"))Log("EVENT_OBJECT_QUERY key=%08X result=%08X owned=%u",key,unsigned(result),unsigned(own));
 if(own&&!result){
  static DWORD last=0;auto now=GetTickCount();if(now-last>3000){last=now;
   Log("EVENT_NATIVE_OBJECT_MISSING requested=%08X",key);
   BYTE* first=nullptr;Copy(reinterpret_cast<void*>(0x91DFE4),&first,4);auto object=first;
   for(unsigned n=0;object&&n<1024;++n){unsigned vtable=0;Collection* collection=nullptr;BYTE* next=nullptr;
    if(!Copy(object,&vtable,4)||!Copy(object+8,&collection,4)||!Copy(object+0x24,&next,4))break;
    Collection c{},parent{};if(collection&&Copy(collection,&c,sizeof(c))){if(c.parent)Copy(c.parent,&parent,sizeof(parent));
     if(vtable==0x8A3AE0)Log("EVENT_NATIVE_OBJECT_OBSERVED key=%08X parent=%08X object=%08X source=%08X",c.key,parent.key,unsigned(object),unsigned(c.source));
    }object=next;if(object==first)break;
   }
  }
 }
 return result;
}
void* __fastcall ParameterEvent(void* self,void*){
 unsigned words[5]{};if(worldLogEnabled){Copy(self,words,sizeof(words));
 static unsigned queries=0;if(queries++<32&&mwel::fs::exists(game/L"MWEventLoader.runtime-validation-test"))Log("EVENT_PARAMETER_QUERY self=%08X words=%08X,%08X,%08X,%08X,%08X",unsigned(self),words[0],words[1],words[2],words[3],words[4]);}
 auto result=originalParameterEvent(self);if(result)return result;
 Log("EVENT_PARAMETER_OBJECT_MISSING self=%08X words=%08X,%08X,%08X,%08X,%08X",unsigned(self),words[0],words[1],words[2],words[3],words[4]);
 // Career creates a native derived race object. Resolve that owned object
 // through the parameters' actual instance rather than guessing an ID alias.
 BYTE* instance=nullptr;Collection* collection=nullptr;
 if(!Copy(static_cast<BYTE*>(self)+8,&instance,4)||!instance||!Copy(instance+4,&collection,4)||!collection)return nullptr;
 Collection value{};Clone* owner=nullptr;Collection* visited[16]{};
 for(unsigned depth=0;collection&&depth<16;++depth){
  for(unsigned n=0;n<depth;++n)if(visited[n]==collection)return nullptr;visited[depth]=collection;
  if(!Copy(collection,&value,sizeof(value)))return nullptr;auto clone=Find(value.key);
  if(clone&&clone->race){owner=clone;break;}collection=value.parent;
 }
 if(!owner)return nullptr;
 auto actual=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(instance);
 result=originalEventLookup(actual);if(!result)return nullptr;
 Collection* eventCollection=nullptr;unsigned objectKey=0;
 if(!Copy(static_cast<BYTE*>(result)+8,&eventCollection,4)||!eventCollection||!Copy(static_cast<BYTE*>(static_cast<void*>(eventCollection))+0x20,&objectKey,4)||objectKey!=actual)return nullptr;
 Log("EVENT_NATIVE_PARAMETER_OBJECT_RESOLVED owner=%08X native_instance_key=%08X object=%08X template_object_used=0",owner->collection.key,actual,unsigned(result));
 return result;
}
bool GameplayClass(void* self);
void* GeometryObject(unsigned key,bool marker){
 auto clone=Find(key);if(!authoredGeometry||!clone||clone->race||clone->bin||clone->placement)return nullptr;
 bool isMarker=clone->templateKey==0xAAF8BB9C;
 if(isMarker!=marker)return nullptr;
 auto manager=*reinterpret_cast<BYTE**>(0x91E00C);
 unsigned capacity=0,table=0;
 if(!manager||!Copy(manager+0x6C,&capacity,4)||!Copy(manager+0x78,&table,4)||!table||capacity<clones.size()+16){Log("EXPERIMENT_REFUSED geometry_manager_capacity");return nullptr;}
 if(auto it=geometryObjects.find(key);it!=geometryObjects.end()){
  bool registered=false;for(unsigned i=0;i<capacity;++i){unsigned object=0;if(!Copy(reinterpret_cast<void*>(table+i*8+4),&object,4))return nullptr;registered|=object==unsigned(it->second);}
  Collection* owner=nullptr;unsigned actualKey=0;
  if(registered&&Copy(static_cast<BYTE*>(it->second)+8,&owner,4)&&owner&&Copy(static_cast<BYTE*>(static_cast<void*>(owner))+0x20,&actualKey,4)&&actualKey==key)return it->second;
  Log("EVENT_GEOMETRY_STALE_INSTANCE_RETIRED key=%08X object=%08X native_owner_released=1",key,unsigned(it->second));geometryObjects.erase(it);
 }
 unsigned occupied=0;for(unsigned i=0;i<capacity;++i){unsigned object=0;if(!Copy(reinterpret_cast<void*>(table+i*8+4),&object,4))return nullptr;occupied+=object!=0;}
 if(occupied+1>=capacity){Log("EXPERIMENT_REFUSED geometry_manager_full");return nullptr;}
 unsigned ctor=clone->character?0x5EF450:marker?0x5E8600:0x601790,size=clone->character?0x80:marker?0x40:0xCC;
 auto storage=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x5D29D0)(reinterpret_cast<void*>(0x925B30),size,0);
 if(!storage){Log("EXPERIMENT_REFUSED geometry_allocation");return nullptr;}
 auto object=reinterpret_cast<void*(__thiscall*)(void*,unsigned*)>(ctor)(storage,&key);
 // Objects constructed outside a vault still need the reference storage
 // normally supplied by that vault. Match GRaceParameters::Init's native
 // allocation contract; the native deleting destructor owns its release.
 auto references=reinterpret_cast<unsigned(__cdecl*)(unsigned)>(0x5E5330)(key);
 if(references>65536)throw std::runtime_error("owned geometry reference capacity invalid");
 if(references){
  auto entries=reinterpret_cast<void*(__cdecl*)(unsigned)>(0x652AD0)(references*8);
  if(!entries)throw std::runtime_error("owned geometry reference allocation failed");
  *reinterpret_cast<void**>(static_cast<BYTE*>(object)+0x1C)=entries;
  *reinterpret_cast<unsigned short*>(static_cast<BYTE*>(object)+0x1A)=0;
  static_cast<BYTE*>(object)[0x18]|=4;
 }
 if(worldLogEnabled&&clone->correction&&!clone->race&&!clone->character&&!marker){float position[3]{};Copy(static_cast<BYTE*>(object)+0x58,position,12);Log("EVENT_NATIVE_SCRIPT_TRIGGER_POSE key=%08X world=%.4f,%.4f,%.4f identity_unchanged=1",key,position[0],position[1],position[2]);}
 geometryObjects.emplace(key,object);
 Log("EXPERIMENT_NATIVE_GEOMETRY_OBJECT key=%08X kind=%s object=%08X constructor=%08X manager_free=%u",key,clone->character?"character":marker?"startgrid":"trigger",unsigned(object),ctor,capacity-occupied-1);
 return object;
}
bool ReleaseGeometry(unsigned owner,bool characters=false){
 // At an owned race's exit boundary, use native deleting destructors. They
 // unregister CARP triggers, release icons/particles, clear the inside-body
 // vector and detach GManager connections. Keeping active objects across
 // unrelated race instances retains the old event's callbacks and references.
 auto manager=*reinterpret_cast<BYTE**>(0x91E00C);unsigned capacity=0,table=0;
 if(!manager||!Copy(manager+0x6C,&capacity,4)||!Copy(manager+0x78,&table,4)||!table||capacity>65536)return false;
 auto registered=[&](void* object){for(unsigned i=0;i<capacity;++i){unsigned value=0;if(!Copy(reinterpret_cast<void*>(table+i*8+4),&value,4))return true;if(value==unsigned(object))return true;}return false;};
 for(auto it=geometryObjects.begin();it!=geometryObjects.end();){auto clone=Find(it->first);if(!clone||clone->ownerKey!=owner||clone->character!=characters){++it;continue;}
  auto object=static_cast<BYTE*>(it->second);auto key=it->first;
  if(!registered(object)){it=geometryObjects.erase(it);continue;}
  Collection* collection=nullptr;unsigned actual=0,vtable=0;
  if(!Copy(object+8,&collection,4)||!collection||!Copy(&collection->key,&actual,4)||actual!=key||!Copy(object,&vtable,4))return false;
  bool marker=clone->templateKey==0xAAF8BB9C;unsigned expected=clone->character?0x8A39EC:marker?0x8A39B0:0x8A3A84,destructor=clone->character?0x5E9C70:marker?0x5E87C0:0x5F1750;
  if(vtable!=expected||*reinterpret_cast<unsigned*>(vtable)!=destructor)return false;
  Log("EVENT_RUNTIME_GEOMETRY_RELEASE key=%08X owner=%08X object=%08X native_destructor=%08X",key,owner,unsigned(object),destructor);
  reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(destructor)(object,1);
  if(registered(object))return false;it=geometryObjects.erase(it);
 }
 Log("EVENT_RUNTIME_GEOMETRY_RELEASED owner=%08X native_lifecycle=1",owner);return true;
}
bool PrepareCharacters(Clone& owner){
 bool created=false;for(auto key:owner.opponentKeys){auto child=Find(key);if(!child||!child->character)continue;auto object=GeometryObject(key,false);auto native=reinterpret_cast<void*(__cdecl*)(unsigned)>(0x5E1880)(key);Log("EVENT_NATIVE_EXTRA_CHARACTER owner=%08X key=%08X object=%08X native_find=%08X original_attributes_modified=0",owner.collection.key,key,unsigned(object),unsigned(native));if(!object||native!=object)return false;created=true;}
 if(created){auto manager=*reinterpret_cast<void**>(0x91E00C);if(!manager)return false;reinterpret_cast<void(__thiscall*)(void*)>(0x5F18A0)(manager);}
 return true;
}
bool ReleaseCharacters(unsigned owner){
 return ReleaseGeometry(owner,true);
}
Clone* ActivityOwner(void* activity){
 Collection* collection=nullptr;Collection* visited[16]{};
 if(!Copy(static_cast<BYTE*>(activity)+8,&collection,4))return nullptr;
 for(unsigned depth=0;collection&&depth<16;++depth){
  for(unsigned n=0;n<depth;++n)if(visited[n]==collection)return nullptr;visited[depth]=collection;
  Collection value{};if(!Copy(collection,&value,sizeof(value)))return nullptr;
  if(auto clone=Find(value.key);clone&&clone->race)return clone;
  collection=value.parent;
 }
 return nullptr;
}
void __fastcall ActivityActivate(void* self,void*){
 // Career can activate a cached next-race activity without calling
 // ActivateEvent, FindVault or GRaceParameters::Init. Mount its own gates and
 // bind references before the native interpreter builds the start commands.
 // Do not recreate the activity or modify stock/story command programs.
 auto owner=authoredGeometry?ActivityOwner(self):nullptr;
 if(owner){
  for(const auto& child:clones)if(child->ownerKey==owner->collection.key&&!child->placement&&!GeometryObject(child->collection.key,child->templateKey==0xAAF8BB9C)){
   Log("EXPERIMENT_RACE_ABORT owned_activity_geometry_unavailable owner=%08X child=%08X",owner->collection.key,child->collection.key);return;
  }
  auto manager=*reinterpret_cast<void**>(0x91E00C);
  if(!manager){Log("EXPERIMENT_RACE_ABORT owned_activity_reference_manager_missing owner=%08X",owner->collection.key);return;}
  reinterpret_cast<void(__thiscall*)(void*)>(0x5F18A0)(manager);
  Log("EVENT_NATIVE_ACTIVITY_GEOMETRY_BOUND owner=%08X activity=%08X native_link_pass=1 before_commands=1",owner->collection.key,unsigned(self));
 }
 originalActivityActivate(self);
}
void* __fastcall ParameterLookup(void* self,void*,unsigned eventId){
 // The native database searches career records before temporary records.
 // Resolve a live owned derived instance from its database owner, never a
 // saved raw pointer, so Stop/roaming can retire it through native teardown.
 // The substitution is confined to ActivateEvent's synchronous call tree;
 // menus must keep their stable career record, not a temporary race record.
 BYTE* active=nullptr;unsigned mode=0,table=0;void* activity=nullptr;
 if(activatingOwned&&Copy(static_cast<BYTE*>(self)+0x2C,&active,4)&&active&&Copy(static_cast<BYTE*>(self)+0x30,&mode,4)&&mode==2&&Copy(active,&table,4)&&table==0x8A39C4&&Copy(active+0x14,&activity,4)&&activity&&reinterpret_cast<unsigned(__thiscall*)(void*)>(0x5FAB40)(active)==eventId){
  BYTE* instance=nullptr;Collection* collection=nullptr;Collection value{};
  if(Copy(active+8,&instance,4)&&instance&&Copy(instance+4,&collection,4)&&collection&&Copy(collection,&value,sizeof(value))&&value.parent){
   Collection parent{};if(Copy(value.parent,&parent,sizeof(parent)))if(auto owner=Find(parent.key);owner&&owner->race&&originalEventLookup(value.key)==activity)return active;
  }
 }
 return originalParameterLookup(self,eventId);
}
void __fastcall ActivateEvent(void* self,void*,unsigned eventId){
 Clone* owner=nullptr;for(const auto& clone:clones)if(clone->race&&!clone->eventId.empty()&&reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)(clone->eventId.c_str())==eventId){owner=clone.get();break;}
 if(!owner){originalActivateEvent(self,eventId);return;}
 // Read and validate the selected course before native activation reaches
 // its first AI query. Other packages stay idle until they are selected.
 if(!MWELPrepareNeoCourse(owner->collection.key))Log("EVENT_NEOAI_COURSE_PREWARM_UNAVAILABLE owner=%08X native_race_unaffected=1",owner->collection.key);
 if(owner->gauntlet||owner->hotPursuit){const bool ready=MWELPrepareGauntletRoad(owner->collection.key);Log("COMBAT_BACKUP_ROAD_PREWARM ready=%u",unsigned(ready));}
 // Reject an unsupported grid before creating a native activity. Returning
 // from InitRacers instead leaves the subsequent native audio/start commands
 // with no vehicles and can dereference a null racer (004F099A).
 if(authoredGeometry&&owner->gridPositions.size()>4){
  try{if(!PrepareExtendedGrid(*owner)){Log("EVENT_START_REFUSED owner=%08X reason=extended_grid_unavailable native_activation_called=0",owner->collection.key);return;}}
  catch(const std::exception& error){Log("EVENT_START_REFUSED owner=%08X reason=%s native_activation_called=0",owner->collection.key,error.what());return;}
 }
 // Corrections replace geometry without replacing the native event identity.
 // They need the same fresh activity and reference binding as additions when
 // the world is already loaded; FindVault is not called on this path.
 // FE's already-in-world branch expects a cached native activity. Added
 // courses own a temporary derived record instead. Use the same copy and
 // Init that native GManager::Init calls, without entering world loading.
 auto database=*reinterpret_cast<void**>(0x91E004);
 if(!database){Log("EXPERIMENT_RACE_ABORT owned_native_database_missing event=%08X",eventId);return;}
 auto selected=originalParameterLookup(database,eventId);
 if(!database||!selected||reinterpret_cast<unsigned(__thiscall*)(void*)>(0x5FAB40)(selected)!=eventId){Log("EXPERIMENT_RACE_ABORT owned_frontend_parameters_missing event=%08X",eventId);return;}
 unsigned count=0;if(!Copy(static_cast<BYTE*>(database)+4,&count,4)||count>=4){Log("EXPERIMENT_RACE_ABORT owned_native_parameters_capacity event=%08X",eventId);return;}
 auto race=*reinterpret_cast<BYTE**>(0x91E000);unsigned status=0;if(!race||!Copy(race+0x1960,&status,4)||status){Log("EXPERIMENT_RACE_ABORT owned_native_race_already_active event=%08X",eventId);return;}
 // An already-loaded world does not run FindVault again. Construct every
 // owned trigger before native Init builds its checkpoint/callback arrays;
 // mounting only the start marker later leaves those arrays full of nulls.
 if(authoredGeometry)for(const auto& child:clones)if(child->ownerKey==owner->collection.key&&!child->placement&&!GeometryObject(child->collection.key,child->templateKey==0xAAF8BB9C)){
  Log("EXPERIMENT_RACE_ABORT owned_activation_geometry_unavailable owner=%08X child=%08X",owner->collection.key,child->collection.key);return;
 }
 Log("EVENT_NATIVE_ACTIVATION_GEOMETRY_READY owner=%08X correction=%u before_native_init=1 world_reload=0",owner->collection.key,unsigned(owner->correction));
 auto parameters=reinterpret_cast<void*(__thiscall*)(void*,void*)>(0x603440)(database,selected);
 if(!parameters){Log("EXPERIMENT_RACE_ABORT owned_native_parameters_allocation event=%08X",eventId);return;}
 reinterpret_cast<void(__thiscall*)(void*,void*,unsigned)>(0x600AB0)(database,parameters,2);
 reinterpret_cast<void(__thiscall*)(void*,void*)>(0x5E39E0)(database,parameters);
 reinterpret_cast<void(__thiscall*)(void*)>(0x5FD370)(parameters);
 auto activity=originalParameterEvent(parameters);
 if(!activity){Log("EXPERIMENT_RACE_ABORT owned_native_activity_initialization event=%08X",eventId);return;}
 // GManager::Init and vault hot loads run this same native reference pass
 // after all instances exist. Without it the interpreter's AddCheckpoint,
 // WarpPlayerToTrigger and spawn arguments resolve to null in a loaded world.
 auto manager=*reinterpret_cast<void**>(0x91E00C);
 if(!manager){Log("EXPERIMENT_RACE_ABORT owned_native_reference_manager_missing");return;}
 reinterpret_cast<void(__thiscall*)(void*)>(0x5F18A0)(manager);
 unsigned short references=0;Copy(static_cast<BYTE*>(activity)+0x1A,&references,2);
 Log("EVENT_NATIVE_ACTIVITY_REFERENCES_BOUND owner=%08X activity=%08X references=%u native_link_pass=1",owner->collection.key,unsigned(activity),unsigned(references));
 Log("EVENT_NATIVE_DERIVED_START owner=%08X event=%08X parameters=%08X activity=%08X world_reload=0 native_lifetime=1",owner->collection.key,eventId,unsigned(parameters),unsigned(activity));
 bool previous=activatingOwned;auto previousOwner=activatingOwner;activatingOwner=owner;activatingOwned=true;originalActivateEvent(self,eventId);activatingOwned=previous;activatingOwner=previousOwner;
}
bool WarpOwnedPlayer(unsigned owner,void* start){
 // IVehicle::Warp uses the vehicle's lateral basis, whereas the authored
 // marker's direction describes the course tangent. Rotate only this call's
 // pose; the live trigger must retain its original direction for road lookup.
 alignas(4) BYTE pose[0x40]{};float direction[3]{},position[3]{};
 if(!Copy(start,pose,sizeof(pose))||!Copy(pose+0x34,direction,12)){
  Log("EXPERIMENT_RACE_ABORT owned_player_start_pose_unreadable owner=%08X",owner);return false;
 }
 float vehicleBasis[3]{-direction[2],direction[1],direction[0]};
 memcpy(pose+0x34,vehicleBasis,12);memcpy(position,pose+0x28,12);
 Log("EVENT_NATIVE_PLAYER_START_POSE owner=%08X position=%.3f,%.3f,%.3f marker_direction=%.4f,%.4f,%.4f warp_basis=%.4f,%.4f,%.4f native_warp=1",owner,position[0],position[1],position[2],direction[0],direction[1],direction[2],vehicleBasis[0],vehicleBasis[1],vehicleBasis[2]);
 originalWarpPlayerToTrigger(pose);return true;
}
void __cdecl WarpPlayerToTrigger(void* marker){
 // Restart commands run after ActivateEvent has returned and may resolve
 // their inherited marker to null. Resolve that owned command from the live
 // race parameters; valid roaming and stock command arguments stay native.
 auto owner=activatingOwned?activatingOwner:(!marker?Find(MWELNeoCurrentKey()):nullptr);
 if(owner&&owner->race&&authoredGeometry){
  for(const auto& child:clones)if(child->ownerKey==owner->collection.key&&!child->placement&&child->templateKey==0xAAF8BB9C){
   auto start=GeometryObject(child->collection.key,true);
   if(!start){Log("EXPERIMENT_RACE_ABORT owned_player_start_marker_unavailable");return;}
   float source[3]{},position[3]{};if(marker)Copy(static_cast<BYTE*>(marker)+0x28,source,12);Copy(static_cast<BYTE*>(start)+0x28,position,12);
   Log("EVENT_NATIVE_PLAYER_START_MARKER_BOUND owner=%08X incoming=%08X source_position=%.3f,%.3f,%.3f authored=%08X position=%.3f,%.3f,%.3f native_warp=1",owner->collection.key,unsigned(marker),source[0],source[1],source[2],unsigned(start),position[0],position[1],position[2]);
   WarpOwnedPlayer(owner->collection.key,start);return;
  }
  Log("EXPERIMENT_RACE_ABORT owned_player_start_marker_missing");return;
 }
 originalWarpPlayerToTrigger(marker);
}
void* __fastcall Attribute(void* self,void*,unsigned field,unsigned index){
 // Use the native explicit-placement path for owned grids. A source spawn's
 // forced position and the nearest-road fallback can both refer to another
 // course after a stock race. Each racer gets its own stable owned marker
 // collection; the native routine still creates and warps the vehicles.
 static unsigned noForcedPosition=0;
 if(initializingOwnedGrid&&initializingGridOwner&&field==0xFB84BE75){
  auto race=*reinterpret_cast<BYTE**>(0x91E000);unsigned count=0;
  if(race&&Copy(race+0x1958,&count,4)&&count<=16){
   for(unsigned slot=0;slot<count&&slot<initializingGridOwner->gridPositions.size();++slot){
    BYTE* spawn=nullptr;
    if(Copy(race+0x18+slot*0x194+4,&spawn,4)&&spawn&&spawn+4==self){
     auto key=&initializingGridOwner->gridPositions[slot];
     Log("EVENT_NATIVE_GRID_PLACEMENT owner=%08X slot=%u key=%08X source_spawn=%08X native_warp=1",initializingGridOwner->collection.key,slot,*key,unsigned(spawn));return key;
    }
   }
  }
  Log("EXPERIMENT_RACE_ABORT owned_grid_spawn_unresolved instance=%08X count=%u",unsigned(self),count);return &noForcedPosition;
 }
 return originalAttribute(self,field,index);
}
void __cdecl InitRacers(void* marker){
 // Inherited trigger programs may retain the source course's start reference.
 // Only an active owned course substitutes its own native GMarker; stock
 // event callbacks retain their original argument and native behavior.
 auto owner=MWELNeoCurrentKey();auto clone=Find(owner);
 Log("EVENT_NATIVE_GRID_ENTRY owner=%08X correction=%u incoming=%08X flow=%u preflight_validated=%u",owner,unsigned(clone&&clone->correction),unsigned(marker),*reinterpret_cast<unsigned*>(0x925E90),unsigned(clone&&clone->gridValidated));
 if(clone&&clone->race&&authoredGeometry){
  for(const auto& child:clones)if(child->ownerKey==owner&&!child->placement&&child->templateKey==0xAAF8BB9C){
   auto start=GeometryObject(child->collection.key,true);
   // Registration has already fitted every extended placement. Preserve the
   // native initialization here; it must never be skipped for a late query.
   if(!start){Log("EXPERIMENT_RACE_ABORT owned_start_marker_unavailable owner=%08X",owner);return;}
   // Command buffers can execute on the following frame, after ActivateEvent
   // has returned. Bind player placement at this actual owned grid boundary.
   {
    unsigned players=0;Copy(reinterpret_cast<void*>(0x92D884),&players,4);
    if(!players||players>8){Log("EXPERIMENT_RACE_ABORT owned_player_not_available count=%u",players);return;}
    Log("EVENT_NATIVE_PLAYER_START_COMMAND owner=%08X omitted_command_restored=1 world_reload=0",owner);
    if(!WarpOwnedPlayer(owner,start))return;
   }
   // A borrowed opponent attribute from another bin has no GCharacter in
   // this event's native vault. Extra rivals own real native character
   // instances, constructed before GetOpponent/InitRacers can dereference them.
   if(!PrepareCharacters(*clone))throw std::runtime_error("native opponent character unavailable");
   Log("EVENT_NATIVE_START_MARKER_BOUND owner=%08X key=%08X incoming=%08X authored=%08X",owner,child->collection.key,unsigned(marker),unsigned(start));
   float position[3]{};Copy(static_cast<BYTE*>(start)+0x28,position,12);
   Log("EVENT_NATIVE_GRID_POSITION owner=%08X position=%.3f,%.3f,%.3f owned_explicit_placements=%u",owner,position[0],position[1],position[2],unsigned(clone->gridPositions.size()));
   unsigned actual=0;auto race=*reinterpret_cast<BYTE**>(0x91E000);if(race)Copy(race+0x1958,&actual,4);
   Log("EVENT_NATIVE_FIELD_COUNT owner=%08X requested=%u actual=%u count_matches=%u native_capacity=16",owner,unsigned(clone->gridPositions.size()),actual,unsigned(actual==clone->gridPositions.size()));
   if(actual>16){Log("EXPERIMENT_RACE_ABORT authored_field_count_invalid_before_init");return;}
   bool previous=initializingOwnedGrid;auto previousOwner=initializingGridOwner;initializingGridOwner=clone;initializingOwnedGrid=true;originalInitRacers(start);initializingOwnedGrid=previous;initializingGridOwner=previousOwner;unsigned initialized=0;if(race)Copy(race+0x1958,&initialized,4);Log("EVENT_NATIVE_FIELD_COUNT_AFTER_INIT owner=%08X requested=%u actual=%u count_matches=%u",owner,unsigned(clone->gridPositions.size()),initialized,unsigned(initialized==clone->gridPositions.size()));if(!initialized||initialized>16||initialized!=(clone->gauntlet?1u:unsigned(clone->gridPositions.size())))Log("EXPERIMENT_RACE_ABORT authored_field_count_mismatch_after_init");return;
  }
  Log("EXPERIMENT_RACE_ABORT owned_start_marker_missing owner=%08X",owner);return;
 }
 originalInitRacers(marker);
}
void __fastcall SetParent(Collection* self,void*,unsigned key){
 auto clone=Find(key);
 if(!clone||!GameplayClass(self->klass)){originalSetParent(self,key);return;}
 originalSetParent(self,0);
 self->parent=&clone->collection;++clone->collection.references;
 Log("EXPERIMENT_OWNED_PARENT child=%08X parent=%08X",self->key,key);
}
bool __fastcall ScopeContains(void* self,void*,void* candidate){
 // Owned scopes can refer to their race or its original template ancestry.
 // Native story commands keep their original marker keys, including when an
 // authored trigger has moved. Ordinary scopes retain the original lookup.
 auto instance=static_cast<BYTE*>(self)+4;
 unsigned key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(instance);
 static unsigned observations=0;
 if(observations++<16)Log("EXPERIMENT_SCOPE_OBSERVATION self=%08X key=%08X candidate=%08X",unsigned(self),key,unsigned(candidate));
 auto clone=Find(key);
 if(!clone){unsigned parent=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x4546A0)(instance);clone=Find(parent);}
 if(!clone)return originalScopeContains(self,candidate);
 auto data=reinterpret_cast<unsigned*(__thiscall*)(void*,unsigned,unsigned)>(0x454810)(static_cast<BYTE*>(candidate)+4,0xA0697302,0);
 unsigned linked=0;if(!data||!Copy(data,&linked,4))return false;
 if(linked==clone->collection.key||clone->ownerKey&&linked==clone->ownerKey)return true;
 auto klass=clone->collection.klass;auto owner=Find(clone->ownerKey);
 auto source=getCollection(klass,owner&&owner->race?owner->templateKey:clone->templateKey);
 for(unsigned depth=0;source&&depth<64;++depth){Collection value{};if(!Copy(source,&value,sizeof(value)))return false;if(value.key==linked)return true;source=value.parent;}
 return false;
}
bool __fastcall ScopeCommand(void* self,void*,unsigned eventKey,void* command){
 if(originalScopeCommand(self,eventKey,command))return true;
 auto instance=static_cast<BYTE*>(self)+4;
 auto key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(instance);
 auto clone=Find(key);
 if(!clone)clone=Find(reinterpret_cast<unsigned(__thiscall*)(void*)>(0x4546A0)(instance));
 if(!clone)return false;
 auto data=reinterpret_cast<unsigned*(__thiscall*)(void*,unsigned,unsigned)>(0x454810)(static_cast<BYTE*>(command)+4,0x918C796E,0);
 unsigned trigger=0;if(!data||!Copy(data,&trigger,4)||trigger!=eventKey)return false;
 data=reinterpret_cast<unsigned*(__thiscall*)(void*,unsigned,unsigned)>(0x454810)(static_cast<BYTE*>(command)+4,0x857FE432,0);
 unsigned scope=0;if(!data||!Copy(data,&scope,4))return false;
 auto source=getCollection(clone->collection.klass,clone->templateKey);
 for(unsigned depth=0;source&&depth<64;++depth){Collection value{};if(!Copy(source,&value,sizeof(value)))return false;if(value.key==scope){Log("EXPERIMENT_TEMPLATE_COMMAND_DEPENDENCY event=%08X scope=%08X",eventKey,scope);return true;}source=value.parent;}
 return false;
}
void* __fastcall FindVault(void* self,void*,unsigned key){
 if(auto clone=Find(key);clone&&clone->race){
  // Database enumeration asks for vaults before the runtime-object manager
  // exists. Keep that lookup read-only; mount objects when it is available.
  BYTE* manager=nullptr;unsigned capacity=0,table=0;
  bool managerReady=Copy(reinterpret_cast<void*>(0x91E00C),&manager,4)&&manager&&Copy(manager+0x6C,&capacity,4)&&Copy(manager+0x78,&table,4)&&table&&capacity>=clones.size()+16;
  if(authoredGeometry&&managerReady)for(const auto& child:clones)if(child->ownerKey==key&&!child->placement&&!GeometryObject(child->collection.key,child->templateKey==0xAAF8BB9C)){
   Log("EXPERIMENT_RACE_ABORT owned_geometry_unavailable event=%08X child=%08X",key,child->collection.key);return nullptr;
  }
  // Only the inherited-route experiment uses a native vault as a geometry
  // dependency. An authored event must eventually own its complete geometry.
  auto dependency=findVault(self,clone->templateKey);
  Log("EXPERIMENT_TEMPLATE_VAULT_DEPENDENCY custom=%08X template=%08X vault=%08X",key,clone->templateKey,unsigned(dependency));
  return dependency;
 }
 return findVault(self,key);
}
bool GameplayClass(void* self){unsigned key=0;return Copy(self,&key,4)&&key==0x5CEA9D46;}
Clone* Find(unsigned key){thread_local mwel_lookup::IndexCache<> cache;return cache.Find(clones,key);}
const char* OwnString(Clone& clone,const char* pointer){
 if(!pointer)return nullptr;
 auto value=std::make_unique<std::string>();
 for(unsigned i=0;i<4096;++i){char c=0;if(!Copy(pointer+i,&c,1))throw std::runtime_error("native string unreadable");if(!c){auto result=value->c_str();clone.strings.push_back(std::move(value));return result;}value->push_back(c);}
 throw std::runtime_error("native string unterminated");
}
void OwnNodeData(Clone& clone,Node& node){
 auto flags=node.flags>>24,type=node.flags&0xffff;
 if(flags==0x20){if(type==1)node.value=reinterpret_cast<unsigned>(OwnString(clone,reinterpret_cast<const char*>(node.value)));return;}
 if(flags==0&&type==0x1c){auto bytes=std::make_unique<std::vector<BYTE>>(12);if(!Copy(reinterpret_cast<void*>(node.value),bytes->data(),12))throw std::runtime_error("native vector unreadable");node.value=reinterpret_cast<unsigned>(bytes->data());clone.arrays.push_back(std::move(bytes));return;}
 if(!(flags&2))throw std::runtime_error("unsupported borrowed scalar storage");
 if(flags&0x10)throw std::runtime_error("unsupported layout array storage");
 unsigned short header[4]{};if(!Copy(reinterpret_cast<void*>(node.value),header,sizeof(header)))throw std::runtime_error("native array header unreadable");
 auto capacity=header[0],count=header[1],stride=header[2];unsigned offset=8+((header[3]&0x8000)?8:0);
 Log("EXPERIMENT_ARRAY_OWNERSHIP key=%08X type=%u capacity=%u count=%u stride=%u flags=%04X",node.key,type,capacity,count,stride,header[3]);
 if(count>capacity||capacity>8192||stride>256||!stride)throw std::runtime_error("unsupported native array layout");
 auto bytes=std::make_unique<std::vector<BYTE>>(offset+unsigned(capacity)*stride);
 if(!Copy(reinterpret_cast<void*>(node.value),bytes->data(),bytes->size()))throw std::runtime_error("native array unreadable");
 if(type==1){if(stride!=4)throw std::runtime_error("unexpected string array stride");for(unsigned i=0;i<count;++i){auto slot=reinterpret_cast<unsigned*>(bytes->data()+offset+i*stride);*slot=reinterpret_cast<unsigned>(OwnString(clone,reinterpret_cast<const char*>(*slot)));}}
 if(type==0x30&&stride==12)for(unsigned i=0;i<count;++i)*reinterpret_cast<unsigned*>(bytes->data()+offset+i*stride+8)=0;
 node.value=reinterpret_cast<unsigned>(bytes->data());clone.arrays.push_back(std::move(bytes));
}
Collection* __fastcall GetCollection(void* self,void*,unsigned key){unsigned classKey=0;if(Copy(self,&classKey,4))if(auto fx=MWELCarbonEffectCollection(classKey,key))return static_cast<Collection*>(fx);if(GameplayClass(self))if(auto c=Find(key)){Log("EXPERIMENT_COLLECTION_LOOKUP key=%08X",key);return &c->collection;}return getCollection(self,key);}
Collection* __cdecl FindCollection(unsigned classKey,unsigned key){if(auto fx=MWELCarbonEffectCollection(classKey,key))return static_cast<Collection*>(fx);if(classKey==0x5CEA9D46)if(auto c=Find(key)){Log("EXPERIMENT_GLOBAL_LOOKUP key=%08X",key);return &c->collection;}return findCollection(classKey,key);}
unsigned AppendedAfter(unsigned key){bool found=!key;for(auto& clone:clones){if(clone->correction||clone->placement)continue;if(found)return clone->collection.key;if(clone->collection.key==key)found=true;}return 0;}
unsigned __fastcall FirstCollection(void* self,void*){unsigned key=firstCollection(self);return key?key:(enumerateAdditional&&GameplayClass(self)?AppendedAfter(0):0);}
unsigned __fastcall NextCollection(void* self,void*,unsigned key){
 if(GameplayClass(self)){
  if(!enumerateAdditional)return nextCollection(self,key);
  if(auto clone=Find(key);clone&&!clone->correction)return AppendedAfter(key);
  unsigned next=nextCollection(self,key);if(!next&&AppendedAfter(0))Log("EXPERIMENT_ENUMERATION_APPEND after=%08X",key);return next?next:AppendedAfter(0);
 }
 return nextCollection(self,key);
}
unsigned __fastcall CountCollections(void* self,void*){unsigned appended=0;for(auto& c:clones)appended+=!c->correction&&!c->placement;return countCollections(self)+(enumerateAdditional&&GameplayClass(self)?appended:0);}
unsigned Hash(const char* text){return reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)(text);}
using Fields=std::map<unsigned,Node>;
Fields ReadFields(Clone& clone){Fields fields;for(auto& node:clone.nodes)if(node.value!=unsigned(&node))fields[node.key]=node;return fields;}
void Rebuild(Clone& clone,const Fields& fields){
 auto size=unsigned(fields.size()*2+1);if(size>4096)throw std::runtime_error("owned field table too large");
 clone.nodes.assign(size,{});std::vector<unsigned> probes(size,0);
 for(auto& pair:fields){unsigned home=pair.first%size,slot=home,probe=0;while(clone.nodes[slot].key){slot=(slot+1)%size;++probe;}
  if(probe>255)throw std::runtime_error("owned field probe bound exceeded");clone.nodes[slot]=pair.second;clone.nodes[slot].flags&=0xff00ffff;probes[home]=std::max(probes[home],probe);
 }
 unsigned worst=0;for(unsigned i=0;i<size;++i){clone.nodes[i].flags|=probes[i]<<16;worst=std::max(worst,probes[i]);if(!clone.nodes[i].key)clone.nodes[i].value=unsigned(&clone.nodes[i]);}
 clone.collection.nodes=clone.nodes.data();clone.collection.capacity=size;clone.collection.count=unsigned(fields.size());clone.collection.keyShift=0;clone.collection.worstCollision=static_cast<unsigned short>(worst);
}
void Scalar(Fields& fields,const char* name,unsigned value,unsigned type){auto key=Hash(name);fields[key]={key,value,0x20000000|type};}
void Text(Clone& clone,Fields& fields,const char* name,const std::string& value){
 auto owned=std::make_unique<std::string>(value);Scalar(fields,name,unsigned(owned->c_str()),1);clone.strings.push_back(std::move(owned));
}
void Vector(Clone& clone,Fields& fields,const char* name,const mwel::Json& vector){
 auto bytes=std::make_unique<std::vector<BYTE>>(12);for(unsigned i=0;i<3;++i){float v=vector.at(i).get<float>();memcpy(bytes->data()+i*4,&v,4);}auto key=Hash(name);fields[key]={key,unsigned(bytes->data()),0x1c};clone.arrays.push_back(std::move(bytes));
}
bool PrepareExtendedGrid(Clone& clone){
 clone.gridValidated=false;
 const mwel::EventPackage* package=nullptr;for(const auto& p:packages)if(p.nativeNamespace==clone.name){package=&p;break;}
 if(!package)return false;const auto& gate=package->authoring.at("gates").front();const auto& position=gate.at("position");
 mwel_grid::Position origin{-position.at(1).get<float>(),position.at(2).get<float>(),position.at(0).get<float>()};
 mwel_grid::Layout layout;if(!MWELFitGrid(clone.collection.key,unsigned(clone.gridPositions.size()),origin,gate.at("rotation").get<float>(),layout))return false;
 // Stage every owned vector pointer before mutating any placement. The
 // records remain attribute-only and keep stable identities across retries.
 float* destinations[16]{};
 for(unsigned i=0;i<layout.count;++i){auto placement=Find(clone.gridPositions[i]);if(!placement||!placement->placement)return false;
  for(auto& node:placement->nodes)if(node.key==Hash("Position")&&node.flags==0x1c){
   for(auto& array:placement->arrays)if(array->size()==12&&unsigned(array->data())==node.value)destinations[i]=reinterpret_cast<float*>(array->data());
  }if(!destinations[i])return false;
 }
 for(unsigned i=0;i<layout.count;++i){const auto& p=layout.positions[i];float editor[]={p[2],-p[0],p[1]};memcpy(destinations[i],editor,12);}
 clone.gridValidated=true;
 Log("EVENT_EXTENDED_GRID_READY owner=%08X count=%u columns=%u road_support_checked=1 walls_checked=1 owned_vectors=1 before_vehicle_generation=1",clone.collection.key,layout.count,layout.columns);return true;
}
void References(Clone& clone,Fields& fields,const char* name,const std::vector<unsigned>& keys){
 if(keys.size()>8192)throw std::runtime_error("owned references exceed limit");auto bytes=std::make_unique<std::vector<BYTE>>(8+keys.size()*4);
 unsigned short header[]={static_cast<unsigned short>(keys.size()),static_cast<unsigned short>(keys.size()),4,0x30};memcpy(bytes->data(),header,8);if(!keys.empty())memcpy(bytes->data()+8,keys.data(),keys.size()*4);
 auto key=Hash(name);fields[key]={key,unsigned(bytes->data()),0x02000030};clone.arrays.push_back(std::move(bytes));
}
void Strings(Clone& clone,Fields& fields,const char* name,const std::vector<std::string>& values){
 if(values.size()>8192)throw std::runtime_error("owned strings exceed limit");auto bytes=std::make_unique<std::vector<BYTE>>(8+values.size()*4);
 unsigned short header[]={static_cast<unsigned short>(values.size()),static_cast<unsigned short>(values.size()),4,1};memcpy(bytes->data(),header,8);
 for(unsigned i=0;i<values.size();++i){auto text=std::make_unique<std::string>(values[i]);auto pointer=unsigned(text->c_str());memcpy(bytes->data()+8+i*4,&pointer,4);clone.strings.push_back(std::move(text));}
 auto key=Hash(name);fields[key]={key,unsigned(bytes->data()),0x02000001};clone.arrays.push_back(std::move(bytes));
}
std::vector<unsigned> SourceReferences(Clone& clone,const char* field){
 auto source=getCollection(clone.collection.klass,clone.templateKey);auto key=Hash(field);Collection* visited[64]{};
 for(unsigned depth=0;source&&depth<64;++depth){
  for(unsigned previous=0;previous<depth;++previous)if(visited[previous]==source)throw std::runtime_error("native opponent inheritance cycle");visited[depth]=source;
  Collection value{};if(!Copy(source,&value,sizeof(value))||value.capacity>4096)throw std::runtime_error("native opponent table unreadable");
  for(unsigned slot=0;slot<value.capacity;++slot){
   Node node{};if(!Copy(value.nodes+slot,&node,sizeof(node)))throw std::runtime_error("native opponent field unreadable");
   if(node.key!=key)continue;
   unsigned short header[4]{};
   if((node.flags&0xffff)!=0x30||!(node.flags&0x02000000)||!Copy(reinterpret_cast<void*>(node.value),header,sizeof(header))||header[1]>header[0]||header[0]>8192||header[2]!=4)throw std::runtime_error("native opponent reference layout unsupported");
   unsigned offset=8+((header[3]&0x8000)?8:0);std::vector<unsigned> result(header[1]);
   if(!result.empty()&&!Copy(reinterpret_cast<void*>(node.value+offset),result.data(),result.size()*4))throw std::runtime_error("native opponent references unreadable");return result;
  }
  source=value.parent;
 }
 return {};
}
std::vector<unsigned> SourceOpponents(Clone& clone){return SourceReferences(clone,"Opponents");}
void PreserveNativeField(Clone& clone,Fields& fields,Collection* source,const char* name){
 const unsigned key=Hash(name);Collection* visited[64]{};
 for(unsigned depth=0;source&&depth<64;++depth){
  for(unsigned previous=0;previous<depth;++previous)if(visited[previous]==source)throw std::runtime_error("native field inheritance cycle");visited[depth]=source;
  Collection value{};if(!Copy(source,&value,sizeof(value))||value.capacity>4096)throw std::runtime_error("native field source unreadable");
  for(unsigned slot=0;slot<value.capacity;++slot){Node node{};if(!Copy(value.nodes+slot,&node,sizeof(node)))throw std::runtime_error("native field unreadable");
   if(node.key==key){OwnNodeData(clone,node);fields[key]=node;Log("EVENT_NATIVE_STORY_FIELD_PRESERVED owner=%08X field=%s type=%04X",clone.collection.key,name,node.flags&0xffff);return;}
  }source=value.parent;
 }
}
void PreserveNativeMarkers(Clone& raceClone,const mwel::EventPackage& package,void* klass,std::vector<unsigned>& children){
 if(!mwel::Correction(package.authoring))return;
 const auto sourceChildren=SourceReferences(raceClone,"Children");std::set<unsigned> selected,used;
 for(const char* field:{"Checkpoint","Shortcuts","Opponents"})for(unsigned key:SourceReferences(raceClone,field))used.insert(key);
 auto raceSource=getCollection(klass,raceClone.templateKey);
 for(const char* field:{"racestart","racefinish"}){auto value=reinterpret_cast<unsigned*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(raceSource,Hash(field),0);unsigned key=0;if(value&&Copy(value,&key,4))used.insert(key);}
 std::vector<unsigned> sourceExtras;
 for(unsigned key:sourceChildren){if(used.count(key))continue;auto source=getCollection(klass,key);if(source&&reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,Hash("Position"),0))sourceExtras.push_back(key);}
 const auto extras=package.authoring.value("extras",mwel::Json::array());bool completeLegacy=extras.size()==sourceExtras.size();
 for(unsigned i=0;completeLegacy&&i<extras.size();++i){auto source=getCollection(klass,sourceExtras[i]);auto parent=source?source->parent:nullptr;
  completeLegacy=extras[i].value("nativeSourceName",std::string()).empty()&&extras[i].value("name",std::string())==package.authoring.at("name").get<std::string>()+"/editor_marker_"+std::to_string(i)&&parent&&parent->key==Hash(extras[i].value("nativeTemplate",std::string()).c_str());
 }
 unsigned index=0;
 for(const auto& marker:extras){
  const unsigned ordinal=index++;if(marker.value("nativeTemplate","")=="speedtrap")continue;
  unsigned match=0;const auto identity=marker.value("nativeSourceName",std::string());
  if(completeLegacy)match=sourceExtras[ordinal];
  else for(unsigned key:sourceExtras){
   auto source=getCollection(klass,key);if(!source)continue;
   if(!identity.empty()){
    if(identity.rfind(package.authoring.at("correctionTarget").at("name").get<std::string>()+"/",0)!=0||Hash(identity.c_str())!=key)continue;
   }else{
    auto position=reinterpret_cast<float*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,Hash("Position"),0);float p[3]{};if(!position||!Copy(position,p,12))continue;
    bool equal=true;for(unsigned k=0;k<3;++k)equal&=std::abs(p[k]-marker.at("position").at(k).get<float>())<.02f;if(!equal)continue;
   }
   if(match&&match!=key)throw std::runtime_error("Ambiguous native marker binding; refresh the event source");match=key;
  }
  if(!match){Log("EVENT_NATIVE_MARKER_UNBOUND owner=%08X authored=%s source_identity_required=1",raceClone.collection.key,marker.at("name").get<std::string>().c_str());continue;}
  if(!selected.insert(match).second||Find(match))throw std::runtime_error("Duplicate native story marker binding");
  auto source=getCollection(klass,match);float native[3]{};auto data=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,Hash("Position"),0);bool moved=!data||!Copy(data,native,12);
  for(unsigned k=0;k<3;++k)moved|=std::abs(native[k]-marker.at("position").at(k).get<float>())>.001f;
  auto rotation=reinterpret_cast<float*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,Hash("Rotation"),0);float nativeRotation=0;if(rotation)Copy(rotation,&nativeRotation,4);moved|=std::abs(nativeRotation-marker.at("rotation").get<float>())>.001f;
  const auto type=marker.value("nativeTemplate",std::string());
  if(moved&&type!="carmarker"&&type!="marker"){
   auto child=std::make_unique<Clone>();child->race=false;child->correction=true;child->ownerKey=raceClone.collection.key;child->templateKey=source->parent?source->parent->key:0;
   if(!Copy(source,&child->collection,sizeof(Collection))||child->collection.capacity>4096||!Copy(child->collection.layout,child->layout,8))throw std::runtime_error("native marker source unreadable");
   child->nodes.resize(child->collection.capacity);if(!Copy(child->collection.nodes,child->nodes.data(),child->nodes.size()*sizeof(Node)))throw std::runtime_error("native marker fields unreadable");
   for(unsigned i=0;i<child->nodes.size();++i){auto& node=child->nodes[i];if(node.value==unsigned(child->collection.nodes+i))node.value=unsigned(&node);else OwnNodeData(*child,node);}
   child->name=identity.empty()?package.authoring.at("correctionTarget").at("name").get<std::string>()+"/native_marker_"+std::to_string(ordinal):identity;
   child->namePointer=child->name.c_str();child->collection.key=match;child->collection.references=1;child->collection.nodes=child->nodes.data();child->collection.layout=child->layout;child->collection.name=&child->namePointer;
   child->layout[0]=unsigned(OwnString(*child,reinterpret_cast<const char*>(child->layout[0])));
   auto fields=ReadFields(*child);Vector(*child,fields,"Position",marker.at("position"));Vector(*child,fields,"Dimensions",marker.at("dimensions"));
   float angle=marker.at("rotation").get<float>();unsigned bits=0;memcpy(&bits,&angle,4);Scalar(fields,"Rotation",bits,9);
   if(type=="planetrigger"){float width=marker.at("dimensions").at(1).get<float>();memcpy(&bits,&width,4);Scalar(fields,"Width",bits,9);}
   Rebuild(*child,fields);clones.push_back(std::move(child));
  }
  if(std::find(children.begin(),children.end(),match)==children.end())children.push_back(match);
  Log("EVENT_NATIVE_STORY_MARKER_RETAINED owner=%08X key=%08X authored_transform=%u script_identity_preserved=1 legacy_complete=%u",raceClone.collection.key,match,unsigned(moved),unsigned(completeLegacy));
 }
}

void RaceProperties(Clone& clone,Fields& fields,const mwel::EventPackage& package,void* klass){
 if(!mwel::Correction(package.authoring)){
  // A template selects assets/geometry, not its career script. An ordinary
  // additional race must not inherit the prologue's movie/player-car/rolling
  // start or post-race activity, even when it was authored from a solo pursuit.
  // Start from the selected mode's defaults and the authored properties.
  // A deny-list cannot cover third-party scripts or every challenge field.
  // The template's EventID node has already been rebound to owned storage.
  auto identity=fields.at(Hash("EventID"));fields.clear();fields.emplace(Hash("EventID"),identity);
  Text(clone,fields,"Name",clone.name);
  Scalar(fields,"Template",0,6);Scalar(fields,"DoCountdown",1,6);
  Log("EVENT_ADDITIONAL_START_POLICY owner=%08X career_script_inherited=0 native_countdown=1 player_car_forced=0",clone.collection.key);
 }
 if(mwel::Correction(package.authoring)){
  // Keep effective native registration flags before replacing the race-mode
  // parent. Challenge/prologue races are not new blacklist entries.
  auto source=getCollection(klass,clone.templateKey);
  for(const char* field:{"DoCountdown","RollingStart","NoPostRaceScreen","CheckpointsVisible","ForceHeatLevel","MaxHeatLevel","InitialPlayerSpeed","PlayerCarType","IntroMovie","OutroMovie","IntroNIS","OutroNIS","PostRaceActivity"})PreserveNativeField(clone,fields,source,field);
  for(const char* name:{"ChallengeSeriesRace","DDayRace","ProgressionLevel"}){
   auto value=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,Hash(name),0);
   if(!value)continue;
   if(std::strcmp(name,"ProgressionLevel")==0){unsigned level=0;if(!Copy(value,&level,4))throw std::runtime_error("native registration level unreadable");Scalar(fields,name,level,8);Log("EVENT_NATIVE_REGISTRATION_PRESERVED owner=%08X field=%s value=%u",clone.collection.key,name,level);}
   else{BYTE flag=0;if(!Copy(value,&flag,1)||flag>1)throw std::runtime_error("native registration flag unreadable");Scalar(fields,name,unsigned(flag),6);Log("EVENT_NATIVE_REGISTRATION_PRESERVED owner=%08X field=%s value=%u",clone.collection.key,name,unsigned(flag));}
  }
 }
 const auto mode=package.authoring.at("mode").get<std::string>();
 if(mode=="tollboothrace"&&!mwel::Correction(package.authoring))Text(clone,fields,"IntroNIS","IntroNisToll01");
 clone.gauntlet=package.authoring.value("combatMode",std::string("none"))=="gauntlet";
 clone.hotPursuit=package.authoring.value("combatMode",std::string("none"))=="hotpursuit";
 if(clone.gauntlet){
  Text(clone,fields,"IntroNIS","IntroNisToll01");
  Log("GAUNTLET_START_POLICY owner=%08X intro=IntroNisToll01 ordinary_cp_distinct=1 authored_time_bonus_preserved=1",clone.collection.key);
 }
 // This opt-in controls presentation only. Do not alter trigger membership,
 // time bonuses, or the source story scripts. Clones never write stock fields.
 const bool visibleCheckpoints=mwel::Feature(package.authoring,"checkpointsVisible");
 Scalar(fields,"CheckpointsVisible",visibleCheckpoints?1u:0u,6);
 Log("EVENT_CHECKPOINT_VISIBILITY owner=%08X visible=%u native_effect=1 next_radar=1 stock_collection_modified=0",clone.collection.key,unsigned(visibleCheckpoints));
 const std::string parent=mode=="sprint"?"p2p":mode;
 auto selected=getCollection(klass,Hash(parent.c_str()));if(!selected)throw std::runtime_error("race mode template unavailable");
 clone.collection.parent=selected;
 const std::map<std::string,std::string> icons={{"sprint","p2p"},{"lapknockout","knockout"},{"speedtraprace","speedtrap"},{"tollboothrace","tollbooth"}};
 auto icon=icons.find(mode);Text(clone,fields,"EventIconType",icon==icons.end()?mode:icon->second);
 if(mode=="drag")Text(clone,fields,"TrafficPattern","drag");
 else Text(clone,fields,"TrafficPattern",package.authoring.value("trafficPattern",std::string()));
 const unsigned racers=package.authoring.at("racerCount").get<unsigned>();auto source=SourceOpponents(clone);std::vector<unsigned> roster;
 const bool generic=!clone.gauntlet&&racers>1&&source.empty();
 if(generic){
  // Solo time-trials and pursuit events have no resident opponent list.
  // Use the global native character archetype and give every entrant owned
  // attributes; never borrow a character from another frontend vault.
  auto prototype=getCollection(klass,Hash("character"));
  if(!prototype)throw std::runtime_error("native character archetype unavailable");
  source.push_back(Hash("character"));
  Log("EVENT_NATIVE_GENERIC_OPPONENT_SOURCE owner=%08X requested=%u archetype=character unrelated_vaults_borrowed=0",clone.collection.key,racers-1);
 }
 // GCharacter::Find caches by attribute key. Additional slots own distinct
 // keys/instances cloned from this vault's own rivals; another bin's character
 // cannot be borrowed safely after its frontend attribute vault is retired.
 const unsigned needed=clone.gauntlet?0u:racers-1;if(!generic)mwel_roster::AppendUnique(roster,source,needed);
 while(roster.size()<needed){unsigned slot=unsigned(roster.size()),sourceKey=source[slot%source.size()];auto original=getCollection(klass,sourceKey);auto rival=std::make_unique<Clone>();rival->race=false;rival->correction=true;rival->character=true;rival->ownerKey=clone.collection.key;rival->templateKey=sourceKey;
  if(!original||!Copy(original,&rival->collection,sizeof(Collection))||rival->collection.key!=sourceKey||rival->collection.capacity>4096||!Copy(rival->collection.layout,rival->layout,8))throw std::runtime_error("opponent attribute source unreadable");
  rival->nodes.resize(rival->collection.capacity);if(!Copy(rival->collection.nodes,rival->nodes.data(),rival->nodes.size()*sizeof(Node)))throw std::runtime_error("opponent fields unreadable");
  for(unsigned i=0;i<rival->nodes.size();++i){auto& node=rival->nodes[i];if(node.value==unsigned(rival->collection.nodes+i)){node.value=unsigned(&node);continue;}OwnNodeData(*rival,node);}
  rival->name=package.nativeNamespace+"_opponent_"+std::to_string(slot);rival->namePointer=rival->name.c_str();unsigned key=Hash(rival->name.c_str());if(!key||getCollection(klass,key)||Find(key))throw std::runtime_error("opponent identity collision");
  rival->collection.key=key;rival->collection.references=1;rival->collection.nodes=rival->nodes.data();rival->collection.layout=rival->layout;rival->collection.name=&rival->namePointer;
  // Keep the source's resident template parent, never the frontend-only
  // source collection itself. Every concrete field/string/array is owned.
  rival->layout[0]=unsigned(OwnString(*rival,reinterpret_cast<const char*>(rival->layout[0])));
  // Keep one story rival. Extra entrants are ordinary racers, not duplicates
  // of its named preset or fixed spawn command. Select their model at spawn.
  auto rivalFields=ReadFields(*rival);Scalar(rivalFields,"Template",0,6);Text(*rival,rivalFields,"PresetRide","");Text(*rival,rivalFields,"CarType","");Text(*rival,rivalFields,"RacerName","");Scalar(rivalFields,"ForceStartPosition",0,0x30);Rebuild(*rival,rivalFields);
  Log("EVENT_NATIVE_EXTRA_CHARACTER_ATTRIBUTES owner=%08X slot=%u source=%08X key=%08X original_character_modified=0",clone.collection.key,slot+1,sourceKey,key);roster.push_back(key);clones.push_back(std::move(rival));
 }
 clone.opponentKeys=roster;
 Log("EVENT_NATIVE_OPPONENT_ROSTER owner=%08X requested=%u unique=%u owned_extra=%u native_character_cache_keys_distinct=1 unrelated_vaults_borrowed=0",clone.collection.key,needed,unsigned(roster.size()),unsigned(std::max(0,int(needed)-int(source.size()))));
 References(clone,fields,"Opponents",roster);
 Log("EXPERIMENT_AUTHORED_PROPERTIES mode=%s parent=%08X racers=%u opponents=%u icon=%s title_binding_pending=1",mode.c_str(),Hash(parent.c_str()),racers,unsigned(roster.size()),(icon==icons.end()?mode:icon->second).c_str());
}
void AuditRaceProperties(Clone& clone,const mwel::EventPackage& package){
 auto field=[&](const char* name,unsigned expected){
  auto pointer=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(&clone.collection,Hash(name),0);unsigned actual=0;
  if(!pointer||!Copy(pointer,&actual,4)||actual!=expected)throw std::runtime_error(std::string("native race property lookup differs: ")+name);
  Log("EXPERIMENT_NATIVE_PROPERTY key=%08X name=%s value=%u native_lookup_verified=1",clone.collection.key,name,actual);
 };
 auto visibility=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(&clone.collection,Hash("CheckpointsVisible"),0);BYTE visible=0;
 if(!visibility||!Copy(visibility,&visible,1)||visible!=unsigned(mwel::Feature(package.authoring,"checkpointsVisible")))throw std::runtime_error("native checkpoint visibility differs");
 Log("EVENT_NATIVE_CHECKPOINT_VISIBILITY owner=%08X visible=%u native_lookup_verified=1",clone.collection.key,unsigned(visible));
 auto cashPointer=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(&clone.collection,Hash("CashValue"),0);float cash=0;
 const auto expectedCash=native_cash::Value(package.authoring.at("cashValue").get<unsigned>());
 if(!cashPointer||!Copy(cashPointer,&cash,4)||cash!=expectedCash)throw std::runtime_error("native race property lookup differs: CashValue float32");
 Log("EVENT_NATIVE_CASH_ATTRIBUTE key=%08X authored=%u value=%.0f storage=float32 native_lookup_verified=1",clone.collection.key,package.authoring.at("cashValue").get<unsigned>(),double(cash));
 if(!mwel::Correction(package.authoring))field("ProgressionLevel",package.authoring.at("blacklistNo").get<unsigned>());
 field("NumLaps",mwel::RaceLaps(package.authoring));field("CopsInRace",clone.gauntlet||clone.hotPursuit||package.authoring.at("cops").get<bool>()?1:0);
 field("CopDensity",clone.gauntlet?0u:unsigned(std::lround(package.authoring.at("copDensity").get<double>())));field("TrafficLevel",unsigned(std::lround(package.authoring.at("traffic").get<double>())));
 auto expectedParent=package.authoring.at("mode").get<std::string>();if(expectedParent=="sprint")expectedParent="p2p";
 if(!clone.collection.parent||clone.collection.parent->key!=Hash(expectedParent.c_str()))throw std::runtime_error("native race mode parent differs");
 Log("EXPERIMENT_NATIVE_PROPERTIES_PASS key=%08X parent=%08X fields=6",clone.collection.key,clone.collection.parent->key);
}
void AuthorGeometry(Clone& raceClone,const mwel::EventPackage& package,void* klass){
 auto fields=ReadFields(raceClone);std::vector<unsigned> children,checkpoints,shortcuts,speedtraps;unsigned serial=0;
 RaceProperties(raceClone,fields,package,klass);
  auto objects=package.authoring.at("gates");
  for(const auto& shortcut:package.authoring.value("shortcuts",mwel::Json::array()))objects.push_back(shortcut);
  if(package.authoring.at("mode")=="speedtraprace")for(const auto& extra:package.authoring.value("extras",mwel::Json::array()))if(extra.value("nativeTemplate","")=="speedtrap")objects.push_back(extra);
  for(const auto& gate:objects){
   const auto kind=gate.at("kind").get<std::string>();bool speedtrap=kind=="marker"&&gate.value("nativeTemplate","")=="speedtrap";auto type=speedtrap?"speedtrap":kind=="start"?"startgrid":kind=="finish"?"finishline":kind=="shortcut"?"shortcut":gate.contains("timeBonus")?"time_bonus_checkpoint":"checkpoint";
   if(kind!="start"&&kind!="finish"&&kind!="checkpoint"&&kind!="shortcut"&&!speedtrap)throw std::runtime_error("unsupported authored trigger kind");
   auto source=getCollection(klass,Hash(type));if(!source)throw std::runtime_error("gate template missing");
  auto child=std::make_unique<Clone>();child->race=false;child->templateKey=Hash(type);child->ownerKey=raceClone.collection.key;
  if(!Copy(source,&child->collection,sizeof(Collection))||child->collection.capacity>4096||!Copy(child->collection.layout,child->layout,8))throw std::runtime_error("gate template unreadable");
  child->nodes.resize(child->collection.capacity);if(!Copy(child->collection.nodes,child->nodes.data(),child->nodes.size()*12))throw std::runtime_error("gate table unreadable");
  for(unsigned i=0;i<child->nodes.size();++i){auto& n=child->nodes[i];if(n.value==unsigned(child->collection.nodes+i)){n.value=unsigned(&n);continue;}OwnNodeData(*child,n);}
  child->name=package.nativeNamespace+"_gate_"+std::to_string(serial++);child->namePointer=child->name.c_str();auto key=Hash(child->name.c_str());
  if(!key||getCollection(klass,key)||Find(key))throw std::runtime_error("gate identity collision");
  child->collection.key=key;child->collection.references=1;child->collection.layout=child->layout;child->collection.name=&child->namePointer;
  child->collection.parent=source;child->layout[0]=unsigned(child->name.c_str());
  auto gateFields=ReadFields(*child);Scalar(gateFields,"Template",0,6);Scalar(gateFields,"Name",unsigned(child->name.c_str()),1);
  Vector(*child,gateFields,"Position",gate.at("position"));Vector(*child,gateFields,"Dimensions",gate.at("dimensions"));
  float rotation=gate.at("rotation").get<float>();unsigned bits=0;memcpy(&bits,&rotation,4);Scalar(gateFields,"Rotation",bits,9);
   if(gate.contains("timeBonus")){float seconds=gate.at("timeBonus").get<float>();unsigned bits=0;memcpy(&bits,&seconds,4);Scalar(gateFields,"TimeBonus",bits,9);}
  if(speedtrap){float radius=gate.value("radius",20.0f);unsigned radiusBits=0;memcpy(&radiusBits,&radius,4);Scalar(gateFields,"Radius",radiusBits,9);Scalar(gateFields,"OpenWorldSpeedTrap",0,6);Log("EVENT_AUTHORED_SPEEDTRAP key=%08X radius=%.3f native_template=1",key,radius);}
  Rebuild(*child,gateFields);children.push_back(key);
   if(speedtrap)speedtraps.push_back(key);else if(kind=="start")Scalar(fields,"racestart",key,0x30);else if(kind=="finish")Scalar(fields,"racefinish",key,0x30);else if(kind=="shortcut")shortcuts.push_back(key);else checkpoints.push_back(key);
  Log("EXPERIMENT_AUTHORED_GATE key=%08X kind=%s position=%.3f,%.3f,%.3f",key,kind.c_str(),gate.at("position").at(0).get<float>(),gate.at("position").at(1).get<float>(),gate.at("position").at(2).get<float>());
  if(kind=="start"){
   // These are attribute-only position records, not live GMarker objects.
   // Keep them outside class enumeration and the event's trigger children.
   const auto count=package.authoring.at("racerCount").get<unsigned>();
   const double radians=rotation*3.14159265358979323846/180.0;
   for(unsigned slot=0;slot<count;++slot){
    auto placement=std::make_unique<Clone>();placement->race=false;placement->placement=true;placement->ownerKey=raceClone.collection.key;placement->templateKey=child->templateKey;
    placement->collection=child->collection;memcpy(placement->layout,child->layout,sizeof(child->layout));placement->nodes=child->nodes;
    for(unsigned i=0;i<placement->nodes.size();++i){auto& node=placement->nodes[i];if(node.value==unsigned(&child->nodes[i])){node.value=unsigned(&node);continue;}OwnNodeData(*placement,node);}
    placement->name=package.nativeNamespace+"_grid_"+std::to_string(slot);placement->namePointer=placement->name.c_str();auto placementKey=Hash(placement->name.c_str());
    if(!placementKey||getCollection(klass,placementKey)||Find(placementKey))throw std::runtime_error("grid placement identity collision");
    placement->collection.key=placementKey;placement->collection.references=1;placement->collection.layout=placement->layout;placement->collection.name=&placement->namePointer;placement->layout[0]=unsigned(placement->name.c_str());
    auto placementFields=ReadFields(*placement);Scalar(placementFields,"Name",unsigned(placement->name.c_str()),1);
    auto position=gate.at("position");const double back=2.0+7.0*(slot/2),side=count==1?0.0:(slot%2?1.5:-1.5);
    position[0]=position[0].get<double>()-std::cos(radians)*back-std::sin(radians)*side;
    position[1]=position[1].get<double>()-std::sin(radians)*back+std::cos(radians)*side;
    // ForceStartPosition consumes the marker rotation as a course tangent.
    // It is not the raw WarpPlayerToTrigger pose basis. Native NIS overwrites
    // the first four cars, hiding a +90-degree error on the remaining grid.
    float placementRotation=rotation;unsigned placementBits=0;memcpy(&placementBits,&placementRotation,4);
    Scalar(placementFields,"Rotation",placementBits,9);
    Vector(*placement,placementFields,"Position",position);Rebuild(*placement,placementFields);
    raceClone.gridPositions.push_back(placementKey);
    Log("EVENT_AUTHORED_GRID_PLACEMENT owner=%08X slot=%u key=%08X position=%.3f,%.3f,%.3f",raceClone.collection.key,slot,placementKey,position[0].get<double>(),position[1].get<double>(),position[2].get<double>());
    clones.push_back(std::move(placement));
   }
  }
  clones.push_back(std::move(child));
 }
 PreserveNativeMarkers(raceClone,package,klass,children);
 References(raceClone,fields,"Checkpoint",checkpoints);References(raceClone,fields,"Children",children);References(raceClone,fields,"Shortcuts",shortcuts);
 References(raceClone,fields,"SpeedTrapList",speedtraps);
 Log("EVENT_AUTHORED_SPEEDTRAP_LIST owner=%08X points=%u checkpoint_list_separate=1",raceClone.collection.key,unsigned(speedtraps.size()));
 std::vector<std::string> barriers;
 for(const auto& barrier:package.authoring.value("barriers",mwel::Json::array()))if(!barrier.contains("position")||barrier.at("position").is_null())barriers.push_back((barrier.value("reverse",false)?"*":"")+barrier.at("name").get<std::string>());
 Strings(raceClone,fields,"Barriers",barriers);
 Log("EXPERIMENT_AUTHORED_BARRIERS native_groups=%u template_barriers_inherited=0",unsigned(barriers.size()));
 Scalar(fields,"CashValue",native_cash::Bits(package.authoring.at("cashValue").get<unsigned>()),9);
 if(!mwel::Correction(package.authoring))Scalar(fields,"ProgressionLevel",package.authoring.at("blacklistNo").get<unsigned>(),8);
  Scalar(fields,"NumLaps",mwel::RaceLaps(package.authoring),8);Scalar(fields,"CopsInRace",raceClone.gauntlet||raceClone.hotPursuit||package.authoring.at("cops").get<bool>()?1:0,6);
  Scalar(fields,"CopDensity",raceClone.gauntlet?0u:unsigned(std::lround(package.authoring.at("copDensity").get<double>())),8);
  Scalar(fields,"TrafficLevel",unsigned(std::lround(package.authoring.at("traffic").get<double>())),8);
  if(package.authoring.contains("raceLengthMetres")){
   float length=package.authoring.at("raceLengthMetres").get<float>();
   if(!std::isfinite(length)||length<=0.f)throw std::runtime_error("authored course distance invalid");
   unsigned bits=0;memcpy(&bits,&length,4);Scalar(fields,"RaceLength",bits,9);
   Log("EVENT_AUTHORED_RACE_LENGTH owner=%08X metres=%.3f inherited=0",raceClone.collection.key,double(length));
  }
  const auto mode=package.authoring.at("mode").get<std::string>();
  Scalar(fields,"IsLoopingRace",mode=="circuit"||mode=="lapknockout",6);
  Scalar(fields,"KnockoutsPerLap",mode=="lapknockout",8);
  if(mode=="tollboothrace"){float seconds=package.authoring.at("timeLimit").get<float>();unsigned bits=0;memcpy(&bits,&seconds,4);Scalar(fields,"TimeLimit",bits,9);}
 Rebuild(raceClone,fields);
  Log("EXPERIMENT_AUTHORED_GEOMETRY objects=%u checkpoints=%u shortcuts=%u native_file_modified=0",unsigned(children.size()),unsigned(checkpoints.size()),unsigned(shortcuts.size()));
}
void AuditTemplates(void* klass){
 if(!worldLogEnabled)return;
 mwel::Json report=mwel::Json::array();
 auto hash=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0);
 for(const char* name:{"startgrid","checkpoint","finishline","shortcut","marker","boxtrigger"}){
  auto key=hash(name);auto source=getCollection(klass,key);if(!source)throw std::runtime_error("geometry template missing");
  Collection value{};if(!Copy(source,&value,sizeof(value))||value.capacity>4096)throw std::runtime_error("geometry template unreadable");
  mwel::Json item={{"name",name},{"key",key},{"layout",unsigned(value.layout)},{"nodes",mwel::Json::array()},{"fields",mwel::Json::array()}};
  for(unsigned i=0;i<value.capacity;++i){Node n{};if(Copy(value.nodes+i,&n,sizeof(n)))item["nodes"].push_back({n.key,n.value,n.flags});}
  for(const char* field:{"Template","Name","Position","Rotation","Dimensions","racestart","racefinish","Checkpoint","Children","CashValue","ProgressionLevel","EventID"}){
   auto fieldKey=hash(field);auto data=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,fieldKey,0);
   unsigned words[4]{};bool readable=data&&Copy(data,words,sizeof(words));
   item["fields"].push_back({{"name",field},{"key",fieldKey},{"pointer",unsigned(data)},{"readable",readable},{"words",words}});
  }
  report.push_back(std::move(item));
 }
 mwel::AtomicWrite(mwel_paths::Diagnostic(game,L"Diagnostics/templates.json"),report.dump(2)+"\n");
}
void AuditBins(void* klass){
 if(!worldLogEnabled)return;
 mwel::Json report=mwel::Json::array();
 const auto binField=Hash("BinIndex"),racesField=Hash("WorldRaces");
 for(unsigned key=firstCollection(klass),visited=0;key&&visited++<8192;key=nextCollection(klass,key)){
  auto source=getCollection(klass,key);Collection value{};
  if(!source||!Copy(source,&value,sizeof(value))||value.capacity>4096)throw std::runtime_error("bin audit collection unreadable");
  bool bin=false;for(unsigned i=0;i<value.capacity;++i){Node node{};if(Copy(value.nodes+i,&node,sizeof(node))&&node.key==binField)bin=true;}
  if(!bin)continue;
  mwel::Json item={{"key",key},{"parent",unsigned(value.parent)},{"fields",mwel::Json::array()}};
  for(const char* name:{"BinIndex","WorldRaces","BossRaces"}){
   auto field=Hash(name);auto data=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,field,0);
   unsigned words[4]{};bool readable=data&&Copy(data,words,sizeof(words));
   item["fields"].push_back({{"name",name},{"key",field},{"pointer",unsigned(data)},{"readable",readable},{"words",words}});
  }
  for(unsigned i=0;i<value.capacity;++i){Node node{};if(!Copy(value.nodes+i,&node,sizeof(node)))throw std::runtime_error("bin audit node unreadable");if(node.key==racesField){unsigned short header[4]{};bool readable=Copy(reinterpret_cast<void*>(node.value),header,sizeof(header));item["worldRacesNode"]={node.key,node.value,node.flags};item["arrayHeader"]={{"readable",readable},{"words",header}};}}
  report.push_back(std::move(item));
 }
 mwel::AtomicWrite(mwel_paths::Diagnostic(game,L"Diagnostics/bins.json"),report.dump(2)+"\n");
 Log("EXPERIMENT_BIN_AUDIT native_bins=%u file_modified=0",unsigned(report.size()));
}
void MountCareerBins(void* klass){
 std::map<unsigned,std::vector<unsigned>> additions;
 for(const auto& package:packages){
  if(mwel::Correction(package.authoring))continue;
  auto race=std::find_if(clones.begin(),clones.end(),[&](const auto& clone){return clone->race&&clone->name==package.nativeNamespace;});
  if(race==clones.end()){Log("EVENT_CAREER_REGISTRATION_SKIPPED id=%s reason=event_preflight_refused",package.id.c_str());continue;}
  additions[package.authoring.at("blacklistNo").get<unsigned>()].push_back((*race)->collection.key);
 }
 for(const auto& entry:additions){
  Collection* source=nullptr;unsigned key=0;
  for(unsigned candidate=firstCollection(klass),visited=0;candidate&&visited++<8192;candidate=nextCollection(klass,candidate)){
   auto record=getCollection(klass,candidate);Collection value{};
   if(!record||!Copy(record,&value,sizeof(value))||value.capacity>4096)throw std::runtime_error("career bin collection unreadable");
   bool references=false;for(unsigned i=0;i<value.capacity;++i){Node node{};if(!Copy(value.nodes+i,&node,sizeof(node)))throw std::runtime_error("career bin node unreadable");references|=node.key==Hash("WorldRaces");}
   if(!references)continue;
   auto data=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(record,Hash("BinIndex"),0);unsigned index=~0u;
   if(!data||!Copy(data,&index,4))throw std::runtime_error("career bin index unreadable");
   if(index!=entry.first)continue;
   if(source)throw std::runtime_error("career bin identity ambiguous");source=record;key=candidate;
  }
  if(!source||Find(key))throw std::runtime_error("career bin unavailable or already owned");
  auto clone=std::make_unique<Clone>();clone->race=false;clone->correction=true;clone->bin=true;clone->templateKey=key;
  if(!Copy(source,&clone->collection,sizeof(Collection))||!Copy(clone->collection.layout,clone->layout,8))throw std::runtime_error("career bin layout unreadable");
  clone->nodes.resize(clone->collection.capacity);
  if(!Copy(clone->collection.nodes,clone->nodes.data(),clone->nodes.size()*sizeof(Node)))throw std::runtime_error("career bin fields unreadable");
  for(unsigned i=0;i<clone->nodes.size();++i){auto& node=clone->nodes[i];if(node.value==unsigned(clone->collection.nodes+i)){node.value=unsigned(&node);continue;}OwnNodeData(*clone,node);}
  // Retail bin collections can omit the optional debug name; retain that
  // representation rather than inventing a replacement native identity.
  if(clone->collection.name){const char* sourceName=nullptr;if(!Copy(clone->collection.name,&sourceName,sizeof(sourceName)))throw std::runtime_error("career bin name unreadable");if(sourceName){clone->name=OwnString(*clone,sourceName);clone->namePointer=clone->name.c_str();}clone->collection.name=&clone->namePointer;}
  clone->collection.layout=clone->layout;clone->layout[0]=unsigned(OwnString(*clone,reinterpret_cast<const char*>(clone->layout[0])));clone->collection.references=1;
  auto fields=ReadFields(*clone);auto node=fields.find(Hash("WorldRaces"));if(node==fields.end())throw std::runtime_error("career bin references unavailable");
  unsigned short header[4]{};if((node->second.flags&0xffff)!=0x30||!(node->second.flags&0x02000000)||!Copy(reinterpret_cast<void*>(node->second.value),header,8)||header[1]>header[0]||header[0]>8192||header[2]!=4)throw std::runtime_error("career bin reference layout unsupported");
  unsigned offset=8+((header[3]&0x8000)?8:0);std::vector<unsigned> races(header[1]);if(!races.empty()&&!Copy(reinterpret_cast<void*>(node->second.value+offset),races.data(),races.size()*4))throw std::runtime_error("career bin references unreadable");
  auto original=races.size();for(auto added:entry.second){if(std::find(races.begin(),races.end(),added)!=races.end())throw std::runtime_error("career event already referenced");races.push_back(added);}
  References(*clone,fields,"WorldRaces",races);Rebuild(*clone,fields);clones.push_back(std::move(clone));
  Log("EXPERIMENT_CAREER_BIN_OWNED key=%08X rank=%u original_races=%u additional_races=%u total_races=%u original_collection_modified=0",key,entry.first,unsigned(original),unsigned(entry.second.size()),unsigned(races.size()));
 }
}
void Prepare(){
 if(!enabled||!clones.empty())return;
 auto database=*reinterpret_cast<void**>(0x90DCBC);if(!database)throw std::runtime_error("attribute database missing");
 void* klass=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x455BC0)(database,0x5CEA9D46);
 if(!klass)throw std::runtime_error("gameplay class missing");
 Log("EXPERIMENT_CLASS_AUDIT class_key=%08X native_collections=%u",*reinterpret_cast<unsigned*>(klass),countCollections(klass));
 AuditTemplates(klass);
 AuditBins(klass);
 for(const auto& package:packages){
  const auto firstOwned=clones.size();
  try{
  // The original editor hash is solely a template selector for this experiment.
  // Identity always comes from the new UUID namespace.
  auto text=package.authoring.at("hash").get<std::string>();
  if(text.size()!=8||text.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::runtime_error("invalid template hash");
  auto clone=std::make_unique<Clone>();clone->package=&package;clone->templateKey=std::stoul(text,nullptr,16);clone->correction=mwel::Correction(package.authoring);
  auto source=getCollection(klass,clone->templateKey);
  if(!source||!Copy(source,&clone->collection,sizeof(Collection)))throw std::runtime_error("template collection unavailable");
  if(!clone->collection.capacity||clone->collection.capacity>4096)throw std::runtime_error("invalid template table");
  clone->nodes.resize(clone->collection.capacity);
  if(!Copy(clone->collection.nodes,clone->nodes.data(),clone->nodes.size()*sizeof(Node))||!Copy(clone->collection.layout,clone->layout,sizeof(clone->layout)))throw std::runtime_error("template unreadable");
  clone->name=package.nativeNamespace;clone->eventId=package.nativeEventId;clone->namePointer=clone->name.c_str();
  unsigned key=clone->correction?clone->templateKey:Hash(clone->name.c_str());
  if(!key||(!clone->correction&&getCollection(klass,key))||Find(key))throw std::runtime_error("native collection identity collision");
  if(clone->correction){
   const auto& target=package.authoring.at("correctionTarget");
   auto token=reinterpret_cast<const char*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(source,0xA78403EC,0);
   unsigned pointer=0;if(!token||!Copy(token,&pointer,4)||!pointer||std::string(OwnString(*clone,reinterpret_cast<const char*>(pointer)))!=target.at("eventId").get<std::string>()||Hash(target.at("name").get<std::string>().c_str())!=key)throw std::runtime_error("native correction source identity mismatch");
   Log("EXPERIMENT_NATIVE_CORRECTION target=%08X id=%s in_memory_only=1 native_file_modified=0",key,package.id.c_str());
  }
  bool replaced=false;
  for(size_t i=0;i<clone->nodes.size();++i){auto& node=clone->nodes[i];
   if(node.value==reinterpret_cast<unsigned>(clone->collection.nodes+i)){node.value=reinterpret_cast<unsigned>(&node);continue;}
   OwnNodeData(*clone,node);
   if(node.key==0xA78403EC){
    if((node.flags>>24)!=0x20)throw std::runtime_error("unexpected EventID native storage");
    node.value=reinterpret_cast<unsigned>(clone->eventId.c_str());replaced=true;
   }
  }
  if(!replaced)throw std::runtime_error("template lacks owned EventID");
  clone->collection.nodes=clone->nodes.data();clone->collection.layout=clone->layout;
  clone->collection.key=key;clone->collection.references=1;clone->collection.name=&clone->namePointer;
  // An added race must use its own collection name for visual event effects.
  // Retaining the source name leaves its old finish gate visible on the new
  // course even though checkpoint/goal collision objects are all authored.
  // Corrections keep the original name and event identity.
  clone->layout[0]=clone->correction?reinterpret_cast<unsigned>(OwnString(*clone,reinterpret_cast<const char*>(clone->layout[0]))):reinterpret_cast<unsigned>(clone->name.c_str());
  Log("EXPERIMENT_COLLECTION_OWNED id=%s native_key=%08X template=%08X original_collection_modified=0",package.id.c_str(),key,clone->templateKey);
  auto root=clone.get();clones.push_back(std::move(clone));if(authoredGeometry){AuthorGeometry(*root,package,klass);AuditRaceProperties(*root,package);}
  if(authoredGeometry&&root->gridPositions.size()>4&&!PrepareExtendedGrid(*root))throw std::runtime_error("extended_grid_unavailable");
  }catch(const std::exception& error){
   clones.erase(clones.begin()+firstOwned,clones.end());
   Log("EVENT_REGISTRATION_REFUSED id=%s reason=%s package_rolled_back=1 native_vehicles_created=0 original_collections_modified=0",package.id.c_str(),error.what());
  }
 }
 try{MountCareerBins(klass);}catch(...){clones.clear();throw;}
}
bool Install(){
 const struct Hook {unsigned address;void* replacement;void** original;const char* guard;unsigned length;} hooks[]={
  {0x455960,GetCollection,reinterpret_cast<void**>(&getCollection),"\x8B\x44\x24\x04\x56\x8B\x71\x08",8},
  {0x5E1820,EventLookup,reinterpret_cast<void**>(&originalEventLookup),"\x56\x8B\x35\xE4\xDF\x91\x00",7},
  {0x5FCE40,ParameterEvent,reinterpret_cast<void**>(&originalParameterEvent),"\x56\x8B\xF1\x8B\x46\x04",6},
  {0x5FDCB0,ActivateEvent,reinterpret_cast<void**>(&originalActivateEvent),"\xA1\x00\xE0\x91\x00\x57",6},
  {0x5F93D0,ActivityActivate,reinterpret_cast<void**>(&originalActivityActivate),"\x6A\xFF\x68\x5F\x5E\x87\x00",7},
  {0x5FB710,ParameterLookup,reinterpret_cast<void**>(&originalParameterLookup),"\x53\x55\x56\x8B\xF1",5},
  {0x61DBE0,InitRacers,reinterpret_cast<void**>(&originalInitRacers),"\x64\xA1\x00\x00\x00\x00",6},
  {0x611570,WarpPlayerToTrigger,reinterpret_cast<void**>(&originalWarpPlayerToTrigger),"\x64\xA1\x00\x00\x00\x00",6},
  {0x454810,Attribute,reinterpret_cast<void**>(&originalAttribute),"\x8B\x49\x04\x85\xC9",5},
  {0x455FD0,FindCollection,reinterpret_cast<void**>(&findCollection),"\xA1\xBC\xDC\x90\x00",5},
  {0x456B00,FirstCollection,reinterpret_cast<void**>(&firstCollection),"\x8B\x41\x08\x83\xC0\x1C",6},
  {0x456B20,NextCollection,reinterpret_cast<void**>(&nextCollection),"\x8B\x49\x08\x8B\x44\x24\x04",7},
  {0x453FC0,CountCollections,reinterpret_cast<void**>(&countCollections),"\x8B\x41\x08\x8B\x40\x24\xC3",7}
  ,{0x5E3EC0,FindVault,reinterpret_cast<void**>(&findVault),"\x83\xEC\x10\x8B\x51\x20",6}
  ,{0x5E2D70,ScopeContains,reinterpret_cast<void**>(&originalScopeContains),"\x64\xA1\x00\x00\x00\x00",6}
  ,{0x45A220,SetParent,reinterpret_cast<void**>(&originalSetParent),"\x57\x8B\xF9\x8B\x47\x10",6}
 };
 const char constructorGuard[]={char(0x6A),char(0xFF),char(0x68)};
 if(memcmp(reinterpret_cast<void*>(0x5E8600),constructorGuard,3)||memcmp(reinterpret_cast<void*>(0x601790),constructorGuard,3))return false;
 const struct NativeCall {unsigned address;const char* guard;unsigned length;} nativeCalls[]={
  {0x603440,"\x64\xA1\x00\x00\x00\x00\x6A\xFF",8},
  {0x600AB0,"\x56\x8B\xF1\x8B\x46\x2C\x85\xC0",8},
  {0x5E39E0,"\x8B\x44\x24\x04\x85\xC0\x74\x09",8},
  {0x5FD370,"\x64\xA1\x00\x00\x00\x00\x6A\xFF",8},
  {0x5F18A0,"\x6A\xFF\x68\xC0\x58\x87\x00",7},
  {0x5E5330,"\x6A\xFF\x68\xE0\x51\x87\x00",7},
  {0x5EF450,"\x6A\xFF\x68\x8D\x56\x87\x00",7},
  {0x5E1880,"\x56\x8B\x35\xE8\xDF\x91\x00",7},
  {0x5E9C70,"\x56\x8B\xF1\xE8\x18\x9F\xFF\xFF",8},
  {0x5EC270,"\x56\x8B\xF1\x66\x8B\x06",6},
  {0x5E5BB0,"\x56\x8B\xF1\x8A\x46\x02\xC0\xE8",8},
  {0x652AD0,"\xE9\xFB\x3B\x17\x00",5}};
 for(const auto& call:nativeCalls)if(memcmp(reinterpret_cast<void*>(call.address),call.guard,call.length))return false;
 for(const auto& h:hooks)if(memcmp(reinterpret_cast<void*>(h.address),h.guard,h.length))return false;
 std::vector<void*> installed;
 for(const auto& h:hooks){auto address=reinterpret_cast<void*>(h.address);if(MH_CreateHook(address,h.replacement,h.original)!=MH_OK){for(auto a:installed)MH_RemoveHook(a);return false;}installed.push_back(address);}
 for(auto address:installed)MH_QueueEnableHook(address);
 if(MH_ApplyQueued()!=MH_OK){for(auto address:installed){MH_DisableHook(address);MH_RemoveHook(address);}return false;}
 enabled=true;return true;
}
}
