#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "ArmsSurvivalPolicy.h"
bool Copy(const void* p,void* o,size_t n){__try{memcpy(o,p,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void Log(const char*,...){}
namespace event_race_policies {
struct Package{nlohmann::json authoring={{"combatMode","armsrace"}};} package;
Package* Current(){return &package;}
}
unsigned eventKey=77,finishCalls=0,recordCalls=0,placementCalls=0;
unsigned MWELNeoCurrentKey(){return eventKey;}
int MWELArmsNativeSlot(void*){return 1;}
bool MWELArmsPrepareRanking(){return true;}
namespace combat_health_runtime {
combat_health::Ledger ledger;unsigned generation=1,depth=0,impactOwner=0;DWORD gameThread=0;
bool installed=true,racing=true;
struct Binding{unsigned driver=0;};std::array<Binding,16> bindings{};
template<class T>bool Read(unsigned p,T& out){return p>=0x10000&&Copy(reinterpret_cast<void*>(p),&out,sizeof(out));}
bool Racing(){return racing;}
}
namespace runtime_lifecycle {
void* FinishActor(void*){return reinterpret_cast<void*>(0x12345000);}
void(__cdecl* originalFinish)(void*)=nullptr;
}
namespace race_start_experiment {bool enabled=false;void(__cdecl* originalFinish)(void*)=nullptr;}
namespace profile_progress {
void Record(const event_race_policies::Package&,void*,bool genuine,bool forced,bool survival){
 if(genuine||forced||!survival)throw std::runtime_error("survival win must remain distinct from a physical finish");++recordCalls;
}
}
#include "ArmsSurvival.h"
static unsigned checks=0;
static void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
constexpr unsigned Race=0x912000;
void __cdecl NativeFinish(void*){
 ++finishCalls;
 *reinterpret_cast<BYTE*>(Race+0x18+0x21)=1;
 arms_survival_runtime::WinningRanks(Race);
 // The real finish can restore the health ledger while ending the race.
 combat_health_runtime::ledger={};combat_health_runtime::racing=false;
}
void __cdecl NativePlacement(void* sender,void* actor,unsigned place){
 if(sender||!actor||place!=1||finishCalls!=1)throw std::runtime_error("native finish must precede the winning placement event");++placementCalls;
}
MWELArmsPlacementFn MWELArmsNativePlacement(){return NativePlacement;}
void Setup(unsigned count=4){
 using namespace combat_health_runtime;
 memset(reinterpret_cast<void*>(Race),0,0x2000);*reinterpret_cast<unsigned*>(0x91E000)=Race;
 *reinterpret_cast<unsigned*>(Race+0x1958)=count;
 ledger={};ledger.Observe(eventKey,generation,true);racing=installed=true;depth=impactOwner=0;gameThread=GetCurrentThreadId();
 event_race_policies::package.authoring["combatMode"]="armsrace";bindings={};
 for(unsigned i=0;i<count&&i<16;++i){
  *reinterpret_cast<unsigned*>(Race+0x18+i*0x194)=i+1;
  auto c=ledger.Join(i,i+1);bindings[i].driver=i?3:0;
  if(i){c->Damage(combat_health::Hit::Collision,300);*reinterpret_cast<BYTE*>(Race+0x18+i*0x194+0x1D)=1;}
 }
 for(unsigned i=0;i<count&&i<16;++i)*reinterpret_cast<unsigned*>(Race+0x18+i*0x194+0x10)=count-i;
 arms_survival_runtime::Reset();runtime_lifecycle::originalFinish=NativeFinish;
 race_start_experiment::originalFinish=NativeFinish;race_start_experiment::enabled=false;finishCalls=recordCalls=placementCalls=0;
}
int main(){try{
 auto fixed=VirtualAlloc(reinterpret_cast<void*>(0x910000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 Check(fixed==reinterpret_cast<void*>(0x910000),"fixed native-data harness unavailable");
 using namespace combat_health_runtime;
 for(unsigned count=2;count<=16;++count){
  for(unsigned first=1;first<=count;++first){std::array<unsigned,16> ranks{};ranks[0]=first;unsigned n=1;for(unsigned rank=1;rank<=count;++rank)if(rank!=first)ranks[n++]=rank;Check(arms_survival::WinningRanks(ranks,count)&&ranks[0]==1,"survivor wins regardless of previous score rank");for(unsigned i=1;i<count;++i)Check(ranks[i]==i+1,"rival order remains stable and unique");}

  Setup(count);std::array<unsigned,16> handles{};for(unsigned i=0;i<count;++i)handles[i]=i+1;
  unsigned wrecks=((1u<<count)-1u)&~1u;
  Check(arms_survival::PlayerAlone(ledger,count,handles,wrecks),"complete observed wrecked field must qualify");
  for(unsigned i=1;i<count;++i){
   auto saved=ledger.racers[i];ledger.racers[i].retired=false;ledger.racers[i].remaining=1;
   Check(!arms_survival::PlayerAlone(ledger,count,handles,wrecks),"one remaining AI must prevent early victory");ledger.racers[i]=saved;
   Check(!arms_survival::PlayerAlone(ledger,count,handles,wrecks&~(1u<<i)),"HP zero alone cannot substitute for native destruction");
   ledger.racers[i].occupied=false;Check(!arms_survival::PlayerAlone(ledger,count,handles,wrecks),"unobserved/despawned slot is not a wreck");ledger.racers[i]=saved;
  }
  ledger.racers[0].Damage(combat_health::Hit::Collision,300);
  Check(!arms_survival::PlayerAlone(ledger,count,handles,wrecks),"simultaneous player destruction must lose");
 }
 Setup(1);arms_survival_runtime::Frame();arms_survival_runtime::Frame();Check(!finishCalls,"one-car event never wins immediately");
 Setup();arms_survival_runtime::Frame();Check(!finishCalls,"defer out of damage callback for a full field recheck");
 ledger.racers[1].retired=false;ledger.racers[1].remaining=1;arms_survival_runtime::Frame();Check(!finishCalls,"changed eligibility cancels pending dispatch");
 Setup();arms_survival_runtime::Frame();arms_survival_runtime::Frame();
 Check(finishCalls==1&&recordCalls==1&&placementCalls==1&&arms_survival_runtime::ResultPending(),"one native victory and one survival record");
 arms_survival_runtime::Frame();Check(finishCalls==1,"postrace frames cannot duplicate rewards/results");
 Setup();++generation;ledger.epoch=generation;arms_survival_runtime::Frame();arms_survival_runtime::Frame();Check(finishCalls==1,"same-pointer retry may earn a fresh victory");
 Setup();event_race_policies::package.authoring["combatMode"]="hotpursuit";arms_survival_runtime::Frame();arms_survival_runtime::Frame();Check(!finishCalls,"Hot Pursuit does not inherit the Arms-only rule");
 Setup();ledger.epoch=generation-1;arms_survival_runtime::Frame();arms_survival_runtime::Frame();Check(!finishCalls,"old retry health cannot win a new generation");
 Setup();*reinterpret_cast<BYTE*>(Race+0x18+0x21)=1;arms_survival_runtime::Frame();arms_survival_runtime::Frame();Check(!finishCalls,"normal finish is not dispatched twice");
 Setup();depth=1;arms_survival_runtime::Frame();arms_survival_runtime::Frame();Check(!finishCalls,"weapon callback cannot trigger nested result teardown");
 VirtualFree(fixed,0,MEM_RELEASE);
 printf("PASS %u survival policy, native dispatch ownership, retry and progress checks\n",checks);return 0;
}catch(const std::exception& e){printf("FAIL %s\n",e.what());return 1;}}
