#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <atomic>
#include <initializer_list>
#include "MinHook.h"
#include "Layout.h"
#include "CombatHud.h"
#pragma warning(disable:4191)
namespace {
HMODULE module;
wchar_t logPath[MAX_PATH];
bool loggingEnabled=false;
std::atomic<bool> executableAccepted{false};
void Log(const char* fmt,...) {
    if(!loggingEnabled)return;
    FILE* f=nullptr;if(_wfopen_s(&f,logPath,L"a")||!f)return;
    std::fprintf(f,"[%lu] ",GetTickCount());
    va_list args;va_start(args,fmt);std::vfprintf(f,fmt,args);va_end(args);
    std::fputc('\n',f);std::fclose(f);
}
#define Log(...) do { if(loggingEnabled) (Log)(__VA_ARGS__); } while(false)
bool HashFile(const wchar_t* path,char (&hex)[65]) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
    BYTE buffer[32768],digest[32];DWORD n=0;
    while(ok){if(!ReadFile(file,buffer,sizeof(buffer),&n,nullptr)){ok=false;break;}
        if(!n)break;ok=BCryptHashData(hash,buffer,n,0)>=0;}
    if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(file);
    if(ok)for(size_t i=0;i<32;++i)sprintf_s(hex+i*2,65-i*2,"%02x",digest[i]);
    return ok;
}
bool HashExe(char (&hex)[65]) {
    wchar_t path[MAX_PATH];return GetModuleFileNameW(nullptr,path,MAX_PATH)&&HashFile(path,hex);
}
bool Readable(uintptr_t p,size_t n) {
    if(p<0x10000 || p+n<p)return false;
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return p+n<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
}
template<class T>T Read(uintptr_t p){return *reinterpret_cast<const T*>(p);}
using FindFn=void*(__cdecl*)(const char*,uint32_t);
auto findObject=reinterpret_cast<FindFn>(0x524850);
using CenterFn=void(__cdecl*)(void*,float*,float*);
using MoveFn=void(__cdecl*)(void*,float,float);
auto getCenter=reinterpret_cast<CenterFn>(0x524ee0);
auto setCenter=reinterpret_cast<MoveFn>(0x525050);
using AdapterFn=int(__thiscall*)(void*,uint32_t,const char*,uint32_t);
AdapterFn originalAdapter=nullptr;
using QueueFn=void(__fastcall*)(void*,void*,uint32_t,const char*,uint32_t);
QueueFn widescreenQueue=nullptr;
unsigned layoutCalls=0;
int __fastcall Adapter(void* self,void*,uint32_t event,const char* package,uint32_t arg) {
    // WSF already owns both FE group layout and native minimap positioning.
    // Restore its message route instead of adding HUDAdapter's second offset.
    widescreenQueue(self,nullptr,event,package,arg);
    if(loggingEnabled && layoutCalls++<64) {
        for(auto hash:{0x1603009eu,0x5d0101f1u}) {
            auto ob=findObject(package,hash);if(!ob)continue;
            float x=0,y=0;getCenter(ob,&x,&y);
            Log("WSF_LAYOUT event=%08X group=%08X x=%g y=%g",event,hash,x,y);
        }
    }
    return 1; // Exact installed HUDAdapter callback's original return value.
}
bool Guard(uintptr_t p,const BYTE* b,size_t n){return Readable(p,n)&&!std::memcmp(reinterpret_cast<void*>(p),b,n);}
bool AdapterReady(HMODULE adapter) {
    if(!adapter)return false;
    const auto wsf=GetModuleHandleW(L"NFSMostWanted.WidescreenFix.asi");
    if(!wsf)return false;
    wchar_t wsfPath[MAX_PATH];char wsfHash[65]{};
    if(!GetModuleFileNameW(wsf,wsfPath,MAX_PATH)||!HashFile(wsfPath,wsfHash)||
       std::strcmp(wsfHash,"af263e997168b7b6663ec7fc72e54ccfda0e85b1d9f507fa69ce3504d145a0b4"))return false;
    const auto base=reinterpret_cast<uintptr_t>(wsf);
    const BYTE queueEntry[]={0x55,0x8b,0xec,0x83,0xec,0x1c,0x53,0x56,0xff,0x75,0x10};
    if(!Guard(base+0x39d50,queueEntry,sizeof(queueEntry))||Read<uintptr_t>(base+0x1ebda4)!=0x516c90)return false;
    widescreenQueue=reinterpret_cast<QueueFn>(base+0x39d50);
    wchar_t path[MAX_PATH];char hash[65]{};
    if(!GetModuleFileNameW(adapter,path,MAX_PATH)||!HashFile(path,hash)||
       std::strcmp(hash,"ab9c6e4939ae7e2bca628d795d2478c2c11d15e2a18554f93223960b94bcd137"))return false;
    const auto target=reinterpret_cast<uintptr_t>(adapter)+0x1520;
    for(uintptr_t site:{0x5696f1u,0x569712u})
        if(Read<BYTE>(site)!=0xe8 || site+5+Read<int32_t>(site+1)!=target)return false;
    const BYTE entry[]={0x55,0x8b,0xec,0x8b,0x0d,0xd8,0xca,0x93,0};
    const BYTE fld[]={0xd9,0x05};
    const BYTE center[]={0x81,0xec,0x14,0x08,0,0,0x56};
    const BYTE move[]={0x83,0xec,0x10,0x56,0x8b,0x74,0x24,0x18};
    return Guard(target,entry,sizeof(entry))&&Guard(0x5678a8,fld,2)&&Guard(0x5678c4,fld,2)&&
        Guard(0x524ee0,center,sizeof(center))&&Guard(0x525050,move,sizeof(move));
}
#include "HealthbarsBridge.h"
unsigned long long LiveHudFeatures(){
    if(!Readable(0x92FD94,4)||!Readable(0x92FD8C,4))return 0;
    const auto count=Read<unsigned>(0x92FD94),list=Read<uintptr_t>(0x92FD8C);
    if(!count||count>8||!Readable(list,4))return 0;const auto hud=Read<uintptr_t>(list);
    return Readable(hud+0x18,8)?Read<unsigned long long>(hud+0x18):0;
}
DWORD WINAPI Init(void*) {
    GetModuleFileNameW(module,logPath,MAX_PATH);wchar_t* dot=wcsrchr(logPath,L'.');if(dot)wcscpy_s(dot,5,L".log");
    wchar_t ini[MAX_PATH]{};wcscpy_s(ini,logPath);dot=wcsrchr(ini,L'.');if(dot)wcscpy_s(dot,5,L".ini");
    wchar_t loggingSetting[8]{};GetPrivateProfileStringW(L"Diagnostics",L"Logging",L"",loggingSetting,8,ini);loggingEnabled=wcscmp(loggingSetting,L"1")==0;
    Log("NFSMWHudLifecycleFix 0.3.0-standalone pid=%lu",GetCurrentProcessId());
    char hash[65]{};
    if(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=0x400000 || !HashExe(hash) ||
       (std::strcmp(hash,"80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c") &&
        std::strcmp(hash,"b248271bf8eac8c9b283b8c95e3add672b713bf529b05f1780e58268493b9d06") &&
        std::strcmp(hash,"0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342"))) {
        Log("REJECTED executable=%s",hash);return 0;
    }
    executableAccepted=true;
    // No global FE visibility/layout hook is needed without the old adapter.
    // Widescreen Fix already owns native group/minimap layout; vanilla keeps
    // its native layout. Combat/healthbars rendering remains independently active.
    wchar_t adapterPath[MAX_PATH]{};GetModuleFileNameW(module,adapterPath,MAX_PATH);
    auto slash=wcsrchr(adapterPath,L'\\');if(!slash)return 0;
    wcscpy_s(slash+1,MAX_PATH-size_t(slash+1-adapterPath),L"NFSMWHUDAdapter.asi");
    if(GetFileAttributesW(adapterPath)==INVALID_FILE_ATTRIBUTES &&
       !GetModuleHandleW(L"NFSMWHUDAdapter.asi")) {
        Log("LAYOUT owner=native-or-widescreen-fix adapter_dependency=0 layout_hooks=0");
        return 0;
    }
    HMODULE adapter=nullptr;bool ready=false;
    for(unsigned i=0;i<200;++i){adapter=GetModuleHandleW(L"NFSMWHUDAdapter.asi");
        if(AdapterReady(adapter)){ready=true;break;}Sleep(50);}
    if(!ready){Log("REJECTED unsupported HUDAdapter or call chain");return 0;}
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
       reinterpret_cast<LPCWSTR>(&Adapter),&pinned))return 0;
    void* target=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(adapter)+0x1520);
    MH_STATUS s=MH_Initialize();
    if(s==MH_ERROR_ALREADY_INITIALIZED)s=MH_OK;
    if(s==MH_OK)s=MH_CreateHook(target,&Adapter,reinterpret_cast<void**>(&originalAdapter));
    if(s==MH_OK)s=MH_EnableHook(target);
    if(s!=MH_OK){if(originalAdapter){MH_DisableHook(target);MH_RemoveHook(target);}Log("REJECTED MinHook=%d",s);return 0;}
    Log("ACTIVE HUDAdapter events routed to verified WSF layout owner; visual acceptance pending");
    return 0;
}
}
#pragma comment(linker,"/EXPORT:MWHUD_CombatDrawV1=_MWHUD_CombatDrawV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWHUD_CombatDrawV1(void* device,const mwcombat::RacerHudV1* cars,unsigned count,unsigned visible){
    if(!executableAccepted||count>16||(!cars&&count))return 0;
    unsigned slots=0;
    for(unsigned i=0;i<count;++i){const auto& h=cars[i].health;
        if(h.size!=sizeof(h)||h.version!=mwcombat::Version||h.slot>=16||!h.eventKey||!h.handle||
           !mwcombat::HudRacer(h.driver)||!std::isfinite(h.remaining)||!std::isfinite(h.maximum)||h.maximum<=0||h.maximum>100000||h.remaining<0||h.remaining>h.maximum||
           (slots&(1u<<h.slot)))return 0;
        for(float coordinate:cars[i].position)if(!std::isfinite(coordinate)||std::abs(coordinate)>1000000)return 0;
        slots|=1u<<h.slot;
    }
    combat_hud::count=count;for(unsigned i=0;i<count;++i)combat_hud::cars[i]=cars[i];combat_hud::sampledAt=GetTickCount();
    const auto features=LiveHudFeatures();
    combat_hud::visible=visible&&features&&Read<unsigned>(0x925E90)==6&&!reinterpret_cast<bool(__cdecl*)()>(0x64B680)();
    if(loggingEnabled){static unsigned previousCount=~0u;static unsigned long long previousFeatures=~0ull;static bool previousVisible=false;
        if(previousCount!=count||previousFeatures!=features||previousVisible!=combat_hud::visible){
            bool player=false;for(unsigned i=0;i<count;++i)player|=cars[i].health.driver==0;
            Log("COMBAT_HUD_STATE count=%u player=%u visible=%u features=%016llX",count,unsigned(player),unsigned(combat_hud::visible),features);
            previousCount=count;previousFeatures=features;previousVisible=combat_hud::visible;
        }
    }
    if(count)healthbars_bridge::Install();
    // Custom speedometers intentionally remove the native gauge feature bits.
    // Follow the complete HUD visibility mask above, not native gauge bits,
    // or the health bar disappears when a replacement speedometer is installed.
    combat_hud::Player(static_cast<IDirect3DDevice9*>(device));
    healthbars_bridge::LeaderOverlay(static_cast<IDirect3DDevice9*>(device));
    return 1;
}
BOOL WINAPI DllMain(HMODULE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);
        HANDLE thread=CreateThread(nullptr,0,Init,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
