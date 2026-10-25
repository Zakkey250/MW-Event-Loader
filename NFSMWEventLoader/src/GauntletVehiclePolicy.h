#pragma once
#include <string_view>
namespace gauntlet_vehicle {
inline bool SUV(std::string_view model){
 if(model.empty()||model.size()>63)return false;
 for(auto token:{std::string_view("SUV"),std::string_view("RHINO")})for(size_t i=0;i+token.size()<=model.size();++i){
  bool match=true;for(size_t j=0;j<token.size();++j){char c=model[i+j];if(c>='a'&&c<='z')c=char(c-'a'+'A');if(c!=token[j]){match=false;break;}}
  if(match)return true;
 }return false;
}
}
