#include <windows.h>
#include <nlohmann/json.hpp>
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include "../../integration/MWCombatContract.h"
// Exercise the same SEH-backed reader as Runtime.cpp: ReadProcessMemory would
// conceal first-chance exceptions and make the released-HUD regression vacuous.
bool Copy(const void* a,void* b,size_t n){__try{memcpy(b,a,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void Log(const char*,...){}
constexpr int MH_OK=0;
int MH_CreateHook(void*,void*,void**){return 1;}
int MH_EnableHook(void*){return 1;}
int MH_DisableHook(void*){return 1;}
int MH_RemoveHook(void*){return 1;}
namespace event_race_policies {
struct Package {nlohmann::json authoring={{"combatMode","armsrace"}};} package;
Package* Current(){return &package;}
}
unsigned MWELNeoCurrentKey(){return 77;}
bool worldLogEnabled=false;
bool gauntletBinding=false;
bool MWELGauntletParticipant(unsigned slot,unsigned handle){return gauntletBinding&&slot==1&&handle==22&&event_race_policies::package.authoring["combatMode"]=="gauntlet";}
#include "CombatHealthRuntime.h"
#include "CombatRetiredMarkers.h"
#include "CombatDamageVisuals.h"
#include "GauntletBackupPolicy.h"
#include "GauntletCombatPolicy.h"
#include "GauntletVehiclePolicy.h"
unsigned checks=0,destroyCalls=0;
unsigned observedAccessFaults=0;
LONG CALLBACK ObserveAccessFault(EXCEPTION_POINTERS* fault){if(fault->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION)++observedAccessFaults;return EXCEPTION_CONTINUE_SEARCH;}

void Check(bool ok,const char* message){++checks;if(!ok){std::printf("FAIL %s\n",message);std::exit(1);}}
void __fastcall NativeDestroyStub(void* self,void*){++destroyCalls;*reinterpret_cast<float*>(static_cast<BYTE*>(self)+0x3C)=1;}
unsigned nativeImpacts=0;bool expectImpactShield=true,injectNativeTotalDamage=false;
float ImpactMassStub(unsigned){return 1000;}
unsigned markerDrawCalls=0;
unsigned roadblockMotionSim=0;
combat_impact::Motion fixturePlayerMotion{},fixtureBlockMotion{};
bool RoadblockMotionStub(unsigned sim,combat_impact::Motion& out){out=sim==roadblockMotionSim?fixtureBlockMotion:fixturePlayerMotion;return out.valid;}
void __fastcall MarkerDrawStub(unsigned,void*,const void*,const void*,unsigned,unsigned){++markerDrawCalls;}
void __cdecl MarkerColorStub(unsigned icon,const int* color){memcpy(reinterpret_cast<void*>(*reinterpret_cast<unsigned*>(icon+0x2C)),color,16);}
void __fastcall ImpactStub(unsigned self,void*,const combat_health_runtime::ImpactVector*,const combat_health_runtime::ImpactVector*,float impulse,float auxiliary,unsigned surface,unsigned other){
 ++nativeImpacts;Check(impulse>0&&auxiliary==4&&surface==10&&other,"native collision six-argument ABI preserved");
 auto spec=*reinterpret_cast<unsigned*>(self+0x78);
 if(expectImpactShield)Check(*reinterpret_cast<float*>(spec+0x64)==0,"owned native impact disables only total-health accumulation");
 else *reinterpret_cast<float*>(self+0x4C+0x3C)+=.4f;
 if(injectNativeTotalDamage)*reinterpret_cast<float*>(self+0x4C+0x3C)+=.4f;
}
int main(int argc,char**){
 Check(gauntlet_vehicle::SUV("copsuv")&&gauntlet_vehicle::SUV("COPSUVEH")&&gauntlet_vehicle::SUV("cop_rhino"),"SUV and Rhino police model aliases excluded case-insensitively");
 Check(!gauntlet_vehicle::SUV("copghost")&&!gauntlet_vehicle::SUV("copcross")&&!gauntlet_vehicle::SUV("copmidsize"),"ordinary pursuit cars remain available at the authored heat");
 Check(gauntlet_combat::EvadeMeter(-1)==-.95f&&gauntlet_combat::EvadeMeter(-.5f)==-.5f&&gauntlet_combat::EvadeMeter(1)==1,"escape cap preserves native busted half and ordinary engagement");
 combat_impact::Motion ramPlayer{},ramCop{};ramPlayer.position={0,0,30};ramPlayer.forward=ramCop.forward={0,0,1};ramCop.velocity={0,0,20};
 Check(gauntlet_combat::Ram(ramPlayer,ramCop),"near aligned native ram eligible");
 ramCop.position={0,0,60};ramCop.forward={0,0,-1};Check(gauntlet_combat::Ram(ramPlayer,ramCop),"head-on intercept uses native head-on ram");
 ramCop.position.y=12;Check(!gauntlet_combat::Ram(ramPlayer,ramCop),"separate road deck cannot trigger ram");


 ramPlayer={};ramCop={};ramPlayer.forward=ramCop.forward={0,0,1};ramPlayer.velocity={0,0,30};ramCop.velocity={0,0,30};ramCop.position={0,0,20};
 Check(gauntlet_combat::FrontBlock(ramPlayer,ramCop)==gauntlet_combat::Front::Brake,"front same-lane cop brake-checks instead of matching speed");
 ramCop.position.x=5;Check(gauntlet_combat::FrontBlock(ramPlayer,ramCop)==gauntlet_combat::Front::Pit,"offset front cop converges with native PIT instead of stopping off line");
 ramCop.position.y=5;Check(gauntlet_combat::FrontBlock(ramPlayer,ramCop)==gauntlet_combat::Front::None,"front interference excludes another road deck");
 ramCop.position={0,0,-20};Check(gauntlet_combat::FrontBlock(ramPlayer,ramCop)==gauntlet_combat::Front::None,"rear pursuer cannot use front brake-check");
 ramCop.position={0,0,20};ramCop.velocity.z=40;Check(gauntlet_combat::FrontBlock(ramPlayer,ramCop)==gauntlet_combat::Front::None,"pulling-away cop first closes into interception range");
 using namespace combat_health_runtime;
 {
  gauntlet_backup::Trail trail;gauntlet_backup::Anchor anchor;
  Check(!trail.Select(anchor),"backup never invents a placement without player road history");
  for(unsigned i=0;i<41;++i)trail.Observe({float(i*10),20,float(i*i)*.1f},{1,0,0},0,float(i));
  Check(trail.Select(anchor)&&anchor.position.x<400&&anchor.position.y==20,"backup follows actual curved player history rather than a straight rear offset");
  Check(!trail.Due(700,40)&&!trail.Due(700,42.9f)&&trail.Due(700,43),"far replacement requires three simulation seconds above 650 metres");
  trail.Committed(43);Check(!trail.Due(800,46)&&!trail.Due(800,62.9f)&&trail.Due(800,63),"backup requests retain their twenty-second event cooldown");
  Check(!trail.Due(100,64)&&!trail.Due(700,65)&&!trail.Due(700,67.9f),"re-entering range clears the far timer");
  const auto before=trail.count;trail.Observe({500,100,0},{1,0,0},20,68);Check(trail.count==before,"airborne samples do not become spawn anchors");
  trail.Observe({10000,20,0},{1,0,0},0,69);Check(!trail.Select(anchor),"player teleport clears the discontinuous placement trail");
  trail.Observe({10010,20,0},{1,0,0},0,1);Check(trail.count==1&&trail.farSince<0,"simulation clock restart invalidates previous race anchors");
  trail={};for(unsigned i=0;i<41;++i)trail.Observe({float(i*10),20,0},{1,0,0},0,float(i),100);
  Check(trail.Select(anchor)&&anchor.speed==60,"rolling replacement retains road-observed speed with a 60 m/s ceiling");
  trail.Observe({420,20,0},{1,0,0},0,42,-5);Check(trail.points[trail.count-1].speed==0,"reverse or negative speed cannot launch a replacement backward");
 }
 // A finished race can leave a retry deadline on a slot which never emitted
 // again. The next event starts its simulation clock near zero.
 for(auto& car:combat_visuals::cars)for(auto anchor:{&car.spark,&car.smoke}){
  anchor->retry=400;anchor->began=390;anchor->diagnosticAt=410;anchor->observed=true;anchor->handle=77;anchor->record[0]=1;
 }
 combat_visuals::epoch=9;combat_visuals::last=400;combat_visuals::Reset();
 for(const auto& car:combat_visuals::cars)for(const auto* anchor:{&car.spark,&car.smoke})
  Check(anchor->retry==0&&anchor->began==-100&&anchor->diagnosticAt==0&&!anchor->observed&&!anchor->handle&&anchor->record==std::array<BYTE,80>{},"event exit resets every spark/smoke deadline and private record");
 Check(combat_visuals::epoch==0&&combat_visuals::last==-100,"next event may process its first simulation frame");
 float emptyMatrix[16]{};
 Check(!combat_visuals::Update(combat_visuals::cars[0].spark,88,0xF991F8E8,emptyMatrix,{},1,true)&&combat_visuals::cars[0].spark.retry==1.75f,"new race reaches emission attempt immediately instead of waiting for old race time");
 combat_visuals::Reset();
 combat_impact::Motion visualMotion{};visualMotion.position={100,20,300};visualMotion.forward={0,0,1};
 float fxMatrix[16]{};combat_visuals::Matrix(visualMotion,fxMatrix);
 Check(std::abs(fxMatrix[12]-301.84f)<.001f&&std::abs(fxMatrix[13]+100)<.001f&&std::abs(fxMatrix[14]-20.15f)<.001f,"shared fallback engine anchor stays below roof and converts physics to native render coordinates");
 Check(fxMatrix[8]==0&&fxMatrix[9]==0&&fxMatrix[10]==1,"native emitter Z-up points upward after physics conversion");
 float fallbackMatrix[16]{};
 Check(!combat_visuals::EngineMatrix(0,visualMotion,fallbackMatrix)&&!memcmp(fallbackMatrix,fxMatrix,sizeof(fxMatrix)),"missing marker data retains the common hood fallback for sparks and smoke");
 combat_impact::Vector localAnchor{};
 Check(combat_visuals::HeadlightLocal({-2.2f,1,.2f},{-2.2f,-1,.2f},localAnchor)&&std::abs(localAnchor.x+1.85f)<.001f&&localAnchor.y==0&&std::abs(localAnchor.z-.25f)<.001f,"smoke uses headlight midpoint slightly behind and above the lights");
 Check(combat_visuals::HeadlightLocal({2.2f,1,.2f},{2.2f,-1,.2f},localAnchor)&&std::abs(localAnchor.x-1.85f)<.001f,"opposite model forward convention still moves toward the hood");
 Check(!combat_visuals::HeadlightLocal({0,0,0},{0,0,0},localAnchor)&&!combat_visuals::HeadlightLocal({NAN,1,1},{1,-1,1},localAnchor),"missing coincident and malformed markers use fallback");
 {
  std::array<BYTE,0x100> sim{};std::array<BYTE,0x80> render{};std::array<BYTE,0x400> conn{};std::array<BYTE,0x180> info{};std::array<BYTE,0x60> lights{};
  const auto put=[](auto& bytes,unsigned offset,unsigned value){memcpy(bytes.data()+offset,&value,4);};
  const unsigned a=unsigned(lights.data()),b=a+0x30,sentinel=unsigned(info.data())+0x104;
  put(sim,0xDC,unsigned(render.data()));put(render,0,0x8ADCC8);put(render,0x10,77);
  const auto model=unsigned(render.data())-0x4C;
  put(conn,0,0x8B5254);put(conn,8,model);put(conn,0x44,unsigned(info.data()));put(info,0x104,a);put(info,0x108,b);
  put(lights,0,b);put(lights,4,sentinel);put(lights,8,0x9DB90133);put(lights,0x30,sentinel);put(lights,0x34,a);put(lights,0x38,0xD09091C6);
  const combat_impact::Vector left{-2.2f,1,.2f},right{-2.2f,-1,.2f};memcpy(lights.data()+0x10,&left,12);memcpy(lights.data()+0x40,&right,12);
  const float body[]={0,-1,0,0,1,0,0,0,0,0,1,0,300,-100,20,1};memcpy(conn.data()+0x330,body,64);
  auto& cached=combat_visuals::connections[0];cached={model,unsigned(conn.data()),unsigned(info.data()),77};cached.markers=combat_visuals::LightPair(unsigned(info.data()),cached.local);combat_impact::Vector world{};
  Check(combat_visuals::HeadlightAnchor(unsigned(sim.data()),visualMotion,world)&&std::abs(world.x-300)<.001f&&std::abs(world.y+98.15f)<.001f&&std::abs(world.z-20.25f)<.001f,"native light list transforms with full rotated body matrix");
  Check(combat_visuals::EngineMatrix(unsigned(sim.data()),visualMotion,fxMatrix)&&std::abs(fxMatrix[12]-world.x)<.001f&&std::abs(fxMatrix[13]-world.y)<.001f&&std::abs(fxMatrix[14]-world.z)<.001f,"both particle emitters receive the validated rotated headlight anchor");
  put(conn,8,model+16);Check(!combat_visuals::HeadlightAnchor(unsigned(sim.data()),visualMotion,world),"reused render connection cannot attach smoke to another car");put(conn,8,model);
  put(render,0x10,78);Check(!combat_visuals::HeadlightAnchor(unsigned(sim.data()),visualMotion,world),"recycled VehicleFX address requires matching HMODEL generation");put(render,0x10,77);
  put(lights,8,0);Check(!combat_visuals::LightPair(unsigned(info.data()),localAnchor),"one missing headlight takes dimension fallback");put(lights,8,0x9DB90133);
  put(lights,0,a);Check(!combat_visuals::LightPair(unsigned(info.data()),localAnchor),"invalid light list cycle rejected within bounded traversal");put(lights,0,b);
  auto shifted=body[12]+100;memcpy(conn.data()+0x360,&shifted,4);Check(!combat_visuals::HeadlightAnchor(unsigned(sim.data()),visualMotion,world),"stale stage matrix cannot move smoke to a distant location");
  combat_visuals::connections={};
 }
 void* fixed=VirtualAlloc(reinterpret_cast<void*>(0x910000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(fixed!=reinterpret_cast<void*>(0x910000)){const auto error=GetLastError();MEMORY_BASIC_INFORMATION info{};VirtualQuery(reinterpret_cast<void*>(0x910000),&info,sizeof(info));std::printf("FIXTURE_ADDRESS_CONFLICT error=%lu base=%p owner=%p bytes=%zu state=%lX type=%lX\n",error,info.BaseAddress,info.AllocationBase,info.RegionSize,info.State,info.Type);}
 Check(fixed==reinterpret_cast<void*>(0x910000),"isolated native-global fixture");
 std::vector<BYTE> status(0x1A00),racerMemory(512),copMemory(256),racerSim(256),copSim(64);
 const auto race=unsigned(status.data()),racer=unsigned(racerMemory.data()+192),cop=unsigned(copMemory.data()+64);
 *reinterpret_cast<unsigned*>(0x91E000)=race;
 *reinterpret_cast<BYTE*>(race+0x198A)=1;*reinterpret_cast<BYTE*>(race+0x1978)=1;
 *reinterpret_cast<unsigned*>(race+0x1960)=1;*reinterpret_cast<unsigned*>(race+0x1958)=2;
 *reinterpret_cast<unsigned*>(race+0x18)=11;*reinterpret_cast<unsigned*>(race+0x18+0x194)=22;
 *reinterpret_cast<unsigned*>(racer)=0x8AD324;*reinterpret_cast<unsigned*>(cop)=0x8AD40C;
 *reinterpret_cast<unsigned*>(racer-0x18)=unsigned(racerSim.data());*reinterpret_cast<unsigned*>(cop-0x18)=unsigned(copSim.data());
 *reinterpret_cast<unsigned*>(racerSim.data()+8)=11;*reinterpret_cast<unsigned*>(copSim.data()+8)=99;
 installed=true;gameThread=GetCurrentThreadId();originalDestroy=reinterpret_cast<void(__thiscall*)(void*)>(NativeDestroyStub);
 Frame();mwcombat::HealthV1 health;
 Check(Observe(0,11,racer,0,&health,sizeof(health))&&health.maximum==300,"bind real native damage layout");
 *reinterpret_cast<float*>(racer+0x3C)=.3f;Frame();Query(0,11,&health,sizeof(health));
 Check(std::abs(health.remaining-285)<.001f&&std::abs(*reinterpret_cast<float*>(racer+0x3C)-.05f)<.00001f,"native fallback collision delta is halved exactly once");
 Check(Effect(racer,11,1,0),"EMP enters scoped native effect");Destroy(reinterpret_cast<void*>(racer),nullptr);
 Check(destroyCalls==0,"native EMP cannot bypass event health via immediate Destroy");
 Check(Effect(racer,11,1,1)&&Query(0,11,&health,sizeof(health))&&health.remaining==260,"EMP still applies exactly 25 police-reference HP to racer");
 Check(Effect(racer,11,2,0)&&Effect(racer,11,2,2)&&Query(0,11,&health,sizeof(health))&&health.remaining==260,"failed effect cannot charge health");
 NewGeneration();Frame();Observe(0,11,racer,0,&health,sizeof(health));
 for(unsigned i=0;i<12;++i){Check(Effect(racer,11,1,0),"repeat native EMP scope");Destroy(reinterpret_cast<void*>(racer),nullptr);Check(Effect(racer,11,1,1),"finish native EMP scope");}
 Check(Query(0,11,&health,sizeof(health))&&health.remaining==0&&destroyCalls==1,"twelve EMPs retire racer through one native destruction");
 for(unsigned i=0;i<3;++i){Frame();Effect(racer,11,2,0);Effect(racer,11,2,1);Destroy(reinterpret_cast<void*>(racer),nullptr);}
 Check(destroyCalls==1,"retired participant cannot dispatch duplicate native loss/menu events");
 *reinterpret_cast<float*>(cop+0x3C)=.2f;
 Check(Effect(cop,99,1,0),"native police health effect scope");Destroy(reinterpret_cast<void*>(cop),nullptr);Effect(cop,99,1,1);
 Check(std::abs(*reinterpret_cast<float*>(cop+0x3C)-.45f)<.00001f&&destroyCalls==1,"cop takes 25 percent and native premature destruction is suppressed");
 event_race_policies::package.authoring["combatMode"]="hotpursuit";
 for(unsigned kind=1;kind<=3;++kind){
  NewGeneration();Frame();Observe(0,11,racer,0,&health,sizeof(health));
  Check(Effect(racer,11,kind,0,true)&&Effect(racer,11,kind,1,true)&&Query(0,11,&health,sizeof(health))&&std::abs(health.remaining-(300-combat_health::WeaponUnits(static_cast<combat_health::Hit>(kind))*.75f))<.001f,"Hot Pursuit police weapons charge 75 percent of the racer weapon damage");
 }
 NewGeneration();Frame();Observe(0,11,racer,0,&health,sizeof(health));
 Check(Effect(racer,11,1,0)&&Effect(racer,11,1,1)&&Query(0,11,&health,sizeof(health))&&health.remaining==275,"racer-origin EMP remains full strength in Hot Pursuit");
 for(unsigned kind=1;kind<=3;++kind){Check(Effect(cop,99,kind,0)&&effects[depth-1].nativePolice,"Hot Pursuit police accept native weapon execution without fixed combat damage");
  *reinterpret_cast<float*>(cop+0x3C)=.8f;Check(Effect(cop,99,kind,1)&&depth==0&&std::abs(*reinterpret_cast<float*>(cop+0x3C)-.8f)<.00001f,"native police damage is retained exactly after EMP spike or shock");}
 const auto nativeDestroyedBefore=destroyCalls;*reinterpret_cast<float*>(cop+0x3C)=.2f;
 static unsigned reportedCop=0,reportedTarget=0;
 policeTarget=[](unsigned handle)->unsigned{return handle+1;};
 policeDestroyed=[](unsigned handle,unsigned target){reportedCop=handle;reportedTarget=target;};
 Check(Effect(cop,99,1,0),"normal police EMP opens a transparent effect scope");Destroy(reinterpret_cast<void*>(cop),nullptr);
 Check(Effect(cop,99,1,1)&&destroyCalls==nativeDestroyedBefore+1&&*reinterpret_cast<float*>(cop+0x3C)>=1,"normal police destruction is never suppressed by Hot Pursuit's racer HP protection");
 Check(reportedCop==99&&reportedTarget==100,"confirmed native destruction retains the target captured before native pursuit teardown");
 static unsigned regenCalls=0,regenAttacker=0;
 isCross=[](unsigned handle){return handle==99;};
 crossKilledByRacer=[](unsigned cop,unsigned attacker){Check(cop==99,"Cross callback retains destroyed handle");++regenCalls;regenAttacker=attacker;};
 attackSource=11;*reinterpret_cast<float*>(cop+0x3C)=.2f;Destroy(reinterpret_cast<void*>(cop),nullptr);
 Check(regenCalls==1&&regenAttacker==11,"Cross kill credits the actual racer hit context instead of its pursuit target");
 Destroy(reinterpret_cast<void*>(cop),nullptr);Check(regenCalls==1,"repeated destruction cannot grant regeneration twice");
 attackSource=0;*reinterpret_cast<float*>(cop+0x3C)=.2f;Destroy(reinterpret_cast<void*>(cop),nullptr);
 Check(regenCalls==1,"unattributed or environmental Cross destruction grants no regeneration");
 isCross=[](unsigned){return false;};attackSource=11;*reinterpret_cast<float*>(cop+0x3C)=.2f;Destroy(reinterpret_cast<void*>(cop),nullptr);
 Check(regenCalls==1,"ordinary Corvette destruction cannot grant Cross regeneration");
 isCross=nullptr;crossKilledByRacer=nullptr;attackSource=0;
 policeTarget=nullptr;policeDestroyed=nullptr;
 destroyCalls=nativeDestroyedBefore;
 *reinterpret_cast<float*>(cop+0x3C)=.45f;
 event_race_policies::package.authoring["combatMode"]="armsrace";
 Check(!Observe(0,99,cop,2,&health,sizeof(health))&&!Observe(16,11,racer,0,&health,sizeof(health)),"traffic/police cannot impersonate a racer slot");
 {
  event_race_policies::package.authoring["combatMode"]="gauntlet";
  *reinterpret_cast<unsigned*>(race+0x1958)=1;gauntletBinding=true;
  *reinterpret_cast<unsigned*>(copSim.data()+8)=22;
  Check(Observe(1,22,cop,2,&health,sizeof(health))&&(health.flags&2)&&health.remaining==300,"only Gauntlet's exact second participant binds as invincible police");
  Check(Effect(cop,22,2,0)&&Effect(cop,22,2,1)&&Query(1,22,&health,sizeof(health))&&health.remaining==300,"Gauntlet pursuer survives spike health damage");
  *reinterpret_cast<float*>(cop+0x3C)=.9f;Frame();
  Check(Query(1,22,&health,sizeof(health))&&health.remaining==300,"Gauntlet pursuer survives physical collision damage");
  Check(!Observe(0,22,cop,2,&health,sizeof(health)),"Gauntlet police cannot impersonate human slot");
  Check(!Observe(1,99,cop,2,&health,sizeof(health)),"unowned ambient police cannot bind the transient duel slot");
  NewGeneration();event_race_policies::package.authoring["combatMode"]="armsrace";
  Check(!Observe(1,22,cop,2,&health,sizeof(health)),"ordinary armed modes cannot inherit Gauntlet police invincibility");
  *reinterpret_cast<unsigned*>(copSim.data()+8)=99;
  *reinterpret_cast<unsigned*>(race+0x1958)=2;gauntletBinding=false;
 }
 Check(!Query(0,12,&health,sizeof(health))&&!Query(0,11,&health,sizeof(health)-1),"stale handle and ABI size rejected");
 NewGeneration();Frame();Observe(0,11,racer,0,&health,sizeof(health));
 Effect(racer,11,2,0);Effect(racer,11,2,1);Query(0,11,&health,sizeof(health));Check(health.remaining==250,"spike causes exactly 50 police-reference HP");
 *reinterpret_cast<unsigned*>(racerSim.data()+8)=12;*reinterpret_cast<unsigned*>(race+0x18)=12;
 *reinterpret_cast<float*>(racer+0x3C)=0;
 Check(Observe(0,12,racer,0,&health,sizeof(health))&&health.remaining==250,"pooled respawn changes handle without health refill");
 NewGeneration();*reinterpret_cast<float*>(racer+0x3C)=0;Frame();Observe(0,12,racer,0,&health,sizeof(health));
 Check(health.remaining==300&&!bindings[0].retirementSent,"retry resets health and native loss latch even with reused pointers");
 *reinterpret_cast<BYTE*>(race+0x18+0x1F)=1;
 for(unsigned i=0;i<6;++i){Effect(racer,12,2,0);Effect(racer,12,2,1);}
 Check(destroyCalls==1&&!bindings[0].retirementSent,"native arrest remains the scene owner when health reaches zero");
 *reinterpret_cast<float*>(racer+0x3C)=.7f;event_race_policies::package.authoring["combatMode"]="none";Frame();
 Check(!ledger.event&&std::abs(*reinterpret_cast<float*>(racer+0x3C)-.7f)<.00001f,"exit never overwrites externally changed damage");
 // Exercise the actual production impact adapter and ownership restoration.
 event_race_policies::package.authoring["combatMode"]="armsrace";*reinterpret_cast<BYTE*>(race+0x18+0x1F)=0;
 NewGeneration();Frame();Observe(0,12,racer,0,&health,sizeof(health));
 void* clock=VirtualAlloc(reinterpret_cast<void*>(0x980000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Check(clock==reinterpret_cast<void*>(0x980000),"native simulation clock fixture");
 std::array<BYTE,128> specs{};*reinterpret_cast<float*>(specs.data()+0x64)=25;
 const auto self=racer-0x4C;*reinterpret_cast<unsigned*>(self+0x78)=unsigned(specs.data());
 originalImpact=reinterpret_cast<ImpactFn>(ImpactStub);impactMass=ImpactMassStub;ImpactVector point{},normal{1,0,0};
 *reinterpret_cast<float*>(0x9885D8)=1;Impact(self,nullptr,&point,&normal,28000,4,10,unsigned(copSim.data()));Query(0,12,&health,sizeof(health));
 Check(health.remaining==287.5f&&nativeImpacts==1&&*reinterpret_cast<unsigned*>(self+0x78)==unsigned(specs.data())&&*reinterpret_cast<float*>(specs.data()+0x64)==25,"wall and ram impulse is halved while native visuals and shared specs are preserved");
 *reinterpret_cast<float*>(0x9885D8)=1.1f;Impact(self,nullptr,&point,&normal,28000,4,10,unsigned(copSim.data()));Query(0,12,&health,sizeof(health));Check(health.remaining==287.5f,"repeated callback for continuous contact cannot multiply HP damage");
 *reinterpret_cast<float*>(0x9885D8)=2;Impact(self,nullptr,&point,&normal,28000,4,10,unsigned(copSim.data()));Query(0,12,&health,sizeof(health));Check(health.remaining==275,"separated physical impact is charged again at half damage");
 expectImpactShield=false;Effect(racer,12,1,0);Impact(self,nullptr,&point,&normal,28000,4,10,unsigned(copSim.data()));Effect(racer,12,1,1);Query(0,12,&health,sizeof(health));Check(health.remaining==250,"weapon scope does not add collision HP a second time");
 expectImpactShield=true;*reinterpret_cast<BYTE*>(race+0x1978)=0;expectImpactShield=false;Impact(self,nullptr,&point,&normal,28000,4,10,unsigned(copSim.data()));Frame();Query(0,12,&health,sizeof(health));Check(health.remaining==250,"countdown bump does not damage event health");
 *reinterpret_cast<BYTE*>(race+0x1978)=1;expectImpactShield=true;
 nativeTypeGetterVerified=true; // Native instruction guard is checked against the retained executable separately.
 std::array<unsigned,2> simTable{0,0x686A30};std::array<BYTE,64> scenery{};
 *reinterpret_cast<unsigned*>(scenery.data())=unsigned(simTable.data());*reinterpret_cast<unsigned*>(scenery.data()+8)=123;
 const unsigned prop=unsigned(scenery.data());
 for(unsigned type:{2u,8u}){
  *reinterpret_cast<unsigned*>(scenery.data()+0x30)=type;Check(BreakableContact(prop),"native smackable and fragment classification");
  const auto before=health.remaining;const auto visualCalls=nativeImpacts;injectNativeTotalDamage=true;
  *reinterpret_cast<float*>(0x9885D8)+=1;Impact(self,nullptr,&point,&normal,10000000,4,10,prop);Frame();Query(0,12,&health,sizeof(health));
  Check(health.remaining==before&&nativeImpacts==visualCalls+1,"breakable scenery has zero HP damage including native fallback; native callback still runs");
  injectNativeTotalDamage=false;
 }
 *reinterpret_cast<unsigned*>(scenery.data()+0x30)=1;Check(!BreakableContact(prop)&&!BreakableContact(0)&&!BreakableContact(1),"vehicles world walls and invalid pointers are not exempt");
 const auto beforeVehicle=health.remaining;*reinterpret_cast<float*>(0x9885D8)+=1;Impact(self,nullptr,&point,&normal,28000,4,10,prop);Query(0,12,&health,sizeof(health));Check(health.remaining==beforeVehicle-12.5f,"vehicle contacts remain damaging");
 {
  std::array<BYTE,0x200> blockCar{};std::array<BYTE,128> blockAi{};
  const auto other=unsigned(blockCar.data()),ai=unsigned(blockAi.data());
  *reinterpret_cast<unsigned*>(other)=unsigned(simTable.data());*reinterpret_cast<unsigned*>(other+8)=321;
  *reinterpret_cast<unsigned*>(other+0x30)=1;*reinterpret_cast<unsigned*>(other+0x80)=0x8AA828;
  *reinterpret_cast<unsigned*>(other+0x80+0x94)=2;*reinterpret_cast<unsigned*>(other+0x80+0x54)=ai;
  *reinterpret_cast<unsigned*>(ai)=0x892480;*reinterpret_cast<unsigned*>(ai+0x74)=1234;
  Check(RoadblockContact(other),"assigned ground roadblock recognized through exact sim vehicle AI chain");
  const auto priorMotionReader=motionReader;motionReader=RoadblockMotionStub;motions={};roadblockMotionSim=other;
  fixturePlayerMotion={};fixtureBlockMotion={};fixturePlayerMotion.valid=fixtureBlockMotion.valid=true;
  fixturePlayerMotion.velocity={0,0,40};fixturePlayerMotion.forward={0,0,1};fixtureBlockMotion.position={0,0,4};fixtureBlockMotion.forward={-1,0,0};
  Check(combat_impact::EnteringRoadblock(fixturePlayerMotion,fixtureBlockMotion),"player drives into transverse stationary roadblock");
  fixtureBlockMotion.forward={0,0,-1};Check(combat_impact::EnteringRoadblock(fixturePlayerMotion,fixtureBlockMotion),"roadblock facing against traffic is still exempt");
  fixtureBlockMotion.position.y=5;Check(!combat_impact::EnteringRoadblock(fixturePlayerMotion,fixtureBlockMotion),"another road deck cannot grant roadblock immunity");fixtureBlockMotion.position.y=0;
  const auto before=health.remaining;injectNativeTotalDamage=true;*reinterpret_cast<float*>(0x9885D8)+=1;
  Impact(self,nullptr,&point,&normal,28000,4,10,other);Frame();Query(0,12,&health,sizeof(health));
  Check(health.remaining==before,"player roadblock crash preserves physics and ignores both impulse and native fallback HP");
  injectNativeTotalDamage=false;bindings[0].driver=3;*reinterpret_cast<float*>(0x9885D8)+=1;
  Impact(self,nullptr,&point,&normal,28000,4,10,other);Query(0,12,&health,sizeof(health));
  Check(health.remaining<before,"AI racer roadblock collision is not exempt");bindings[0].driver=0;
  fixturePlayerMotion.velocity={};fixtureBlockMotion.velocity={0,0,-20};
  const auto beforeIncoming=health.remaining;*reinterpret_cast<float*>(0x9885D8)+=1;
  Impact(self,nullptr,&point,&normal,28000,4,10,other);Query(0,12,&health,sizeof(health));
  Check(health.remaining<beforeIncoming,"roadblock police hitting a stationary player still inflicts damage");
  fixturePlayerMotion.valid=false;const auto beforeUnknown=health.remaining;*reinterpret_cast<float*>(0x9885D8)+=1;
  Impact(self,nullptr,&point,&normal,28000,4,10,other);Query(0,12,&health,sizeof(health));
  Check(health.remaining<beforeUnknown,"unknown motion cannot grant player roadblock immunity");
  fixturePlayerMotion.valid=true;fixturePlayerMotion.velocity={0,0,40};fixtureBlockMotion.velocity={};const auto beforeReleased=health.remaining;
  *reinterpret_cast<unsigned*>(ai+0x74)=0;Check(!RoadblockContact(other),"released roadblock chase car loses contact exemption");
  *reinterpret_cast<float*>(0x9885D8)+=1;Impact(self,nullptr,&point,&normal,28000,4,10,other);Query(0,12,&health,sizeof(health));
  Check(health.remaining<beforeReleased,"ordinary chaser still damages player after leaving roadblock");
  for(unsigned victim:{0u,3u}){
   bindings[0].driver=victim;event_race_policies::package.authoring["combatMode"]="armsrace";
   ledger.racers[0].remaining=300;impactContacts[0]={};Write(0);
   *reinterpret_cast<float*>(0x9885D8)+=1;Impact(self,nullptr,&point,&normal,28000,4,10,other);
   const float ordinary=300-ledger.racers[0].remaining;
   event_race_policies::package.authoring["combatMode"]="hotpursuit";
   ledger.racers[0].remaining=300;impactContacts[0]={};Write(0);
   *reinterpret_cast<float*>(0x9885D8)+=1;Impact(self,nullptr,&point,&normal,28000,4,10,other);
   Check(ordinary>0&&std::abs((300-ledger.racers[0].remaining)-ordinary*.5f)<.0001f,"actual contact adapter halves Hot Pursuit police damage for human and AI racers");
  }
  bindings[0].driver=0;event_race_policies::package.authoring["combatMode"]="armsrace";
  motionReader=priorMotionReader;motions={};
 }
 *reinterpret_cast<unsigned*>(scenery.data()+0x30)=2;nativeTypeGetterVerified=false;Check(!BreakableContact(prop),"unverified native type getter cannot exempt a contact");
 // Exercise the production map ownership/tint adapter without real FEng calls.
 combat_markers::enabled=true;combat_markers::setColor=MarkerColorStub;combat_markers::nativeDraw=reinterpret_cast<combat_markers::NativeDraw>(MarkerDrawStub);
 const unsigned vehicle=unsigned(racerSim.data())+0x80;*reinterpret_cast<unsigned*>(vehicle)=0x8AA828;
 ledger.racers[0].remaining=0;ledger.racers[0].retired=true;bindings[0].retirementSent=true;
 Check(combat_markers::RetiredSlot(vehicle)<0,"player is never recolored as a crashed AI");
 bindings[0].driver=3;Check(combat_markers::RetiredSlot(vehicle)==0,"retired AI marker resolves exact live sim handle");
 std::array<BYTE,128> icon{};combat_markers::Channels tint{51,204,26,191};const auto nativeTint=tint;const auto image=unsigned(icon.data());
 *reinterpret_cast<unsigned*>(icon.data()+8)=77;*reinterpret_cast<unsigned*>(icon.data()+12)=88;*reinterpret_cast<unsigned*>(icon.data()+0x2C)=unsigned(tint.data());
 combat_markers::pending=vehicle;combat_markers::Draw(0,nullptr,nullptr,nullptr,image,0);
 Check(tint[0]==255&&tint[1]==255&&tint[2]==255&&tint[3]==191&&markerDrawCalls==1,"crashed AI uses native integer white channels with alpha and positioning preserved");
 ledger.racers[0].retired=false;combat_markers::pending=vehicle;combat_markers::Draw(0,nullptr,nullptr,nullptr,image,0);
 Check(tint==nativeTint&&markerDrawCalls==2,"living racer restores original tint when an icon is reused");
 ledger.racers[0].retired=true;combat_markers::pending=vehicle;combat_markers::Draw(0,nullptr,nullptr,nullptr,image,0);tint={102,77,51,204};const auto externalTint=tint;combat_markers::Reset();
 Check(tint==externalTint,"teardown preserves a tint changed by another HUD owner");
 {
  auto page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Check(page!=nullptr,"private retired HUD fixture page");
  const auto retiredIcon=unsigned(page);combat_markers::tints[0].icon=retiredIcon;combat_markers::tints[0].guid=77;combat_markers::tints[0].name=88;
  VirtualFree(page,4096,MEM_DECOMMIT);auto handler=AddVectoredExceptionHandler(1,ObserveAccessFault);observedAccessFaults=0;
  combat_markers::Reset();RemoveVectoredExceptionHandler(handler);
  Check(!observedAccessFaults&&!combat_markers::tints[0].icon,"retry discards released HUD icon without provoking a first-chance access violation");
  VirtualFree(page,0,MEM_RELEASE);
 }
 combat_markers::enabled=true;*reinterpret_cast<unsigned*>(racerSim.data()+8)=99;Check(combat_markers::RetiredSlot(vehicle)<0,"recycled sim handles cannot inherit retired marker state");*reinterpret_cast<unsigned*>(racerSim.data()+8)=12;ledger.racers[0].retired=false;bindings[0].driver=0;
 if(argc>1){
  // Optional CPU measurement of the real production adapter with a no-work
  // native callback; this is not a substitute for game frame-time acceptance.
  expectImpactShield=true;*reinterpret_cast<BYTE*>(race+0x1978)=1;
  LARGE_INTEGER frequency{},start{},finish{};QueryPerformanceFrequency(&frequency);
  const unsigned iterations=10000;QueryPerformanceCounter(&start);
  for(unsigned i=0;i<iterations;++i)Impact(self,nullptr,&point,&normal,1,4,10,unsigned(copSim.data()));
  QueryPerformanceCounter(&finish);
  const double ownedUs=double(finish.QuadPart-start.QuadPart)*1000000.0/double(frequency.QuadPart)/iterations;
  volatile unsigned unrelated=0x12340000;volatile int found=-1;QueryPerformanceCounter(&start);
  for(unsigned i=0;i<iterations*100;++i)found=ImpactSlot(unrelated);
  QueryPerformanceCounter(&finish);
  const double unrelatedUs=double(finish.QuadPart-start.QuadPart)*1000000.0/double(frequency.QuadPart)/(iterations*100);
  std::printf("BENCH adapter_owned_us=%.3f unbound_fastpath_us=%.3f actual_game_frame_acceptance=0\n",ownedUs,unrelatedUs);
 }
 VirtualFree(clock,0,MEM_RELEASE);
 VirtualFree(fixed,0,MEM_RELEASE);std::printf("PASS %u native combat-health ownership and effect checks\n",checks);
}
