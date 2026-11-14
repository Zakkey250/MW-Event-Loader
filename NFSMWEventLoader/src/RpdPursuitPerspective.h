#pragma once
// Native Speech selects the foreground pursuit with IsPlayerPursuit. In a
// police mission the local officer listens to the suspect's real pursuit.
// Select the suspect's foreground role only at verified native consumers.
// Career/bounty initialization and detection-stat consumers keep their native
// result. No IPlayer pointer or local-player ownership is substituted.
namespace rpd_pursuit_perspective {
bool(__thiscall* original)(unsigned)=nullptr;
bool Adapt(unsigned caller){
 // 0072086B/81 retain Speech's selected foreground activity between frames.
 // Native arrest owns its own confirmation/delay. Its eventual NIS request
 // is intercepted separately and binds the suspect instead of IPlayer's car.
 return caller==0x443C70||caller==0x704E86||caller==0x704EA7||caller==0x704EF2||caller==0x72086B||caller==0x720881;
}
bool __fastcall IsForeground(unsigned pursuit,void*){
 const auto caller=unsigned(_ReturnAddress());
 if(MWELRpdHotPursuitScope()){
  unsigned race=0,officer=0;
  if(caller!=0x443C70&&Adapt(caller)&&combat_health_runtime::Read(0x91E000,race)&&combat_health_runtime::Read(race+0x18,officer)){
   const auto selected=rpd_presentation_tracking::RadioPursuit(officer);
   if(selected)return selected==pursuit;
  }
  return original(pursuit);
 }
 if(worldLogEnabled){
  const auto p=event_race_policies::Current();unsigned race=0,handle=0;
  if(p&&mwel::RpdStartOnly(p->authoring)&&combat_health_runtime::Read(0x91E000,race)&&
     combat_health_runtime::Read(race+0x18+0x194,handle)&&hot_pursuit_runtime::FindPursuit(handle)==pursuit){
   static std::array<unsigned,96> observed{};
   if(std::find(observed.begin(),observed.end(),caller)==observed.end()){
    auto slot=std::find(observed.begin(),observed.end(),0u);if(slot!=observed.end())*slot=caller;
    Log("RPD_NATIVE_ROLE_QUERY caller=%08X native_player_pursuit=%u speech_viewpoint=%u read_only=1",caller,unsigned(original(pursuit)),unsigned(caller==0x704E86||caller==0x704EF2));
   }
  }
 }
 if(Adapt(caller)){
  const auto p=event_race_policies::Current();unsigned race=0,handle=0;
  if(p&&mwel::RpdStartOnly(p->authoring)&&combat_health_runtime::Read(0x91E000,race)&&
     combat_health_runtime::Read(race+0x18+0x194,handle)&&hot_pursuit_runtime::FindPursuit(handle)==pursuit)return true;
 }
 return original(pursuit);
}
bool Install(){
 if(original)return true;
 const BYTE speech0[]={0xFF,0x90,0x8C,0,0,0,0x84,0xC0,0x74,0x16};
 const BYTE speech1[]={0xFF,0x92,0x8C,0,0,0,0x84,0xC0,0x74,0x07};
 const BYTE arrest[]={0xFF,0x92,0x8C,0,0,0,0x8B,0x8E,0xB4,0,0,0,0x88,0x44,0x24,0x58};
 if(memcmp(reinterpret_cast<void*>(0x720865),speech0,sizeof(speech0))||
    memcmp(reinterpret_cast<void*>(0x72087B),speech1,sizeof(speech1))||
    memcmp(reinterpret_cast<void*>(0x443C6A),arrest,sizeof(arrest)))return false;
 const BYTE expected[]={0x56,0x8B,0xF1,0x8B,0x06,0xFF,0x50,0x08};auto site=reinterpret_cast<void*>(0x40AD80);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,IsForeground,reinterpret_cast<void**>(&original))!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);original=nullptr;return false;}return true;
}
}
