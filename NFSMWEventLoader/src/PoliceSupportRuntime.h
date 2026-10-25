#pragma once
namespace hot_pursuit_runtime {
unsigned FindPursuit(unsigned target);
bool ReadyTarget(unsigned handle);
unsigned TargetHandle(unsigned pursuit);
bool ExclusiveJoin(unsigned car,unsigned desired);
using SupportRequestFn=int(__thiscall*)(unsigned);
using HelicopterFn=bool(__thiscall*)(unsigned,unsigned);
SupportRequestFn originalSupportRequest=nullptr;
void* requestSiteTrampoline=nullptr;
HelicopterFn originalHelicopter=nullptr;
void* candidateTrampoline=nullptr;
void* airBudgetTrampoline=nullptr;
void* copRequestTrampoline=nullptr;
using CopRequestFn=const char*(__thiscall*)(unsigned);
CopRequestFn originalCopRequest=nullptr;
struct BlockPlacement {combat_impact::Vector centre{};ULONGLONG until=0;};
std::array<BlockPlacement,16> placedBlocks{};
combat_impact::Vector candidateCentre{};bool candidateKnown=false;
struct HeliBinding {unsigned car=0,handle=0,object=0,target=0;};
std::array<HeliBinding,4> helicopterBindings{};
unsigned supportLeaderLogged=0;ULONGLONG nextSupportLog=0;
ULONGLONG nextCopRequestLog=0,nextSupportFlightLog=0;
void ResetHelicopterSnapshot();
void PublishHelicopterSnapshot(unsigned);
void PublishHelicopterObject(unsigned,unsigned,unsigned);
unsigned __cdecl HelicopterLeaderSim();
bool IsHelicopterTarget(unsigned);
void ResetSupport(){ResetHelicopterSnapshot();placedBlocks={};candidateKnown=false;helicopterBindings={};supportLeaderLogged=0;nextSupportLog=nextCopRequestLog=nextSupportFlightLog=0;}
unsigned SupportLeader(){
 if(!Active())return 0;
 unsigned race=0,count=0,best=0,bestRank=~0u;
 if(!gauntlet_runtime::Read(0x91E000,race)||!gauntlet_runtime::Read(race+0x1958,count)||count>8)return 0;
 for(unsigned slot=0;slot<count;++slot){unsigned handle=0,rank=0,flags=0;BYTE finished=0;
  if(!gauntlet_runtime::Read(race+0x18+slot*0x194,handle)||!gauntlet_runtime::Read(race+0x18+slot*0x194+0x1C,flags)||flags||
     !gauntlet_runtime::Read(race+0x18+slot*0x194+0x21,finished)||finished||!ReadyTarget(handle))continue;
  gauntlet_runtime::Read(race+0x18+slot*0x194+0x10,rank);if(!rank||rank>count)rank=slot+1;
  if(rank<bestRank){bestRank=rank;best=handle;}
 }return best;
}
bool SupportAllowed(unsigned pursuit){
 if(!Active()){
  if(!gauntlet_runtime::Duel())return true;
  unsigned race=0,player=0;return gauntlet_runtime::Read(0x91E000,race)&&gauntlet_runtime::Read(race+0x18,player)&&TargetHandle(pursuit)==player;
 }
 const auto leader=SupportLeader();return leader&&TargetHandle(pursuit)==leader;
}
int __fastcall RequestSupport(unsigned pursuit,void*){
 const bool allowed=SupportAllowed(pursuit);auto callback=originalSupportRequest;
 if(!callback){unsigned table=0,fn=0;if(!gauntlet_runtime::Read(pursuit,table)||!gauntlet_runtime::Read(table+0x84,fn))return 0;callback=reinterpret_cast<SupportRequestFn>(fn);}
 const int result=allowed?callback(pursuit):0;
 if(worldLogEnabled&&Active()&&GetTickCount64()>=nextSupportLog){nextSupportLog=GetTickCount64()+3000;
  Log("HOT_PURSUIT_SUPPORT_REQUEST racer=%u leader=%u allowed=%u roadblock_cops=%d",TargetHandle(pursuit),SupportLeader(),unsigned(allowed),result);}
 return result;
}
// Gate the native manager's call site. The request function may already be
// hooked by another installed MOD; leave its entry and callback chain intact.
void __declspec(naked) RoadblockRequestSite(){
 __asm {call RequestSupport
  push 0x43EBF5
  ret
 }
}
bool __fastcall RequestHelicopter(unsigned manager,void*,unsigned pursuit){
 if(Active()){
  const auto leader=SupportLeader(),desired=leader?FindPursuit(leader):0;if(!desired)return false;
  Log("HOT_PURSUIT_HELI_REQUEST incoming=%u leader=%u native_target_redirect=1",TargetHandle(pursuit),leader);
  pursuit=desired;
 }
 if(!SupportAllowed(pursuit))return false;
 const bool spawned=originalHelicopter(manager,pursuit);
 if(worldLogEnabled&&(Active()||gauntlet_runtime::Duel()))Log("COMBAT_NATIVE_HELI_FACTORY spawned=%u racer=%u",unsigned(spawned),TargetHandle(pursuit));
 return spawned;
}
// The stock manager gates CopRequest (including helicopters) by the ground
// count. Permit the query when an AIR slot is free, then reject ground models
// at the dispatch site if the actual ground budget is full. Counts stay real.
bool __cdecl AirBudgetAvailable(unsigned manager){
 if(!Active()&&!gauntlet_runtime::Duel())return false;
 unsigned active=0,limit=0,air=0,airLimit=0;
 return gauntlet_runtime::Read(manager+0x94,active)&&gauntlet_runtime::Read(manager+0x98,limit)&&active>=limit&&
  gauntlet_runtime::Read(manager+0x9C,air)&&gauntlet_runtime::Read(manager+0xA0,airLimit)&&airLimit<=8&&air<airLimit;
}
const char* __fastcall CopRequest(unsigned pursuit,void*){
 auto callback=originalCopRequest;
 if(!callback){unsigned table=0,fn=0;if(!gauntlet_runtime::Read(pursuit,table)||!gauntlet_runtime::Read(table+0x50,fn))return nullptr;callback=reinterpret_cast<CopRequestFn>(fn);}
 const auto result=callback(pursuit);
 if(worldLogEnabled&&(Active()||gauntlet_runtime::Duel())&&SupportAllowed(pursuit)&&GetTickCount64()>=nextCopRequestLog){
  nextCopRequestLog=GetTickCount64()+3000;float groundTimer=0,heliTimer=0,elapsed=0;unsigned heliGlobal=0;BYTE forced=0;
  gauntlet_runtime::Read(pursuit+0xCC,groundTimer);gauntlet_runtime::Read(pursuit+0xD0,heliTimer);gauntlet_runtime::Read(pursuit+0xB4,elapsed);
  gauntlet_runtime::Read(pursuit+0xD5,forced);gauntlet_runtime::Read(0x90D61C,heliGlobal);
  char model[32]{};if(result)Copy(result,model,sizeof(model)-1);
  Log("COMBAT_NATIVE_SUPPORT_QUERY racer=%u model=%s ground_timer=%.2f heli_timer=%.2f elapsed=%.2f force_heli=%u global_heli=%08X",TargetHandle(pursuit),model,groundTimer,heliTimer,elapsed,unsigned(forced),heliGlobal);
 }
 if(!result||(!Active()&&!gauntlet_runtime::Duel()))return result;
 char name[32]{};if(!Copy(result,name,sizeof(name))||!memchr(name,0,sizeof(name)))return nullptr;
 if(!strcmp(name,"copheli"))return Active()&&!SupportLeader()?nullptr:result;
 unsigned manager=0,active=0,limit=0;
 if(!gauntlet_runtime::Read(0x92C67C,manager)||!gauntlet_runtime::Read(manager-0x54+0x94,active)||
    !gauntlet_runtime::Read(manager-0x54+0x98,limit)||active>=limit)return nullptr;
 return result;
}
void __declspec(naked) AirBudgetSite(){
 __asm {
  pushfd
  pushad
  mov ebp,esp
  sub esp,528
  and esp,0FFFFFFF0h
  fxsave [esp]
  push edi
  call AirBudgetAvailable
  add esp,4
  test al,al
  jz originalGroundCount
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  mov ecx,0
  push 0x43EB8A
  ret
 originalGroundCount:
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  mov ecx,[edi+0x94]
  push 0x43EB8A
  ret
 }
}
void __declspec(naked) CopRequestSite(){
 __asm {call CopRequest
  mov ebx,eax
  push 0x43EBA3
  ret
 }
}
bool SupportSurface(){
 BYTE bytes[8]{};const BYTE request[]={0xFF,0x90,0x84,0,0,0};
 const BYTE heli[]={0x6A,0xFF,0x68,0xBB,0x80,0x86,0,0x64},candidate[]={0x56,0x8D,0x4C,0x24,0x48};
 const bool a=(requestSiteTrampoline||(Copy(reinterpret_cast<void*>(0x43EBEF),bytes,6)&&!memcmp(bytes,request,6)));
 const bool b=(originalHelicopter||(Copy(reinterpret_cast<void*>(0x4269A0),bytes,8)&&!memcmp(bytes,heli,8)));
 const bool c=(candidateTrampoline||(Copy(reinterpret_cast<void*>(0x43DF9A),bytes,5)&&!memcmp(bytes,candidate,5)));
 if(!a||!b||!c)Log("COMBAT_SUPPORT_SURFACE request=%u heli=%u candidate=%u",unsigned(a),unsigned(b),unsigned(c));
 const BYTE budget[]={0x8B,0x8F,0x94,0,0,0},dispatch[]={0xFF,0x52,0x50,0x8B,0xD8};
 const bool d=airBudgetTrampoline||(Copy(reinterpret_cast<void*>(0x43EB84),bytes,6)&&!memcmp(bytes,budget,6));
 const bool e=copRequestTrampoline||(Copy(reinterpret_cast<void*>(0x43EB9E),bytes,5)&&!memcmp(bytes,dispatch,5));
 return a&&b&&c&&d&&e;
}
bool __cdecl CheckRoadblockCandidate(unsigned frame){
 if(!Active()&&!gauntlet_runtime::Duel())return true;
 using namespace gauntlet_runtime;
 combat_impact::Vector left{},right{};
 if(!Copy(reinterpret_cast<void*>(frame+0x20),&left,sizeof(left))||!Copy(reinterpret_cast<void*>(frame+0x38),&right,sizeof(right)))return false;
 const combat_impact::Vector centre{(left.x+right.x)*.5f,(left.y+right.y)*.5f,(left.z+right.z)*.5f};
 if(!std::isfinite(centre.x)||!std::isfinite(centre.y)||!std::isfinite(centre.z))return false;
 const auto now=GetTickCount64();
 for(const auto& b:placedBlocks)if(now<b.until&&police_deployment::Near(centre.x,centre.y,centre.z,b.centre.x,b.centre.y,b.centre.z,180.f))return false;
 std::vector<unsigned> cars;if(!List(cars))return false;
 for(const auto car:cars){unsigned ai=0,driver=0;if(!Read(car+0x94,driver))continue;
  combat_impact::Motion motion;if(!combat_health_runtime::NativeMotion(car-0x80,motion))continue;
  if((driver==0||driver==3)&&police_deployment::Near(centre.x,centre.y,centre.z,motion.position.x,motion.position.y,motion.position.z,90.f)){
   Log("COMBAT_ROADBLOCK_REFUSED near_racer=1 before_pool_activation=1");return false;
  }
  if(Read(car+0x54,ai)&&Table(ai,0x892480)&&Roadblock(car,ai)&&police_deployment::Near(centre.x,centre.y,centre.z,motion.position.x,motion.position.y,motion.position.z,180.f)){
   Log("COMBAT_ROADBLOCK_REFUSED existing_block_nearby=1 before_pool_activation=1");return false;
  }
 }
 candidateCentre=centre;candidateKnown=true;return true;
}
void CommitCandidate(){
 if(!candidateKnown)return;
 const auto now=GetTickCount64();auto it=std::min_element(placedBlocks.begin(),placedBlocks.end(),[](const auto&a,const auto&b){return a.until<b.until;});
 *it={candidateCentre,now+15000};candidateKnown=false;
 Log("COMBAT_ROADBLOCK_PLACED centre=%.2f,%.2f,%.2f shared_support_authority=1",candidateCentre.x,candidateCentre.y,candidateCentre.z);
}
void __declspec(naked) RoadblockCandidate(){
 __asm {
  pushfd
  pushad
  mov ebp,esp
  sub esp,528
  and esp,0FFFFFFF0h
  fxsave [esp]
  lea eax,[ebp+36]
  push eax
  call CheckRoadblockCandidate
  add esp,4
  test al,al
  jz refused
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  push esi
  lea ecx,[esp+0x48]
  push 0x43DF9F
  ret
 refused:
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  push 0x43E1F3
  ret
 }
}
void RedirectHelicopterTarget(unsigned object,unsigned& sim){
 if(IsHelicopterTarget(object)){const auto target=HelicopterLeaderSim();if(target)sim=target;}

}
void RefreshHelicopters(const std::vector<unsigned>& cars){
 using namespace gauntlet_runtime;
 const auto leader=SupportLeader(),desired=leader?FindPursuit(leader):0;
 PublishHelicopterSnapshot(desired?leader:0);
 if(leader!=supportLeaderLogged){supportLeaderLogged=leader;Log("HOT_PURSUIT_SUPPORT_LEADER racer=%u pursuit=%08X native_requests_shared=1",leader,desired);}
 for(auto& b:helicopterBindings)if(b.handle&&Vehicle(b.handle)!=b.car)b={};
 if(!leader||!desired)return;
#ifdef MWEL_SUPPORT_FLIGHT_PROOF
 // Explicit isolated-lab integration stimulus. Never enabled in handoff builds.
 static unsigned flightRequests=0;static ULONGLONG nextFlightRequest=0;
 unsigned proofRace=0,proofPlayer=0,proofManager=0,proofAir=0;
 if(flightRequests<12&&GetTickCount64()>=nextFlightRequest&&Read(0x91E000,proofRace)&&Read(proofRace+0x18,proofPlayer)&&leader!=proofPlayer&&
    Read(0x92C67C,proofManager)&&Read(proofManager-0x54+0x9C,proofAir)&&!proofAir){
  ++flightRequests;nextFlightRequest=GetTickCount64()+1500;Log("EXPERIMENT_NATIVE_HELI_FACTORY_STIMULUS leader=%u natural_request=0 attempt=%u",leader,flightRequests);
  RequestHelicopter(proofManager-0x54,nullptr,desired);
 }
#endif
 const auto racer=Vehicle(leader);if(!racer)return;
 for(const auto car:cars){unsigned ai=0,object=0,current=0,sim=0;
  if(!Read(car+0x54,ai)||!Helicopter(ai)||!Read(ai+0x54,object)||!Table(object,0x891100))continue;
  const auto handle=Handle(car);auto record=std::find_if(helicopterBindings.begin(),helicopterBindings.end(),[&](const auto& b){return b.handle==handle||!b.handle;});
  if(record==helicopterBindings.end())continue;
  *record={car,handle,object,leader};PublishHelicopterObject(object,car,handle);Read(ai+0x70,current);Read(object+0x1C,sim);
  if(worldLogEnabled&&GetTickCount64()>=nextSupportFlightLog){nextSupportFlightLog=GetTickCount64()+3000;
   combat_impact::Motion flight,targetMotion,playerMotion;unsigned race=0,player=0;Read(0x91E000,race);Read(race+0x18,player);
   const auto playerCar=Vehicle(player);
   if(combat_health_runtime::NativeMotion(car-0x80,flight)&&combat_health_runtime::NativeMotion(racer-0x80,targetMotion)&&playerCar&&combat_health_runtime::NativeMotion(playerCar-0x80,playerMotion)){
    const auto toLeader=flight.position-targetMotion.position,toPlayer=flight.position-playerMotion.position;unsigned actual=0;Read(sim+8,actual);
    Log("HOT_PURSUIT_HELI_FLIGHT heli=%u leader=%u actual_target=%u leader_gap=%.1f player_gap=%.1f speed_mps=%.2f position=%.2f,%.2f,%.2f",handle,leader,actual,std::hypot(toLeader.x,toLeader.z),std::hypot(toPlayer.x,toPlayer.z),std::hypot(flight.velocity.x,flight.velocity.z),flight.position.x,flight.position.y,flight.position.z);
   }
  }
  if(current==desired&&sim==racer-0x80)continue;
  if(current!=desired)ExclusiveJoin(car,desired);
  originalTarget(object,racer-0x80);
  Read(ai+0x70,current);Read(object+0x1C,sim);
  Log("HOT_PURSUIT_HELI_HANDOFF heli=%u racer=%u desired=%08X actual=%08X target_sim=%08X verified=%u native_flight=1 fuel_unchanged=1",handle,leader,desired,current,sim,unsigned(current==desired&&sim==racer-0x80));
 }
}
}
