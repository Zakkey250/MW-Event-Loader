#pragma once
namespace event_speedtraps {
struct PointState {
 unsigned key=0;
 DWORD nextSpawn=0,nextIcon=0;
 bool observed=false,missingLogged=false;
};
const registration_experiment::Collection* observedRace=nullptr;
unsigned observedKey=0,observedNativeKey=0;
std::vector<PointState> points;
void Reset(){
 observedRace=nullptr;observedKey=0;observedNativeKey=0;points.clear();
}
void Frame(unsigned key,BYTE* race,bool running){
 if(!running){if(observedRace)Reset();return;}
 auto collection=registration_experiment::CurrentRaceCollection(race);
 unsigned nativeKey=0;
 if(!collection||!Copy(&collection->key,&nativeKey,4))return;
 if(collection!=observedRace||key!=observedKey||nativeKey!=observedNativeKey){
  observedRace=collection;observedKey=key;observedNativeKey=nativeKey;points.clear();
  try{
   auto owner=registration_experiment::Find(key);
   auto keys=owner?owner->speedTrapKeys:registration_experiment::CollectionReferences(collection,"SpeedTrapList");
   for(auto pointKey:keys)if(pointKey)points.push_back({pointKey});
   if(worldLogEnabled&&!points.empty())Log("EVENT_SPEEDTRAP_LIST key=%08X count=%u package=%u",key?key:nativeKey,unsigned(points.size()),unsigned(owner!=nullptr));
  }catch(const std::exception& error){if(worldLogEnabled)Log("EVENT_SPEEDTRAP_LIST_REFUSED key=%08X reason=%s",nativeKey,error.what());}
 }
 if(points.empty())return;
 registration_experiment::GeometryTable geometry;
 if(!geometry.Ready())return;

 using namespace NFSPluginSDK::MW05;
 static_assert(offsetof(GIcon,mFlags)==2&&offsetof(GIcon,mEmitter)==0xC&&sizeof(GIcon)==0x20,"Native icon ABI");
 constexpr auto onMap=static_cast<unsigned short>(GIcon::Flags::ShowOnMap);
 constexpr auto inWorld=static_cast<unsigned short>(GIcon::Flags::ShowInWorld);
 constexpr auto spawned=static_cast<unsigned short>(GIcon::Flags::Spawned);
 constexpr auto enabled=static_cast<unsigned short>(GIcon::Flags::Enabled);

 // eView uses render-space axes, as does the position stored on GIcon.
 void* matrices=nullptr;float view[16]{},camera[3]{};
 const bool cameraReady=Copy(reinterpret_cast<void*>(0x919650),&matrices,4)&&matrices&&Copy(matrices,view,sizeof(view));
 if(cameraReady)for(unsigned i=0;i<3;++i)camera[i]=-(view[12]*view[i*4]+view[13]*view[i*4+1]+view[14]*view[i*4+2]);
 const auto now=GetTickCount();
 for(auto& point:points){
  const auto pointKey=point.key;
  auto trigger=key?registration_experiment::LiveGeometry(geometry,pointKey):static_cast<BYTE*>(geometry.Lookup(pointKey));
  unsigned table=0;BYTE active=0,kind=0;
  if(!trigger||!Copy(trigger,&table,4)||table!=0x8A3A84||!Copy(trigger+0x19,&kind,1)||!(kind&1)||
     !Copy(trigger+0xC4,&active,1)||!active||!registration_experiment::GeometryIdentity(trigger,pointKey))continue;
  // The trigger owns this native icon until its deleting destructor.
  GIcon* icon=nullptr;
  if(!Copy(trigger+0xC0,&icon,4))continue;
  if(!icon&&(!point.nextIcon||static_cast<LONG>(now-point.nextIcon)>=0)){
   point.nextIcon=now+1000;
   registration_experiment::Collection* attributes=nullptr;
   if(Copy(trigger+8,&attributes,4)&&attributes){
    auto position=reinterpret_cast<void*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(attributes,0x9F743A0E,0);
    registration_experiment::owned_speedtrap_icons::Position coordinates{};
    if(Copy(position,&coordinates,sizeof(coordinates))&&std::isfinite(coordinates.x)&&std::isfinite(coordinates.y)&&std::isfinite(coordinates.z))
     icon=registration_experiment::owned_speedtrap_icons::RestoreRaceIcon(geometry.manager,trigger,coordinates);
   }
  }
  if(!icon){
   if(worldLogEnabled&&!point.missingLogged){
    point.missingLogged=true;unsigned count=0;Copy(geometry.manager+0x2BC,&count,4);
    Log("EVENT_SPEEDTRAP_ICON_UNAVAILABLE owner=%08X point=%08X native_icon_pool=%u",key?key:nativeKey,pointKey,count);
   }
   continue;
  }
  const auto type=icon->mType;
  if(type!=GIcon::Type::Checkpoint&&type!=GIcon::Type::SpeedTrapInRace)continue;
  const auto before=static_cast<unsigned short>(icon->mFlags);
  const auto dx=icon->mPosition.x-camera[0],dy=icon->mPosition.y-camera[1],dz=icon->mPosition.z-camera[2];
  const bool nearby=cameraReady&&dx*dx+dy*dy+dz*dz<=220.f*220.f;
  const auto flags=static_cast<unsigned short>(before|onMap|inWorld);
  if(flags!=before)icon->mFlags=static_cast<GIcon::Flags>(flags);
  if(!nearby)continue;

  bool restored=false;
  if((!(before&spawned)||!icon->mEmitter)&&(!point.nextSpawn||static_cast<LONG>(now-point.nextSpawn)>=0)){
   point.nextSpawn=now+1000;icon->Spawn();restored=true;
  }
  BYTE modelVisible=0;
  if(icon->mModel)Copy(reinterpret_cast<BYTE*>(icon->mModel)+0x28,&modelVisible,1);
  // Spawn can allocate resources at their default transforms. Enable places
  // and resumes them through the original game path.
  if(restored||!(before&enabled)||(icon->mModel&&!modelVisible))
   reinterpret_cast<void(__thiscall*)(GIcon*)>(0x5E5BB0)(icon);
  if(worldLogEnabled&&!point.observed){
   point.observed=true;
   if(icon->mModel)Copy(reinterpret_cast<BYTE*>(icon->mModel)+0x28,&modelVisible,1);
   Log("EVENT_SPEEDTRAP_FIELD_ICON owner=%08X point=%08X type=%u flags_before=%04X flags_after=%04X native_model=%08X native_emitter=%08X native_spawn=1 native_enable=1 model_visible=%u position=%.3f,%.3f,%.3f custom_effects=0",key?key:nativeKey,pointKey,unsigned(type),before,unsigned(icon->mFlags),unsigned(icon->mModel),unsigned(icon->mEmitter),unsigned(modelVisible),icon->mPosition.x,icon->mPosition.y,icon->mPosition.z);
  }
 }
}
}
