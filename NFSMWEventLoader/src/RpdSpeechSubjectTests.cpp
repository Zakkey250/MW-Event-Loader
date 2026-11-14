#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cmath>
bool worldLogEnabled=false;void Log(const char*,...){}
bool Copy(const void* p,void* out,size_t n){__try{memcpy(out,p,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool mission=true,racing=true,pending=false,sprint=false;
bool MWELRpdHotPursuitScope(){return sprint;}
unsigned MWELRpdSprintTarget(unsigned){return sprint?3u:0u;}
bool MWELRpdSprintRetired(unsigned){return false;}
unsigned racePointer=0;
std::array<std::array<BYTE,0x200>,4> vehicles{};
namespace rpd_mission {bool Current(){return mission;}bool ResultPending(){return pending;}}
namespace combat_health_runtime {unsigned generation=1;void(*speechSuspectContact)(unsigned,float,float)=nullptr;DWORD gameThread=GetCurrentThreadId();bool Racing(){return racing;}}
namespace gauntlet_runtime {
template<class T>bool Read(unsigned p,T& out){if(p==0x91E000){out=T(racePointer);return true;}__try{memcpy(&out,reinterpret_cast<void*>(p),sizeof(out));return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
unsigned Vehicle(unsigned h){return h&&h<=vehicles.size()?unsigned(vehicles[h-1].data()+0x80):0;}
unsigned Handle(unsigned car){return *reinterpret_cast<unsigned*>(car-0x78);}
}
#include "RpdSpeechSubject.h"
unsigned __fastcall NativePlayer(unsigned,void*){return 1234;}
unsigned focusCalls=0,focusSelf=0;
void __fastcall NativeFocus(unsigned self,void*){++focusCalls;focusSelf=self;}
void Put(unsigned p,unsigned v){*reinterpret_cast<unsigned*>(p)=v;}
void* Shim(void* original,unsigned resume){
 auto copy=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));assert(copy);
 const auto bytes=static_cast<BYTE*>(original);unsigned end=0,relocated=0;
 for(unsigned i=0;i<256;++i)if(bytes[i]==0x68&&*reinterpret_cast<unsigned*>(bytes+i+1)==resume&&bytes[i+5]==0xC3){end=i;break;}
 assert(end);memcpy(copy,bytes,end+6);*reinterpret_cast<unsigned*>(copy+end+1)=unsigned(copy+end+5);
 for(unsigned i=0;i+5<end;++i)if(bytes[i]==0xE8&&unsigned(bytes+i+5+*reinterpret_cast<int*>(bytes+i+1))==unsigned(rpd_speech_subject::Count)){
  *reinterpret_cast<int*>(copy+i+1)=int(unsigned(rpd_speech_subject::Count)-unsigned(copy+i+5));++relocated;}
 assert(relocated==1);FlushInstructionCache(GetCurrentProcess(),copy,end+6);return copy;
}
void* MultipleShim(){
 auto copy=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));assert(copy);
 const auto bytes=reinterpret_cast<BYTE*>(rpd_speech_subject::MultipleFlag);memcpy(copy,bytes,256);
 unsigned calls=0,exits=0;
 for(unsigned i=0;i<250;++i){
  if(bytes[i]==0xE8&&unsigned(bytes+i+5+*reinterpret_cast<int*>(bytes+i+1))==unsigned(rpd_speech_subject::MultiplePolicy)){
   *reinterpret_cast<int*>(copy+i+1)=int(unsigned(rpd_speech_subject::MultiplePolicy)-unsigned(copy+i+5));++calls;
  }
  if(bytes[i]==0x68&&bytes[i+5]==0xC3){const auto target=*reinterpret_cast<unsigned*>(bytes+i+1);
   if(target==0x720ECE||target==0x720E9A||target==0x720ED6){
    auto stub=copy+512+exits*16;
    if(target==0x720ECE){stub[0]=0xB8;*reinterpret_cast<unsigned*>(stub+1)=0xDEADBEEF;stub[5]=0xC3;}else stub[0]=0xC3;
    *reinterpret_cast<unsigned*>(copy+i+1)=unsigned(stub);++exits;
   }
  }
 }
 assert(calls==1&&exits==3);FlushInstructionCache(GetCurrentProcess(),copy,4096);return copy;
}
unsigned __declspec(naked) __stdcall RunMultiple(void*,unsigned){__asm{
 push ebp
 mov eax,[esp+8]
 mov ebp,[esp+12]
 call eax
 pop ebp
 ret 8
}}
int main(){
 std::array<BYTE,0x1A00> race{};racePointer=unsigned(race.data());Put(racePointer+0x1958,2);
 Put(racePointer+0x18,1);Put(racePointer+0x18+0x194,2);
 for(unsigned h=1;h<=4;++h){Put(gauntlet_runtime::Vehicle(h)-0x78,h);Put(gauntlet_runtime::Vehicle(h)+0x94,h==1?0:3);}
 rpd_speech_subject::SuspectContact(1,10,5);assert(!rpd_speech_subject::RecentContact(2,5));
 rpd_speech_subject::SuspectContact(2,0,5);assert(!rpd_speech_subject::RecentContact(2,5));
 rpd_speech_subject::SuspectContact(2,1,5);assert(rpd_speech_subject::RecentContact(2,5.5f));
 assert(!rpd_speech_subject::RecentContact(1,5.5f)&&!rpd_speech_subject::RecentContact(2,7)&&!rpd_speech_subject::RecentContact(2,4));
 ++combat_health_runtime::generation;assert(!rpd_speech_subject::RecentContact(2,5.5f));
 unsigned playerTable[4]={0,unsigned(NativePlayer)},player=unsigned(playerTable);
 assert(rpd_speech_subject::Subject(unsigned(&player),0)==gauntlet_runtime::Vehicle(2)-0x80);
 sprint=true;
 rpd_speech_subject::officerTarget=[](unsigned officer){return officer==1?2u:0u;};
 assert(rpd_speech_subject::Suspect()==gauntlet_runtime::Vehicle(2)); // attached group takes precedence over nearest
 rpd_speech_subject::officerTarget=nullptr;
 assert(rpd_speech_subject::Suspect()==gauntlet_runtime::Vehicle(3));
 bool confirmed=false;
 static bool* visual=&confirmed;
 rpd_speech_subject::stillInSight=[](unsigned){return *visual;};
 rpd_speech_subject::originalDispatch=[](unsigned,unsigned,unsigned,unsigned,unsigned){return 73u;};
 for(const auto declaration:{0x901E0Cu,0x901E1Cu}){
  confirmed=true;assert(rpd_speech_subject::Dispatch(0,0,declaration,0,0)==0);
  confirmed=false;assert(rpd_speech_subject::Dispatch(0,0,declaration,0,0)==73);
 }
 confirmed=true;assert(rpd_speech_subject::Dispatch(0,0,0x901ECCu,0,0)==73); // heli report is independent
 rpd_speech_subject::stillInSight=nullptr;sprint=false;
 assert(rpd_speech_subject::Count(2)==1&&rpd_speech_subject::Others(1)==0); // No foreground Speech object is required.
 rpd_speech_subject::trampolines[19]=reinterpret_cast<void*>(NativeFocus);
 assert(rpd_speech_subject::SingleTarget());rpd_speech_subject::FocusChange(42,nullptr);assert(!focusCalls);
 Put(racePointer+0x1958,4);Put(racePointer+0x18+2*0x194,3);Put(racePointer+0x18+3*0x194,4);
 Put(gauntlet_runtime::Vehicle(3)+0x94,2);assert(rpd_speech_subject::Count(4)==2&&rpd_speech_subject::Others(3)==1);
 assert(!rpd_speech_subject::SingleTarget());rpd_speech_subject::FocusChange(42,nullptr);assert(focusCalls==1&&focusSelf==42);
 race[0x18+3*0x194+0x21]=1;assert(rpd_speech_subject::Count(4)==1);
 mission=false;assert(rpd_speech_subject::Subject(unsigned(&player),0)==1234&&rpd_speech_subject::Count(4)==4&&rpd_speech_subject::Others(3)==3);mission=true;
 mission=false;assert(!rpd_speech_subject::SingleTarget());rpd_speech_subject::FocusChange(43,nullptr);assert(focusCalls==2&&focusSelf==43);mission=true;
 pending=true;assert(!rpd_speech_subject::Suspect()&&rpd_speech_subject::Count(4)==4);pending=false;
 combat_health_runtime::gameThread=0;assert(!rpd_speech_subject::Suspect());combat_health_runtime::gameThread=GetCurrentThreadId();
 Put(gauntlet_runtime::Vehicle(2)+0x94,0);assert(!rpd_speech_subject::Suspect());Put(gauntlet_runtime::Vehicle(2)+0x94,3);
 // Execute the real x86 shims and their continuation contract in isolated pages.
 const auto shim0=Shim(rpd_speech_subject::Count0,0x7176EF),shim1=Shim(rpd_speech_subject::Count1,0x717785),shim2=Shim(rpd_speech_subject::Count2,0x71EFFF);
 std::array<BYTE,0x200> speech{};speech[0x15C]=4;unsigned speechPointer=unsigned(speech.data()),result=0,before=0,after=0;
 __asm{pushad}
 __asm{mov before,esp}
 __asm{mov edi,speechPointer}
 __asm{mov ecx,12345678h}
 __asm{call dword ptr [shim0]}
 __asm{mov result,ecx}
 __asm{mov after,esp}
 __asm{popad}
 assert(result==0x12345601&&before==after);
 __asm{pushad}
 __asm{mov edi,speechPointer}
 __asm{mov eax,12345678h}
 __asm{call dword ptr [shim1]}
 __asm{mov result,eax}
 __asm{popad}
 assert(result==0x12345601);
 __asm{pushad}
 __asm{mov ebx,speechPointer}
 __asm{mov ecx,87654321h}
 __asm{call dword ptr [shim2]}
 __asm{mov result,ecx}
 __asm{popad}
 assert(result==0x87654301);for(const auto p:{shim0,shim1,shim2})VirtualFree(p,0,MEM_RELEASE);
 const auto multi=MultipleShim();Put(speechPointer+0x184,0x12345678);
 __asm{mov before,esp}
 result=RunMultiple(multi,speechPointer);
 __asm{mov after,esp}
 assert(result==0xDEADBEEF&&before==after);
 mission=false;
 result=RunMultiple(multi,speechPointer);
 assert(result==0x12345678);
 rpd_speech_subject::trackedCount=[](bool additional){return additional?2u:3u;};Put(speechPointer+0x54,0x1234);
 assert(RunMultiple(multi,speechPointer)==0x1A34&&rpd_speech_subject::Count(0)==3&&rpd_speech_subject::Others(0)==2);
 rpd_speech_subject::trackedCount=[](bool additional){return additional?0u:1u;};
 assert(RunMultiple(multi,speechPointer)==0xDEADBEEF&&rpd_speech_subject::Others(4)==0);
 rpd_speech_subject::trackedCount=nullptr;mission=true;VirtualFree(multi,0,MEM_RELEASE);
 std::puts("PASS RPD radio: subject identity; native passthrough; officer/allied/finished exclusion; multiple racers; thread/result guards; focus-change scope; 4 real x86 shims and native/single-target continuations preserve stack");
}
