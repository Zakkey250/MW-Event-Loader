#pragma once
// The native helicopter does not exclusively follow AITarget. Its think path,
// navigation and pursuit action fetch IPlayer[0] independently. Redirect those
// exact reads, preserving unrelated native callers and the player singleton.
namespace hot_pursuit_runtime {
void* heliThinkTrampoline=nullptr;
void* heliFlightTrampoline=nullptr;
void* heliActionEnterTrampoline=nullptr;
void* heliActionUpdateTrampoline=nullptr;
void* heliActionSideTrampoline=nullptr;
std::atomic<unsigned long long> helicopterLeader{};
std::atomic<unsigned> helicopterRace{},helicopterParams{},helicopterObject{};
std::atomic<unsigned long long> helicopterInstance{};
void ResetHelicopterSnapshot(){helicopterLeader.store(0);helicopterRace.store(0);helicopterParams.store(0);helicopterObject.store(0);helicopterInstance.store(0);}
void PublishHelicopterSnapshot(unsigned leader){
 unsigned race=0,params=0;const auto car=gauntlet_runtime::Vehicle(leader);
 if(!car||!gauntlet_runtime::Read(0x91E000,race)||!gauntlet_runtime::Read(race+0x1968,params)){ResetHelicopterSnapshot();return;}
 helicopterRace.store(race);helicopterParams.store(params);
 helicopterLeader.store((static_cast<unsigned long long>(leader)<<32)|(car-0x80));
}
void PublishHelicopterObject(unsigned object,unsigned car,unsigned handle){helicopterInstance.store((static_cast<unsigned long long>(handle)<<32)|car);helicopterObject.store(object);}
bool IsHelicopterTarget(unsigned object){
 if(!object||object!=helicopterObject.load())return false;
 const auto instance=helicopterInstance.load();const auto car=unsigned(instance),handle=unsigned(instance>>32);
 unsigned actual=0,ai=0,target=0;
 return car&&gauntlet_runtime::Read(car-0x80+8,actual)&&actual==handle&&gauntlet_runtime::Read(car+0x54,ai)&&
  gauntlet_runtime::Helicopter(ai)&&gauntlet_runtime::Read(ai+0x54,target)&&target==object;
}
unsigned __cdecl HelicopterLeaderSim(){
 // AI flight work may execute on a native worker. Consume the bounded frame
 // snapshot; never walk authoring maps or pursuit lists on that worker.
 const auto snapshot=helicopterLeader.load();const auto sim=unsigned(snapshot),handle=unsigned(snapshot>>32);
 unsigned race=0,params=0,actual=0,play=0;BYTE active=0,timer=0;
 if(!sim||!handle||!gauntlet_runtime::Read(0x91E000,race)||race!=helicopterRace.load()||
    !gauntlet_runtime::Read(race+0x1968,params)||params!=helicopterParams.load()||
    !gauntlet_runtime::Read(race+0x1960,play)||play!=1||!gauntlet_runtime::Read(race+0x198A,active)||!active||
    !gauntlet_runtime::Read(race+0x1978,timer)||!timer||!gauntlet_runtime::Read(sim+8,actual)||actual!=handle||
    !gauntlet_runtime::Table(sim+0x80,0x8AA828))return 0;
 return sim;
}
__declspec(naked) void HelicopterThinkPlayer(){__asm {
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call HelicopterLeaderSim
 test eax,eax
 jz nativeRead
 mov [ebp+28],eax
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push esi
 lea esi,[ebx+4Ch]
 push 042ADDFh
 ret
 nativeRead:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [heliThinkTrampoline]
}}
__declspec(naked) void HelicopterFlightPlayer(){__asm {
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call HelicopterLeaderSim
 test eax,eax
 jz nativeRead
 mov [ebp+28],eax
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 042AF0Eh
 ret
 nativeRead:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [heliFlightTrampoline]
}}
// AIGoalHeliPursuit owns its own player-physics cache, independently of
// AIVehicleHelicopter::Update and its flight navigation. Replace only this
// exact IPlayer::GetSimable call; native actions still own flight and fuel.
__declspec(naked) void HelicopterActionEnterPlayer(){__asm {
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call HelicopterLeaderSim
 test eax,eax
 jz nativeRead
 mov [ebp+28],eax
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 042778Fh
 ret
 nativeRead:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [heliActionEnterTrampoline]
}}
__declspec(naked) void HelicopterActionUpdatePlayer(){__asm {
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call HelicopterLeaderSim
 test eax,eax
 jz nativeRead
 mov [ebp+28],eax
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 0427821h
 ret
 nativeRead:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [heliActionUpdateTrampoline]
}}
__declspec(naked) void HelicopterActionSidePlayer(){__asm {
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call HelicopterLeaderSim
 test eax,eax
 jz nativeRead
 mov [ebp+28],eax
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 0427922h
 ret
 nativeRead:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [heliActionSideTrampoline]
}}
bool HelicopterNativeSurface(){
 BYTE code[6]{};const BYTE think[]={0xA1,0x84,0xD8,0x92,0},flight[]={0xA1,0x84,0xD8,0x92,0};
 const BYTE playerCall[]={0x8B,0x11,0xFF,0x52,0x04};
 return (heliActionEnterTrampoline||(Copy(reinterpret_cast<void*>(0x42778A),code,5)&&!memcmp(code,playerCall,5)))&&
  (heliActionUpdateTrampoline||(Copy(reinterpret_cast<void*>(0x42781C),code,5)&&!memcmp(code,playerCall,5)))&&
  (heliActionSideTrampoline||(Copy(reinterpret_cast<void*>(0x42791D),code,5)&&!memcmp(code,playerCall,5)))&&
  (heliThinkTrampoline||(Copy(reinterpret_cast<void*>(0x42ADC2),code,5)&&!memcmp(code,think,5)))&&
  (heliFlightTrampoline||(Copy(reinterpret_cast<void*>(0x42AEF4),code,5)&&!memcmp(code,flight,5)));
}
}
