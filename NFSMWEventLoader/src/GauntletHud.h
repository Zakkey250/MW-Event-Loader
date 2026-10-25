#pragma once
// FEngHud's feature dispatcher is distinct from the shared HudElement entry.
// Keep native pursuit values/animation and scope the extra bottom meters to
// the authored duel. Its shared native presentation is filtered after update.
namespace gauntlet_hud {
using FeaturesFn=void(__thiscall*)(void*,unsigned long long);
FeaturesFn original=nullptr;
bool pursuitInstalled=false;
constexpr unsigned long long Bottom=(1ull<<23)|(1ull<<9);
constexpr unsigned long long PursuitBoard=1ull<<20;
bool Active(){unsigned race=0,play=0;
 return (gauntlet_runtime::Current()||hot_pursuit_runtime::Current())&&gauntlet_runtime::Read(0x91E000,race)&&gauntlet_runtime::Read(race+0x1960,play)&&play==1;
}
void* queryTrampoline=nullptr;
bool __fastcall PursuitPresentation(void* params,void*){
 const bool native=reinterpret_cast<bool(__thiscall*)(void*)>(0x5FBE70)(params);
 return native||Active();
}
void __declspec(naked) PursuitQuery(){
 __asm {call PursuitPresentation
  push 0x6F1810
  ret
 }
}
using UpdateFn=void(__thiscall*)(void*,void*);
UpdateFn boardUpdate=nullptr,infoUpdate=nullptr;
void Filter(void* element){
 if(!Active())return;
 auto engine=expanded_leaderboard::Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto name=expanded_leaderboard::Read<const char*>(element,0x10);
 auto package=engine&&name?reinterpret_cast<unsigned*(__thiscall*)(void*,const char*)>(0x516AF0)(engine,name):nullptr;
 if(!package)return;
 // Native PursuitBoardGroup (upper right) and its three vehicle counters.
 // Keep the native Busted/Evade and cooldown meter groups, texture bindings,
 // animation and value setters intact. These hashes come from the verified
 // native constructors, not a screen rectangle or installation-specific size.
 for(unsigned hash:{0xDE89E070u,0xE0B1430Bu})
  reinterpret_cast<void(__cdecl*)(void*)>(0x514C70)(expanded_leaderboard::Find(package,hash));
}
void __fastcall BoardUpdate(void* self,void*,void* player){boardUpdate(self,player);Filter(self);}
void __fastcall InfoUpdate(void* self,void*,void* player){
 infoUpdate(self,player);Filter(self);
 if(Active()&&worldLogEnabled){
  unsigned race=0;int booths=0,cp=0;
  if(gauntlet_runtime::Read(0x91E000,race)&&gauntlet_runtime::Read(race+0x18+0x30,booths)&&gauntlet_runtime::Read(race+0x18+0x2C,cp)){
   static int previous=-1;if(previous!=booths){previous=booths;Log("GAUNTLET_TOLL_COUNTER booths=%d native_cp_hit=%d native_counter_and_time_bonus=1",booths,cp);}
  }
 }
}

unsigned long long Desired(unsigned long long requested){
 auto actual=requested;
 if(Active()){
  actual&=~(PursuitBoard|(hot_pursuit_runtime::Current()?0ull:24ull)); // retain the HP race leaderboard and native race values
  unsigned race=0;BYTE racing=0,timer=0;
  gauntlet_runtime::Read(0x91E000,race);gauntlet_runtime::Read(race+0x198A,racing);gauntlet_runtime::Read(race+0x1978,timer);
  if(requested&&racing&&timer&&!reinterpret_cast<bool(__cdecl*)()>(0x468390)())actual|=Bottom|PursuitBoard;
 }
 return actual;
}
void __fastcall Features(void* self,void*,unsigned long long requested){
 const auto actual=MWELCarbonFilterHud(self,Desired(requested));
 if(Active()&&worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+5000;
  Log("GAUNTLET_HUD_FEATURES hud=%08X requested=%016llX actual=%016llX bottom=%u native_pursuit_data=1",unsigned(self),requested,actual,unsigned((actual&Bottom)==Bottom));
 }}
 original(self,actual);
 hot_pursuit_radar::FeaturesChanged(actual);checkpoint_display::FeaturesChanged(actual);
 if(Active()&&actual){
  for(unsigned offset:{0x300u,0x308u,0x324u}){
   auto element=expanded_leaderboard::Read<void*>(self,offset);
   if(element&&expanded_leaderboard::Read<unsigned long long>(element,0x20)!=actual)
    reinterpret_cast<void(__thiscall*)(void*,unsigned long long)>(0x5793F0)(element,actual);
  }
 }
 if(Active()){auto board=expanded_leaderboard::Read<void*>(self,0x300);if(board)Filter(board);}

}
void Frame(){
 if(!pursuitInstalled||!Active())return;
 // Native FEngHud dispatches on feature changes. Stock intros can dispatch
 // before the race timer starts and never request the pursuit bits again.
 // Reconcile that state transition independently of diagnostics, without
 // forcing a full HUD update every frame or overriding a native zero mask.
 static ULONGLONG next=0;const auto now=GetTickCount64();if(now<next)return;next=now+200;
 auto dispatcher=expanded_leaderboard::Read<void*>(reinterpret_cast<void*>(0x92FD8C));
 auto hud=dispatcher?expanded_leaderboard::Read<void*>(dispatcher):nullptr;
 if(!hud||expanded_leaderboard::Read<unsigned>(hud)!=0x8A2538)return; // observed FEngHud COM object, not its global-list storage
 const auto requested=expanded_leaderboard::Read<unsigned long long>(hud,0x18);
 const auto actual=Desired(requested);
 if(actual!=requested)Features(hud,nullptr,requested);
}
// One feature dispatcher is shared with Carbon. Its worker must never race
// this owner by installing a second detour at 57CDB0.
bool InstallFeatures(){
 if(original)return true;
 auto address=reinterpret_cast<void*>(0x57CDB0);
 const BYTE guard[]={0x53,0x8B,0x5C,0x24,0x08,0x55,0x8B,0x6C,0x24,0x10};
 if(memcmp(address,guard,sizeof(guard))||MH_CreateHook(address,Features,reinterpret_cast<void**>(&original))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);original=nullptr;return false;}
 return true;
}
bool Install(){
 if(pursuitInstalled)return true;
 if(!InstallFeatures())return false;
 auto query=reinterpret_cast<void*>(0x6F180B);
 auto board=reinterpret_cast<void*>(0x57AEE0),info=reinterpret_cast<void*>(0x57A900);
 const BYTE queryGuard[]={0xE8,0x60,0xA6,0xF0,0xFF};
 const BYTE boardGuard[]={0x83,0xEC,0x24,0x56,0x8B,0xF1};
 const BYTE infoGuard[]={0xA1,0x00,0xE0,0x91,0x00,0x83,0xEC,0x14};
 if(memcmp(query,queryGuard,sizeof(queryGuard))||
    memcmp(board,boardGuard,sizeof(boardGuard))||memcmp(info,infoGuard,sizeof(infoGuard)))return false;
 void* targets[]={query,board,info};
 void* detours[]={reinterpret_cast<void*>(PursuitQuery),reinterpret_cast<void*>(BoardUpdate),reinterpret_cast<void*>(InfoUpdate)};
 void** outputs[]={&queryTrampoline,reinterpret_cast<void**>(&boardUpdate),reinterpret_cast<void**>(&infoUpdate)};
 unsigned created=0;bool ok=true;
 for(unsigned i=0;i<3;++i){
  if(MH_CreateHook(targets[i],detours[i],outputs[i])!=MH_OK){ok=false;break;}++created;
 }
 if(ok)for(unsigned i=0;i<3;++i)if(MH_EnableHook(targets[i])!=MH_OK){ok=false;break;}
 if(!ok){for(unsigned i=0;i<created;++i){MH_DisableHook(targets[i]);MH_RemoveHook(targets[i]);}
  queryTrampoline=nullptr;boardUpdate=infoUpdate=nullptr;
 }
 pursuitInstalled=ok;return ok;
}
}
