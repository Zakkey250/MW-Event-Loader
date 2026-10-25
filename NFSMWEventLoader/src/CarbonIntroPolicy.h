#pragma once
#include <string_view>
namespace mwel_carbon {
inline unsigned Ug2Participants(std::string_view name){
 if(name=="UG2_INTRONIS20")return 6;
 for(auto candidate:{"UG2_INTRONIS01","UG2_INTRONIS02","UG2_INTRONIS03","UG2_INTRONIS04","UG2_INTRONIS05","UG2_INTRONIS06","UG2_INTRONIS07","UG2_INTRONIS08","UG2_INTRONIS10","UG2_INTRONIS11","UG2_INTRONIS12","UG2_INTRONIS13","UG2_INTRONIS15","UG2_INTRONIS16","UG2_INTRONIS17","UG2_INTRONIS18","UG2_INTRONIS19","UG2_INTRONIS23","UG2_INTRONIS25","UG2_INTRONIS27","UG2_INTRONIS28"})if(name==candidate)return 4;
 return 0;
}
inline unsigned Participants(std::string_view name){
 if(auto count=Ug2Participants(name))return count;
 if(name=="INTRO02NG02")return 8;
 if(name=="INTRO12B02NG")return 4;
 if(name=="INTRO12NG")return 4;
 if(name=="INTRODEFAULT")return 4;
 if(name=="INTRO01NG02"||name=="INTRO04NG02"||name=="INTRO05NG02")return 8;
 if(name=="INTRO07B02NG"||name=="INTRO11B02NG")return 4;
 return 0;
}
inline bool SceneId(std::string_view name){
 if(name.empty()||name.size()>64||name=="auto"||name=="stock")return false;
 for(char c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
 return true;
}
inline bool Valid(std::string_view name,unsigned participants){
 if(name=="auto"||name=="stock")return true;
 const auto count=Participants(name);return SceneId(name)&&participants>=1&&participants<=16&&(!count||count==participants);
}
inline bool Matches(std::string_view selection,std::string_view name,unsigned participants,unsigned authoredParticipants){
 return selection!="stock"&&participants==authoredParticipants&&(selection=="auto"||selection==name);
}
}
