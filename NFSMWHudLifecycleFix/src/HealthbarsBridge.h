#pragma once
// Adapter for the exact installed healthbars.asi, SHA-256 checked below.
// Only its read operands are redirected. Native police globals/tables are
// never replaced, and synthetic view records are never passed to game code.
namespace healthbars_bridge {
std::array<unsigned,144> table{};unsigned tablePointer=unsigned(table.data()),tableCount=0,managerPointer=0;
std::array<BYTE,0x134> manager{};
struct View {
 std::array<BYTE,0x120> record{};
 std::array<BYTE,0x40> linkA{};std::array<BYTE,4> linkB{};std::array<BYTE,0x20> linkC{};std::array<BYTE,0x40> damage{};
};
std::array<View,16> views{};
void(__thiscall* originalDraw)(void*)=nullptr;
bool attempted=false,active=false;
using LeaderFn=unsigned(__cdecl*)(unsigned);
LeaderFn leaderQuery=nullptr;unsigned leaderHandle=0;float leaderPosition[3]{};bool leaderVisible=false;uintptr_t moduleBase=0;
float leaderMatrix[16]{};DWORD leaderMatrixAt=0;
unsigned Address(const void* p){return static_cast<unsigned>(reinterpret_cast<uintptr_t>(p));}
template<class T>void Store(void* p,size_t offset,T value){std::memcpy(static_cast<BYTE*>(p)+offset,&value,sizeof(value));}
void Mirror(){
 tableCount=0;leaderVisible=false;leaderHandle=0;unsigned nativeCount=0,nativeTable=0,leaderDamage=0;
 if(combat_hud::Active()){
  if(!leaderQuery){auto event=GetModuleHandleW(L"MWEventLoader.asi");if(event)leaderQuery=reinterpret_cast<LeaderFn>(GetProcAddress(event,"MWEL_GauntletLeaderV1"));}
  if(leaderQuery)leaderHandle=leaderQuery(mwcombat::Version);
  if(leaderHandle&&Readable(0x92CD1C,12)){
   const auto list=Read<unsigned>(0x92CD1C),n=Read<unsigned>(0x92CD24);
   if(n<=512&&Readable(list,n*4))for(unsigned i=0;i<n;++i){const auto vehicle=Read<unsigned>(list+i*4);
    if(vehicle<0x10080||!Readable(vehicle-0x80,16)||Read<unsigned>(vehicle-0x80+8)!=leaderHandle)continue;
    const auto com=Read<unsigned>(vehicle-0x80+4);
    if(Readable(com,4))leaderDamage=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x405000);
    break;
   }
  }
 }
 if(Readable(0x92CEA4,4)&&Readable(0x92CE9C,4)){nativeCount=Read<unsigned>(0x92CEA4);nativeTable=Read<unsigned>(0x92CE9C);}
 if(nativeCount<=128&&(!nativeCount||Readable(nativeTable,nativeCount*4)))for(unsigned i=0;i<nativeCount;++i){
  const auto row=Read<unsigned>(nativeTable+i*4);bool leader=false;
  if(leaderHandle&&Readable(row+0x4C,4)){
   const auto damage=Read<unsigned>(row+0x4C);
   if(leaderDamage&&(damage==leaderDamage||damage==leaderDamage-0x4C)){
    if(Readable(row-0x34,4)){
     auto link=Read<unsigned>(row-0x34);
     if(Readable(link+0x30,4)){link=Read<unsigned>(link+0x30);if(Readable(link,4)){link=Read<unsigned>(link);
      if(Readable(link+0x10,12)){std::memcpy(leaderPosition,reinterpret_cast<void*>(link+0x10),12);leaderVisible=leader=true;}
     }}
    }
   }
  }
  if(!leader)table[tableCount++]=row; // ordinary police retain their original native health records
 }
 if(loggingEnabled){static unsigned previous=~0u;static bool seen=false;
  if(previous!=leaderHandle||seen!=leaderVisible){previous=leaderHandle;seen=leaderVisible;
   Log("HEALTHBARS_LEADER handle=%u matched=%u native_cops=%u ordinary_bars=%u",leaderHandle,unsigned(leaderVisible),nativeCount,tableCount);
  }
 }
 managerPointer=Readable(0x993CC8,4)?Read<unsigned>(0x993CC8):0;
 if(!combat_hud::Active())return;
 managerPointer=Address(manager.data());Store(manager.data(),0x130,1u);
 for(unsigned i=0;i<combat_hud::count;++i){const auto& car=combat_hud::cars[i];if(car.health.driver!=3||car.health.slot>=16||car.health.maximum<=0)continue;
  auto& v=views[car.health.slot];const unsigned row=Address(v.record.data()+0x80);
  Store(v.record.data(),0x4C,Address(v.linkA.data()));Store(v.linkA.data(),0x30,Address(v.linkB.data()));Store(v.linkB.data(),0,Address(v.linkC.data()));
  std::memcpy(v.linkC.data()+0x10,car.position,sizeof(car.position));
  // The third-party renderer reads IDamageable+3C only; this is a private
  // data record with that one float, not a fake game damage component.
  Store(v.damage.data(),0x3C,1.f-std::clamp(car.health.remaining/car.health.maximum,0.f,1.f));
  Store(v.record.data(),0x80+0x4C,Address(v.damage.data()));
  Store(v.record.data(),0x80+0x6C,0x336FCACFu);Store(v.record.data(),0x80+0x84,car.health.remaining>0?1u:0u);
  table[tableCount++]=row;
 }
}
void __fastcall Draw(void* self,void*){
 Mirror();
 if(combat_hud::Active()&&(!combat_hud::visible||Read<unsigned>(0x925E90)!=6||reinterpret_cast<bool(__cdecl*)()>(0x468390)()))return;
 originalDraw(self);
 if(leaderVisible&&moduleBase&&Readable(0x984350,64)){
  // healthbars first transforms the anchor by 9842D0, then draws its
  // billboard with inverse(9842D0)*984350. The anchor's net transform is
  // therefore 984350. Its internal 60CC is for those camera-space quads,
  // not for projecting a world-space anchor a second time.
  std::memcpy(leaderMatrix,reinterpret_cast<const void*>(0x984350),sizeof(leaderMatrix));leaderMatrixAt=GetTickCount();
 }
}
void LeaderOverlay(IDirect3DDevice9* device){
 if(leaderVisible&&GetTickCount()-leaderMatrixAt<100){
  // Draw at the shared final HUD pass; the native healthbars callback is
  // inside world rendering and later postprocessing can erase screen quads.
  combat_hud::LeaderStar(device,leaderPosition,leaderMatrix);
  if(loggingEnabled){static DWORD next=0;if(GetTickCount()>=next){next=GetTickCount()+3000;D3DVIEWPORT9 viewport{};float x=0,y=0,depth=0;
   if(device&&SUCCEEDED(device->GetViewport(&viewport))){const bool projected=combat_hud::Project(leaderPosition,leaderMatrix,viewport,x,y,depth);Log("HEALTHBARS_LEADER_PROJECT handle=%u visible=%u projected=%u x=%.2f y=%.2f depth=%.5f",leaderHandle,unsigned(combat_hud::visible),unsigned(projected),x,y,depth);}
  }}
 }
}
bool Install(){
 if(active)return true;if(attempted)return false;
 auto healthModule=GetModuleHandleW(L"healthbars.asi");if(!healthModule)return false;attempted=true;
 wchar_t path[MAX_PATH]{};char hash[65]{};
 if(!GetModuleFileNameW(healthModule,path,MAX_PATH)||!HashFile(path,hash)||std::strcmp(hash,"a84a5a09605aa8bc4227ed0982cb80324b09c1b62431ce184aa0f15fcc415248")){Log("HEALTHBARS_ADAPTER declined=unsupported_identity");return false;}
 const auto base=reinterpret_cast<uintptr_t>(healthModule);
 struct Patch {unsigned offset,expected,replacement;};
 const Patch patches[]={{0x13FD,0x92CEA4,Address(&tableCount)},{0x1415,0x92CE9C,Address(&tablePointer)},
  {0x1583,0x92CE9C,Address(&tablePointer)},{0x15A9,0x92CE9C,Address(&tablePointer)},{0x16DF,0x993CC8,Address(&managerPointer)}};
 const BYTE entry[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x81,0xEC,0x28,1,0,0};
 if(!Guard(base+0x1610,entry,sizeof(entry)))return false;
 for(const auto& p:patches)if(!Readable(base+p.offset,4)||Read<unsigned>(base+p.offset)!=p.expected)return false;
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
 auto target=reinterpret_cast<void*>(base+0x1610);
 if(MH_CreateHook(target,Draw,reinterpret_cast<void**>(&originalDraw))!=MH_OK)return false;
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Draw),&pinned)||
    !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(healthModule),&pinned)){
  MH_RemoveHook(target);return false;
 }
 unsigned written=0;
 for(const auto& p:patches){DWORD protection=0;
  if(!VirtualProtect(reinterpret_cast<void*>(base+p.offset),4,PAGE_EXECUTE_READWRITE,&protection))break;
  Store(reinterpret_cast<void*>(base+p.offset),0,p.replacement);DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(base+p.offset),4,protection,&ignored);++written;
 }
 if(written!=std::size(patches)||MH_EnableHook(target)!=MH_OK){
  for(unsigned i=0;i<written;++i){const auto& p=patches[i];DWORD protection=0;if(VirtualProtect(reinterpret_cast<void*>(base+p.offset),4,PAGE_EXECUTE_READWRITE,&protection)){Store(reinterpret_cast<void*>(base+p.offset),0,p.expected);DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(base+p.offset),4,protection,&ignored);}}
  MH_RemoveHook(target);return false;
 }
 FlushInstructionCache(GetCurrentProcess(),healthModule,0x2000);moduleBase=base;active=true;Log("HEALTHBARS_ADAPTER active=1 native_police_tables_unchanged=1 racer_views=16 leader_star=1");return true;
}
}
