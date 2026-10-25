#pragma once
// CopMgr::UpdateRoadblockRequest calls the placement routine synchronously.
// Do not redistribute or detach activity members inside that transaction.
namespace hot_pursuit_runtime {
void CommitCandidate();
extern bool candidateKnown;
unsigned roadblockDepth=0;
using RoadblockFn=bool(__thiscall*)(unsigned,unsigned,unsigned,unsigned,unsigned);
RoadblockFn originalRoadblock=nullptr;
void* roadblockCommitTrampoline=nullptr;
bool __fastcall PlaceRoadblock(unsigned manager,void*,unsigned pursuit,unsigned type,unsigned option,unsigned output){
 if((Active()||gauntlet_runtime::Duel())&&!SupportAllowed(pursuit))return false;
 candidateKnown=false;
 ++roadblockDepth;
 bool result=false;
 __try {result=originalRoadblock(manager,pursuit,type,option,output);}
 __finally {--roadblockDepth;}
 if(result&&(Active()||gauntlet_runtime::Duel()))CommitCandidate();
 return result;
}
void CancelledRoadblock(){Log("HOT_PURSUIT_ROADBLOCK_CANCELLED native_request_invalidated_during_placement=1 native_failure_event=1");}
// The native placement call can cancel its own request. The observed Main
// dump at 0043E7E8 read [NULL+8] after returning success. Take the existing
// failure-event path, before committing request state, when its owner vanished.
void __declspec(naked) CommitRoadblock(){
 __asm {
  pushfd
  mov ecx,[esi]
  test ecx,ecx
  jnz validRequest
  pushad
  mov ebp,esp
  sub esp,528
  and esp,0FFFFFFF0h
  fxsave [esp]
  call CancelledRoadblock
  fxrstor [esp]
  mov esp,ebp
  popad
  popfd
  push 0x43E807
  ret
 validRequest:
  popfd
  mov dword ptr [esi+0x78],2
  push 0x43E7E8
  ret
 }
}
}
