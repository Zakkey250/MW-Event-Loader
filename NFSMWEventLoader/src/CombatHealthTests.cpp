#include "CombatHealthPolicy.h"
#include "CombatImpactPolicy.h"
#include "PoliceDeploymentPolicy.h"
#include "HotPursuitPolicy.h"
#include "EventPresentationPolicy.h"
#include "../../integration/MWCombatContract.h"
#include <cstdio>
#include <cstdlib>
void Check(bool ok,const char* what){if(!ok){std::printf("FAIL %s\n",what);std::exit(1);}}
int main(){
 for(unsigned count=1;count<=16;++count){
  Check(mwcombat::Compatible(mwcombat::Mode::Gauntlet,"tollboothrace",count)==(count<=2),"Gauntlet accepts one racer and legacy two-slot packages only");
  Check(!mwcombat::Compatible(mwcombat::Mode::Gauntlet,"sprint",count),"Gauntlet never accepts a sprint base");
 }
 for(auto m:{mwcombat::Mode::ArmsRace,mwcombat::Mode::HotPursuit})for(auto base:{"sprint","circuit","lapknockout","speedtraprace"})
  Check(mwcombat::Compatible(m,base,8)==(m==mwcombat::Mode::ArmsRace||!std::strcmp(base,"sprint")||!std::strcmp(base,"speedtraprace")),"Arms four rules; Hot Pursuit linear rules only");
 for(unsigned limit:{4u,8u,16u})for(unsigned active=0;active<=18;++active)for(unsigned pending=0;pending<=18;++pending)
  Check(police_deployment::Room(limit,active,pending)==(active+pending<limit),"native queued requests reserve deployment capacity without culling");
 Check(police_deployment::Near(0,0,0,100,0,0,180)&&!police_deployment::Near(0,0,0,100,15,0,180),"roadblock separation respects layered roads");
 {combat_health::Car hp;hp.Begin(20,false);hp.Damage(combat_health::Hit::Collision,200);
  Check(hp.Heal(.15f)==45&&hp.remaining==145,"Cross regeneration restores 15 percent of max racer HP");
  hp.remaining=290;Check(hp.Heal(.15f)==10&&hp.remaining==300,"regeneration clamps at maximum");
  hp.Damage(combat_health::Hit::Collision,300);Check(hp.Heal(.15f)==0&&hp.retired&&hp.remaining==0,"regeneration never revives retired racers");}
 for(unsigned n=1;n<=16;++n)Check(mwcombat::Compatible(mwcombat::Mode::HotPursuit,"sprint",n)==(n<=8),"Hot Pursuit accepts at most eight racers");
 Check(mwcombat::Compatible(mwcombat::Mode::ArmsRace,"sprint",16),"Arms Race retains sixteen racers");
 Check(hot_pursuit::Reward(50000,8,0,1)==400000&&hot_pursuit::Reward(50000,8,0,8)==50000,"survivor reward pool is divided by actual finishing rank");
 Check(hot_pursuit::Reward(50000,8,7,1)==50000&&hot_pursuit::Reward(50000,8,3,2)==125000,"retirements remove one base reward each");
 Check(hot_pursuit::Reward(50000,8,8,1)==0&&hot_pursuit::Reward(50000,9,0,1)==0&&hot_pursuit::Reward(50000,8,0,0)==0,"invalid outcome cannot award cash");
 Check(hot_pursuit::Reward(~0u,8,0,1)==2147483520u,"native signed cash conversion cannot overflow");
 Check(hot_pursuit::RollingSpeed(0,100)==8&&hot_pursuit::RollingSpeed(0,10)==0&&hot_pursuit::RollingSpeed(70,100)==30&&hot_pursuit::RollingSpeed(NAN,100)==0&&hot_pursuit::RollingSpeed(0,NAN)==0,"distant reinforcement rolls safely while nearby stopped contact retains native speed");
 Check(hot_pursuit::RollingSpeed(70)==30&&hot_pursuit::RollingSpeed(12)==12&&hot_pursuit::RollingSpeed(0)==0&&hot_pursuit::RollingSpeed(NAN)==0,"reinforcements inherit bounded observed speed, never an invalid launch impulse");
 Check(hot_pursuit::PoliceContactScale(true,0,2)==.5f&&hot_pursuit::PoliceContactScale(true,3,2)==.5f,"Hot Pursuit police contacts reduce player and AI racer damage equally");
 Check(hot_pursuit::PoliceContactScale(false,0,2)==1&&hot_pursuit::PoliceContactScale(true,0,3)==1&&hot_pursuit::PoliceContactScale(true,2,3)==1&&hot_pursuit::PoliceContactScale(true,0,~0u)==1,"other modes, racer contacts, police victims and unknown scenery retain collision damage");
 for(unsigned rank=1;rank<=4;++rank)Check(hot_pursuit::Quota(rank,4,24,8)==4,"eight-racer police budget redistributes to four native survivors");
 Check(hot_pursuit::Quota(1,1,24,8)==16&&hot_pursuit::Quota(1,1,8,8)==8,"last survivor inherits the existing budget while respecting native capacity");
 for(const auto hit:{combat_health::Hit::Emp,combat_health::Hit::Spike,combat_health::Hit::Shockwave}){
  combat_health::Car car;car.Begin(123,false);car.Damage(hit,0,.75f);
  Check(std::abs(car.remaining-(300-combat_health::WeaponUnits(hit)*.75f))<.0001f,"police weapon scope reduces damage exactly 25 percent");
 }
 {combat_health::Car car;car.Begin(123,false);car.Damage(combat_health::Hit::Collision,20,.75f);Check(car.remaining==280,"weapon reduction never changes contact damage");}
 Check(event_presentation_policy::Supported("hotpursuit")&&!event_presentation_policy::Supported("none")&&event_presentation_policy::Supported("drift"),"presentation recognizes owned event modes without changing ordinary races");
 for(const auto rules:{"sprint","circuit","lapknockout","speedtraprace"}){
  Check(event_presentation_policy::Name("hotpursuit",rules,false).find(L"Hot Pursuit - ")==0,"all Hot Pursuit native rule variants retain a distinct English mode name");
  Check(event_presentation_policy::Name("hotpursuit",rules,true).find(L"\u30db\u30c3\u30c8\u30d1\u30fc\u30b9\u30fc\u30c8 - ")==0,"all Hot Pursuit native rule variants retain a distinct Japanese mode name");
 }
 Check(event_presentation_policy::Name("hotpursuit","sprint",false)==L"Hot Pursuit - Sprint"&&event_presentation_policy::Name("hotpursuit","circuit",false)==L"Hot Pursuit - Street","sprint and circuit rules display the correct Hot Pursuit suffix");
 Check(event_presentation_policy::Name("armsrace","circuit",false)==L"Arms Race - Street"&&event_presentation_policy::Name("gauntlet","tollboothrace",false)==L"Gauntlet","existing Arms Race and Gauntlet labels remain unchanged");
 Check(event_presentation_policy::Name("none","sprint",false).empty()&&event_presentation_policy::Name("hotpursuit","drag",false).empty(),"ordinary and unsupported event rules retain native presentation");
 Check(!hot_pursuit::StartEligible(0)&&!hot_pursuit::StartEligible(25)&&hot_pursuit::StartEligible(25.001f),"pursuit starts strictly after individual race progress passes 25 percent");
 Check(!hot_pursuit::StartEligible(NAN)&&!hot_pursuit::StartEligible(INFINITY)&&!hot_pursuit::StartEligible(101)&&hot_pursuit::StartEligible(100),"invalid and out-of-range progress cannot start a pursuit");
 hot_pursuit::ReplacementWindow replacement;
 Check(replacement.Destroyed(100,1,1000)&&replacement.Held(1,3999)==1&&replacement.Held(2,3999)==0,"destroyed police reserves only its target's replacement slot for three seconds");
 Check(!replacement.Destroyed(100,1,2000)&&replacement.Held(1,4000)==0,"repeat wreck observation cannot extend the replacement cooldown");
 Check(!replacement.RefillReady(3999)&&replacement.RefillReady(4000),"all new native replenishment requests pause for three seconds after a destruction");
 Check(replacement.Destroyed(101,1,2000)&&replacement.Held(1,4999)==1&&replacement.Held(1,5000)==0,"each destruction has an independent bounded replacement delay");
 replacement={};Check(replacement.Held(1,0)==0&&replacement.RefillReady(0),"event reset clears replacement reservations");
 for(unsigned racers=2;racers<=16;++racers){unsigned assigned=0;for(unsigned rank=1;rank<=racers;++rank)assigned+=hot_pursuit::Quota(rank,racers);
  Check(assigned==mwcombat::PoliceBudget(racers),"hot pursuit quotas share the global police budget without over-allocation");
  Check(hot_pursuit::Quota(1,racers)>=hot_pursuit::Quota(racers,racers),"leaders receive priority without excluding trailing racers");}
 Check(mwcombat::PoliceBudget(8)==16,"eight racers receive sixteen ordinary ground chasers");
 for(unsigned capacity=0;capacity<=32;++capacity){unsigned assigned=0;
  for(unsigned rank=1;rank<=8;++rank){const auto quota=hot_pursuit::Quota(rank,8,capacity);assigned+=quota;
   if(capacity>=8)Check(quota>=1,"every racer including the player has a reserved native controller before leader bonuses");
   if(capacity==8)Check(quota==1,"an eight-controller pool never gives leaders a second car while excluding the player");}
  Check(assigned==std::min(16u,capacity),"effective quota cannot overbook the verified native controller pool");}
 for(unsigned rank=1;rank<=8;++rank)Check(hot_pursuit::Quota(rank,8,24)==2,"each of eight racers has two chasers when native capacity permits");
 Check(hot_pursuit::Outcome(true,8,1,1)==hot_pursuit::Result::Lost,"king arrest wins over another racer's escape");
 Check(hot_pursuit::Outcome(false,8,1,7)==hot_pursuit::Result::Won,"one genuine escape wins for the racer team");
 Check(hot_pursuit::Outcome(false,8,0,8)==hot_pursuit::Result::Lost&&hot_pursuit::Outcome(false,8,0,7)==hot_pursuit::Result::Running,"only actual eliminated participants count toward team loss");
 Check(hot_pursuit::Retain(true,true,true,899,0),"police stay with their live racer even outside player range");
 Check(!hot_pursuit::Retain(false,true,true,10,0)&&!hot_pursuit::Retain(true,false,true,10,0)&&!hot_pursuit::Retain(true,true,false,10,0),"ended target inactive vehicle and race exit restore native recycling");
 Check(!hot_pursuit::Retain(true,true,true,901,0)&&!hot_pursuit::Retain(true,true,true,20,41)&&!hot_pursuit::Retain(true,true,true,NAN,0),"lost target far deck and invalid motion do not pin police pools");
 using namespace combat_health;
 Check(combat_impact::Units(3000,1000,0)==0&&combat_impact::Units(8000,1000,1)==0,"ordinary scrape and normal road landing thresholds");
 Check(std::abs(combat_impact::Units(28000,1000,0)-12.5f)<.0001f,"contact damage is half the previous 25 police-reference HP");
 Check(combat_impact::Units(40000,1000,0)>combat_impact::Units(20000,1000,0)&&combat_impact::Units(40000,2000,0)<combat_impact::Units(40000,1000,0),"impact severity rises with impulse and respects body mass");
 Check(combat_impact::Units(10000000,1000,0)==37.5f&&combat_impact::Units(NAN,1000,0)==0&&combat_impact::Units(1000,0,0)==0,"halved contact cap and invalid physics inputs");
 for(float impulse:{4000.f,8000.f,16000.f,28000.f,40000.f,10000000.f}){
  const float previous=std::min(75.f,(impulse/1000.f-3.f)*(impulse/1000.f-3.f)*.04f);
  Check(std::abs(combat_impact::Units(impulse,1000,0)-previous*.5f)<.00001f,"contact curve including saturation is exactly halved");
 }
 combat_impact::Contacts contacts;Check(contacts.Charge(1,10,1,10)==10&&contacts.Charge(1,10,1.1f,10)==0&&contacts.Charge(1,10,1.2f,20)==10,"continuous contact charges only increased peak, not every physics tick");
 Check(contacts.Charge(1,10,2,20)==20&&contacts.Charge(1,11,2.1f,20)==20&&contacts.Charge(2,11,2.2f,20)==20,"new impact, pooled body handle and other body are independent");
 combat_impact::Motion attacker{{0,0,0},{0,0,40},{0,0,1},1,0,true},victim{{0,0,4},{0,0,20},{0,0,1},2,0,true};
 auto attack=combat_impact::RamContact(attacker,victim,{0,0,2}),receive=combat_impact::RamContact(victim,attacker,{0,0,2});
 Check(attack.attacker&&attack.factor==.1f&&!receive.attacker&&receive.closing==20&&receive.factor>.9f,"rear-end initiator takes only ten percent while receiving car scales with relative speed");
 attacker.velocity.z=70;auto fast=combat_impact::RamContact(victim,attacker,{0,0,2});Check(fast.factor>receive.factor&&fast.factor<=1.25f,"receiving damage grows with closing speed and stays bounded");
 victim.forward.z=-1;victim.velocity.z=-40;attacker.velocity.z=40;Check(!combat_impact::RamContact(attacker,victim,{0,0,2}).attacker,"head-on collision does not grant attacker protection");
 attacker.forward={1,0,0};attacker.velocity={40,0,0};victim.position={4,0,0};victim.forward={0,0,1};victim.velocity={0,0,20};
 Check(combat_impact::RamContact(attacker,victim,{2,0,0}).attacker&&!combat_impact::RamContact(victim,attacker,{2,0,0}).attacker,"T-bone roles follow impact direction rather than vehicle speed alone");
 victim.valid=false;Check(combat_impact::RamContact(attacker,victim,{2,0,0}).factor==1,"unknown motion keeps ordinary collision damage");
 combat_impact::Contacts ramContacts;Check(ramContacts.Charge(9,4,1,20,.1f)==2&&ramContacts.Charge(9,4,1.1f,20,1.25f)==0&&ramContacts.Charge(9,4,2,20,1.25f)==25,"ram attribution cannot flip mid-contact and resets for a separate hit");
 Check(combat_impact::VisualZone(181,300)==0&&combat_impact::VisualZone(180,300)==1&&combat_impact::VisualZone(90,300)==2&&combat_impact::VisualZone(0,300)==2&&combat_impact::VisualZone(NAN,300)==0,"visual thresholds match yellow sixty percent and red thirty percent HUD zones");

 Car cop,racer;cop.Begin(1,true);racer.Begin(2,false);
 Check(cop.maximum==100&&racer.maximum==300,"Corvette reference and racer triple capacity");
 cop.Damage(Hit::Collision,30);racer.Damage(Hit::Collision,30);
 Check(cop.remaining==70&&racer.remaining==270,"same physical damage costs racer one third of gauge");
 for(auto hit:{Hit::Emp,Hit::Spike,Hit::Shockwave}){
  cop.Begin(1,true);racer.Begin(2,false);cop.Damage(hit);racer.Damage(hit);
  Check(std::abs((100-cop.remaining)-(300-racer.remaining))<.00001f&&std::abs((100-cop.remaining)-WeaponUnits(hit))<.00001f,"same weapon removes the same police-reference HP from either class");
 }
 for(auto hit:{Hit::Emp,Hit::Spike,Hit::Shockwave}){
  racer.Begin(2,false);const unsigned hits=hit==Hit::Emp?12u:hit==Hit::Spike?6u:20u;
  for(unsigned i=0;i<hits;++i){Check(!racer.retired&&racer.Damage(hit),"each police-reference hit accepted until triple capacity is exhausted");Check(racer.retired==(i+1==hits),"racer retires only at twelve EMP six spike or twenty shock hits");}
 }
 Check(racer.retired&&racer.remaining==0&&!racer.Damage(Hit::Spike),"retirement happens once");
 cop.Begin(1,true,true);Check(!cop.Damage(Hit::Spike)&&!cop.Damage(Hit::Collision,10000)&&cop.remaining==100,"Gauntlet opponent invincibility");
 racer.Begin(2,false);Check(!racer.Damage(Hit::Collision,NAN)&&!racer.Damage(Hit::Collision,-10)&&racer.remaining==300,"invalid damage rejected");
 Ledger ledger;Check(!ledger.Join(0,1),"no health outside explicit event");
 ledger.Observe(7,3,true);auto c=ledger.Join(1,11);c->Damage(Hit::Spike);
 Check(ledger.Join(1,12)->remaining==250,"respawn preserves slot health");
 Check(!ledger.Observe(7,3,true)&&ledger.Join(1,12)->remaining==250,"same event frame cannot refill");
 Check(ledger.Observe(7,4,true)&&ledger.Join(1,12)->remaining==300,"restart creates new health generation");
 Check(!ledger.Join(16,13)&&!ledger.Join(1,0),"invalid participant rejected");
 Check(ledger.Observe(0,0,false)&&!ledger.event&&!ledger.Join(1,12),"exit clears all transient health");
 std::puts("PASS combat health capacity, damage, retirement, respawn, restart, exit and invalid inputs");
}
