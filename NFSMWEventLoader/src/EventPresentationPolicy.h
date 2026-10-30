#pragma once
#include <string>
namespace event_presentation_policy {
inline bool Supported(const std::string& combat){return combat=="armsrace"||combat=="gauntlet"||combat=="hotpursuit"||combat=="drift";}
inline std::wstring Name(const std::string& combat,const std::string& rules,bool japanese){
 if(!Supported(combat))return {};
 if(combat=="gauntlet")return japanese?L"\u30ac\u30f3\u30c8\u30ec\u30c3\u30c8":L"Gauntlet";
 if(combat=="drift")return japanese?L"\u30c9\u30ea\u30d5\u30c8":L"Drift";
 std::wstring name=combat=="hotpursuit"?(japanese?L"\u30db\u30c3\u30c8\u30d1\u30fc\u30b9\u30fc\u30c8 - ":L"Hot Pursuit - "):(japanese?L"\u30a2\u30fc\u30e0\u30ba\u30ec\u30fc\u30b9 - ":L"Arms Race - ");
 if(rules=="sprint")return name+(japanese?L"\u30b9\u30d7\u30ea\u30f3\u30c8":L"Sprint");
 if(rules=="circuit")return name+(japanese?L"\u30b9\u30c8\u30ea\u30fc\u30c8":L"Street");
 if(rules=="lapknockout")return name+(japanese?L"\u30e9\u30c3\u30d7\u30ce\u30c3\u30af\u30a2\u30a6\u30c8":L"Lap Knockout");
 if(rules=="speedtraprace")return name+(japanese?L"\u30b9\u30d4\u30fc\u30c9\u30c8\u30e9\u30c3\u30d7":L"Speed Trap");
 return {};
}
}
