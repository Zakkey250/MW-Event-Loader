#pragma once
#include "CombatHealthPolicy.h"
#include "CombatImpactPolicy.h"
#include "HotPursuitPolicy.h"
namespace combat_health_runtime {
combat_health::Ledger ledger;
unsigned generation=1;DWORD gameThread=0;bool installed=false,refused=false;
struct Binding {unsigned damage=0,handle=0,driver=0;float original=0,written=0;bool retirementSent=false;};
std::array<Binding,16> bindings{};
void(__thiscall* originalDestroy)(void*)=nullptr;
struct ImpactVector {float x,y,z;};
using ImpactFn=void(__thiscall*)(unsigned,const ImpactVector*,const ImpactVector*,float,float,unsigned,unsigned);
ImpactFn originalImpact=nullptr;
bool impactInstalled=false,impactRefused=false;
bool nativeTypeGetterVerified=false;
unsigned(*policeTarget)(unsigned)=nullptr;
void(*policeDestroyed)(unsigned,unsigned)=nullptr;
std::array<combat_impact::Contacts,16> impactContacts{};
std::array<combat_impact::Motion,16> motions{};
thread_local unsigned impactOwner=0,attackSource=0;
bool(*isCross)(unsigned)=nullptr;
void(*crossKilledByRacer)(unsigned,unsigned)=nullptr;

struct Effect {unsigned damage=0,handle=0,kind=0;float original=0;bool used=false,nativePolice=false,policeSource=false;};
thread_local std::array<Effect,8> effects{};thread_local unsigned depth=0;
template<class T>bool Read(unsigned p,T& out){return p>=0x10000&&Copy(reinterpret_cast<const void*>(p),&out,sizeof(out));}
bool Native(unsigned damage,unsigned handle,unsigned* tableOut=nullptr){
 unsigned table=0,sim=0,instance=0;
 if(damage<0x10000||!Read(damage,table)||(table!=0x8AD324&&table!=0x8AD398&&table!=0x8AD40C)||
    !Read(damage-0x18,sim)||!Read(sim+8,instance)||instance!=handle)return false;
 if(tableOut)*tableOut=table;return true;
}
bool Active(){
 unsigned race=0,play=0;if(!Read(0x91E000,race)||race<0x10000||!Read(race+0x1960,play)||play!=1)return false;
 auto p=event_race_policies::Current();return p&&mwcombat::Weapons(mwcombat::Parse(p->authoring.value("combatMode",std::string("none")).c_str()));
}
bool Participant(unsigned slot,unsigned handle){
 if(MWELGauntletParticipant(slot,handle))return true;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);unsigned count=0,actual=0;
 return race&&Copy(race+0x1958,&count,4)&&count<=16&&slot<count&&Copy(race+0x18+slot*0x194,&actual,4)&&actual==handle;
}
void Write(unsigned slot){
 auto& binding=bindings[slot];auto& car=ledger.racers[slot];if(!Native(binding.damage,binding.handle))return;
 binding.written=1.f-car.Normalized();*reinterpret_cast<float*>(binding.damage+0x3C)=binding.written;
}
void Restore(){
 for(auto& b:bindings){float current=0;
  if(b.damage&&Native(b.damage,b.handle)&&Read(b.damage+0x3C,current)&&std::abs(current-b.written)<.00001f)
   *reinterpret_cast<float*>(b.damage+0x3C)=b.original;
 }
 bindings={};ledger={};impactContacts={};motions={};depth=0;impactOwner=0;
}
void NewGeneration(){Restore();if(++generation==0)++generation;}
void Notice(unsigned kind,unsigned handle,unsigned actor=0,unsigned amount=0){
 auto module=GetModuleHandleW(L"MWArmsAssist.asi");
 auto submit=module?reinterpret_cast<mwcombat::SubmitNotice>(GetProcAddress(module,"MWAA_CombatFeedV1")):nullptr;
 if(!submit||!ledger.event)return;
 mwcombat::NoticeV1 value;value.eventKey=ledger.event;value.epoch=ledger.epoch;value.kind=kind;value.target=handle;value.actor=actor;value.reserved=amount;
 submit(&value,sizeof(value));
}
// The native destruction event marks the racer totalled, runs the ordinary
// loss message and opens ShowLosingPostRaceScreen. Never open a menu directly
// or post it again while the script is already handling this participant.
// A native arrest owns its own scene and is never converted into destruction.
bool Retire(unsigned slot){
 auto& b=bindings[slot];
 if(b.retirementSent||!ledger.racers[slot].retired||!Participant(slot,b.handle)||!Native(b.damage,b.handle))return false;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);unsigned nativeOutcome=0;BYTE finished=0;
 if(!Copy(race+0x18+slot*0x194+0x1C,&nativeOutcome,4)||nativeOutcome||
    !Copy(race+0x18+slot*0x194+0x21,&finished,1)||finished)return false;
 b.retirementSent=true;
 Notice(1,b.handle);
 *reinterpret_cast<float*>(b.damage+0x3C)=.999999f;
 originalDestroy(reinterpret_cast<void*>(b.damage));b.written=1;
 Log("COMBAT_NATIVE_DEFEAT slot=%u handle=%08X player=%u native_destroy_event=1 direct_menu=0 arrest_scene_replaced=0",slot,b.handle,unsigned(b.driver==0));
 return true;
}
bool Racing(){
 unsigned race=0;BYTE active=0,timer=0;
 return Read(0x91E000,race)&&Read(race+0x198A,active)&&Read(race+0x1978,timer)&&active&&timer;
}
void Collision(unsigned slot){
 auto& b=bindings[slot];float raw=0;if(!Native(b.damage,b.handle)||!Read(b.damage+0x3C,raw)||!std::isfinite(raw))return;
 const float delta=std::max(0.f,raw-b.written);
 if(delta>0&&Racing())ledger.racers[slot].Damage(combat_health::Hit::Collision,combat_impact::Scale(delta*100.f));
 Write(slot);
}
int ImpactSlot(unsigned self){
 if(!installed||gameThread!=GetCurrentThreadId()||depth)return -1;
 // Police, traffic and unbound cars take the native path without repeated
 // event/dependency/state queries on every physics contact. Owned racers
 // still validate the current event, running state and live handle here.
 for(unsigned i=0;i<bindings.size();++i)if(bindings[i].damage==self+0x4C){
  if(!Active()||!Racing())return -1;
  if(Native(bindings[i].damage,bindings[i].handle)&&Participant(i,bindings[i].handle)&&!ledger.racers[i].retired)return int(i);
 }
 return -1;
}
float ImpactMass(unsigned self){
 unsigned rb=0,table=0,getMass=0;if(!Read(self+0x90,rb)||!Read(rb,table)||!Read(table+24,getMass))return 0;
 if(getMass!=0x671160&&getMass!=0x6B6340)return 0;
 return reinterpret_cast<float(__thiscall*)(unsigned)>(getMass)(rb);
}
float (*impactMass)(unsigned)=ImpactMass;
bool NativeMotion(unsigned sim,combat_impact::Motion& out){
 unsigned rb=0,table=0,p=0,v=0,f=0,handle=0;
 if(!Read(sim+8,handle)||!handle||!Read(sim+0x4C,rb)||!Read(rb,table)||!Read(table+32,p)||!Read(table+36,v)||!Read(table+52,f)||
    !((p==0x671180&&v==0x671190&&f==0x671270)||(p==0x6B6360&&v==0x6B6370&&f==0x69A040)))return false;
 combat_impact::Motion value;
 const auto position=reinterpret_cast<const combat_impact::Vector*(__thiscall*)(unsigned)>(p)(rb);
 const auto velocity=reinterpret_cast<const combat_impact::Vector*(__thiscall*)(unsigned)>(v)(rb);
 if(!Copy(position,&value.position,sizeof(value.position))||!Copy(velocity,&value.velocity,sizeof(value.velocity)))return false;
 reinterpret_cast<void(__thiscall*)(unsigned,combat_impact::Vector*)>(f)(rb,&value.forward);
 unsigned dimensions=0;const BYTE dimensionsGuard[]={0x8B,0x44,0x24,4,0x81,0xC1,0x98,0,0,0};BYTE observed[10]{};
 if(Read(table+72,dimensions)&&dimensions==0x670E50&&Copy(reinterpret_cast<void*>(dimensions),observed,10)&&!memcmp(observed,dimensionsGuard,10)){
  combat_impact::Vector half{};reinterpret_cast<void(__thiscall*)(unsigned,combat_impact::Vector*)>(dimensions)(rb,&half);
  if(combat_impact::Finite(half)&&half.x>.5f&&half.x<3.5f&&half.y>.2f&&half.y<3&&half.z>1&&half.z<12){value.halfWidth=half.x;value.halfHeight=half.y;value.halfLength=half.z;}
 }
 const float forwardLength=combat_impact::Dot(value.forward,value.forward);
 if(!combat_impact::Finite(value.position)||!combat_impact::Finite(value.velocity)||!combat_impact::Finite(value.forward)||forwardLength<.5f||forwardLength>1.5f||combat_impact::Dot(value.velocity,value.velocity)>350.f*350.f)return false;
 value.handle=handle;Read(0x9885D8,value.sampledAt);value.valid=true;out=value;return true;
}
bool(*motionReader)(unsigned,combat_impact::Motion&)=NativeMotion;
bool MotionFor(unsigned sim,float now,combat_impact::Motion& out){
 unsigned handle=0;if(!Read(sim+8,handle))return false;
 for(unsigned i=0;i<bindings.size();++i)if(bindings[i].handle==handle&&motions[i].valid&&motions[i].handle==handle&&now>=motions[i].sampledAt&&now-motions[i].sampledAt<=.2f){
  unsigned boundSim=0;if(Read(bindings[i].damage-0x18,boundSim)&&boundSim==sim){out=motions[i];return true;}
 }
 return motionReader(sim,out);
}
bool BreakableContact(unsigned other){
 // OnImpact receives ISimable (native code uses vtable[1] for GetSimableType).
 // Recognize the EN1.3 PhysicsObject getter, then read its field without an
 // indirect call. Smackables and their fragments are disposable scenery;
 // vehicles, world walls, unknown layouts and invalid pointers are not exempt.
 unsigned table=0,getType=0,type=0;
 return nativeTypeGetterVerified&&other&&Read(other,table)&&Read(table+4,getType)&&getType==0x686A30&&
        Read(other+0x30,type)&&(type==2||type==8);
}
bool RoadblockContact(unsigned other){
 // Exact EN1.3 ISimable -> IVehicle -> ground AICop chain. A released
 // roadblock car resumes normal collision damage once its assignment clears.
 unsigned table=0,getter=0,type=0,ai=0,driver=0,block=0;
 return nativeTypeGetterVerified&&Read(other,table)&&Read(table+4,getter)&&getter==0x686A30&&
  Read(other+0x30,type)&&type==1&&Read(other+0x80,table)&&table==0x8AA828&&
  Read(other+0x80+0x94,driver)&&driver==2&&Read(other+0x80+0x54,ai)&&Read(ai,table)&&table==0x892480&&
  Read(ai+0x74,block)&&block;
}
float PoliceContactScale(unsigned victimDriver,unsigned otherDriver){
 const auto policy=event_race_policies::Current();
 return hot_pursuit::PoliceContactScale(policy&&policy->authoring.value("combatMode",std::string("none"))=="hotpursuit",victimDriver,otherDriver);
}
void __fastcall Impact(unsigned self,void*,const ImpactVector* point,const ImpactVector* normal,float impulse,float auxiliary,unsigned surface,unsigned other){
 if(drift_race::Current()){
  unsigned sim=0,handle=0,otherTable=0,getter=0,type=0;ImpactVector n{};
  const bool vehicle=other&&Read(other,otherTable)&&Read(otherTable+4,getter)&&getter==0x686A30&&Read(other+0x30,type)&&type==1;
  if(Read(self+0x34,sim)&&Read(sim+8,handle)&&Copy(normal,&n,sizeof(n)))drift_race::WallContact(handle,impulse,impactMass(self),n.y,BreakableContact(other),vehicle);
 }
 const int slot=ImpactSlot(self);
 if(slot<0){
  if(!installed||gameThread!=GetCurrentThreadId()||!Active()){originalImpact(self,point,normal,impulse,auxiliary,surface,other);return;}
  const unsigned prior=attackSource;unsigned otherHandle=0,otherTable=0;
  attackSource=0; // An environment contact must not inherit another hit's killer.
  if(Read(other+8,otherHandle)&&Read(other+0x80,otherTable)&&otherTable==0x8AA828)
   for(unsigned i=0;i<bindings.size();++i)if(bindings[i].handle==otherHandle&&Participant(i,otherHandle))attackSource=otherHandle;
  __try {originalImpact(self,point,normal,impulse,auxiliary,surface,other);}
  __finally {attackSource=prior;}
  return;
 }
 auto& binding=bindings[slot];Collision(unsigned(slot));
 const bool breakable=BreakableContact(other);
 bool roadblock=false;
 ImpactVector n{};const float mass=breakable?0:impactMass(self);float units=!breakable&&Copy(normal,&n,sizeof(n))?combat_impact::Units(impulse,mass,n.y):0;
 unsigned otherHandle=0;if(other)Read(other+8,otherHandle);
 float now=0;Read(0x9885D8,now);
 combat_impact::Ram ram;
 unsigned sim=0,otherTable=0,otherTypeGetter=0,otherType=0;
 if(!breakable&&nativeTypeGetterVerified&&Read(other,otherTable)&&Read(otherTable+4,otherTypeGetter)&&otherTypeGetter==0x686A30&&Read(other+0x30,otherType)&&otherType==1&&Read(binding.damage-0x18,sim)){
  combat_impact::Motion own{},opponent{};combat_impact::Vector contact{};
  if(MotionFor(sim,now,own)&&MotionFor(other,now,opponent)){
   roadblock=bindings[slot].driver==0&&RoadblockContact(other)&&combat_impact::EnteringRoadblock(own,opponent);
   if(Copy(point,&contact,sizeof(contact)))ram=combat_impact::RamContact(own,opponent,contact);
  }
 }
 const bool exempt=breakable||roadblock;
 // Keep native zone/part/light damage. For this owned call alone, disable
 // only native total-health addition, whose one-shot totalled event would
 // otherwise bypass the 300-unit ledger. Specs remain shared and immutable.
 alignas(16) std::array<BYTE,128> localSpecs{};unsigned originalSpecs=0;float threshold=0;bool privateSpecs=false;
 if(Read(self+0x78,originalSpecs)&&Read(originalSpecs+0x64,threshold)&&std::isfinite(threshold)&&threshold>0&&Copy(reinterpret_cast<void*>(originalSpecs),localSpecs.data(),localSpecs.size())){
  *reinterpret_cast<float*>(localSpecs.data()+0x64)=0;
  *reinterpret_cast<unsigned*>(self+0x78)=unsigned(localSpecs.data());privateSpecs=true;
 }
 const unsigned priorOwner=impactOwner;impactOwner=binding.damage;
 __try {originalImpact(self,point,normal,impulse,auxiliary,surface,other);}
 __finally {
  impactOwner=priorOwner;
  if(privateSpecs&&*reinterpret_cast<unsigned*>(self+0x78)==unsigned(localSpecs.data()))*reinterpret_cast<unsigned*>(self+0x78)=originalSpecs;
 }
 if(!Native(binding.damage,binding.handle)||!Participant(unsigned(slot),binding.handle))return;
 // If an unknown spec layout could not be isolated, retain the native result
 // as a floor rather than charging the same physical impact twice.
 float raw=0;if(!exempt&&Read(binding.damage+0x3C,raw)&&std::isfinite(raw))units=std::max(units,combat_impact::Scale(std::max(0.f,raw-binding.written)*100.f));
 // Scale only positively identified police-to-racer physical contacts. Wall,
 // traffic, racer and weapon paths retain their independent damage rules.
 unsigned contactDriver=~0u;
 if(nativeTypeGetterVerified&&otherType==1&&Read(other+0x80,otherTable)&&otherTable==0x8AA828)Read(other+0x80+0x94,contactDriver);
 const float contactScale=contactDriver==2?PoliceContactScale(binding.driver,contactDriver):1.f;
 const float charge=exempt?0:impactContacts[slot].Charge(other,otherHandle,now,units,ram.factor*contactScale);
 if(charge>0){ledger.racers[slot].Damage(combat_health::Hit::Collision,charge);
  if(worldLogEnabled&&charge>=.01f)Log("COMBAT_COLLISION slot=%u handle=%08X units=%.3f impulse=%.3f mass=%.1f other=%08X hp=%.2f ram_attacker=%u closing_mps=%.2f role_scale=%.3f",unsigned(slot),binding.handle,charge,impulse,mass,other,ledger.racers[slot].remaining,unsigned(ram.attacker),ram.closing,ram.factor);
 }
 if(breakable&&worldLogEnabled){static unsigned reported=0;if(reported++<16)Log("COMBAT_COLLISION_IGNORED slot=%u other=%08X reason=breakable_or_fragment native_visuals_preserved=1",unsigned(slot),other);}
 if(roadblock&&worldLogEnabled){static unsigned reported=0;if(reported++<16)Log("COMBAT_COLLISION_IGNORED slot=%u other=%08X reason=player_roadblock_contact native_physics_preserved=1",unsigned(slot),other);}
 Write(unsigned(slot));Retire(unsigned(slot));
}
void __fastcall Destroy(void* self,void*){
 const auto damage=unsigned(self);
 if(installed&&gameThread==GetCurrentThreadId()&&Active()){
  if(impactOwner==damage)return;
  if(depth&&effects[depth-1].used&&!effects[depth-1].nativePolice&&effects[depth-1].damage==damage&&Native(damage,effects[depth-1].handle))return;
  for(unsigned i=0;i<bindings.size();++i)if(bindings[i].damage==damage&&Participant(i,bindings[i].handle)&&Native(damage,bindings[i].handle)){
   Collision(i);if(!ledger.racers[i].retired)return;
   // The native Destroy routine dispatches its one-shot event only below 1.
   Retire(i);return;
  }
 }
 unsigned handle=0,sim=0,table=0;float before=1;
 const bool police=installed&&gameThread==GetCurrentThreadId()&&Active()&&Read(damage,table)&&table==0x8AD40C&&
  Read(damage-0x18,sim)&&Read(sim+8,handle)&&Native(damage,handle)&&Read(damage+0x3C,before)&&before<1;
 const auto target=police&&policeTarget?policeTarget(handle):0;
 const auto attacker=attackSource;const bool cross=police&&attacker&&isCross&&isCross(handle);
 originalDestroy(self);
 float after=0;if(police&&Native(damage,handle)&&Read(damage+0x3C,after)&&after>=1){if(policeDestroyed)policeDestroyed(handle,target);if(cross&&crossKilledByRacer)crossKilledByRacer(handle,attacker);Notice(1,handle);}
}
bool EnsureImpact(){
 if(impactInstalled)return true;if(impactRefused)return false;
 const BYTE guard[]={0x6A,0xFF,0x68,0xF7,0xC3,0x87,0,0x64,0xA1};auto address=reinterpret_cast<void*>(0x69C030);
 if(memcmp(address,guard,sizeof(guard))||MH_CreateHook(address,Impact,reinterpret_cast<void**>(&originalImpact))!=MH_OK||MH_EnableHook(address)!=MH_OK){impactRefused=true;return false;}
 const BYTE expected[]={0x8B,0x41,0x30,0xC3};BYTE actual[4]{};
 nativeTypeGetterVerified=Copy(reinterpret_cast<void*>(0x686A30),actual,sizeof(actual))&&!memcmp(actual,expected,sizeof(expected));
 impactInstalled=true;Log("EVENT_NATIVE_CONTACT_ACTIVE shared_owner=1 drift_without_weapons=1");return true;
}
void Frame(){
 if(!Active()){if(ledger.event)Restore();return;}
 gameThread=GetCurrentThreadId();
 if(!installed&&!refused){
  auto arms=GetModuleHandleW(L"MWArmsAssist.asi");using Ready=unsigned(__cdecl*)();
  auto ready=arms?reinterpret_cast<Ready>(GetProcAddress(arms,"MWAA_CombatReadyV1")):nullptr;
  if(!ready||!ready())return; // ArmsAssist checks unmodified native bytes first.
  const BYTE expected[]={0x6A,0xFF,0x68,0x1F,0xC4,0x87,0,0x64,0xA1};
  auto address=reinterpret_cast<void*>(0x69C540);
  if(memcmp(address,expected,sizeof(expected))||MH_CreateHook(address,Destroy,reinterpret_cast<void**>(&originalDestroy))!=MH_OK||MH_EnableHook(address)!=MH_OK){
   refused=true;Log("COMBAT_HEALTH_REFUSED native_destroy_owner=1");return;
  }
  if(!EnsureImpact()){
   MH_DisableHook(address);MH_RemoveHook(address);refused=true;Log("COMBAT_HEALTH_REFUSED native_impact_owner=1");return;
  }
  installed=true;Log("COMBAT_HEALTH_ACTIVE racers_maximum=300 police_reference=100 transient=1 breakable_filter_verified=%u",unsigned(nativeTypeGetterVerified));
 }
 if(!installed)return;
 const auto key=MWELNeoCurrentKey();if(ledger.Observe(key,generation,true)){bindings={};impactContacts={};motions={};}
 float motionTime=0;Read(0x9885D8,motionTime);
 for(unsigned i=0;i<bindings.size();++i)if(bindings[i].damage&&Participant(i,bindings[i].handle)){
  Collision(i);Retire(i);if(motions[i].valid&&motionTime>=motions[i].sampledAt&&motionTime-motions[i].sampledAt<.025f)continue;
  unsigned sim=0;if(Native(bindings[i].damage,bindings[i].handle)&&Read(bindings[i].damage-0x18,sim)){combat_impact::Motion current;if(motionReader(sim,current))motions[i]=current;else motions[i]={};}
 }
}
unsigned Query(unsigned slot,unsigned handle,mwcombat::HealthV1* out,unsigned size){
 if(!out||size!=sizeof(*out)||!installed||!Active()||slot>=16||!ledger.event||!Participant(slot,handle))return 0;
 const auto& c=ledger.racers[slot];if(!c.occupied||c.handle!=handle)return 0;
 mwcombat::HealthV1 value;value.eventKey=ledger.event;value.epoch=ledger.epoch;value.handle=handle;value.slot=slot;
 value.remaining=c.remaining;value.maximum=c.maximum;value.flags=(c.retired?1u:0u)|(c.invincible?2u:0u);value.driver=bindings[slot].driver;*out=value;return 1;
}
unsigned Observe(unsigned slot,unsigned handle,unsigned damage,unsigned driver,mwcombat::HealthV1* out,unsigned size){
 unsigned table=0;
 auto current=event_race_policies::Current();
 const bool gauntletCop=slot==1&&driver==2&&current&&current->authoring.value("combatMode",std::string("none"))=="gauntlet";
 if(!out||size!=sizeof(*out)||!installed||gameThread!=GetCurrentThreadId()||!Active()||!Participant(slot,handle)||
    (!gauntletCop&&driver!=0&&driver!=3)||!Native(damage,handle,&table)||(!gauntletCop&&table==0x8AD40C))return 0;
 const auto key=MWELNeoCurrentKey();if(ledger.Observe(key,generation,true)){bindings={};impactContacts={};motions={};}
 auto p=event_race_policies::Current();const bool immortal=driver!=0&&p->authoring.value("combatMode",std::string("none"))=="gauntlet";
 auto car=ledger.Join(slot,handle,false,immortal);if(!car)return 0;
 auto& b=bindings[slot];
 if(b.damage!=damage||b.handle!=handle){float raw=0;if(!Read(damage+0x3C,raw)||!std::isfinite(raw))return 0;b={damage,handle,driver,raw,raw};impactContacts[slot]={};motions[slot]={};
  Log("COMBAT_HEALTH_JOIN slot=%u handle=%08X driver=%u maximum=%.0f remaining=%.1f",slot,handle,driver,car->maximum,car->remaining);
 }
 b.driver=driver;Collision(slot);return Query(slot,handle,out,size);
}
unsigned Effect(unsigned damage,unsigned handle,unsigned kind,unsigned phase,bool policeSource=false){
 if(!installed||gameThread!=GetCurrentThreadId()||!Active()||kind<1||kind>3||phase>2||!Native(damage,handle))return 0;
 if(phase==0){
  if(depth>=effects.size())return 0;unsigned table=0;Native(damage,handle,&table);
  // Hot Pursuit's ordinary cops retain Arsenal/native damage and destruction.
  // The fixed combat HP model applies to racers, not to these police cars.
  auto policy=event_race_policies::Current();
  const bool nativePolice=table==0x8AD40C&&policy&&policy->authoring.value("combatMode",std::string("none"))=="hotpursuit";
  bool bound=table==0x8AD40C;
  for(unsigned i=0;i<bindings.size()&&!bound;++i)bound=bindings[i].damage==damage&&bindings[i].handle==handle&&Participant(i,handle);
  if(!bound)return 0;float raw=0;if(!Read(damage+0x3C,raw)||!std::isfinite(raw))return 0;
  effects[depth++]={damage,handle,kind,raw,true,nativePolice,policeSource};return 1;
 }
 if(!depth)return 0;const auto entry=effects[depth-1];
 if(entry.damage!=damage||entry.handle!=handle||entry.kind!=kind)return 0;
 effects[--depth]={};
 if(entry.nativePolice)return 1; // Successful or failed native effects own their exact damage/result.
 if(phase!=1){*reinterpret_cast<float*>(damage+0x3C)=entry.original;return 1;}
 for(unsigned i=0;i<bindings.size();++i)if(bindings[i].damage==damage&&bindings[i].handle==handle&&Participant(i,handle)){
  const auto policy=event_race_policies::Current();
  const bool reduced=(entry.policeSource||policeSource)&&policy&&policy->authoring.value("combatMode",std::string("none"))=="hotpursuit";
  ledger.racers[i].Damage(static_cast<combat_health::Hit>(kind),0,reduced?.75f:1.f);Write(i);
  Log("COMBAT_HEALTH_HIT slot=%u handle=%08X kind=%u remaining=%.1f maximum=%.0f",i,handle,kind,ledger.racers[i].remaining,ledger.racers[i].maximum);
  Retire(i);
  return 1;
 }
 unsigned table=0;if(!Native(damage,handle,&table)||table!=0x8AD40C){*reinterpret_cast<float*>(damage+0x3C)=entry.original;return 0;}
 const float raw=std::clamp(entry.original+combat_health::PoliceFraction(static_cast<combat_health::Hit>(kind)),0.f,1.f);
 if(raw>=1){Notice(1,handle);*reinterpret_cast<float*>(damage+0x3C)=std::min(entry.original,.999999f);originalDestroy(reinterpret_cast<void*>(damage));}
 else *reinterpret_cast<float*>(damage+0x3C)=raw;
 return 1;
}
}
