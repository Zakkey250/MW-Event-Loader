#pragma once
#include <cstdint>

namespace touge_speech_lifetime {
// Verified EN 1.3 GameSpeech (00716D20), not the smaller NISSFXModule.
// 006FF740 detaches its definition at +44h without retiring the secondary
// sound at +98h. 006FF9E0 subsequently reads that missing definition.
// Match the ownership and native deleting destructor used at 0070F435.
template<class Read,class Write,class Destroy>
bool Retire(bool returning,std::uintptr_t self,std::uintptr_t owner,
            bool definitionDetached,Read read,Write write,Destroy destroy){
 if(!returning||!definitionDetached||!self||self!=owner||read(self)!=0x8B24F0)return false;
 const auto handle=read(self+0x98);
 if(!handle||read(handle)!=0x896AD0)return false;
 // Detach first so a synchronous stop callback cannot retire it twice.
 write(self+0x98,0);
 destroy(handle);
 return true;
}
}
