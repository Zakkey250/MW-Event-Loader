#pragma once
#include <Windows.h>
#include <cstring>
#include <cstdint>
// Query the same native accessors used by the game/Unlimiter. The original
// 0x9B09D8 table and 84-entry count are stale after add-on cars are installed.
// Callers still validate the executable and contain native faults with SEH.
namespace mwcartype {
inline int FindPlayable(const char* model) {
 if(!model||!model[0])return -1;
 unsigned hash=~0u;unsigned length=0;
 for(;model[length]&&length<32;++length)hash=hash*33+static_cast<unsigned char>(model[length]);
 if(!length||length>=32)return -1;
 auto entry=reinterpret_cast<const unsigned char*(__cdecl*)(unsigned)>(0x739940)(hash);
 if(!entry)return -1;
 char name[16]{};std::memcpy(name,entry,sizeof(name));
 if(!std::memchr(name,0,sizeof(name))||_stricmp(name,model)||entry[0x94]!=0)return -1;
 unsigned type=0;std::memcpy(&type,entry+0x90,4);if(type>=1024)return -1;
 auto byIndex=reinterpret_cast<const unsigned char*(__cdecl*)(unsigned)>(0x739920)(type);
 return byIndex==entry?static_cast<int>(type):-1;
}
}
