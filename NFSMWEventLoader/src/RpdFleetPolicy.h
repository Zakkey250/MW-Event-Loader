#pragma once
#include <cstdint>
#include <set>
#include <string>
namespace rpd_fleet_policy {
inline bool Enabled(bool lbReady,unsigned heat,unsigned uniqueModels){return lbReady&&heat>=1&&heat<=10&&uniqueModels>1;}
inline bool AtHeat(unsigned requested,unsigned registered,unsigned regulated){
 return requested>=1&&requested<=10&&(registered==requested||(regulated&(1u<<requested))!=0);
}
// Collection aliases remain eligible, but do not count as new visual models.
struct Inventory {
 std::set<unsigned> sources;std::set<std::string> models;
 bool Add(unsigned source,std::string model){
  sources.insert(source);
  for(auto& c:model)if(c>='a'&&c<='z')c=char(c-'a'+'A');
  return models.insert(std::move(model)).second;
 }
};
}
