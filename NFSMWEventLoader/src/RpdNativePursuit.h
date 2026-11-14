#pragma once
// Retarget the stock PursueAtHeatLevel entry itself. Only its four local
// IPlayer::GetSimable calls are redirected, only during our scoped request.
// IPlayer, its owned car and global player lists are never replaced.
namespace rpd_native_pursuit {
unsigned targetSim=0,startedRace=0,startedEpoch=0; ULONGLONG nextAttempt=0;
void* trampolines[4]{};
unsigned __cdecl Sim(unsigned player){
 if(targetSim)return targetSim;
 unsigned table=0,fn=0;if(!gauntlet_runtime::Read(player,table)||!gauntlet_runtime::Read(table+4,fn))return 0;
 return reinterpret_cast<unsigned(__thiscall*)(unsigned)>(fn)(player);
}
#define RPD_SIM_SITE(Name,Resume) void __declspec(naked) Name(){__asm{push esi} __asm{call Sim} __asm{add esp,4} __asm{push Resume} __asm{ret}}
RPD_SIM_SITE(Query0,0x42EC38)
RPD_SIM_SITE(Query1,0x42EC64)
RPD_SIM_SITE(Query2,0x42ED36)
RPD_SIM_SITE(Query3,0x42ED69)
#undef RPD_SIM_SITE
bool Install(){
 if(trampolines[0])return true;
 unsigned sites[]={0x42EC33,0x42EC5F,0x42ED31,0x42ED64};
 void* hooks[]={Query0,Query1,Query2,Query3};
 const BYTE bytes[][5]={{0x8B,0xCE,0xFF,0x50,4},{0x8B,0xCE,0xFF,0x52,4},{0x8B,0xCE,0xFF,0x52,4},{0x8B,0xCE,0xFF,0x50,4}};
 for(unsigned i=0;i<4;++i)if(memcmp(reinterpret_cast<void*>(sites[i]),bytes[i],5))return false;
 unsigned made=0;bool ok=true;
 for(unsigned i=0;i<4;++i){if(MH_CreateHook(reinterpret_cast<void*>(sites[i]),hooks[i],&trampolines[i])!=MH_OK){ok=false;break;}++made;}
 if(ok)for(unsigned i=0;i<4;++i)if(MH_EnableHook(reinterpret_cast<void*>(sites[i]))!=MH_OK){ok=false;break;}
 if(!ok)for(unsigned i=0;i<made;++i){MH_DisableHook(reinterpret_cast<void*>(sites[i]));MH_RemoveHook(reinterpret_cast<void*>(sites[i]));trampolines[i]=nullptr;}
 return ok;
}
void Start(unsigned race,unsigned manager,unsigned handle,int heat){
 if(MWELRpdPursuitCompleted(handle))return;
 const auto existing=hot_pursuit_runtime::FindPursuit(handle);BYTE bailed=0,busted=0;
 if(existing&&((gauntlet_runtime::Read(existing+0xE9,bailed)&&bailed)||
               (gauntlet_runtime::Read(existing+0xE8,busted)&&busted)))return;
 // The human officer is not a disposable CopMgr dispatch slot. Losing the
 // last AI ally must not replay PursueAtHeatLevel and all setup radio calls.
 if(startedRace==race&&startedEpoch==combat_health_runtime::generation)return;
 if(GetTickCount64()<nextAttempt)return;nextAttempt=GetTickCount64()+3000;
 const auto car=gauntlet_runtime::Vehicle(handle);if(!car||!trampolines[0])return;
 targetSim=car-0x80;
 __try{reinterpret_cast<void(__thiscall*)(unsigned,int)>(0x42EBF0)(manager,heat);}
 __finally{targetSim=0;}
 const auto group=hot_pursuit_runtime::FindPursuit(handle);
 if(group){startedRace=race;startedEpoch=combat_health_runtime::generation;
  // The native entry, rather than EnsurePursuit, created this event's group.
  // Give it the same teardown ownership: otherwise ReleaseGroups misses its
  // GroundSupportRequest, whose cop references outlive native pool teardown.
  if(!existing&&hot_pursuit_runtime::owner==MWELNeoCurrentKey()){
   hot_pursuit_runtime::createdPursuits[1]=group;
   hot_pursuit_runtime::createdTargets[1]=handle;
   Log("RPD_NATIVE_PURSUIT_LIFETIME_OWNED suspect=%u group=%08X native_factory=1 support_cleanup_before_pool=1",handle,group);
  }
  // This is the actual native factory + target attachment + initial cop
  // placement path, not a manually created background activity.
  unsigned active=0,capacity=0;gauntlet_runtime::Read(manager-0x54+0x94,active);gauntlet_runtime::Read(manager-0x54+0x98,capacity);
  const bool normal=reinterpret_cast<bool(__thiscall*)(unsigned,bool)>(0x410E20)(manager,false);
  const bool chase=reinterpret_cast<bool(__thiscall*)(unsigned,bool)>(0x410E20)(manager,true);
  Log("RPD_NATIVE_PURSUIT_START suspect=%u group=%08X native_entry=0042EBF0 player_replaced=0 active=%u capacity=%u can_spawn_normal=%u can_spawn_pursuit=%u",handle,group,active,capacity,unsigned(normal),unsigned(chase));
 }
}
}
