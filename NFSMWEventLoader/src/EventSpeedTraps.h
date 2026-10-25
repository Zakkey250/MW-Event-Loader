#pragma once
namespace event_speedtraps {
unsigned observedKey=0;std::set<unsigned> observed;
std::map<unsigned,DWORD> retryAt;
void Frame(unsigned key,bool running){
 if(key!=observedKey){observedKey=key;observed.clear();retryAt.clear();}
 if(!running||!key)return;
 auto owner=registration_experiment::Find(key);if(!owner||!owner->race)return;
 bool speedRace=false;for(const auto& p:packages)if(p.nativeNamespace==owner->name){speedRace=p.authoring.at("mode")=="speedtraprace";break;}
 if(!speedRace)return;
 using namespace NFSPluginSDK::MW05;
 static_assert(offsetof(GIcon,mFlags)==2&&offsetof(GIcon,mEmitter)==0xC&&sizeof(GIcon)==0x20,"Native icon ABI");
 for(const auto& child:registration_experiment::clones){
  if(child->ownerKey!=key||child->templateKey!=0xB05871D3||child->placement)continue;
  auto found=registration_experiment::geometryObjects.find(child->collection.key);if(found==registration_experiment::geometryObjects.end())continue;
  auto trigger=static_cast<BYTE*>(found->second);unsigned table=0;if(!Copy(trigger,&table,4)||table!=0x8A3A84)continue;
  // 00601E6F stores the icon at +C0. The SDK's non-virtual base layout does
  // not include GRuntimeInstance's native leading vtable word.
  GIcon* icon=nullptr;if(!Copy(trigger+0xC0,&icon,4)||!icon)continue;
  GIcon::Type type{};if(!Copy(&icon->mType,&type,2)||(type!=GIcon::Type::Checkpoint&&type!=GIcon::Type::SpeedTrapInRace))continue;
  // GIcon's transform is in render axes. Match native streaming residency:
  // retain only nearby effects, and recreate a native emitter if its timed
  // group has retired and cleared the icon's callback-owned pointer.
  // eView's first pointer owns the verified render view/projection matrices.
  void* matrices=nullptr;float view[16]{};if(!Copy(reinterpret_cast<void*>(0x919650),&matrices,4)||!matrices||!Copy(matrices,view,sizeof(view)))continue;
  float camera[3]={-(view[12]*view[0]+view[13]*view[1]+view[14]*view[2]),-(view[12]*view[4]+view[13]*view[5]+view[14]*view[6]),-(view[12]*view[8]+view[13]*view[9]+view[14]*view[10])};
  if(std::sqrt(std::pow(icon->mPosition.x-camera[0],2)+std::pow(icon->mPosition.y-camera[1],2)+std::pow(icon->mPosition.z-camera[2],2))>220.f)continue;
  // Native race speed traps use checkpoint-type icons; their map visibility
  // is independent of ShowInWorld. A sprint template hides normal checkpoints
  // and can leave these owned icons map-only. Re-enable only SpeedTrapList's
  // actual owned icons, keeping the native model/emitter and spawn lifecycle.
  const auto before=static_cast<unsigned short>(icon->mFlags);
  icon->mFlags=static_cast<GIcon::Flags>(before|static_cast<unsigned short>(GIcon::Flags::ShowInWorld));
  bool spawned=false;
  if(!(before&static_cast<unsigned short>(GIcon::Flags::Spawned))||!icon->mEmitter){auto now=GetTickCount();auto& last=retryAt[child->collection.key];if(!last||now-last>=1000){last=now;icon->Spawn();spawned=true;}}
  BYTE modelVisible=0;if(icon->mModel)Copy(reinterpret_cast<BYTE*>(icon->mModel)+0x28,&modelVisible,1);
  bool first=observed.insert(child->collection.key).second;
  // Spawn allocates disabled resources at their default transform. The native
  // Enable operation positions the model/emitter and resumes the emitter;
  // merely setting GIcon::Enabled leaves invisible resources at the origin.
  if(first||spawned||!(before&static_cast<unsigned short>(GIcon::Flags::Enabled))||(icon->mModel&&!modelVisible))
   reinterpret_cast<void(__thiscall*)(GIcon*)>(0x5E5BB0)(icon);
  if(first){if(icon->mModel)Copy(reinterpret_cast<BYTE*>(icon->mModel)+0x28,&modelVisible,1);Log("EVENT_SPEEDTRAP_FIELD_ICON owner=%08X point=%08X type=%u flags_before=%04X flags_after=%04X native_model=%08X native_emitter=%08X native_spawn=1 native_enable=1 model_visible=%u position=%.3f,%.3f,%.3f custom_effects=0",key,child->collection.key,unsigned(type),before,unsigned(icon->mFlags),unsigned(icon->mModel),unsigned(icon->mEmitter),unsigned(modelVisible),icon->mPosition.x,icon->mPosition.y,icon->mPosition.z);}
 }
}
}
