#pragma once
namespace hot_pursuit_runtime {
bool LiveRacer(unsigned handle){
 unsigned race=0,count=0,flags=0;BYTE dnf=0;const auto slot=Slot(handle,race,count);
 return handle&&slot<count&&Read(race+0x18+slot*0x194+0x1C,flags)&&!flags&&Read(race+0x18+slot*0x194+0x190,dnf)&&!dnf;
}
bool Cross(unsigned handle){
 const auto car=Vehicle(handle);unsigned driver=0;
 if(!car||!Table(car,0x8AA828)||!Read(car+0x94,driver)||driver!=2)return false;
 const BYTE code[]={0x8B,0x41,0x2C,0x8B,0x40,0x24,0xC3};
 if(memcmp(reinterpret_cast<void*>(0x688090),code,sizeof(code)))return false;
 const auto text=reinterpret_cast<const char*(__thiscall*)(unsigned)>(0x688090)(car);char name[64]{};
 // Actual Main attributes contain a dedicated pvehicle/copcross collection,
 // with aivehicle/copcross and damagespecs/copcross. Ordinary Corvette and
 // unmarked/henchman collections must never trigger the reward.
 return Copy(text,name,sizeof(name))&&memchr(name,0,sizeof(name))&&_stricmp(name,"copcross")==0;
}
void Regenerate(unsigned cop,unsigned attacker){
 if(!Active()||!combat_health_runtime::Racing()||!LiveRacer(attacker))return;
 unsigned race=0,player=0;Read(0x91E000,race);Read(race+0x18,player);
 const auto car=Vehicle(player);if(!car||Heat(car)<5)return;
 unsigned healed=0;float total=0;
 for(unsigned slot=0;slot<8;++slot){auto& hp=combat_health_runtime::ledger.racers[slot];auto& binding=combat_health_runtime::bindings[slot];
  if(!LiveRacer(hp.handle)||!combat_health_runtime::Participant(slot,hp.handle)||!combat_health_runtime::Native(binding.damage,hp.handle))continue;
  const float amount=hp.Heal(.15f);if(amount>0){combat_health_runtime::Write(slot);++healed;total+=amount;}
 }
 if(healed)combat_health_runtime::Notice(3,cop,attacker,15);
 Log("HOT_PURSUIT_CROSS_REGEN cop=%u attacker=%u healed_racers=%u restored_units=%.1f fraction=.15 retired_revived=0",cop,attacker,healed,total);
}
struct RegenBinding {RegenBinding(){combat_health_runtime::isCross=Cross;combat_health_runtime::crossKilledByRacer=Regenerate;}};
inline RegenBinding regenBinding;
}
