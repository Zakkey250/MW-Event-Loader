#pragma once
#include <cstdint>
#include <cstring>

// MW 1.3: WCollisionMgr +0x90 is ISimable::GetWorldID(), +0x94 is
// the native exclusion mask. 0x7844F0 tests barrier byte +0x0d against
// the mask's low byte. Apply this only to event guards, never world walls.
namespace mw_race_barriers {
constexpr unsigned UnknownDriver=~0u;
struct Query {unsigned exclusions=0,driver=UnknownDriver,worldId=0;};
inline bool Collides(unsigned flags,const Query& query){
    if(query.driver==1||query.driver==2)return false; // Traffic, Cop.
    return (query.exclusions&((flags>>8)&0xff))==0;
}
#ifdef _WIN32
// No cached vehicle pointers: a race exit can recycle both handles and bodies.
// IVehicle's verified GetSimable (0x688030) returns vehicle-0x80;
// ISimable::GetWorldID (0x686AF0) reads +0x60. DriverClass reads +0x94.
inline Query ForCollision(const void* manager,const void* list=reinterpret_cast<const void*>(0x92CD1C)) noexcept {
    Query query;
    __try {
        const auto data=static_cast<const unsigned char*>(manager);
        std::memcpy(&query.worldId,data+0x90,4);
        std::memcpy(&query.exclusions,data+0x94,4);
        if(!query.worldId)return query;
        const auto storage=static_cast<const unsigned char*>(list);
        const auto vehicles=*reinterpret_cast<unsigned char* const* const*>(storage);
        const unsigned capacity=*reinterpret_cast<const unsigned*>(storage+4);
        const unsigned count=*reinterpret_cast<const unsigned*>(storage+8);
        if(!vehicles||count>capacity||count>8192)return query;
        for(unsigned i=0;i<count;++i){
            const auto vehicle=vehicles[i];
            if(!vehicle||*reinterpret_cast<const unsigned*>(vehicle)!=0x8AA828)continue;
            if(*reinterpret_cast<const unsigned*>(vehicle-0x20)!=query.worldId)continue;
            query.driver=*reinterpret_cast<const unsigned*>(vehicle+0x94);
            break;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return query;
}
#endif
}
