#pragma once
#include <cstddef>
namespace mw_native_speech {
// Disabled/unresolved native event declarations can have an empty name. The
// native dispatcher hashes that pointer without checking it (00713B9D).
// Reject only the unavailable declaration; do not patch global string hashing
// or rewrite audio attributes, registration state or the caller's payload.
template<class Read> bool Available(const void* event,Read read){
 const char* name=nullptr;char first=0;
 return event&&read(event,&name,sizeof(name))&&name&&read(name,&first,1)&&first!=0;
}
}
