#pragma once
#include "RpdRetryBoundary.h"
namespace rpd_presentation {
using namespace expanded_leaderboard;
DWORD medalAt=0;
unsigned medalRank=0;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
bool PreviewMedal(){
 if(productionRuntime||!worldLogEnabled||!race_start_experiment::enabled)return false;
 static const bool enabled=[](){
  const auto marker=game/L"MWEventLoader.rpd-medal-layout-test";
  return mwel::fs::is_regular_file(marker)&&mwel::Read(marker,128)=="MWEL_RPD_MEDAL_LAYOUT_V1\n";
 }();
 return enabled;
}
#endif
const char* Label(unsigned index){
 static const char* ids[]={"MWEL_RPD_OBJECTIVE","MWEL_RPD_ESCAPED","MWEL_RPD_RETRY","MWEL_RPD_END","MWEL_RPD_ARRESTS","MWEL_RPD_OFFICER_DISABLED","MWEL_RPD_PURSUIT_RESULT","MWEL_RPD_BOUNTY_REWARD"};
 static const wchar_t* english[]={L"ARREST THE RACER",L"The racer got away...",L"Retry",L"End",L"ARRESTS",L"VEHICLE DISABLED",L"PURSUIT RESULT",L"BOUNTY"};
 static const wchar_t* japanese[]={L"\u30ec\u30fc\u30b5\u30fc\u3092\u902e\u6355\u3057\u308d",L"\u9003\u3052\u3089\u308c\u3066\u3057\u307e\u3063\u305f\u2026",L"\u30ea\u30c8\u30e9\u30a4",L"\u7d42\u4e86",L"\u902e\u6355",L"\u8eca\u4e21\u5927\u7834",L"\u8ffd\u8de1\u7d50\u679c",L"\u30d0\u30a6\u30f3\u30c6\u30a3\u5831\u916c"};
 if(index>=std::size(ids))return "";const auto key=Hash(ids[index]);
 if(event_titles::titles.find(key)==event_titles::titles.end())event_titles::titles.emplace(key,event_titles::Title{english[index],ids[index],{},japanese[index]});
 return event_titles::Lookup(nullptr,key);
}
unsigned LabelKey(unsigned index){Label(index);const char* ids[]={"MWEL_RPD_OBJECTIVE","MWEL_RPD_ESCAPED","MWEL_RPD_RETRY","MWEL_RPD_END","MWEL_RPD_ARRESTS","MWEL_RPD_OFFICER_DISABLED"};return Hash(ids[index]);}
Object ObjectIn(const char* package,unsigned key){return package?reinterpret_cast<Object(__cdecl*)(const char*,unsigned)>(0x524850)(package,key):nullptr;}
void Info(void* self){
 if(!rpd_mission::Current())return;
 auto package=Read<const char*>(self,0x10);if(!package)return;
 // These native fields are FE object pointers, including animated groups;
 // 5253B0 formats both groups and FEStrings as the stock caller does.
 auto numerator=Read<Object>(self,0x74);auto denominator=Read<Object>(self,0x78);
 auto objective=Read<Object>(self,0x60);auto progress=Read<Object>(self,0x7C);
 auto format=reinterpret_cast<int(__cdecl*)(const char*,Object,const char*,...)>(0x5253B0);
 const auto score=rpd_hot_pursuit_runtime::ledger.Result(rpd_hot_pursuit_runtime::owner);
 const bool sprint=MWELRpdHotPursuitScope();
 medalRank=sprint&&rpd_hot_pursuit_runtime::Owns()&&objective&&!(objective[7]&1)?unsigned(score.Rank()):0;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(sprint&&objective&&!(objective[7]&1)&&rpd_hot_pursuit_runtime::Owns()&&PreviewMedal())medalRank=1;
#endif
 medalAt=GetTickCount();
 if(numerator)format(package,numerator,"%u",sprint?score.arrested:unsigned(rpd_mission::resultWon));
 if(denominator)format(package,denominator,"%u",sprint?std::max(1u,score.suspects):1u);
 if(objective)Text(objective,Label(0));
 if(progress){
  using rpd_hot_pursuit_runtime::LeadProgress;
  const auto lead=sprint?rpd_hot_pursuit_runtime::Lead():LeadProgress{};
  if(lead.handle)format(package,progress,"%d %%",lead.percent);
  else format(package,progress,"%s","");
  if(worldLogEnabled&&lead.handle){
   static LeadProgress previous{};
   if(lead.handle!=previous.handle||lead.percent!=previous.percent){
    previous=lead;
    Log("RPD_SPRINT_LEAD_PROGRESS handle=%u course_percent=%d native_hud_field=1",lead.handle,lead.percent);
   }
  }
 }
}
bool OfficerCrash(){
 unsigned race=0;BYTE totalled=0;
 const auto policy=event_race_policies::Current();
 return policy&&mwel_rpd::Armed(policy->authoring)&&!mwel_rpd::OfficerHandoff(policy->authoring)&&
  !MWELRpdSprintResult()&&gauntlet_runtime::Read(0x91E000,race)&&race&&
  gauntlet_runtime::Read(race+0x18+0x1D,totalled)&&totalled;
}
bool Defeat(){return OfficerCrash()||(!MWELRpdHotPursuitScope()&&rpd_mission::Current()&&rpd_mission::ResultPending()&&!rpd_mission::resultWon);}
unsigned DefeatLabel(){return OfficerCrash()||MWELRpdNoAllyDefeat()?5u:1u;}
void*(__thiscall* nativeRetry)(void*)=nullptr;
struct PendingRetry {unsigned race=0,key=0,epoch=0;bool unpauseQueued=false;};
PendingRetry pendingRetry;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
unsigned auditRetryRace=0;ULONGLONG auditRetryNext=0;
void RetryAudit(){
 if(!worldLogEnabled||!auditRetryRace||GetTickCount64()<auditRetryNext)return;
 auditRetryNext=GetTickCount64()+1000;
 unsigned race=0,manager=0,status=0;BYTE waiting=0;
 gauntlet_runtime::Read(0x91E000,race);if(race!=auditRetryRace){auditRetryRace=0;return;}
 gauntlet_runtime::Read(race+0x19A4,waiting);if(!waiting)return;
 gauntlet_runtime::Read(0x92C67C,manager);gauntlet_runtime::Read(0x91E00C,status);
 const auto ready=reinterpret_cast<bool(__thiscall*)(unsigned)>(0x5E95F0)(race);
 const auto assets=reinterpret_cast<bool(__thiscall*)(unsigned)>(0x5E9660)(race);
 const auto objects=status?reinterpret_cast<bool(__thiscall*)(unsigned)>(0x5EBF30)(status):false;
 const auto stream=reinterpret_cast<bool(__thiscall*)(unsigned)>(0x7303D0)(0x9AA9D0);
 const auto cops=manager?reinterpret_cast<bool(__thiscall*)(unsigned)>(*reinterpret_cast<unsigned*>(*reinterpret_cast<unsigned*>(manager)+0x18))(manager):false;
 Log("PRIVATE_RETRY_LOAD_AUDIT race=%08X racer_not_ready=%u assets_not_ready=%u objects_ready=%u stream_ready=%u cops_loading=%u",race,unsigned(ready),unsigned(assets),unsigned(objects),unsigned(stream),unsigned(cops));
}
#endif
void* DeferredEvent(void* event){
 // Verified EEvent base: no action; its native destructor advances the event
 // arena by this aligned 16-byte allocation. Never leave an unbuilt record.
 auto words=static_cast<unsigned*>(event);words[0]=0x8A66F8;words[1]=0x10;return event;
}
void* __fastcall Retry(void* activity,void*){
 if(rpd_mission::Current()){
  // 6307F0 starts Restart immediately: RaceOver(reason=5) and MRestartRace
  // run before its event is dispatched. Release the held NIS before that change,
  // so its resume/unload actions cannot overwrite the new restart state.
  // 625870/625880 are EUnPause, which is dispatched after Restart has begun.
  if(pendingRetry.race)return DeferredEvent(activity);
  if(rpd_native_arrest_nis::Owns()||MWELCombatResultPending()){
   unsigned race=0;gauntlet_runtime::Read(0x91E000,race);
   pendingRetry={race,MWELNeoCurrentKey(),combat_health_runtime::generation};
   DeferredEvent(activity);
   rpd_finish_camera::Reset();rpd_hot_pursuit_runtime::ReleaseLocks();
   Log("RPD_RETRY_DEFERRED race=%08X key=%08X epoch=%u native_result_resume_before_restart=1",race,pendingRetry.key,pendingRetry.epoch);
   return activity;
  }
  rpd_finish_camera::Reset();
  rpd_hot_pursuit_runtime::ReleaseLocks();
  Log("RPD_SPRINT_RETRY_PREPARED camera_and_locks_released=1 before_native_restart_state=1 native_constructor=006307F0");
 }
 return nativeRetry(activity);
}
void RetryFrame(){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 RetryAudit();
#endif
 if(!pendingRetry.race||GetCurrentThreadId()!=combat_health_runtime::gameThread)return;
 unsigned race=0,simState=~0u;gauntlet_runtime::Read(0x91E000,race);
 if(!mwel_rpd_retry::SameOwner(rpd_mission::Current(),race,MWELNeoCurrentKey(),combat_health_runtime::generation,pendingRetry.race,pendingRetry.key,pendingRetry.epoch)){
  pendingRetry={};Log("RPD_RETRY_CANCELLED owner_changed=1");return;
 }
 // Release(IActivity) schedules destruction; owned==0 alone is insufficient.
 // Completion suppression is acknowledged at the end of that destructor.
 const bool flowReadable=gauntlet_runtime::Read(0x988E04,simState);
 if(!mwel_rpd_retry::SceneReleased(rpd_native_arrest_nis::retiringActivity,rpd_native_arrest_nis::Current(),flowReadable,simState))return;
 if(!pendingRetry.unpauseQueued){
  // The post-race photo's retry callback does not emit EUnPause. The held
  // arrest scene leaves the simulation paused even after its activity dies;
  // use the same native event as the pause menu, then wait for its dispatch.
  auto resume=reinterpret_cast<void*(__cdecl*)(unsigned)>(0x627400)(8);
  if(!resume)return;
  reinterpret_cast<void*(__thiscall*)(void*)>(0x625870)(resume);
  pendingRetry.unpauseQueued=true;
  Log("RPD_RETRY_UNPAUSE_QUEUED native_event=00625870 before_restart=1");
  return;
 }
 unsigned world=0,worldState=4;
 const bool worldReadable=gauntlet_runtime::Read(0x9885E0,world)&&world&&gauntlet_runtime::Read(world+0x2C,worldState);
 if(!mwel_rpd_retry::SimulationResumed(worldReadable,world,worldState))return;
 auto event=reinterpret_cast<void*(__cdecl*)(unsigned)>(0x627400)(8);
 if(!event)return;
 pendingRetry={};
 nativeRetry(event);
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 auditRetryRace=race;auditRetryNext=0;
#endif
 Log("RPD_RETRY_DISPATCHED owned_nis_destroyed=1 native_sim_running=1 native_restart=006307F0");
}
bool InstallRetry(){
 if(nativeRetry)return true;
 const BYTE expected[]={0x6A,0xFF,0x68,0x47,0x82,0x87,0,0x64,0xA1,0,0,0,0};
 const unsigned base[]={0x62A430,0x7C5200,0x62A460,0x61FF40};
 const BYTE allocation[]={0x8B,0x54,0x24,4,0x8B,0x0D,0x90,3,0x92,0};
 const BYTE resume[]={0x8B,0xC1,0xC7,0x40,4,0x10,0,0,0,0xC7,0,0xBC,0x76,0x8A,0,0xC3};
 if(memcmp(reinterpret_cast<void*>(0x8A66F8),base,sizeof(base))||memcmp(reinterpret_cast<void*>(0x627400),allocation,sizeof(allocation))||memcmp(reinterpret_cast<void*>(0x625870),resume,sizeof(resume)))return false;
 auto site=reinterpret_cast<void*>(0x6307F0);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,Retry,reinterpret_cast<void**>(&nativeRetry))!=MH_OK)return false;
 if(MH_EnableHook(site)==MH_OK)return true;
 MH_RemoveHook(site);nativeRetry=nullptr;return false;
}
void Results(void* self){
 if(!Defeat())return;
 const auto name=Read<const char*>(self,0x10);auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto package=engine&&name?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,name):nullptr;if(!package)return;
 // Keep the native results screen as the callback owner for its verified
 // Restart/End messages, but remove its table/tabs/footer from presentation.
 auto list=package+18;auto object=reinterpret_cast<Object>(list[1]);
 for(unsigned n=0;object&&n<list[0]&&n<4096;++n,object=reinterpret_cast<Object>(object[1]))Show(object,false);
 static unsigned screen=0,generation=~0u,raceOwner=0;
 unsigned race=0;gauntlet_runtime::Read(0x91E000,race);
 if(screen==unsigned(self)&&generation==combat_health_runtime::generation&&raceOwner==race)return;
 screen=unsigned(self);generation=combat_health_runtime::generation;raceOwner=race;
 // Exact two-button native dialog contract from PostRaceResultsScreen's
 // restart confirmation. Its response messages call the stock restart/end
 // flow. Cancel goes to End, never back to the hidden standings.
 using Dialog=void(__cdecl*)(const char*,const char*,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
 constexpr unsigned NativeRetryLabel=0x039DF715; // Verified in both native English and Japanese banks.
 reinterpret_cast<Dialog>(0x598C00)(name,"InGameDialog.fng",1,NativeRetryLabel,LabelKey(3),0xE1A57D51,0xE1FDE1D1,0xE1FDE1D1,0,LabelKey(DefeatLabel()));
 Log("RPD_DEFEAT_RETRY_MENU native_restart_end_callbacks=1 standings_hidden=1 sprint_crash=%u officer_crash=%u",
     unsigned(MWELRpdHotPursuitScope()&&OfficerCrash()),unsigned(OfficerCrash()));
}
}
#pragma comment(linker,"/EXPORT:MWEL_RpdMedalV1=_MWEL_RpdMedalV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMedalV1(){
 return rpd_presentation::medalRank&&GetTickCount()-rpd_presentation::medalAt<150&&
        MWELRpdHotPursuitScope()&&!MWELRpdSprintResult()?rpd_presentation::medalRank:0;
}
const char* MWELRpdLanguage(unsigned key){
 if(MWELRpdSprintResult()&&(key==0xE815BC6D||key==0x5DFB6667||key==0xEAC720D3||key==0x82E4DAFD))return rpd_presentation::Label(6);
 if(rpd_mission::Current()&&(key==0xEAC720D3||key==0x82E4DAFD))return rpd_presentation::Label(rpd_presentation::DefeatLabel());
 return nullptr;
}
void MWELRpdResults(void* self){rpd_presentation::Results(self);}
void MWELRpdInfo(void* self){rpd_presentation::Info(self);}
void MWELRpdSprintPhoto(void* self){
 using namespace expanded_leaderboard;
 const auto package=Read<const char*>(self,0x10);if(!package)return;
 const auto BountyKey=Hash("MWEL_RPD_RESULT_BOUNTY");
 auto reward=rpd_presentation::ObjectIn(package,BountyKey);
 const bool sprint=MWELRpdSprintResult();
 const bool interceptor=rpd_mission::Current()&&rpd_mission::ResultPending()&&rpd_mission::resultWon;
 if(!sprint&&!interceptor){Show(reward,false);return;}
 const auto value=rpd_presentation::ObjectIn(package,0xAB6AAFDDu);
 if(!value||Read<unsigned>(value,0x18)!=2)return;
 if(sprint){
  const auto score=rpd_hot_pursuit_runtime::ledger.Result(rpd_hot_pursuit_runtime::owner);
  char text[80];sprintf_s(text,"%s %u/%u",rpd_presentation::Label(4),score.arrested,score.suspects);Text(value,text);
 }
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 const auto owner=engine?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,package):nullptr;
 if(!owner)return;
 if(!reward)reward=Clone(owner,value,BountyKey);
 if(!reward)return;
 float x=0,y=0;
 reinterpret_cast<void(__cdecl*)(Object,float*,float*)>(0x514FD0)(value,&x,&y);
 reinterpret_cast<void(__cdecl*)(Object,float,float)>(0x524CC0)(reward,x,y+28.f);
 const auto bounty=sprint?rpd_hot_pursuit_runtime::resultBounty:rpd_mission::resultBounty;
 const auto amount=std::to_string(bounty);
 std::string grouped;
 for(size_t i=0;i<amount.size();++i){if(i&&(amount.size()-i)%3==0)grouped+=',';grouped+=amount[i];}
 const auto text=std::string(rpd_presentation::Label(7))+"  "+grouped;
 Text(reward,text.c_str());Show(reward,true);
 Log("RPD_RESULT_BOUNTY_DISPLAY amount=%u sprint=%u native_photo_string=1",bounty,unsigned(sprint));
 if(worldLogEnabled){
  unsigned world=0;float raw=0,native=0,scaled=0;
  if(combat_health_runtime::Read(0x9885E0,world)&&combat_health_runtime::Read(world+0x18,raw)&&combat_health_runtime::Read(world+0x24,native)&&combat_health_runtime::Read(world+0x1C,scaled)&&raw>0&&native>0)
   Log("RPD_RESULT_TIMING native_dt_scale=%.3f camera_scale=%.3f",scaled/(raw*native),rpd_capture_slow_motion::scale);
 }
}
