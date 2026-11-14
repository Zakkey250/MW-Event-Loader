#pragma once
#include "CarbonIntroPolicy.h"
// A case file is a separate activity, never a correction of a career race.
namespace mwel_rpd {
inline constexpr char UndercoverPreset[]="MWEL_UNDERCOVER";
inline constexpr char UndercoverLabel[]="Undercover";
inline constexpr char InGameSelectedPreset[]="MWEL_IN_GAME_SELECTED";
inline constexpr char InGameSelectedLabel[]="In Game Selected";
inline bool InGameSelected(const mwel::Json& event){
 const auto value=event.find("policeVehicle");return value!=event.end()&&*value==InGameSelectedPreset;
}
inline bool Undercover(const mwel::Json& event){
 auto value=event.find("policeVehicle");return value!=event.end()&&*value==UndercoverPreset;
}
inline bool CaseFile(const mwel::Json& event){
 auto value=event.find("campaign");return value!=event.end()&&value->is_string()&&*value=="rpd";
}
inline bool ValidRank(unsigned rank){return rank>=1&&rank<=15;}
inline bool ListedAt(const mwel::Json& event,unsigned rank){
 if(!ValidRank(rank)||!CaseFile(event))return false;
 auto authored=event.find("blacklistNo");return authored!=event.end()&&authored->is_number_integer()&&*authored==rank;
}
inline bool CanLaunchFromBlacklist(const mwel::Json& event,bool safehouse,unsigned sheetRank,unsigned activeRank){
 return safehouse&&sheetRank==activeRank&&ListedAt(event,sheetRank);
}
inline bool Armed(const mwel::Json& event){return CaseFile(event)&&event.value("combatMode",std::string("none"))=="hotpursuit";}
inline unsigned Allies(const mwel::Json& event);
inline bool FreeRun(const mwel::Json& event){return event.value("policeRule",std::string("freerun"))=="freerun";}
inline bool PoliceSprint(const mwel::Json& event){
 auto mission=event.find("policeMission"),rule=event.find("policeRule"),mode=event.find("mode");
 return mission!=event.end()&&*mission=="hotpursuit"&&rule!=event.end()&&*rule=="sprint"&&
  mode!=event.end()&&*mode=="sprint"&&Armed(event);
}
inline bool OfficerHandoff(const mwel::Json& event){
 const auto value=event.find("policeHandoff");
 return !PoliceSprint(event)&&(value==event.end()||!value->is_boolean()||value->get<bool>());
}
inline unsigned Allies(const mwel::Json& event){
 if(!PoliceSprint(event))return event.value("policeAllies",0u);
 const auto slots=event.value("racerCount",0u);
 return slots>=3&&slots<=9?slots-2:0;
}
inline bool ArmedSuspectSlot(const mwel::Json& event,unsigned slot,unsigned driver){
 return driver==3&&slot>0&&slot<16&&(slot==1?Armed(event):PoliceSprint(event));
}
enum class Outcome {Running,Arrested,PlayerDestroyed,TimedOut,Escaped};
inline Outcome Decide(bool playerDestroyed,bool suspectDestroyed,bool timeExpired,bool arrested=false,bool escaped=false){
 if(playerDestroyed)return Outcome::PlayerDestroyed;
 if(timeExpired)return Outcome::TimedOut;
 if(suspectDestroyed||arrested)return Outcome::Arrested;
 return escaped?Outcome::Escaped:Outcome::Running;
}
inline void Validate(const mwel::Json& event){
 if(!event.contains("campaign"))return;
 if(!event.at("campaign").is_string())throw std::runtime_error("Invalid campaign");
 const auto campaign=event.at("campaign").get<std::string>();
 if(campaign.empty()||campaign=="racer")return;
 if(campaign!="rpd")throw std::runtime_error("Unknown campaign");
 const bool pursuit=PoliceSprint(event);
 if(mwel::Correction(event)||event.at("mode")!="sprint"||event.at("cops")!=false||
    (pursuit?(event.at("racerCount")<3||event.at("racerCount")>9||!mwel::NeoEnabled(event)):
    (event.value("policeMission",std::string())!="interceptor"||event.at("racerCount")!=2||
    (event.value("combatMode",std::string("none"))!="none"&&!Armed(event)))
    ))throw std::runtime_error("Invalid RPD mission, roster or route");
 // Native registration resolves owned presets against real installed police
 // collections and stock parts. An arbitrary preset name is never a loan car.
 const auto preset=event.value("policeVehicle",std::string());
 const bool owned=preset.size()==17&&preset.rfind("MWEL_COP_",0)==0&&std::all_of(preset.begin()+9,preset.end(),[](char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F');});
 if(preset!="CS_CAR_COPGTO"&&preset!=UndercoverPreset&&preset!=InGameSelectedPreset&&!owned)throw std::runtime_error("Unsupported RPD loan vehicle");
 if(InGameSelected(event)){
  const auto heat=event.find("startHeatLevel");
  if(heat==event.end()||!heat->is_number_integer()||*heat<1||*heat>10)throw std::runtime_error("In Game Selected requires a start heat of 1-10");
 }
 if(event.contains("policeHandoff")&&!event.at("policeHandoff").is_boolean())throw std::runtime_error("Invalid RPD handoff setting");
 if(Undercover(event)&&OfficerHandoff(event))throw std::runtime_error("Undercover requires Interceptor handoff OFF");
 const auto intro=event.value("carbonIntro",std::string());
 if(pursuit?(!mwel_carbon::RpdSprintScene(intro)||!mwel_carbon::Valid(intro,event.at("racerCount").get<unsigned>())):!mwel_carbon::RpdScene(intro))throw std::runtime_error("RPD requires a matching HP reproduction intro");
 if(!Armed(event))return; // Read the earlier private menu-only prototype.
 if(event.value("policeRule",std::string("freerun"))!="freerun"&&event.at("policeRule")!="sprint")throw std::runtime_error("Invalid RPD rule");
 for(const auto& item:std::array<std::pair<const char*,unsigned>,3>{{{"policeSuspectHP",100000},{"policeTimeLimit",86399},{"policeAllies",16}}}){
  if(!event.contains(item.first))throw std::runtime_error("Missing RPD setting");
  const auto& value=event.at(item.first);
  if(!value.is_number_integer()||value.get<long long>()<0||value.get<long long>()>item.second||(std::string(item.first)!="policeAllies"&&value==0))throw std::runtime_error("Invalid RPD numeric setting");
 }
 for(const char* name:{"policeAutoRoadblock","policeAutoHelicopter"})if(!event.contains(name)||!event.at(name).is_boolean())throw std::runtime_error("Invalid RPD support setting");
}
}
