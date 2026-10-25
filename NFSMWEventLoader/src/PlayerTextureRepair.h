#pragma once
// Preserve the whole permanent-layer ownership before native emergency eviction
// drops it. Temporary layers and the player's own destruction remain native.
namespace player_texture_repair {
template<class T>T ReadValue(const void* p,unsigned offset=0){T value{};if(p)Copy(static_cast<const BYTE*>(p)+offset,&value,sizeof(value));return value;}
bool armed=false;
thread_local void* retiringConnection=nullptr;
int(__thiscall* originalUnloadPerms)(void*,void*)=nullptr;
void(__thiscall* originalUnloadConnection)(void*)=nullptr;
void Queue(unsigned key){if(key)armed=true;}
bool LivePlayerOwnsSkin(void* skin){
 if(!skin||ReadValue<unsigned>(reinterpret_cast<void*>(0x925E90))!=6)return false;
 auto data=ReadValue<BYTE**>(reinterpret_cast<void*>(0x9B37CC));
 auto count=ReadValue<unsigned>(reinterpret_cast<void*>(0x9B37D4));if(!data||count>64)return false;
 unsigned handle=0;
 for(unsigned i=0;i<count;++i){
  auto conn=ReadValue<BYTE*>(data,i*4);
  if(!conn||conn==retiringConnection||ReadValue<unsigned>(conn)!=0x8B5254||ReadValue<unsigned>(conn,0x28)<2)continue;
  auto ride=ReadValue<BYTE*>(conn,0x40);auto render=ReadValue<BYTE*>(conn,0x44);
  if(!ride||!render||ReadValue<unsigned>(ride,0x308)!=0||ReadValue<BYTE*>(render,0x84)!=ride)continue;
  handle=ReadValue<unsigned>(ride,0x304);break;
 }
 if(!handle)return false;
 auto sentinel=reinterpret_cast<BYTE*>(0x9B2C58);auto node=ReadValue<BYTE*>(sentinel);
 for(unsigned i=0;node&&node!=sentinel&&i<128;++i){
  if(ReadValue<unsigned>(node,0x6D0)==handle)
   return ReadValue<int>(node,8)>0&&ReadValue<unsigned>(node,0x0C)==2&&ReadValue<void*>(node,0x350)==skin;
  node=ReadValue<BYTE*>(node);
 }
 return false;
}
int __fastcall UnloadPerms(void* self,void*,void* skin){
 const bool live=self==reinterpret_cast<void*>(0x9B2C00)&&(armed||worldLogEnabled)&&LivePlayerOwnsSkin(skin);
 if(live&&!armed)Log("EVENT_PLAYER_PERMANENT_EVICTION_UNPROTECTED skin=%08X layers=%u caller=%08X first_race_guard_unarmed=1",unsigned(skin),ReadValue<unsigned>(skin,0x20),unsigned(_ReturnAddress()));
 if(live&&armed){
  if(worldLogEnabled){
   Log("EVENT_PLAYER_PERMANENT_EVICTION_PREVENTED skin=%08X layers=%u caller=%08X live_render_owner=1 native_temporary_release=1",unsigned(skin),ReadValue<unsigned>(skin,0x20),unsigned(_ReturnAddress()));
   void* frames[12]{};auto count=CaptureStackBackTrace(1,12,frames,nullptr);
   for(unsigned i=0;i<count;++i)Log("EVENT_PLAYER_PERMANENT_EVICTION_STACK frame=%u address=%08X",i,unsigned(frames[i]));
  }
  return 0; // Nothing freed: native eviction must examine other rides.
 }
 return originalUnloadPerms(self,skin);
}
void __fastcall UnloadConnection(void* self,void*){
 auto previous=retiringConnection;retiringConnection=self;originalUnloadConnection(self);retiringConnection=previous;
}
}
