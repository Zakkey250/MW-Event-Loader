#pragma once
#include "ArmsSurvivalPolicy.h"
int MWELArmsNativeSlot(void*);
using MWELArmsPlacementFn=void(__cdecl*)(void*,void*,unsigned);
MWELArmsPlacementFn MWELArmsNativePlacement();
bool MWELArmsPrepareRanking();
namespace arms_survival_runtime {
using combat_health_runtime::Read;
unsigned pendingRace=0,pendingEpoch=0,resultRace=0,resultKey=0,resultEpoch=0;
unsigned resultCount=0;std::array<unsigned,16> resultHandles{};
bool completing=false;
void Reset(){pendingRace=pendingEpoch=resultRace=resultKey=resultEpoch=resultCount=0;resultHandles={};}
bool ResultPending(){
 unsigned race=0;return resultRace&&Read(0x91E000,race)&&race==resultRace&&MWELNeoCurrentKey()==resultKey;
}
void WinningRanks(unsigned race){
 if(!resultCount||race!=resultRace||resultEpoch!=combat_health_runtime::generation||!ResultPending())return;
 std::array<unsigned,16> ranks{};
 for(unsigned i=0;i<resultCount;++i){unsigned handle=0;BYTE wreck=0;
  const auto info=race+0x18+i*0x194;
  if(!Read(info,handle)||handle!=resultHandles[i]||!Read(info+0x10,ranks[i])||!Read(info+0x1D,wreck)||bool(wreck)!=bool(i))return;
 }
 if(!arms_survival::WinningRanks(ranks,resultCount))return;
 for(unsigned i=0;i<resultCount;++i)*reinterpret_cast<unsigned*>(race+0x18+i*0x194+0x10)=ranks[i];
}
void Finish(void* actor,void(__cdecl* nativeFinish)(void*)){
 __try{nativeFinish(actor);}__finally{completing=false;}
}
void Frame(){
 using namespace combat_health_runtime;
 if(completing||!installed||gameThread!=GetCurrentThreadId()||depth||impactOwner||!Racing())return;
 const auto package=event_race_policies::Current();
 if(!package||package->authoring.value("combatMode",std::string("none"))!="armsrace"){pendingRace=pendingEpoch=0;return;}
 unsigned race=0,count=0;BYTE playerFinished=0;unsigned playerOutcome=0;
 if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count<2||count>16||
    !Read(race+0x18+0x21,playerFinished)||playerFinished||!Read(race+0x18+0x1C,playerOutcome)||playerOutcome)return;
 const auto key=MWELNeoCurrentKey();
 if(!key||ledger.event!=key||ledger.epoch!=generation||(resultRace==race&&resultKey==key&&resultEpoch==generation))return;
 std::array<unsigned,16> handles{};unsigned totalled=0;
 for(unsigned slot=0;slot<count;++slot){BYTE wreck=0;
  if(!Read(race+0x18+slot*0x194,handles[slot])||!Read(race+0x18+slot*0x194+0x1D,wreck))return;
  if(wreck)totalled|=1u<<slot;
 }
 if(bindings[0].driver!=0||!arms_survival::PlayerAlone(ledger,count,handles,totalled)){pendingRace=pendingEpoch=0;return;}
 // Finish outside the destruction/weapon callback, after one complete frame
 // of confirmed native outcomes. Re-check the entire field before dispatch.
 if(pendingRace!=race||pendingEpoch!=generation){pendingRace=race;pendingEpoch=generation;return;}
 auto actor=runtime_lifecycle::FinishActor(nullptr);
 if(!actor||MWELArmsNativeSlot(actor)!=1)return;
 const auto nativeFinish=race_start_experiment::enabled?race_start_experiment::originalFinish:runtime_lifecycle::originalFinish;
 const auto nativePlacement=MWELArmsNativePlacement();
 if(!nativeFinish||!nativePlacement||!MWELArmsPrepareRanking())return;
 pendingRace=pendingEpoch=0;resultRace=race;resultKey=key;resultEpoch=generation;completing=true;
 resultCount=count;resultHandles=handles;
 Log("ARMS_SURVIVAL_VICTORY key=%08X racers=%u totalled_mask=%X physical_goal=0 native_finish=1 duplicate_dispatch=0",key,count,totalled);
 Finish(actor,nativeFinish);
 unsigned current=0,rank=0;BYTE finished=0;
 if(Read(0x91E000,current)&&current==race&&Read(race+0x18+0x10,rank)&&Read(race+0x18+0x21,finished)){
  Log("ARMS_SURVIVAL_NATIVE_RESULT key=%08X rank=%u finished=%u physical_goal=0",key,rank,unsigned(finished));
  if(finished&&rank==1){
   profile_progress::Record(*package,reinterpret_cast<void*>(race+0x18),false,false,true);
   // The original basicrace finish handler emits both notifications, in this
   // order. NotifyRaceFinished alone stops the timer but leaves the script in
   // racing. A null sender is explicitly supported by native 005DAD50.
   // No synthetic trigger crossing, lap increment or direct menu call.
   Log("ARMS_SURVIVAL_COMPLETION_EVENT script_place=1 native_notify_placement=1 physical_goal=0");
   nativePlacement(nullptr,actor,1);
  }
 }
}
}
