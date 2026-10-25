#pragma once
#include <cstdint>
namespace gauntlet_bootstrap {
// A native void request is not an acknowledgement that any car was admitted.
// Retry only an empty, drained queue, with room in the ordinary chase budget.
inline bool Retry(bool requested,bool leader,bool handoff,unsigned pending,unsigned active,unsigned limit,
                  std::uint64_t now,std::uint64_t next){
 return requested&&!leader&&!handoff&&!pending&&!active&&limit&&now>=next;
}
}
