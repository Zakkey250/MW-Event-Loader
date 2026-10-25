#pragma once
#include <array>
#include <cstddef>
#include <limits>

namespace mwel_lookup {
// Cache indices, never addresses: database reconstruction may recycle keys
// and move/replace entries. Every hit validates the current collection key.
// Misses are not cached because registration can append a collection later.
template<std::size_t Slots=64> class IndexCache {
 std::array<std::size_t,Slots> indices;
public:
 IndexCache(){indices.fill(std::numeric_limits<std::size_t>::max());}
 template<class Entries> auto Find(Entries& entries,unsigned key)->decltype(entries[0].get()){
  auto& saved=indices[key%Slots];
  if(saved<entries.size()&&entries[saved]&&entries[saved]->collection.key==key)return entries[saved].get();
  for(std::size_t i=0;i<entries.size();++i)if(entries[i]&&entries[i]->collection.key==key){saved=i;return entries[i].get();}
  return nullptr;
 }
};
}
