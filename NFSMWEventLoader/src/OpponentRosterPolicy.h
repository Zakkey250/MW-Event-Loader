#pragma once
#include <algorithm>
#include <vector>
namespace mwel_roster {
inline void AppendUnique(std::vector<unsigned>& roster,const std::vector<unsigned>& candidates,unsigned needed){
 for(unsigned key:candidates){if(roster.size()>=needed)return;if(key&&std::find(roster.begin(),roster.end(),key)==roster.end())roster.push_back(key);}
}
}
