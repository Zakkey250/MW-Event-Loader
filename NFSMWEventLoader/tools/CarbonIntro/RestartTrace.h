// Private, read-only restart phase tracing. Never built into handoff ASIs.
#ifdef MWEL_PRIVATE_RESTART_TRACE
const char* restartPhase="none";
void RestartSnapshot(const char* phase){
 if(!logging)return;
 unsigned race=0,manager=0,pending=0,wait=0,active=0;float delay=0;
 __try{
  race=*reinterpret_cast<unsigned*>(0x91E000);manager=*reinterpret_cast<unsigned*>(0x92C67C);
  if(race){wait=*reinterpret_cast<BYTE*>(race+0x19A4);active=*reinterpret_cast<BYTE*>(race+0x198A);}
  if(manager&&*reinterpret_cast<unsigned*>(manager)==0x891900){pending=*reinterpret_cast<unsigned*>(manager+0x1C);delay=*reinterpret_cast<float*>(manager+0x58);}
 }__except(EXCEPTION_EXECUTE_HANDLER){return;}
 char text[320];sprintf_s(text,"PRIVATE_RESTART_PHASE phase=%s inside=%s race=%08X active=%u script_wait=%u cop_loading=%u cop_delay=%.3f native_state_unchanged=1",phase,restartPhase,race,active,wait,pending,delay);Log(text);
}
void(__cdecl* restartResetOriginal)()=nullptr;
void(__cdecl* restartRestoreOriginal)()=nullptr;
void(__cdecl* restartWarpOriginal)(unsigned)=nullptr;
void(__thiscall* restartNotifyOriginal)(unsigned)=nullptr;
void __cdecl RestartReset(){restartPhase="reset_cops";RestartSnapshot("begin");restartResetOriginal();RestartSnapshot("end");restartPhase="none";}
void __cdecl RestartRestore(){restartPhase="restore_grid";RestartSnapshot("begin");restartRestoreOriginal();RestartSnapshot("end");restartPhase="none";}
void __cdecl RestartWarp(unsigned trigger){restartPhase="warp_trigger";RestartSnapshot("begin");restartWarpOriginal(trigger);RestartSnapshot("end");restartPhase="none";}
void __fastcall RestartNotify(unsigned self,void*){restartPhase="notify_loaded";RestartSnapshot("begin");restartNotifyOriginal(self);RestartSnapshot("end");restartPhase="none";}
#endif
