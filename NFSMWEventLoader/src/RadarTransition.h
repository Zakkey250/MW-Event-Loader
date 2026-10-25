#pragma once
#include <string>
#include <cstring>
namespace event_radar {
inline void ResetCompressedIndex(void* manager){
 *static_cast<unsigned*>(manager)=0;
 std::memset(static_cast<unsigned char*>(manager)+0x48,0,64*sizeof(unsigned));
}
// Race records may outlive the race. The native map selector checks play mode
// before examining that record; package selection must use the same boundary.
inline bool UseCourse(int mapMode,unsigned playMode,unsigned key){return mapMode!=2&&playMode==1&&key!=0;}
enum class RestoreResult {Skipped,Unavailable,Queued};
template<class Backend>RestoreResult Restore(Backend& native,bool roaming,bool ownedPrefix){
 if(!roaming||!ownedPrefix)return RestoreResult::Skipped;
 std::string prefix,path;
 // Resolve the native unlock/region map before dismantling the current owner.
 if(!native.Query(prefix,path))return RestoreResult::Unavailable;
 // Native chunk registration is append/remove, not an independent second
 // registry. Remove decompressed tiles and the old TPK before loading another.
 native.ClearTiles();native.Release();native.ResetIndex();native.SetPrefix(prefix);
 return native.Load(path)?RestoreResult::Queued:RestoreResult::Unavailable;
}
}
