#pragma once
// Read-only, opt-in diagnosis. Preserve the native assertion dialog and its
// Abort/Retry/Ignore behaviour; never turn a failed heap check into success.
// RoadPlace also hosts the RPD placement guard. Its functional installation
// is independent of logging; all diagnostic work remains behind the ON gate.
namespace native_runtime_diagnostics {
// Diagnosis must not provoke an access violation just to discover an
// uninitialized HUD slot. Validate every page before the existing guarded copy.
// This helper is used only after the diagnostic ON gate; gameplay reads keep
// their original hot path and native assertions retain their normal behavior.
bool Snapshot(const void* from,void* to,size_t count){
 if(!from||!to||count>4096)return false;
 auto at=reinterpret_cast<uintptr_t>(from);const auto end=at+count;
 if(end<at)return false;
 while(at<end){MEMORY_BASIC_INFORMATION region{};
  if(!VirtualQuery(reinterpret_cast<void*>(at),&region,sizeof(region))||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
  const DWORD access=region.Protect&0xff;
  if(access!=PAGE_READONLY&&access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)return false;
  const uintptr_t next=reinterpret_cast<uintptr_t>(region.BaseAddress)+static_cast<uintptr_t>(region.RegionSize);
  if(next<=at)return false;at=std::min(end,next);
 }
 return Copy(from,to,count);
}
void* reportTrampoline=nullptr;
void Controls(const char* boundary){
 if(!worldLogEnabled)return;
 // UIGetControlText (00628230) reads 52-byte action records here: action ID
 // followed by keyboard primary/secondary and pad primary/secondary triples.
 // Read the actual pad bindings too; PlayerSettings alone cannot prove them.
 unsigned mappings[19][13]{};
 if(Snapshot(reinterpret_cast<void*>(0x91F418),mappings,sizeof(mappings))){
  unsigned fingerprint=2166136261u;for(const auto& row:mappings)for(unsigned n=7;n<13;++n)fingerprint=(fingerprint^row[n])*16777619u;
  Log("NATIVE_PAD_BINDINGS boundary=%s fingerprint=%08X records=19 read_only=1",boundary,fingerprint);
  for(unsigned i=0;i<19;++i){const auto& row=mappings[i];Log("NATIVE_PAD_BINDING boundary=%s index=%u action=%08X primary=%08X,%08X,%08X secondary=%08X,%08X,%08X",boundary,i,row[0],row[7],row[8],row[9],row[10],row[11],row[12]);}
 }
 unsigned list=0,count=0;Snapshot(reinterpret_cast<void*>(0x92D87C),&list,4);Snapshot(reinterpret_cast<void*>(0x92D884),&count,4);
 if(!list||count>4)return;
 for(unsigned i=0;i<count;++i){unsigned player=0,table=0,methods[14]{};
  if(!Snapshot(reinterpret_cast<void*>(list+i*4),&player,4)||!Snapshot(reinterpret_cast<void*>(player),&table,4)||!Snapshot(reinterpret_cast<void*>(table),methods,sizeof(methods)))continue;
  if(methods[5]<0x401000||methods[5]>=0x889000||methods[7]<0x401000||methods[7]>=0x889000||methods[13]<0x401000||methods[13]>=0x889000)continue;
  const auto settings=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(methods[5])(player);
  const auto index=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(methods[7])(player);
  const auto port=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(methods[13])(player);
  unsigned words[5]{};if(!Snapshot(reinterpret_cast<void*>(settings),words,sizeof(words)))continue;
  Log("NATIVE_CONTROL_SETTINGS boundary=%s player=%08X settings=%08X index=%u port=%u words=%08X,%08X,%08X,%08X,%08X read_only=1",boundary,player,settings,index,port,words[0],words[1],words[2],words[3],words[4]);
 }
}
bool(__thiscall* originalVehiclePlace)(unsigned,const void*,const void*)=nullptr;
bool(__thiscall* originalRoadPlace)(unsigned,unsigned,unsigned,unsigned)=nullptr;
bool __fastcall RoadPlace(unsigned ai,void*,unsigned segment,unsigned distance,unsigned direction){
 if(!MWELRpdNativePlacementAllowed(reinterpret_cast<void*>(ai)))return true;
 if(worldLogEnabled){
  static unsigned reports=0;static std::set<unsigned> callers;const auto caller=unsigned(_ReturnAddress());
  unsigned table=0,vehicle=0,handle=0;
  if(Snapshot(reinterpret_cast<void*>(ai),&table,4)&&table==0x892480&&
     Snapshot(reinterpret_cast<void*>(ai-4),&vehicle,4)&&vehicle&&Snapshot(reinterpret_cast<void*>(vehicle-0x80+8),&handle,4)&&
     (reports<12||callers.insert(caller).second)){
   ++reports;Log("NATIVE_COP_ROAD_PLACEMENT handle=%u caller=%08X segment=%08X distance=%08X direction=%08X observer_only=1",handle,caller,segment,distance,direction);
  }
 }
 return originalRoadPlace(ai,segment,distance,direction);
}
bool InstallRoadPlacementGuard(){
 if(originalRoadPlace)return true;
 const BYTE guard[]={0x53,0x56,0x8B,0xF1,0x8B,0x06,0x57,0xFF,0x50,0x48};
 auto site=reinterpret_cast<void*>(0x4226E0);
 if(memcmp(site,guard,sizeof(guard))||MH_CreateHook(site,RoadPlace,reinterpret_cast<void**>(&originalRoadPlace))!=MH_OK)return false;
 if(MH_EnableHook(site)==MH_OK)return true;
 MH_RemoveHook(site);originalRoadPlace=nullptr;return false;
}
bool __fastcall VehiclePlace(unsigned self,void*,const void* position,const void* direction){
 if(!MWELRpdVehiclePlacementAllowed(reinterpret_cast<void*>(self)))return true;
 unsigned driver=~0u,table=0,handle=0;float requested[3]{},before[3]{},after[3]{};
 static unsigned reports=0;bool audit=worldLogEnabled&&reports<300&&Snapshot(reinterpret_cast<void*>(self),&table,4)&&table==0x8AA828&&
  Snapshot(reinterpret_cast<void*>(self+0x94),&driver,4)&&driver==2&&Snapshot(reinterpret_cast<void*>(self-0x80+8),&handle,4)&&Snapshot(position,requested,12);
 if(audit)Copy(&reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(self)->GetPosition(),before,12);
 const bool placed=originalVehiclePlace(self,position,direction);
 if(audit){++reports;Copy(&reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(self)->GetPosition(),after,12);
  const auto caller=unsigned(_ReturnAddress());
  Log("NATIVE_COP_PLACEMENT handle=%u caller=%08X result=%u requested=%.3f,%.3f,%.3f before=%.3f,%.3f,%.3f after=%.3f,%.3f,%.3f observer_only=1",handle,caller,unsigned(placed),requested[0],requested[1],requested[2],before[0],before[1],before[2],after[0],after[1],after[2]);
  static std::set<unsigned> seen;if(seen.insert(caller).second){void* frames[10]{};const auto count=CaptureStackBackTrace(0,10,frames,nullptr);for(unsigned i=0;i<count;++i)Log("NATIVE_COP_PLACEMENT_STACK caller=%08X index=%u address=%08X",caller,i,unsigned(frames[i]));}
 }
 return placed;
}
bool InstallVehiclePlacementGuard(){
 if(originalVehiclePlace)return true;
 const BYTE guard[]={0x6A,0xFF,0x64,0xA1,0,0,0,0,0x68,0x58,0xB7,0x87,0};
 auto site=reinterpret_cast<void*>(0x685810);
 if(memcmp(site,guard,sizeof(guard))||MH_CreateHook(site,VehiclePlace,reinterpret_cast<void**>(&originalVehiclePlace))!=MH_OK)return false;
 if(MH_EnableHook(site)==MH_OK)return true;
 MH_RemoveHook(site);originalVehiclePlace=nullptr;return false;
}
bool Heap(const char* boundary,bool quiet=false){
 if(!worldLogEnabled)return true;
 constexpr unsigned head=0x90DFF4;unsigned at=0,previous=head,count=0;bool valid=Snapshot(reinterpret_cast<void*>(head),&at,4);
 for(;valid&&at!=head&&count<8192;++count){
  unsigned words[4]{};valid=Snapshot(reinterpret_cast<void*>(at),words,sizeof(words));
  if(!valid||words[1]!=previous||words[3]!=0x44443333){Log("NATIVE_HEAP_LINK boundary=%s valid=0 node=%08X previous=%08X next=%08X back=%08X size=%u marker=%08X nodes=%u read_only=1",boundary,at,previous,words[0],words[1],words[2],words[3],count);return false;}
  previous=at;at=words[0];
 }
 if(!quiet)Log("NATIVE_HEAP_LINK boundary=%s valid=%u nodes=%u end=%08X read_only=1",boundary,unsigned(valid&&at==head),count,at);return valid&&at==head;
}
void(__thiscall* originalBonusTime)(void*,float)=nullptr;
void __fastcall BonusTime(void* self,void*,float seconds){
 float before=0,after=0;unsigned crossedBefore=0,crossedAfter=0;
 Snapshot(static_cast<BYTE*>(self)+0x1980,&before,4);Snapshot(static_cast<BYTE*>(self)+0x48,&crossedBefore,4);
 originalBonusTime(self,seconds);
 Snapshot(static_cast<BYTE*>(self)+0x1980,&after,4);Snapshot(static_cast<BYTE*>(self)+0x48,&crossedAfter,4);
 Log("NATIVE_TOLL_AWARD owner=%08X seconds=%.3f countdown_before=%.3f countdown_after=%.3f passages_before=%u passages_after=%u native_award=1",MWELNeoCurrentKey(),seconds,before,after,crossedBefore,crossedAfter);
}
void __cdecl Report(unsigned* saved){
 if(!worldLogEnabled)return;
 unsigned stack[15]{};if(!Snapshot(saved,stack,sizeof(stack)))return;
 unsigned pointer=0;Snapshot(reinterpret_cast<void*>(stack[2]+8),&pointer,4);
 Log("NATIVE_CRT_REPORT type=%u file=%08X line=%u expression=%08X caller=%08X ebp=%08X argument=%08X",stack[10],stack[11],stack[12],stack[14],stack[9],stack[2],pointer);
 if(stack[10]!=2)return;
 if(stack[11]==0x8B8274&&stack[12]==1132){
  unsigned words[24]{};
  if(pointer>=32&&Snapshot(reinterpret_cast<void*>(pointer-32),words,sizeof(words)))
   for(unsigned i=0;i<24;++i)Log("NATIVE_CRT_FREE_WORD offset=%d value=%08X",int(i*4)-32,words[i]);
 }
 void* frames[48]{};unsigned count=CaptureStackBackTrace(0,48,frames,nullptr);
 for(unsigned i=0;i<count;++i){
  HMODULE module=nullptr;wchar_t path[MAX_PATH]{};
  if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(frames[i]),&module))GetModuleFileNameW(module,path,MAX_PATH);
  Log("NATIVE_CRT_REPORT_STACK index=%u address=%08X module=%ls offset=%08X",i,unsigned(frames[i]),path,unsigned(frames[i])-unsigned(module));
 }
}
void __declspec(naked) ReportBridge(){
 __asm {pushfd
  pushad
  mov eax,esp
  push eax
  call Report
  add esp,4
  popad
  popfd
  jmp dword ptr [reportTrampoline]
 }
}
void Install(){
 if(!worldLogEnabled)return;
 if(InstallRoadPlacementGuard())Log("NATIVE_COP_ROAD_PLACEMENT_OBSERVER enabled=1 no_pose_writes=1 rpd_guard_independent_of_logging=1");
 if(InstallVehiclePlacementGuard())Log("NATIVE_COP_PLACEMENT_OBSERVER enabled=1 no_pose_writes=1 maximum_reports=300 rpd_guard_independent_of_logging=1");
 const BYTE bonusGuard[]={0x53,0x8B,0xD9,0x8B,0x83,0x58,0x19,0,0};
 auto bonus=reinterpret_cast<void*>(0x5FE600);
 if(!memcmp(bonus,bonusGuard,sizeof(bonusGuard))&&MH_CreateHook(bonus,BonusTime,reinterpret_cast<void**>(&originalBonusTime))==MH_OK){
  if(MH_EnableHook(bonus)==MH_OK)Log("NATIVE_TOLL_AWARD_OBSERVER enabled=1 native_countdown_retained=1");
  else{MH_RemoveHook(bonus);originalBonusTime=nullptr;}
 }
 const BYTE guard[]={0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xC0,0xCD,0x8B,0,0x68,0xE4};
 auto site=reinterpret_cast<void*>(0x7D2670);
 if(memcmp(site,guard,sizeof(guard))){Log("NATIVE_CRT_REPORT_OBSERVER refused=code_mismatch");return;}
 if(MH_CreateHook(site,ReportBridge,&reportTrampoline)!=MH_OK)return;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);reportTrampoline=nullptr;return;}
 Log("NATIVE_CRT_REPORT_OBSERVER enabled=1 native_assertion_retained=1");
}
void Hud(){
 if(!worldLogEnabled)return;
 static ULONGLONG last=0;auto now=GetTickCount64();if(now-last<5000)return;last=now;
 unsigned flow=0,list=0,count=0;
 Snapshot(reinterpret_cast<void*>(0x925E90),&flow,4);if(flow!=6)return;
 Snapshot(reinterpret_cast<void*>(0x92FD8C),&list,4);Snapshot(reinterpret_cast<void*>(0x92FD94),&count,4);
 Log("NATIVE_HUD_AUDIT list=%08X count=%u",list,count);if(!list||count>8)return;
 for(unsigned i=0;i<count;++i){
  unsigned object=0,words[216]{};if(!Snapshot(reinterpret_cast<void*>(list+i*4),&object,4)||!object||!Snapshot(reinterpret_cast<void*>(object),words,sizeof(words)))continue;
  Log("NATIVE_HUD_WORDS hud=%u object=%08X vtable=%08X features=%08X%08X",i,object,words[0],words[7],words[6]);
  unsigned profile=0,settings=0;Snapshot(reinterpret_cast<void*>(0x91CF90),&profile,4);
  if(profile)Snapshot(reinterpret_cast<void*>(profile+0x10),&settings,4);
  unsigned options[5]{};auto player=words[0x2C/4];
  if(settings&&player<2&&Snapshot(reinterpret_cast<void*>(settings+0x80+player*20),options,sizeof(options)))
   Log("NATIVE_HUD_SETTINGS hud=%u player=%u gauge_enabled=%u options=%08X,%08X,%08X,%08X,%08X read_only=1",i,player,options[0]&255,options[0],options[1],options[2],options[3],options[4]);
  // Native FEngHud::SetFeatures reads these fields, including speedometer
  // +2C0 and tachometer +2C4. Use observed code offsets, not SDK sizeof guesses.
  for(unsigned at=0x2C0;at<=0x32C;at+=4){
   unsigned element=words[at/4],header[10]{};
   if(!element||!Snapshot(reinterpret_cast<void*>(element),header,sizeof(header)))continue;
   Log("NATIVE_HUD_ELEMENT hud=%u offset=%03X object=%08X vtable=%08X mask=%08X%08X features=%08X%08X first=%08X last=%08X",i,at,element,header[0],header[7],header[6],header[9],header[8],header[2],header[3]);
   unsigned node=header[2];
   for(unsigned n=0;node&&node!=element+8&&n<24;++n){
    unsigned entry[3]{},fe[23]{};if(!Snapshot(reinterpret_cast<void*>(node),entry,sizeof(entry))||!entry[2]||!Snapshot(reinterpret_cast<void*>(entry[2]),fe,sizeof(fe)))break;
    Log("NATIVE_HUD_FE hud=%u element_offset=%03X object=%08X type=%u key=%08X flags=%08X cached=%08X data=%08X size=%u",i,at,entry[2],fe[6],fe[4],fe[7],fe[22],fe[11],fe[12]);node=entry[0];
   }
  }
 }
 auto custom=GetModuleHandleW(L"CustomHud.asi");
 if(custom){
  unsigned char code[7]{},enabled=0;auto base=reinterpret_cast<BYTE*>(custom);
  // Recognize the installed build's observed "mov IsHudEnabled,0" cave.
  // Never write a third-party flag or guess offsets for other HUD builds.
  if(Snapshot(base+0x139C7,code,sizeof(code))&&code[0]==0xC6&&code[1]==5&&code[6]==0){
   unsigned address=0;memcpy(&address,code+2,4);
   if(address==unsigned(base)+0x4F5E3&&Snapshot(reinterpret_cast<void*>(address),&enabled,1))
    Log("CUSTOM_HUD_VISIBILITY recognized_build=1 enabled=%u observer_only=1",unsigned(enabled));
  }
 }
}
}
