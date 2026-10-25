#pragma once
#include "../World2MW/src/NativeLoader/CheckpointCode.h"
#include "MWCheckpointRuntimeApi.h"
namespace mw_checkpoint {
// A single native GRaceStatus is allocated by 00603900. Its existing ABI and
// offsets stay intact; extra checkpoint times/segment lengths live in its tail.
// Route vectors are shared only by its synchronous, main-thread calculator.
static float checkpointRoutePositions[CheckpointLimit+2][4]{};
static float checkpointRouteDirections[CheckpointLimit+2][4]{};
static bool checkpointExtensionActive=false;
static BYTE* checkpointCalculatorEntry=nullptr;
template<class Logger>static bool InstallCheckpointExtension(HMODULE other,const char* symbol,Logger Log){
 if(checkpointExtensionActive)return true;
 auto query=other?reinterpret_cast<Query>(GetProcAddress(other,symbol)):nullptr;Runtime shared{};
 if(query&&query(&shared,sizeof(shared))){
  auto entry=reinterpret_cast<BYTE*>(0x5FEF30);
  if(shared.size!=sizeof(shared)||shared.segmentsOffset!=CheckpointSegmentsOffset||shared.capacity!=CheckpointLimit+2||shared.raceBytes!=CheckpointRaceBytes||entry[0]!=0xE9||entry+5+*reinterpret_cast<int*>(entry+1)!=shared.calculator){Log("CHECKPOINT_EXTENSION_REFUSED shared_ABI");return false;}
  for(auto& patch:CheckpointPatches)if(memcmp(reinterpret_cast<void*>(patch.address),patch.replacement,patch.size)){Log("CHECKPOINT_EXTENSION_REFUSED shared_patch address=%08X",patch.address);return false;}
  checkpointCalculatorEntry=static_cast<BYTE*>(shared.calculator);checkpointExtensionActive=true;Log("CHECKPOINT_EXTENSION_SHARED owner=%s duplicate_allocation=0",symbol);return true;
 }
 if(*reinterpret_cast<void**>(0x91E000)) {Log("CHECKPOINT_EXTENSION_REFUSED race_already_allocated");return false;}
 if(memcmp(reinterpret_cast<void*>(0x5FEF30),CheckpointCalculatorOriginal,sizeof(CheckpointCalculatorOriginal))){Log("CHECKPOINT_EXTENSION_REFUSED calculator_guard");return false;}
 for(auto& p:CheckpointPatches)if(memcmp(reinterpret_cast<void*>(p.address),p.original,p.size)){Log("CHECKPOINT_EXTENSION_REFUSED patch_guard address=%08X",p.address);return false;}
 auto code=static_cast<BYTE*>(VirtualAlloc(nullptr,sizeof(CheckpointCalculatorCode),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 if(!code)return false;
 memcpy(code,CheckpointCalculatorCode,sizeof(CheckpointCalculatorCode));
 for(auto& r:CheckpointCalls)*reinterpret_cast<unsigned*>(code+r.offset)=r.target-unsigned(code+r.offset+4);
 for(auto& r:CheckpointBuffers)*reinterpret_cast<unsigned*>(code+r.offset)=unsigned(r.buffer?checkpointRouteDirections:checkpointRoutePositions)+r.delta;
 DWORD old=0;
 if(!VirtualProtect(code,sizeof(CheckpointCalculatorCode),PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return false;}
 // Obtain write access once before any change so protection failures are atomic.
 auto begin=reinterpret_cast<BYTE*>(0x5DC000);constexpr unsigned bytes=0x28000;
 if(!VirtualProtect(begin,bytes,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(code,0,MEM_RELEASE);return false;}
 for(auto& p:CheckpointPatches)memcpy(reinterpret_cast<void*>(p.address),p.replacement,p.size);
 auto entry=reinterpret_cast<BYTE*>(0x5FEF30);entry[0]=0xE9;*reinterpret_cast<unsigned*>(entry+1)=unsigned(code)-unsigned(entry+5);
 DWORD ignored=0;VirtualProtect(begin,bytes,old,&ignored);
 FlushInstructionCache(GetCurrentProcess(),begin,bytes);FlushInstructionCache(GetCurrentProcess(),code,sizeof(CheckpointCalculatorCode));
 checkpointCalculatorEntry=code;checkpointExtensionActive=true;Log("CHECKPOINT_EXTENSION_ACTIVE limit=256 route_vectors=258 timing_racers=16 timing_laps=10 native_path_search=1");return true;
}
static unsigned QueryRuntime(Runtime* out,unsigned size){if(!checkpointExtensionActive||!out||size!=sizeof(*out))return 0;*out={sizeof(*out),CheckpointSegmentsOffset,CheckpointLimit+2,CheckpointRaceBytes,checkpointCalculatorEntry};return 1;}
}
