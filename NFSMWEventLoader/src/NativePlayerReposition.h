#pragma once
// Only ResetVehicle's reset-zone query and final ResetTo call are redirected. Native cookies,
// reset effects, suspension reset and RPD input preservation retain ownership.
namespace native_player_reposition {
void* trampoline=nullptr;
void* zoneTrampoline=nullptr;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
bool bypass=false;
#else
constexpr bool bypass=false;
#endif

bool Owns(unsigned self,unsigned& car,unsigned& leg){
 using combat_health_runtime::Read;
 const auto package=event_race_policies::Current();
 unsigned race=0,handle=0,driver=~0u;BYTE finished=0;
 if(!package||!MWELPlayerResetCourseReady(MWELNeoCurrentKey())||!Read(0x91E000,race)||!race||!Read(self+0x48,car)||!gauntlet_runtime::Table(car,0x8AA828)||
    !Read(car-0x78,handle)||!Read(race+0x18,driver)||handle!=driver||!Read(car+0x94,driver)||
    driver!=0||
    !Read(race+0x18+0x2C,leg)||!Read(race+0x18+0x21,finished)||finished||
    mwel::RpdStartOnly(package->authoring))return false;
 auto sim=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->GetSimable();return sim&&sim->IsPlayer();
}
bool __fastcall Zone(unsigned self,void*,unsigned type){
 const auto native=reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x6B06D0);
 unsigned car=0,leg=0;
 if(!Owns(self,car,leg))return native(self,type);
 // Stock RESET zones point into the original world's path network. They
 // take priority over FindNearestRoad, even for a completely different
 // custom course. Continue through ResetVehicle's normal cookie path.
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(bypass){const bool result=native(self,type);Log("EVENT_PLAYER_REPOSITION_ZONE CP=%u bypass=1 native_zone_return=%u",leg,unsigned(result));return result;}
#endif
 Log("EVENT_PLAYER_REPOSITION_ZONE CP=%u bypass=0 original_world_zone_skipped=1",leg);
 return false;
}
bool __fastcall Place(unsigned self,void*,const float* position,const float* heading,bool manual){
 const auto native=reinterpret_cast<bool(__thiscall*)(unsigned,const float*,const float*,bool)>(0x6A45F0);
 unsigned car=0,leg=0;
 if(!manual||!Owns(self,car,leg))return native(self,position,heading,manual);
 float corrected[3]{},forward[3]{};
 const bool safe=MWELPlayerResetPose(MWELNeoCurrentKey(),leg,car,corrected,forward);
 if(worldLogEnabled&&position)Log("EVENT_PLAYER_REPOSITION key=%08X CP=%u safe=%u bypass=%u native_requested=%.3f,%.3f,%.3f course_requested=%.3f,%.3f,%.3f native_progress_writes=0",
  MWELNeoCurrentKey(),leg,unsigned(safe),unsigned(bypass),position[0],position[1],position[2],corrected[0],corrected[1],corrected[2]);
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(bypass)return native(self,position,heading,manual);
#endif
 // A failed checked placement must not send the player to an unchecked
 // neighbouring road. The native menu can be used again after it clears.
 return safe&&native(self,corrected,forward,manual);
}
void __declspec(naked) Bridge(){__asm{
 push 006B0965h
 jmp Place
}}
void __declspec(naked) ZoneBridge(){__asm{
 push 006B08D9h
 jmp Zone
}}
bool Install(){
 if(trampoline&&zoneTrampoline)return true;
 const BYTE expected[]={0xE8,0x8B,0x3C,0xFF,0xFF,0x84,0xC0,0x74,0x44};
 const BYTE setup[]={0x53,0x8D,0x4C,0x24,0x24,0x51,0x8D,0x54,0x24,0x18,0x52,0x8D,0x4D,0xB4};
 auto site=reinterpret_cast<void*>(0x6B0960);
 const BYTE zone[]={0xE8,0xF7,0xFD,0xFF,0xFF,0x84,0xC0,0x74,0x0A};
 auto zoneSite=reinterpret_cast<void*>(0x6B08D4);
 if(memcmp(site,expected,sizeof(expected))||memcmp(reinterpret_cast<void*>(0x6B0952),setup,sizeof(setup))||
    memcmp(zoneSite,zone,sizeof(zone))||MH_CreateHook(site,Bridge,&trampoline)!=MH_OK)return false;
 if(MH_CreateHook(zoneSite,ZoneBridge,&zoneTrampoline)==MH_OK){
  if(MH_EnableHook(site)==MH_OK&&MH_EnableHook(zoneSite)==MH_OK)return true;
  MH_DisableHook(site);MH_RemoveHook(zoneSite);zoneTrampoline=nullptr;
 }
 MH_RemoveHook(site);trampoline=nullptr;return false;
}
}
