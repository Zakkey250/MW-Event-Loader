#pragma once
#include "RpdArrestReceipt.h"
// Private Interceptor prototype. Native TimeLimit/GetRaceTimeRemaining owns
// timeout, pause, countdown and retry; no wall-clock timer or fake finish gate.
namespace rpd_mission {
using combat_health_runtime::Read;
unsigned owner=0,epoch=0,pendingRace=0,resultRace=0,resultKey=0,resultEpoch=0,resultBounty=0;
std::array<unsigned,2> resultHandles{};
bool completing=false,goalObserved=false,resultWon=false,takedownSent=false;
bool arsenalPursuit=true;
rpd_arrest::Receipt nativeArrest;
unsigned escapeRace=0,escapeKey=0,escapeEpoch=0,escapeHandle=0;
float topSpeedMps=0;
void* escapeTrampoline=nullptr;
void* arrestIncrementTrampoline=nullptr;
void* arrestThresholdTrampoline=nullptr;
ULONGLONG nextLog=0;
bool Current(){const auto p=event_race_policies::Current();return p&&mwel_rpd::Armed(p->authoring);}
struct HudBinding {HudBinding(){expanded_leaderboard::policeSide=Current;}};
inline HudBinding hudBinding;
void Reset(){owner=epoch=pendingRace=resultRace=resultKey=resultEpoch=resultBounty=0;resultHandles={};completing=goalObserved=resultWon=takedownSent=false;nextLog=0;escapeRace=escapeKey=escapeEpoch=escapeHandle=0;topSpeedMps=0;nativeArrest.Reset();}
bool Escaped(unsigned handle){
 unsigned race=0;return handle&&handle==escapeHandle&&escapeKey==MWELNeoCurrentKey()&&
  escapeEpoch==combat_health_runtime::generation&&Read(0x91E000,race)&&race==escapeRace;
}
struct ArrestState {
 bool alive=false,visible=false;float metres=100000;
 bool healthKnown=false;float remaining=0,maximum=0;
 bool HealthEligible()const{return rpd_interceptor_policy::ArrestHealth(arsenalPursuit,healthKnown,remaining,maximum);}
 bool Eligible()const{return HealthEligible()&&rpd_interceptor_policy::ArrestContribution(alive,visible,metres);}
};
ArrestState LocalArrestState(unsigned primary,unsigned player,unsigned suspect){
 const auto officer=gauntlet_runtime::Vehicle(player),target=gauntlet_runtime::Vehicle(suspect);
 unsigned ai=0,group=0,cop=0,com=0;float unseen=1000;combat_impact::Motion a{},b{};
 bool visible=officer&&Read(officer+0x54,ai)&&Read(ai+0x70,group)&&group==primary+0x40&&
  Read(officer+4,com)&&com;
 if(visible){cop=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x4038E0);visible=gauntlet_runtime::Table(cop,0x8923E8)&&Read(cop+0x40,unseen)&&std::isfinite(unseen)&&unseen<=.25f;}
 const auto& health=combat_health_runtime::ledger.racers[0];
 bool alive=health.occupied&&health.remaining>0&&combat_health_runtime::bindings[0].handle==player;
 float metres=100000;
 if(officer&&target&&combat_health_runtime::NativeMotion(officer-0x80,a)&&combat_health_runtime::NativeMotion(target-0x80,b)){
  const auto d=a.position-b.position;metres=std::sqrt(combat_impact::Dot(d,d));
 }
 const auto& victim=combat_health_runtime::ledger.racers[1];
 const bool known=suspect&&victim.occupied&&victim.handle==suspect&&
  combat_health_runtime::bindings[1].handle==suspect&&combat_health_runtime::ledger.event==MWELNeoCurrentKey()&&
  combat_health_runtime::ledger.epoch==combat_health_runtime::generation;
 return {alive,visible,metres,known,victim.remaining,victim.maximum};
}
bool LocalArrestInProgress(unsigned suspect){
 unsigned race=0,player=0;const auto group=hot_pursuit_runtime::FindPursuit(suspect);float timer=0,increment=0;
 return group&&Read(0x91E000,race)&&Read(race+0x18,player)&&Read(group-0x40+0x11C,timer)&&timer>0&&
  Read(group-0x40+0x120,increment)&&increment>0&&LocalArrestState(group-0x40,player,suspect).Eligible();
}
void __cdecl FilterArrestIncrement(unsigned primary,float dt){
 // Native AI visibility/escape stays intact. Only the human officer builds
 // arrest progress; leaving a target allows that progress to decay to neutral.
 if(!Current()||GetCurrentThreadId()!=combat_health_runtime::gameThread||!combat_health_runtime::Racing())return;
 unsigned race=0,player=0,suspect=0;float increment=0;
 if(!Read(0x91E000,race)||!Read(race+0x18,player)||!Read(race+0x18+0x194,suspect)||
    hot_pursuit_runtime::FindPursuit(suspect)!=primary+0x40||!Read(primary+0x120,increment)||!(increment>0))return;
 const auto state=LocalArrestState(primary,player,suspect);
 if(!state.Eligible()){
  *reinterpret_cast<float*>(primary+0x120)=rpd_interceptor_policy::ArrestIncrement(false,increment,dt);
  if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+1000;Log("RPD_ARREST_BLOCKED local_player=%u distance=%.2f visible=%u alive=%u arsenal=%u hp_known=%u hp=%.2f maximum=%.2f health_eligible=%u ai_spotting_retained=1 return_to_neutral=1",player,state.metres,unsigned(state.visible),unsigned(state.alive),unsigned(arsenalPursuit),unsigned(state.healthKnown),state.remaining,state.maximum,unsigned(state.HealthEligible()));}}
 }
}
void __declspec(naked) ArrestIncrement(){__asm{
 pushfd
 pushad
 mov ebx,esp
 and esp,0FFFFFFF0h
 sub esp,512
 fxsave [esp]
 push dword ptr [ebx+0x80]
 push esi
 call FilterArrestIncrement
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 fld dword ptr [esi+0x120]
 push 0x443BB8
 ret
}}
void __cdecl ObserveEscape(unsigned primary){
 // Native cooldown completion writes state 4 here, then removes its activity
 // before our next Frame. Retain that decision, not an inferred gauge/time.
 const auto p=event_race_policies::Current();unsigned race=0,count=0,handle=0;
 if(!p||!mwel_rpd::Armed(p->authoring)||!combat_health_runtime::Racing()||
    GetCurrentThreadId()!=combat_health_runtime::gameThread||hot_pursuit_runtime::draining||
    !Read(0x91E000,race)||!Read(race+0x1958,count)||count!=2||
    !Read(race+0x18+0x194,handle)||hot_pursuit_runtime::FindPursuit(handle)!=primary+0x40)return;
 escapeRace=race;escapeKey=MWELNeoCurrentKey();escapeEpoch=combat_health_runtime::generation;escapeHandle=handle;
 Log("RPD_NATIVE_ESCAPE_OBSERVED suspect=%u race=%08X native_cooldown_complete=1 outcome_written=0",handle,race);
}
void __declspec(naked) EscapeCompleted(){__asm{
 mov dword ptr [esi+0x258],4
 pushfd
 pushad
 push esi
 call ObserveEscape
 add esp,4
 popad
 popfd
 push 0x4449F7
 ret
}}
bool __cdecl SprintOwnsArrests(){return MWELRpdHotPursuitScope();}
void __declspec(naked) ArrestThreshold(){__asm{
 pushfd
 pushad
 call SprintOwnsArrests
 test al,al
 jnz handled
 popad
 popfd
 jmp arrestThresholdTrampoline
 handled:
 popad
 popfd
 push 0x444726
 ret
}}
bool InstallEscapeObserver(){
 if(escapeTrampoline&&arrestIncrementTrampoline&&arrestThresholdTrampoline)return true;
 const BYTE expected[]={0xC7,0x86,0x58,0x02,0,0,4,0,0,0};auto site=reinterpret_cast<void*>(0x4449ED);
 const BYTE increment[]={0xD9,0x86,0x20,0x01,0,0};auto timer=reinterpret_cast<void*>(0x443BB2);
 const BYTE threshold[]={0x8A,0x44,0x24,0x58,0x84,0xC0};auto thresholdSite=reinterpret_cast<void*>(0x4445DD);
 if(memcmp(site,expected,sizeof(expected))||memcmp(timer,increment,sizeof(increment))||memcmp(thresholdSite,threshold,sizeof(threshold)))return false;
 const bool madeEscape=MH_CreateHook(site,EscapeCompleted,&escapeTrampoline)==MH_OK;
 const bool madeTimer=madeEscape&&MH_CreateHook(timer,ArrestIncrement,&arrestIncrementTrampoline)==MH_OK;
 const bool madeThreshold=madeTimer&&MH_CreateHook(thresholdSite,ArrestThreshold,&arrestThresholdTrampoline)==MH_OK;
 if(madeThreshold&&MH_EnableHook(site)==MH_OK&&MH_EnableHook(timer)==MH_OK&&MH_EnableHook(thresholdSite)==MH_OK)return true;
 if(madeThreshold){MH_DisableHook(thresholdSite);MH_RemoveHook(thresholdSite);}
 if(madeTimer){MH_DisableHook(timer);MH_RemoveHook(timer);}if(madeEscape){MH_DisableHook(site);MH_RemoveHook(site);}
 escapeTrampoline=arrestIncrementTrampoline=arrestThresholdTrampoline=nullptr;return false;
}
bool ResultPending(){
 if(MWELRpdHotPursuitScope())return MWELRpdSprintResult()||rpd_finish_camera::Context();
 unsigned race=0;BYTE done=0,expired=0;unsigned outcome=0;
 if(!Current()||!Read(0x91E000,race))return false;
 return (resultRace==race&&resultKey==MWELNeoCurrentKey())||
  (Read(race+0x18+0x21,done)&&done)||(Read(race+0x18+0x1C,outcome)&&outcome)||(Read(race+0x1989,expired)&&expired);
}
void Ranking(unsigned race){
 if(!resultRace||race!=resultRace||resultKey!=MWELNeoCurrentKey()||resultEpoch!=combat_health_runtime::generation)return;
 unsigned count=0,player=0,suspect=0;
 if(!Read(race+0x1958,count)||count!=2||!Read(race+0x18,player)||player!=resultHandles[0]||
    !Read(race+0x18+0x194,suspect)||suspect!=resultHandles[1])return;
 *reinterpret_cast<unsigned*>(race+0x18+0x10)=resultWon?1u:2u;
 *reinterpret_cast<unsigned*>(race+0x18+0x194+0x10)=resultWon?2u:1u;
}
bool Surface(){
 const BYTE elapsed[]={0x56,0x8B,0xF1,0x8B,0x86,0x68,0x19,0,0};
 const BYTE remaining[]={0x51,0x56,0x8B,0xF1,0x8B,0x8E,0x68,0x19,0,0};
 return !memcmp(reinterpret_cast<void*>(0x5E3060),elapsed,sizeof(elapsed))&&
  !memcmp(reinterpret_cast<void*>(0x5FE090),remaining,sizeof(remaining));
}
void Finish(void* actor,void(__cdecl* nativeFinish)(void*)){
 __try{nativeFinish(actor);}__finally{completing=false;}
}
void Frame(){
 rpd_finish_camera::Tick();
 if(MWELRpdHotPursuitScope())return;
 const bool cameraPending=rpd_finish_camera::Context();
 if(!Current()||!combat_health_runtime::installed||GetCurrentThreadId()!=combat_health_runtime::gameThread||*reinterpret_cast<unsigned*>(0x925E90)!=6)return;
 const auto package=event_race_policies::Current();unsigned race=0,count=0;
 if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count!=2)return;
 if(owner!=MWELNeoCurrentKey()||epoch!=combat_health_runtime::generation){Reset();owner=MWELNeoCurrentKey();epoch=combat_health_runtime::generation;}
 if(!owner||resultRace)return;
 unsigned suspect=0;if(!Read(race+0x18+0x194,suspect)||!suspect)return;
 BYTE busted=0,arrested=0,bailed=0;Read(race+0x18+0x194+0x1F,busted);
 const auto pursuit=hot_pursuit_runtime::FindPursuit(suspect);
 if(pursuit){Read(pursuit+0xE8,arrested);Read(pursuit+0xE9,bailed);}
 const bool prior=nativeArrest.Matches(race,owner,epoch,suspect);
 nativeArrest.Observe(race,owner,epoch,suspect,busted!=0,arrested!=0);
 const bool arrestPending=nativeArrest.Matches(race,owner,epoch,suspect);
 rpd_finish_camera::Remember(race,owner,epoch,suspect);
 if(arrestPending&&!prior)Log("RPD_NATIVE_ARREST_RECEIPT suspect=%u busted=%u pursuit_arrest=%u racing=%u activity_may_detach=1",suspect,unsigned(busted),unsigned(arrested),unsigned(combat_health_runtime::Racing()));
 // A full gauge is not a cutscene request. Let native AIPursuit finish its
 // bust confirmation and dispatch the real GMovieActivity callback first.
 BYTE targetTotalled=0;Read(race+0x18+0x194+0x1D,targetTotalled);
 if(arrestPending&&!targetTotalled&&!cameraPending&&!rpd_native_arrest_nis::Requested(race,owner,epoch,suspect))return;
 if(!combat_health_runtime::Racing()&&!cameraPending&&!arrestPending)return;
 std::array<unsigned,2> handles{};std::array<BYTE,2> totalled{};
 for(unsigned slot=0;slot<2;++slot){
  if(!Read(race+0x18+slot*0x194,handles[slot])||!handles[slot]||!Read(race+0x18+slot*0x194+0x1D,totalled[slot]))return;
  if(slot==1&&(arrestPending||(cameraPending&&handles[slot]==rpd_finish_camera::suspect)))continue;
  if(totalled[slot])continue; // Native destruction may already have deactivated this body.
  const auto car=gauntlet_runtime::Vehicle(handles[slot]);unsigned driver=~0u,com=0;
  if(!car||!Read(car+0x94,driver)||driver!=(slot?3u:0u)||!Read(car-0x80+4,com))return;
  if(!slot){combat_impact::Motion body;if(combat_health_runtime::NativeMotion(car-0x80,body)){
   const float speed=std::sqrt(combat_impact::Dot(body.velocity,body.velocity));
   if(std::isfinite(speed)&&speed<250)topSpeedMps=std::max(topSpeedMps,speed);
   reinterpret_cast<NFSPluginSDK::MW05::GRacerInfo*>(race+0x18)->mTopSpeed=topSpeedMps;
  }}
  const auto damage=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x405000);
  mwcombat::HealthV1 health;
  if(!combat_health_runtime::Observe(slot,handles[slot],damage,driver,&health,sizeof(health)))return;
 }
 BYTE expired=0;Read(race+0x1989,expired);
 // With handoff disabled, the native vehicle-destroyed event owns the
 // player's loss and retry. Do not also submit a custom result.
 if(totalled[0]&&!mwel_rpd::OfficerHandoff(package->authoring)&&!cameraPending&&!arrestPending)return;
 const auto outcome=cameraPending?mwel_rpd::Outcome::Arrested:MWELRpdNoAllyDefeat()?mwel_rpd::Outcome::PlayerDestroyed:mwel_rpd::Decide(totalled[0]!=0,totalled[1]!=0,expired!=0,arrestPending,bailed!=0||Escaped(handles[1]));
 if(GetTickCount64()>=nextLog){nextLog=GetTickCount64()+2000;
  const auto elapsed=reinterpret_cast<float(__thiscall*)(unsigned)>(0x5E3060)(race);
  Log("RPD_INTERCEPTOR_STATE player=%u suspect=%u player_hp=%.1f suspect_hp=%.1f elapsed=%.2f native_timeout=%u police_allies=%u",handles[0],handles[1],combat_health_runtime::ledger.racers[0].remaining,combat_health_runtime::ledger.racers[1].remaining,elapsed,unsigned(expired),mwel_rpd::Allies(package->authoring));
 }
 if(outcome!=mwel_rpd::Outcome::Arrested&&outcome!=mwel_rpd::Outcome::Escaped&&outcome!=mwel_rpd::Outcome::PlayerDestroyed){pendingRace=0;
  if(outcome!=mwel_rpd::Outcome::Running)return; // Native destruction/timeout owns defeat.
  // AIGoalFleePursuit is the native police withdrawal goal: its callers are
  // AIPursuit::FleeCopOfType and AIVehiclePursuit::StartFlee. Assigning it to
  // an unfinished suspect caused repeated reversing in the physical trial.
  // Keep the event-created racer controller and authored escape route. Free
  // Run does not make route/CP completion a victory or defeat condition.
  if(!goalObserved){
   const auto car=gauntlet_runtime::Vehicle(handles[1]);unsigned ai=0,goal=0;
   if(car&&Read(car+0x54,ai)&&Read(ai+0x78,goal)){
    goalObserved=true;Log("RPD_SUSPECT_CONTROLLER native_goal=%08X controller_replaced=0 route_finish_not_objective=1",goal);
   }
  }return;
 }
 // Wait until destruction and its callbacks have fully unwound.
 if(outcome==mwel_rpd::Outcome::Arrested&&!takedownSent){
  takedownSent=true;combat_health_runtime::Takedown(totalled[1]?mwcombat::Takedown::RacerWreck:mwcombat::Takedown::RacerArrest,handles[0],handles[1]);
 }
 if(pendingRace!=race){pendingRace=race;return;}
 if(outcome==mwel_rpd::Outcome::Arrested&&rpd_finish_camera::Hold(handles[0],handles[1],totalled[1]!=0))return;
 if(!MWELRpdPrepareResult()||!Read(race+0x18,handles[0]))return;
 auto actor=runtime_lifecycle::FinishActor(nullptr);
 const auto finish=race_start_experiment::enabled?race_start_experiment::originalFinish:runtime_lifecycle::originalFinish;
 const auto placement=MWELArmsNativePlacement();
 if(!actor||MWELArmsNativeSlot(actor)!=1||!finish||!placement||!MWELArmsPrepareRanking())return;
 resultRace=race;resultKey=owner;resultEpoch=epoch;resultHandles=handles;completing=true;resultWon=outcome==mwel_rpd::Outcome::Arrested;
 resultBounty=resultWon?package->authoring.at("cashValue").get<unsigned>():0;
 Ranking(race);
 Log("RPD_INTERCEPTOR_OUTCOME suspect=%u arrested=%u escaped=%u native_totalled=%u native_pursuit_arrest=%u physical_finish=0",handles[1],unsigned(resultWon),unsigned(!resultWon),unsigned(totalled[1]),unsigned(arrested));
 MWELRpdLabEndAutodrive();
 if(resultWon&&!totalled[1])rpd_native_arrest_nis::Victory();
 Finish(actor,finish);
 unsigned current=0,rank=0;BYTE done=0;
 const unsigned place=resultWon?1u:2u;
 if(Read(0x91E000,current)&&current==race&&Read(race+0x18+0x10,rank)&&rank==place&&Read(race+0x18+0x21,done)&&done){
  placement(nullptr,actor,place);Log("RPD_INTERCEPTOR_RESULT arrest=%u escaped=%u native_finished=1 placement=%u",unsigned(resultWon),unsigned(!resultWon),place);
  if(resultWon){
   profile_progress::Record(*package,reinterpret_cast<void*>(race+0x18),false,false,false,true);
   void* database=nullptr;if(Copy(reinterpret_cast<void*>(0x91E004),&database,4)&&database)profile_progress::SaveNativeProgress(database);
   const auto bounty=resultBounty;
   if(bounty&&!profile_progress::CurrentIdentity().empty()){
    reinterpret_cast<void(__cdecl*)(int)>(0x612220)(int(bounty));
    Log("RPD_BOUNTY_AWARDED amount=%u native_award=00612220 cash=0 once_per_result=1",bounty);
   }
  }
 }
}
}
bool MWELRpdFinishAllowed(){return !rpd_mission::Current()||rpd_mission::completing;}
bool MWELRpdPursuitCompleted(unsigned handle){return MWELRpdHotPursuitScope()?MWELRpdSprintRetired(handle):rpd_mission::Current()&&rpd_mission::Escaped(handle);}
