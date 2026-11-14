#include "TougeSpeechLifetime.h"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

static unsigned self[0xB8/4]{},handle[0x6C/4]{},table[1]{};
static unsigned destroyed=0,detachments=0,faultEip=0,faultTarget=0;
static bool returning=true;
static void* nativeBind=nullptr;static void* nativeUpdate=nullptr;static void* nativeGetter=nullptr;
static void __fastcall Destroy(void* sound,void*,unsigned flags){
 assert(sound==handle&&flags==1&&self[0x98/4]==0);++destroyed;
}
static void Retire(bool detached,std::uintptr_t object=reinterpret_cast<std::uintptr_t>(self)){
 auto read=[](std::uintptr_t p)->std::uintptr_t{return *reinterpret_cast<unsigned*>(p);};
 auto write=[](std::uintptr_t p,std::uintptr_t v){*reinterpret_cast<unsigned*>(p)=unsigned(v);};
 auto destroy=[](std::uintptr_t p){assert(*reinterpret_cast<unsigned*>(p)==0x896AD0);Destroy(reinterpret_cast<void*>(p),nullptr,1);};
 touge_speech_lifetime::Retire(returning,object,reinterpret_cast<std::uintptr_t>(self),detached,read,write,destroy);
}
static void Bind(bool fixed){
 if(fixed)Retire(true);
 reinterpret_cast<void(__thiscall*)(void*,void*,unsigned)>(nativeBind)(self,nullptr,1);++detachments;
}
static int Fault(EXCEPTION_POINTERS* e){faultEip=unsigned(e->ExceptionRecord->ExceptionAddress);faultTarget=unsigned(e->ExceptionRecord->ExceptionInformation[1]);return EXCEPTION_EXECUTE_HANDLER;}
static bool Update(){
 __try{reinterpret_cast<void(__thiscall*)(void*)>(nativeUpdate)(self);return true;}
 __except(Fault(GetExceptionInformation())){return false;}
}
static void Reset(){memset(self,0,sizeof(self));memset(handle,0,sizeof(handle));self[0]=0x8B24F0;self[0x44/4]=0xDEADBEEF;self[0x98/4]=unsigned(handle);handle[0]=0x896AD0;reinterpret_cast<unsigned char*>(handle)[0x20]=1;returning=true;destroyed=detachments=0;}
static void* Load(const std::filesystem::path& folder,unsigned address,unsigned length){
 char name[32]{};sprintf_s(name,"%08X.bin",address);std::ifstream f(folder/name,std::ios::binary);
 std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});assert(bytes.size()==length);
 auto allocated=VirtualAlloc(nullptr,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(allocated);memcpy(allocated,bytes.data(),bytes.size());return allocated;
}
int wmain(int argc,wchar_t** argv){
 assert(argc==2);nativeBind=Load(argv[1],0x6FF740,0x59);
 nativeUpdate=Load(argv[1],0x6FF9E0,0x4C);nativeGetter=Load(argv[1],0x4EB7D0,3);
 // Relocate the single reached external CALL; retain all native instructions
 // and intra-function branches, including the unguarded NULL dereference.
 auto call=static_cast<unsigned char*>(nativeUpdate)+0x2D;assert(*call==0xE8);
 *reinterpret_cast<int*>(call+1)=int(static_cast<unsigned char*>(nativeGetter)-(call+5));
 Reset();Bind(false);assert(!Update()&&faultEip==unsigned(nativeGetter)&&faultTarget==0x10&&destroyed==0);
 Reset();Bind(true);assert(Update()&&destroyed==1&&detachments==1&&self[0x44/4]==0&&self[0x98/4]==0);
 Bind(true);assert(Update()&&destroyed==1); // No double retirement.
 Reset();self[0x44/4]=0;Retire(true);assert(Update()&&destroyed==1); // Already detached on entry.
 Reset();Retire(false);assert(!destroyed&&self[0x98/4]); // Definition still live.
 Reset();returning=false;Retire(true);assert(!destroyed&&self[0x98/4]); // Ordinary races unchanged.
 Reset();self[0]=0x8B2228;Retire(true);assert(!destroyed); // Smaller NISSFXModule is not GameSpeech.
 Reset();handle[0]=0x1234;Retire(true);assert(!destroyed); // Unverified sound object untouched.
 Reset();unsigned other[0xB8/4]{};other[0]=0x8B24F0;Retire(true,reinterpret_cast<std::uintptr_t>(other));assert(!destroyed);
 printf("PASS actual native detach/update reproduces EIP=004EB7D0 target=10; return cleanup prevents it; native binding preserved; exactly one retirement; ordinary/foreign owners untouched\n");
}
