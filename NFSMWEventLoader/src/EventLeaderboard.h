#pragma once
#include "PlayerLapNotice.h"
namespace event_leaderboard {
void(__thiscall* original)(void*,void*)=nullptr;
void* featureTrampoline=nullptr;
using LapPopupFn=bool(__thiscall*)(void*,void*);
LapPopupFn nativeLapPopup=nullptr;
PlayerLapNotice lapNotice;
bool ExpandedRace(BYTE*& race,unsigned& count){
 race=expanded_leaderboard::Read<BYTE*>(reinterpret_cast<void*>(0x91E000));count=expanded_leaderboard::Read<unsigned>(race,0x1958);
 return nativeLapPopup&&MWELNeoCurrentKey()&&race&&count>4&&count<=16&&expanded_leaderboard::Read<unsigned>(race,0x1960)==1;
}
bool __fastcall LapPopup(void* board,void*,void* player){
 BYTE* race=nullptr;unsigned count=0;
 if(ExpandedRace(race,count))return false; // Native rank-window callback is not vehicle identity.
 return nativeLapPopup(board,player);
}
void PlayerLap(void* board,void* player){
 BYTE* race=nullptr;unsigned count=0;if(!ExpandedRace(race,count)){lapNotice={};return;}
 using namespace expanded_leaderboard;
 unsigned slot=PlayerSlot(player,race,count);auto info=race+0x18+slot*0x194;
 unsigned completed=Read<unsigned>(info,0x28),total=Read<unsigned>(board,0x3C),handle=Read<unsigned>(info);
 float now=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)();
 if(!lapNotice.Observe(race,board,MWELNeoCurrentKey(),handle,completed,total,now))return;
 if(completed>10)return;
 float elapsed=Read<float>(race,0x19BC+((completed-1)*16+slot)*4);
 if(!std::isfinite(elapsed)||elapsed<=0)return;
 // The native popup reads one of four HUD cache entries. Borrow entry zero
 // only for this synchronous display call, restoring both cache fields even
 // on a native fault. Authoritative lap clocks, counts and ranks are read-only.
 auto bytes=static_cast<BYTE*>(board);unsigned priorIndex=Read<unsigned>(board,0x40);float priorTime=Read<float>(board,0xAC);
 bool displayed=false;
 __try{*reinterpret_cast<unsigned*>(bytes+0x40)=0;*reinterpret_cast<float*>(bytes+0xAC)=elapsed;displayed=nativeLapPopup(board,player);}
 __finally{*reinterpret_cast<unsigned*>(bytes+0x40)=priorIndex;*reinterpret_cast<float*>(bytes+0xAC)=priorTime;}
 Log("EVENT_PLAYER_LAP_NOTICE handle=%u slot=%u completed=%u rank=%u seconds=%.4f displayed=%u native_message=1 CP_lap_clock_writes=0",handle,slot,completed,Read<unsigned>(info,0x10),elapsed,unsigned(displayed));
}
void(__thiscall* nativeFeatures)(void*,unsigned long long)=reinterpret_cast<void(__thiscall*)(void*,unsigned long long)>(0x5793F0);
void __fastcall Features(void* self,void*,unsigned long long features){nativeFeatures(self,features);expanded_leaderboard::FeaturesChanged(self,features);}
void __declspec(naked) LeaderboardFeatures(){
 // Replace only FEngHud's LeaderBoard call, never the common HudElement
 // entry used by the tachometer, speedometer, minimap and other HUD mods.
 __asm {call Features
  push 0x57CF75
  ret
 }
}
void __fastcall Update(void* self,void*,void* player){
 original(self,player);
 expanded_leaderboard::Update(self,player);
 PlayerLap(self,player);
}
bool Install(){
 // Optional presentation extension: decline a conflicting HUD mod without
 // changing its code or preventing normal event loading.
 const BYTE guard[]={0x51,0x55,0x56,0x8B,0x74,0x24,0x10,0x8B,0x06};
 auto site=reinterpret_cast<void*>(0x58C160);
 const BYTE featureGuard[]={0xE8,0x7B,0xC4,0xFF,0xFF};auto featureSite=reinterpret_cast<void*>(0x57CF70);
 if(memcmp(site,guard,sizeof(guard))||memcmp(featureSite,featureGuard,sizeof(featureGuard)))return false;
 if(MH_CreateHook(site,Update,reinterpret_cast<void**>(&original))!=MH_OK)return false;
 if(MH_CreateHook(featureSite,LeaderboardFeatures,&featureTrampoline)!=MH_OK){MH_RemoveHook(site);original=nullptr;return false;}
 bool ok=MH_QueueEnableHook(site)==MH_OK&&MH_QueueEnableHook(featureSite)==MH_OK&&MH_ApplyQueued()==MH_OK;
 if(!ok){for(auto hook:{site,featureSite}){MH_DisableHook(hook);MH_RemoveHook(hook);}original=nullptr;featureTrampoline=nullptr;return false;}
 const BYTE lapGuard[]={0x83,0xEC,0x30,0x56,0x57,0x8B,0xF9};auto lapSite=reinterpret_cast<void*>(0x57ACC0);
 if(!memcmp(lapSite,lapGuard,sizeof(lapGuard))&&MH_CreateHook(lapSite,LapPopup,reinterpret_cast<void**>(&nativeLapPopup))==MH_OK){
  if(MH_EnableHook(lapSite)!=MH_OK){MH_RemoveHook(lapSite);nativeLapPopup=nullptr;}
 }
 return true;
}
}
