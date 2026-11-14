#pragma once
#include <string_view>
namespace mwel_carbon {
inline bool RpdScene(std::string_view name){return name=="HPR_INTERCEPTOR_ONLINE_10SEC"||name=="HPR_INTERCEPTOR_NO_SLOWMO"||name=="HPR_INTERCEPTOR_HANG_TOUGH";}
inline unsigned GridCount(std::string_view name,std::string_view prefix,unsigned maximum=8){
 if(name.size()!=prefix.size()+1||name.substr(0,prefix.size())!=prefix)return 0;
 const auto count=unsigned(name.back()-'0');return count>=1&&count<=maximum?count:0;
}
inline unsigned HprRaceCount(std::string_view name){
 if(name=="HPR_RACE_ROLLING_2")return 2;
 for(auto prefix:{"HPR_RACE_ALIGN_","HPR_RACE_ALIGN_ONLINE_","HPR_RACE_DRIVE_ONLINE_"})if(auto n=GridCount(name,prefix))return n>=2?n:0;
 return 0;
}
inline unsigned HprPursuitCount(std::string_view name){
 if(auto n=GridCount(name,"HPR_HOT_PURSUIT_ONLINE_",7))return n;
 return GridCount(name,"HPR_HOT_PURSUIT_GAUNTLET_",4);
}
inline bool HprRaceScene(std::string_view name){return HprRaceCount(name)!=0;}
inline bool HprLaunchScene(std::string_view name){return GridCount(name,"HPR_RACE_ALIGN_ONLINE_")>=2;}
inline unsigned RpdSprintFieldCount(std::string_view name){
 if(name=="HPR_RPD_SPRINT_8_REAR")return 9;
 for(const auto& family:{"HPR_RPD_SPRINT_ONLINE_","HPR_RPD_SPRINT_GAUNTLET_","HPR_RPD_SPRINT_DRIVE_"}){
  const std::string_view prefix(family);
  const unsigned max=prefix=="HPR_RPD_SPRINT_ONLINE_"?7:prefix=="HPR_RPD_SPRINT_GAUNTLET_"?4:8;
  if(const auto racers=GridCount(name,prefix,max);racers>=2)return racers+1;
 }
 return 0;
}
inline bool RpdRearOfficerScene(std::string_view name){return RpdSprintFieldCount(name)!=0;}
inline bool RpdSprintScene(std::string_view name){return RpdRearOfficerScene(name)||GridCount(name,"HPR_HOT_PURSUIT_ONLINE_",7)>=3||name=="HPR_RACE_ALIGN_8";}
inline bool HprPursuitScene(std::string_view name){return HprPursuitCount(name)!=0;}
inline bool HprGauntletScene(std::string_view name){return name=="HPR_GAUNTLET_CANNONBALL";}
inline bool HprScene(std::string_view name){return RpdScene(name)||HprRaceScene(name)||HprPursuitScene(name)||HprGauntletScene(name)||RpdRearOfficerScene(name);}
inline bool HprRaceMode(std::string_view mode){return mode=="sprint"||mode=="circuit"||mode=="lapknockout"||mode=="speedtraprace";}
inline bool HprRacerAllowed(std::string_view name,std::string_view mode,std::string_view combat){
 if(HprGauntletScene(name))return mode=="tollboothrace"&&combat=="gauntlet";
 if(HprPursuitScene(name))return HprRaceMode(mode)&&(combat=="armsrace"||combat=="hotpursuit");
 return HprRaceScene(name)&&HprRaceMode(mode)&&combat!="gauntlet"&&(combat!="drift"||HprLaunchScene(name));
}
inline unsigned Ug2Participants(std::string_view name){
 if(name=="UG2_INTRONIS20")return 6;
 for(auto candidate:{"UG2_INTRONIS01","UG2_INTRONIS02","UG2_INTRONIS03","UG2_INTRONIS04","UG2_INTRONIS05","UG2_INTRONIS06","UG2_INTRONIS07","UG2_INTRONIS08","UG2_INTRONIS10","UG2_INTRONIS11","UG2_INTRONIS12","UG2_INTRONIS13","UG2_INTRONIS15","UG2_INTRONIS16","UG2_INTRONIS17","UG2_INTRONIS18","UG2_INTRONIS19","UG2_INTRONIS23","UG2_INTRONIS25","UG2_INTRONIS27","UG2_INTRONIS28"})if(name==candidate)return 4;
 return 0;
}
inline unsigned Participants(std::string_view name){
 if(auto count=RpdSprintFieldCount(name))return count;
 if(RpdScene(name))return 2;
 if(auto count=HprRaceCount(name))return count;
 if(auto count=HprPursuitCount(name))return count;
 if(HprGauntletScene(name))return 1;
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
 if(HprScene(name)&&selection=="auto")return false;
 return selection!="stock"&&participants==authoredParticipants&&(selection=="auto"||selection==name);
}
}
