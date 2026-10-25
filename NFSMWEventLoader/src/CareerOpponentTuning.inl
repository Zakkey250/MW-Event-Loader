// Included inside event_race_policies. Clamp the PRIVATE attribute instance
// after native performance matching and forced-NOS flags (006899BC..006899F5),
// before its physics components are created. Presets/shop/global banks stay intact.
static void* tuningTrampoline=nullptr;
static void*(__thiscall* originalCreateOpponent)(void*,unsigned)=nullptr;
struct TuningContext {void* info=nullptr;bool applied=false;mwel_career_tuning::Levels ceiling{};};
static thread_local TuningContext tuningContext{};
static bool CareerTuningSurface(){
 const BYTE maximum[]={0x6A,0xFF,0x68,0x58,0xA9,0x87,0,0x64};
 const BYTE level[]={0x6A,0xFF,0x68,0x38,0xA9,0x87,0,0x64};
 const BYTE unlocked[]={0x51,0xA0,0x24,0x61,0x92,0,0x84,0xC0};
 const BYTE setLevel[]={0x6A,0xFF,0x68,0x08,0xAC,0x87,0,0x64};
 const BYTE removeJunkman[]={0x57,0x8B,0x7C,0x24,0x08,0xF6,0x47,0x10};
 return !memcmp(reinterpret_cast<void*>(0x672E80),maximum,sizeof(maximum))&&
  !memcmp(reinterpret_cast<void*>(0x672D30),level,sizeof(level))&&
  !memcmp(reinterpret_cast<void*>(0x678AD0),setLevel,sizeof(setLevel))&&
  !memcmp(reinterpret_cast<void*>(0x67C220),removeJunkman,sizeof(removeJunkman))&&
  !memcmp(reinterpret_cast<void*>(0x58A960),unlocked,sizeof(unlocked));
}
static bool OpponentSlot(void* info,unsigned& slot,unsigned& count){
 BYTE* race=nullptr;if(!Copy(reinterpret_cast<void*>(0x91E000),&race,4)||!race||!info||
  !Copy(race+0x1958,&count,4)||count<2||count>16)return false;
 auto offset=uintptr_t(info)-uintptr_t(race+0x18);
 if(offset%0x194||offset/0x194>=count)return false;slot=unsigned(offset/0x194);return slot>0;
}
static void TuneOpponentUnsafe(void* attributes){
 auto package=Current();if(!package)return;
 unsigned slot=0,count=0;if(!OpponentSlot(tuningContext.info,slot,count))return;
 BYTE* db=nullptr;void* profile=nullptr;unsigned mode=0;BYTE loaded=0;
 if(!Copy(reinterpret_cast<void*>(0x91CF90),&db,4)||!db||!Copy(db+3,&loaded,1)||
  !Copy(db+0x10,&profile,4)||!profile||!Copy(db+0x12C,&mode,4))return;
 if(!mwel_career_tuning::Applies(true,mwel::NeoEnabled(package->authoring),mwel::Feature(package->authoring,"nativeOpponentAssist"),
  (mode&1)!=0,loaded!=0,slot,count))return;
 mwel_career_tuning::Levels maximum{},ceiling{},before{};
 for(unsigned type=0;type<7;++type)maximum[type]=reinterpret_cast<unsigned(__cdecl*)(void*,unsigned)>(0x672E80)(attributes,type);
 if(!mwel_career_tuning::Ceilings(maximum,ceiling,[](unsigned type,unsigned shop){
  return reinterpret_cast<bool(__cdecl*)(int,unsigned,int,int,bool)>(0x58A960)(2,type,int(shop),0,false);
 }))return;
 for(unsigned type=0;type<7;++type){before[type]=reinterpret_cast<unsigned(__cdecl*)(void*,unsigned)>(0x672D30)(attributes,type);if(before[type]>6)return;}
 auto limited=mwel_career_tuning::Limit(before,ceiling);
 for(unsigned type=0;type<7;++type){
  if(!ceiling[type])reinterpret_cast<void(__cdecl*)(void*,unsigned)>(0x67C220)(attributes,type);
  if(limited[type]!=before[type]&&!reinterpret_cast<bool(__cdecl*)(void*,unsigned,unsigned)>(0x678AD0)(attributes,type,limited[type])){
   Log("EVENT_CAREER_TUNING refused=native_level_apply_failed slot=%u type=%u",slot,type);return;
  }
 }
 tuningContext.applied=true;tuningContext.ceiling=ceiling;
 BYTE bin=0;Copy(static_cast<BYTE*>(profile)+0xB0,&bin,1);
 Log("EVENT_CAREER_TUNING slot=%u bin=%u before=%u,%u,%u,%u,%u,%u,%u ceiling=%u,%u,%u,%u,%u,%u,%u chosen=%u,%u,%u,%u,%u,%u,%u private_attributes=1 after_native_forced_nos=1",
  slot,unsigned(bin),before[0],before[1],before[2],before[3],before[4],before[5],before[6],
  ceiling[0],ceiling[1],ceiling[2],ceiling[3],ceiling[4],ceiling[5],ceiling[6],
  limited[0],limited[1],limited[2],limited[3],limited[4],limited[5],limited[6]);
}
static void __cdecl TuneOpponent(void* attributes){
 __try{TuneOpponentUnsafe(attributes);}__except(EXCEPTION_EXECUTE_HANDLER){Log("EVENT_CAREER_TUNING refused=native_read_fault");}
}
static void __declspec(naked) TuningBridge(){__asm{
 pushfd
 pushad
 lea eax,[esp+038h]
 push eax
 call TuneOpponent
 add esp,4
 popad
 popfd
 jmp dword ptr [tuningTrampoline]
}}
static void AuditCreatedOpponent(void* info,void* vehicle){
 if(!worldLogEnabled||!vehicle||!Current())return;
 __try{
  unsigned slot=0,count=0;if(!OpponentSlot(info,slot,count))return;
  auto attributes=static_cast<BYTE*>(vehicle)+0x24;
  unsigned levels[7]{};for(unsigned type=0;type<7;++type)levels[type]=reinterpret_cast<unsigned(__cdecl*)(void*,unsigned)>(0x672D30)(attributes,type);
  bool matches=tuningContext.applied;for(unsigned type=0;type<7;++type)if(levels[type]>tuningContext.ceiling[type])matches=false;
  Log("EVENT_CAREER_TUNING_EFFECTIVE slot=%u levels=%u,%u,%u,%u,%u,%u,%u native_spawn_complete=1 capped=%u matches=%u",slot,levels[0],levels[1],levels[2],levels[3],levels[4],levels[5],levels[6],unsigned(tuningContext.applied),unsigned(matches));
 }__except(EXCEPTION_EXECUTE_HANDLER){Log("EVENT_CAREER_TUNING_EFFECTIVE refused=native_read_fault");}
}
static void* __fastcall CreateOpponent(void* info,void*,unsigned fallback){
 auto previous=tuningContext;tuningContext={};tuningContext.info=info;void* result=nullptr;
 __try{result=originalCreateOpponent(info,fallback);AuditCreatedOpponent(info,result);}
 __finally{tuningContext=previous;}
 return result;
}
#include "OpponentAppearanceSafety.inl"
static bool InstallCareerTuning(){
 const BYTE siteBytes[]={0x8B,0x4C,0x24,0x1C,0x8B,0x41,0x18};
 const BYTE createBytes[]={0x64,0xA1,0,0,0,0,0x6A,0xFF};
 const BYTE appearanceBytes[]={0x8B,0x3D,0,0xE0,0x91,0};
 auto site=reinterpret_cast<void*>(0x6899F8),create=reinterpret_cast<void*>(0x5E8E60),appearance=reinterpret_cast<void*>(0x5E8FF9);
 if(!CareerTuningSurface()||memcmp(site,siteBytes,sizeof(siteBytes))||memcmp(create,createBytes,sizeof(createBytes))||memcmp(appearance,appearanceBytes,sizeof(appearanceBytes)))return false;
 if(MH_CreateHook(site,TuningBridge,&tuningTrampoline)!=MH_OK)return false;
 if(MH_CreateHook(create,CreateOpponent,reinterpret_cast<void**>(&originalCreateOpponent))!=MH_OK){MH_RemoveHook(site);return false;}
 if(MH_CreateHook(appearance,AppearanceBridge,&appearanceTrampoline)!=MH_OK){MH_RemoveHook(create);MH_RemoveHook(site);return false;}
 if(MH_QueueEnableHook(site)!=MH_OK||MH_QueueEnableHook(create)!=MH_OK||MH_QueueEnableHook(appearance)!=MH_OK||MH_ApplyQueued()!=MH_OK){
  MH_DisableHook(site);MH_DisableHook(create);MH_DisableHook(appearance);MH_RemoveHook(site);MH_RemoveHook(create);MH_RemoveHook(appearance);return false;
 }
 Log("EVENT_CAREER_TUNING_ACTIVE native_unlocks=1 categories=7 player_untouched=1 original_events_unchanged=1");return true;
}
