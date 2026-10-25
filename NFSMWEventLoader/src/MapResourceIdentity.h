#pragma once
#include <string>

namespace event_resource {
// Event IDs and attribute keys remain byte-exact. Native radar texture names
// are a separate, uppercase ASCII resource namespace used by the exporter.
inline std::string Token(std::string id){
 for(char& c:id){if(c=='.')c='_';else if(c>='a'&&c<='z')c=char(c-'a'+'A');}
 return id;
}
inline std::string RadarPrefix(const std::string& id){return "MINI_MAP_"+Token(id);}
}
