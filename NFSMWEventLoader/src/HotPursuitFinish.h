#pragma once
namespace hot_pursuit_finish {
unsigned scopeRace=0,scopeInfo=0,scopeParams=0,scopeBase=0;
unsigned resultRace=0,resultKey=0,resultParams=0,resultCash=0,resultPlace=0,resultActor=0;
std::array<unsigned,8> escaping{};
std::array<unsigned,8> pending{};
bool installed=false;
float(__thiscall* originalCash)(void*)=nullptr;
void(__cdecl* originalLosing)()=nullptr;
void(__cdecl* originalPlacement)(void*,void*,unsigned)=nullptr;
void* rankTrampoline=nullptr;
using combat_health_runtime::Read;
bool Scope(){unsigned race=0;return scopeRace&&GetCurrentThreadId()==combat_health_runtime::gameThread&&Read(0x91E000,race)&&race==scopeRace;}
unsigned Calculate(){
 unsigned count=0,retired=0,rank=0;
 if(!Scope()||!Read(scopeRace+0x1958,count)||!count||count>8||!Read(scopeInfo+0x10,rank))return 0;
 for(unsigned slot=0;slot<count;++slot){unsigned outcome=0;BYTE dnf=0;
  if(!Read(scopeRace+0x18+slot*0x194+0x1C,outcome)||!Read(scopeRace+0x18+slot*0x194+0x190,dnf))return 0;
  if(outcome||dnf)++retired;
 }
 resultCash=hot_pursuit::Reward(scopeBase,count,retired,rank);resultPlace=rank;
 Log("HOT_PURSUIT_SURVIVOR_REWARD base=%u racers=%u retired=%u survivors=%u place=%u cash=%u rank_written=0",scopeBase,count,retired,count-retired,rank,resultCash);
 return resultCash;
}
float __fastcall Cash(void* params,void*){
 if(Scope()&&unsigned(params)==scopeParams){const auto cash=Calculate();Log("HOT_PURSUIT_NATIVE_CASH_GETTER place=%u amount=%u native_award_latch_retained=1",resultPlace,cash);return float(cash);}
 // Result UI reads the same parameters after native EndRace/Stop. Keep the
 // awarded amount until the next staging boundary, not a borrowed object.
 unsigned race=0;
 if(resultRace&&Read(0x91E000,race)&&race==resultRace&&unsigned(params)==resultParams)return float(resultCash);
 return originalCash(params);
}
bool WinningRank(){const bool win=Scope();if(win)Log("HOT_PURSUIT_NATIVE_WIN_BRANCH actual_place=%u rank_written=0",resultPlace);return win;}
void __declspec(naked) RankRule(){
 __asm {
  pushfd
  pushad
  mov ebp,esp
  sub esp,528
  and esp,0FFFFFFF0h
  fxsave [esp]
  call WinningRank
  test al,al
  jz nativeRank
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  push 0x611B1A
  ret
 nativeRank:
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  cmp [ebx+0x10],ebp
  jne nativeLoss
  push 0x611B1A
  ret
 nativeLoss:
  push 0x611CCE
  ret
 }
}
bool WinningResult(){unsigned race=0;return resultRace&&resultPlace&&Read(0x91E000,race)&&race==resultRace;}
void __cdecl Losing(){if(WinningResult()){Log("HOT_PURSUIT_NATIVE_WIN_RESULT actual_place=%u native_screen=0060E240",resultPlace);reinterpret_cast<void(__cdecl*)()>(0x60E240)();}else originalLosing();}
// Basicrace branches on the placement event separately from native cash and
// post-race UI. A lower-place survivor must enter its normal postrace state;
// redirecting only ShowLosingPostRaceScreen strands the script in raceover.
// Preserve GRacerInfo ranking and award arithmetic. Only this success event
// for the exact genuinely finished local actor reports a winning placement.
void __cdecl Placement(void* sender,void* actor,unsigned place){
 if(drift_race::Current()&&actor){
  const auto slot=reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor)-1;
  if(slot>=0&&unsigned(slot)<drift_race::count&&(drift_race::genuineMask&(1u<<slot))){
   drift_race::Ranking(drift_race::owner);
   const auto ranked=expanded_leaderboard::Read<unsigned>(reinterpret_cast<void*>(drift_race::owner+0x18+slot*0x194),0x10);
   if(ranked&&ranked<=drift_race::count){Log("DRIFT_NATIVE_PLACEMENT slot=%d native_place=%u score_place=%u",slot,place,ranked);originalPlacement(sender,actor,ranked);return;}
  }
 }
 unsigned race=0,params=0,handle=0,playerHandle=0,outcome=0;BYTE finished=0,dnf=0;
 const bool survivor=resultActor&&unsigned(actor)==resultActor&&resultRace&&resultPlace&&
  GetCurrentThreadId()==combat_health_runtime::gameThread&&Read(0x91E000,race)&&race==resultRace&&
  Read(race+0x1968,params)&&params==resultParams&&MWELNeoCurrentKey()==resultKey&&
  Read(unsigned(actor)+8,handle)&&Read(race+0x18,playerHandle)&&handle&&handle==playerHandle&&
  Read(race+0x18+0x1C,outcome)&&!outcome&&Read(race+0x18+0x190,dnf)&&!dnf&&
  Read(race+0x18+0x21,finished)&&finished;
 if(survivor)Log("HOT_PURSUIT_NATIVE_COMPLETION_EVENT actual_place=%u script_place=1 native_rank_written=0",place);
 originalPlacement(sender,actor,survivor?1u:place);
}
bool Install(){
 if(installed)return true;
 const BYTE placement[]={0x83,0xEC,0x20,0x8B,0x54,0x24,0x24},cash[]={0x56,0x8B,0xF1,0x8B,0x46,4},rank[]={0x39,0x6B,0x10,0x0F,0x85,0xB4,1,0,0},losing[]={0x6A,0,0xE8,0xE9,0xC9,0xFF,0xFF};
 if(memcmp(reinterpret_cast<void*>(0x60AA00),placement,sizeof(placement))||memcmp(reinterpret_cast<void*>(0x5FB920),cash,sizeof(cash))||memcmp(reinterpret_cast<void*>(0x611B11),rank,sizeof(rank))||memcmp(reinterpret_cast<void*>(0x60E230),losing,sizeof(losing)))return false;
 void* sites[]={reinterpret_cast<void*>(0x5FB920),reinterpret_cast<void*>(0x611B11),reinterpret_cast<void*>(0x60E230),reinterpret_cast<void*>(0x60AA00)};
 void* hooks[]={reinterpret_cast<void*>(Cash),reinterpret_cast<void*>(RankRule),reinterpret_cast<void*>(Losing),reinterpret_cast<void*>(Placement)};
 void** outputs[]={reinterpret_cast<void**>(&originalCash),&rankTrampoline,reinterpret_cast<void**>(&originalLosing),reinterpret_cast<void**>(&originalPlacement)};
 unsigned made=0;bool ok=true;
 for(unsigned i=0;i<4;++i){if(MH_CreateHook(sites[i],hooks[i],outputs[i])!=MH_OK){ok=false;break;}++made;}
 if(ok)for(auto site:sites)if(MH_QueueEnableHook(site)!=MH_OK){ok=false;break;}
 if(ok)ok=MH_ApplyQueued()==MH_OK;
 if(!ok){for(unsigned i=0;i<made;++i){MH_DisableHook(sites[i]);MH_RemoveHook(sites[i]);}originalCash=nullptr;originalLosing=nullptr;originalPlacement=nullptr;rankTrampoline=nullptr;return false;}
 installed=true;return true;
}
void Reset(){scopeRace=scopeInfo=scopeParams=scopeBase=0;resultRace=resultKey=resultParams=resultCash=resultPlace=resultActor=0;escaping={};pending={};}
bool Escape(unsigned slot,unsigned handle){
 if(slot>=escaping.size()||!handle)return false;
 const auto car=gauntlet_runtime::Vehicle(handle);unsigned ai=0;
 if(!car||!Read(car+0x54,ai)||!ai)return false;
 unsigned table=0;Read(ai,table);if(table!=0x892640)return false;
 // Use the registered native goal factory, never change driver class or
 // reuse AI pointers across a controller allocation.
 const auto hash=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIGoalFleePursuit");
 unsigned node=0,factory=0;Read(0x90D8E8,node);
 for(unsigned i=0;node&&i<128;++i){unsigned name=0;Read(node,name);if(name==hash){Read(node+4,factory);break;}if(!Read(node+8,node))break;}
 if(factory<0x401000||factory>=0x889000)return false;
 const BYTE goalCode[]={0x6A,0xFF,0x64,0xA1,0,0,0,0};
 if(memcmp(reinterpret_cast<void*>(0x422480),goalCode,sizeof(goalCode)))return false;
 reinterpret_cast<void(__thiscall*)(unsigned,const unsigned*)>(0x422480)(ai-0x4C,&hash);
 unsigned actual=0,goal=0;Read(car+0x54,ai);Read(ai+0x78,actual);Read(ai-0x4C+0xB8,goal);
 if(actual!=hash||!goal)return false;
 escaping[slot]=handle;Log("HOT_PURSUIT_FINISHED_ESCAPE slot=%u handle=%u native_flee_goal=1 finish_flags_preserved=1",slot,handle);return true;
}
void CallNative(void* actor,void(__cdecl* nativeFinish)(void*)){
 __try {nativeFinish(actor);}
 __finally {scopeRace=scopeInfo=scopeParams=scopeBase=0;}
}
void Frame(){
 if(!hot_pursuit_runtime::Active()||!combat_health_runtime::Racing())return;
 unsigned race=0;Read(0x91E000,race);
 for(unsigned slot=1;slot<pending.size();++slot)if(pending[slot]){
  unsigned handle=0,flags=0;BYTE finished=0;Read(race+0x18+slot*0x194,handle);Read(race+0x18+slot*0x194+0x1C,flags);Read(race+0x18+slot*0x194+0x21,finished);
  const auto wanted=pending[slot];pending[slot]=0;
#ifdef MWEL_FINISH_NO_ESCAPE_DIAG
  if(handle==wanted&&!flags&&finished){Log("EXPERIMENT_FINISHED_ESCAPE_SUPPRESSED slot=%u handle=%u diagnostic_only=1",slot,handle);continue;}
#endif
  if(handle==wanted&&!flags&&finished&&!Escape(slot,handle))Log("HOT_PURSUIT_FINISHED_ESCAPE_REFUSED slot=%u handle=%u native_goal_unavailable=1",slot,handle);
 }
}
}
bool MWELHotPursuitEscaping(unsigned handle){return handle&&std::find(hot_pursuit_finish::escaping.begin(),hot_pursuit_finish::escaping.end(),handle)!=hot_pursuit_finish::escaping.end();}
void MWELHotPursuitFinish(void* actor,bool genuine,bool player,int slot,void(__cdecl* nativeFinish)(void*)){
 using namespace hot_pursuit_finish;
 const auto package=event_race_policies::Current();unsigned race=0,params=0;
 const bool hot=genuine&&slot>=0&&slot<8&&package&&package->authoring.value("combatMode",std::string("none"))=="hotpursuit"&&Read(0x91E000,race)&&Read(race+0x1968,params);
 if(!hot){nativeFinish(actor);return;}
 if(player){scopeRace=race;scopeInfo=race+0x18+slot*0x194;scopeParams=params;scopeBase=package->authoring.at("cashValue").get<unsigned>();
  resultRace=race;resultKey=MWELNeoCurrentKey();resultParams=params;resultActor=unsigned(actor);Calculate();
 }
 CallNative(actor,nativeFinish);
 unsigned current=0;if(!Read(0x91E000,current)||current!=race)return;
 unsigned flags=0,handle=0;BYTE finished=0;Read(race+0x18+slot*0x194+0x1C,flags);Read(race+0x18+slot*0x194+0x21,finished);Read(race+0x18+slot*0x194,handle);
 // Finish can be delivered by a controller's own event dispatch. Deleting
 // its goal here would invalidate that callback's stack. Switch next frame.
 if(!player&&finished&&!flags)pending[unsigned(slot)]=handle;
}
