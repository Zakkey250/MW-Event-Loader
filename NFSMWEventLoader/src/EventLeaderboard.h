#pragma once
#include "PlayerLapNotice.h"
#include "DriftSplitPolicy.h"
namespace event_leaderboard {
void(__thiscall* original)(void*,void*)=nullptr;
void* featureTrampoline=nullptr;
using LapPopupFn=bool(__thiscall*)(void*,void*);
LapPopupFn nativeLapPopup=nullptr;
LapPopupFn nativeSplitPopup=nullptr;
PlayerLapNotice lapNotice;
bool __fastcall SplitPopup(void* board,void*,void* player){
 if(!drift_race::Current())return nativeSplitPopup(board,player);
 if(drift_race::faulted||drift_race::finalScores)return false;
 using namespace expanded_leaderboard;
 // EN1.3 LeaderBoard::ShowSplitTime 0057AB10: preserve its settings,
 // busy-message test, priority, animation and native interval trigger.
 auto table=Read<unsigned*>(player);if(!table)return false;
 auto settings=reinterpret_cast<BYTE*(__thiscall*)(void*)>(table[5])(player);
 if(!settings)return false;
 if(Read<BYTE>(settings,0x10)==4){*reinterpret_cast<BYTE*>(static_cast<BYTE*>(board)+0x44)=0;return false;}
 auto sim=reinterpret_cast<BYTE*(__thiscall*)(void*)>(table[8])(player);if(!sim)return false;
 auto com=Read<void*>(sim,4);if(!com)return false;
 auto message=reinterpret_cast<void*(__thiscall*)(void*,void*)>(0x5D59F0)(com,reinterpret_cast<void*>(0x5650B0));
 auto methods=Read<unsigned*>(message);if(!methods||reinterpret_cast<bool(__thiscall*)(void*)>(methods[3])(message))return false;
 const auto slot=PlayerSlot(player,reinterpret_cast<BYTE*>(drift_race::owner),drift_race::count);
 std::array<unsigned,16> points{};unsigned retired=0;
 for(unsigned i=0;i<drift_race::count;++i){points[i]=drift_race::scores[i].standing();auto info=reinterpret_cast<void*>(drift_race::owner+0x18+i*0x194);if(Read<unsigned>(info,0x1C)||Read<BYTE>(info,0x190))retired|=1u<<i;}
 const auto difference=mwel_drift_split::Compare(points,drift_race::count,slot,retired);if(!difference.valid)return false;
 char text[160];sprintf_s(text,"%s\n%+d Pts",drift_hud::SplitLabel(),difference.points);
 const unsigned animation=difference.points>=0?0xA19BB14Cu:0x5230FAF6u;
 reinterpret_cast<void(__thiscall*)(void*,const char*,bool,unsigned,unsigned,unsigned,unsigned)>(methods[1])(message,text,false,animation,0,0,3);
 Log("DRIFT_SPLIT_POINTS slot=%u opponent=%u player_points=%u opponent_points=%u delta=%d native_interval=1 native_animation=1 lines=2",slot,difference.opponent,points[slot],points[difference.opponent],difference.points);
 return true;
}
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
void __fastcall Features(void* self,void*,unsigned long long features){nativeFeatures(self,features);expanded_leaderboard::FeaturesChanged(self,features);drift_hud::Features(self,features);}
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
 drift_hud::Update(self,player);
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
 const BYTE splitGuard[]={0x83,0xEC,0x30,0x56,0x57,0x8B,0x7C,0x24,0x3C};auto splitSite=reinterpret_cast<void*>(0x57AB10);
 if(!memcmp(splitSite,splitGuard,sizeof(splitGuard))&&MH_CreateHook(splitSite,SplitPopup,reinterpret_cast<void**>(&nativeSplitPopup))==MH_OK){
  if(MH_EnableHook(splitSite)!=MH_OK){MH_RemoveHook(splitSite);nativeSplitPopup=nullptr;}
 }
 return true;
}
}
