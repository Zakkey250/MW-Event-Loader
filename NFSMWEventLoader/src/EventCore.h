#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <climits>
#include <regex>
#include <stdexcept>
#include <string>
#include <cmath>
#include <cctype>
#include <nlohmann/json.hpp>
#include "NativeSaveFilter.h"
#include "CarbonIntroPolicy.h"
#include "EventPreviewStatsPolicy.h"
#include "../../integration/EventLoaderPaths.h"
#include "../../integration/MWCombatContract.h"
#pragma comment(lib,"bcrypt.lib")

namespace mwel {
namespace fs=std::filesystem;
using Json=nlohmann::json;
// Optional editor switches are opt-in, even in older verified packages.
inline bool Feature(const Json& event,const char* name){auto v=event.find(name);return v!=event.end()&&v->is_boolean()&&v->get<bool>();}
// Interceptor free-run has no course objective. Other event modes keep the
// normal start/checkpoint/finish contract.
inline bool RpdFreeRun(const Json& e){return e.value("campaign",std::string())=="rpd"&&e.value("policeMission",std::string())=="interceptor"&&e.value("policeRule",std::string("freerun"))=="freerun";}
inline bool RpdStartOnly(const Json& e){return RpdFreeRun(e)&&e.contains("gates")&&e.at("gates").is_array()&&e.at("gates").size()==1;}
// Original MW prototype layout, authored in metres. No imported trajectory.
// Legacy private drafts migrate to this layout without retaining HPR samples.
inline Json RpdRacerStart(const Json& e){
 const auto layout=e.value("policeStartLayout",std::string("MW_RPD_STANDARD_V1"));
 if(layout!="MW_RPD_STANDARD_V1"&&!mwel_carbon::RpdScene(layout))throw std::runtime_error("RPD start layout");
 auto start=e.at("gates").at(0);auto p=start.at("position");const double angle=start.at("rotation").get<double>()*3.14159265358979323846/180.;
 const auto intro=e.value("carbonIntro",std::string());
 double u=0,v=40,yaw=0;
 if(intro=="HPR_INTERCEPTOR_ONLINE_10SEC"){u=-.35026061524887364;v=80.23511154740417;yaw=-.6006327013952324;}
 if(intro=="HPR_INTERCEPTOR_HANG_TOUGH"){u=.37101468812240945;v=50.37788771987691;yaw=1.7018449080116227;}
 if(intro=="HPR_INTERCEPTOR_NO_SLOWMO"){u=.9749354270640329;v=79.31227113410291;yaw=-.9424943444149392;}
 start["position"]=Json::array({p.at(0).get<double>()+std::sin(angle)*u+std::cos(angle)*v,p.at(1).get<double>()-std::cos(angle)*u+std::sin(angle)*v,p.at(2).get<double>()});
 start["rotation"]=start.at("rotation").get<double>()+yaw;return start;
}
inline bool NeoEnabled(const Json& event){auto v=event.find("aiRouteMode");return v!=event.end()&&v->is_string()&&*v=="yellow";}
// Drift owns a police-free race, even when an imported package retains old
// police or starting-heat values. Normalize the in-memory copy only.
inline void ApplyFixedRaceRules(Json& event){
 const auto mode=event.value("combatMode",std::string("none"));
 if(mode=="drift"||mode=="touge"){
  event["cops"]=false;event["copDensity"]=0;event["startHeatLevel"]=nullptr;
  if(mode=="touge")event["aiRouteMode"]="yellow";
 }
}
// Only circuit and knockout modes own laps. A copied authoring value must
// never turn a sprint-derived speed-trap race into a multi-lap HUD/distance.
inline unsigned RaceLaps(const Json& event){const auto mode=event.at("mode").get<std::string>();return mode=="circuit"||mode=="lapknockout"?event.at("laps").get<unsigned>():1u;}
inline constexpr size_t MaximumPackageBytes=16*1024*1024;
inline std::string Digest(const std::string& bytes){
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};unsigned char digest[32]{};
 bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),ULONG(bytes.size()),0)>=0;
 if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
 if(!ok)throw std::runtime_error("SHA-256 unavailable");
 std::string result;constexpr char hex[]="0123456789ABCDEF";
 for(auto value:digest){result+=hex[value>>4];result+=hex[value&15];}return result;
}
inline std::string Read(const fs::path& path,size_t limit=MaximumPackageBytes){
 if(!fs::is_regular_file(path)||fs::is_symlink(path))throw std::runtime_error("Not an ordinary file");
 auto size=fs::file_size(path);if(!size||size>limit)throw std::runtime_error("File size limit");
 std::ifstream stream(path,std::ios::binary);std::string bytes(size,'\0');
 if(!stream.read(bytes.data(),std::streamsize(size)))throw std::runtime_error("Read failed");return bytes;
}
inline bool EventId(const std::string& id){
 static const std::regex pattern("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
 return id!="00000000-0000-0000-0000-000000000000"&&std::regex_match(id,pattern);
}
inline bool Sha256(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789ABCDEF")==std::string::npos;}
inline int Integer(const Json& object,const char* key,int low,int high){
 const auto& value=object.at(key);if(!value.is_number_integer())throw std::runtime_error(std::string("Integer required: ")+key);
 auto result=value.get<int64_t>();if(result<low||result>high)throw std::runtime_error(std::string("Range: ")+key);return int(result);
}
inline double Number(const Json& object,const char* key,double low,double high){
 const auto& value=object.at(key);if(!value.is_number())throw std::runtime_error(std::string("Number required: ")+key);
 auto result=value.get<double>();if(!std::isfinite(result)||result<low||result>high)throw std::runtime_error(std::string("Range: ")+key);return result;
}
inline bool Correction(const Json& authored){return authored.value("authoringMode",std::string("additional"))=="correction";}
inline void ValidateOperation(const Json& authored){
 auto mode=authored.value("authoringMode",std::string("additional"));
 if(mode!="additional"&&mode!="correction")throw std::runtime_error("Invalid authoring mode");
 if(mode=="additional"){if(authored.contains("correctionTarget"))throw std::runtime_error("Additional replacement target refused");return;}
 const auto& target=authored.at("correctionTarget");
 auto hash=authored.at("hash").get<std::string>();std::transform(hash.begin(),hash.end(),hash.begin(),[](unsigned char c){return char(std::toupper(c));});
 static const std::regex token("[A-Za-z0-9_.-]{1,9}");
 if(!target.is_object()||target.size()!=4||target.at("name")!=authored.at("sourceEventName")||target.at("eventId")!=authored.at("sourceEventId")||target.at("hash")!=hash||hash.size()!=8||hash.find_first_not_of("0123456789ABCDEF")!=std::string::npos||!std::regex_match(target.at("eventId").get<std::string>(),token))throw std::runtime_error("Correction target mismatch");
 static const std::set<std::string> modes={"sprint","circuit","lapknockout","drag","speedtraprace","tollboothrace"};
 if(!modes.count(target.at("mode").get<std::string>()))throw std::runtime_error("Correction source mode invalid");
}
struct EventPackage {std::string id,worldSnapshotId,nativeNamespace,nativeEventId;unsigned revision=0;Json authoring,assets=Json::object();fs::path directory;};
inline bool AssetName(const std::string& name){
 static const std::set<std::string> names={"maps/map.json","maps/radar.bin","maps/preview/textures.bin","maps/preview/meta.json","course/walls.wmw","course/wall-models.json","course/ai-route.bin","course/ai-native.bin","course/ai-branches.bin","course/ai-recovery.bin","course/ai-obstacles.bin","course/shortcut-route.bin","course/opponents.ini"};
 static const std::regex texture("course/textures/[0-9A-F]{8}\\.dds");return names.count(name)||std::regex_match(name,texture);
}
inline EventPackage ParsePackage(const fs::path& directory){
 if(fs::is_symlink(directory))throw std::runtime_error("Linked package directory refused");
 auto manifest=Json::parse(Read(directory/L"manifest.json",65536));
 auto raw=Read(directory/L"event.json");auto package=Json::parse(raw);
 EventPackage result;result.id=package.at("id").get<std::string>();
 if(!EventId(result.id)||directory.filename().string()!=result.id||manifest.at("id")!=result.id||
    manifest.at("schema")!=1||package.at("schema")!=1||!manifest.at("files").is_object()||manifest.at("files").size()>4096||
    manifest.at("files").at("event.json").get<std::string>()!=Digest(raw))throw std::runtime_error("Package identity or integrity");
 result.directory=directory;
 for(const auto& item:manifest.at("files").items())if(item.key()!="event.json"){
  if(!AssetName(item.key())||!item.value().is_string()||!Sha256(item.value().get<std::string>()))throw std::runtime_error("Invalid asset manifest");
  auto path=directory/fs::u8path(item.key());for(auto parent=path.parent_path();parent!=directory&&parent.has_relative_path();parent=parent.parent_path())if(fs::is_symlink(parent))throw std::runtime_error("Linked asset directory");
  if(Digest(Read(path,128*1024*1024))!=item.value().get<std::string>())throw std::runtime_error("Asset integrity mismatch");result.assets[item.key()]=item.value();
 }
 result.revision=Integer(package,"revision",1,INT_MAX);
 if(manifest.at("revision")!=result.revision)throw std::runtime_error("Package revision mismatch");
 result.worldSnapshotId=package.at("worldSnapshotId").get<std::string>();
 if(!Sha256(result.worldSnapshotId))throw std::runtime_error("World snapshot identity");
 auto compact=result.id;compact.erase(std::remove(compact.begin(),compact.end(),'-'),compact.end());
 result.nativeNamespace="mwel_"+compact;
 if(package.at("nativeNamespace")!=result.nativeNamespace)throw std::runtime_error("Native namespace mismatch");
 // GEvent's metadata stores at most nine EventID characters. Keep the full
 // UUID in collection ownership and sidecars, and use a bounded native token.
 result.authoring=package.at("authoring");ValidateOperation(result.authoring);
 result.nativeEventId=Correction(result.authoring)?result.authoring.at("correctionTarget").at("eventId").get<std::string>():"E"+Digest(result.id).substr(0,8);
 if(package.at("nativeEventId")!=result.nativeEventId)throw std::runtime_error("Native event token mismatch");
 result.authoring=package.at("authoring");const auto& authored=result.authoring;
 if(authored.at("customEventId")!=result.id||!authored.at("title").is_string()||authored.at("title").get<std::string>().empty()||authored.at("title").get<std::string>().size()>1024)throw std::runtime_error("Authoring identity or title");
 static const std::set<std::string> modes={"sprint","circuit","lapknockout","drag","speedtraprace","tollboothrace"};
 auto mode=authored.at("mode").get<std::string>();if(!modes.count(mode))throw std::runtime_error("Unsupported mode");
 if(authored.contains("combatMode")&&!authored.at("combatMode").is_string())throw std::runtime_error("Invalid combat mode type");
 const auto combat=mwcombat::Parse(authored.value("combatMode",std::string("none")).c_str());
 if(combat==mwcombat::Mode::Touge&&(!authored.contains("worldCompatibility")||authored.at("worldCompatibility").value("kind",std::string())!="w2c"))throw std::runtime_error("Touge requires W2C world identity");
 const auto nativeCount=unsigned(Integer(authored,"racerCount",1,16));
 const bool rpdField=authored.value("campaign",std::string())=="rpd"&&authored.value("policeMission",std::string())=="hotpursuit";
 if(!mwcombat::Compatible(combat,mode.c_str(),rpdField&&nativeCount>=3?nativeCount-1:nativeCount))throw std::runtime_error("Combat mode and base race are incompatible");
 if(authored.contains("combatAmmo")){
  const auto& ammo=authored.at("combatAmmo");if(!ammo.is_object()||ammo.size()>5)throw std::runtime_error("Invalid event ammunition");
  for(const auto& item:ammo.items()){
   static const std::set<std::string> weapons={"emp","turbo","spike","shockwave","jammer"};
   if(!weapons.count(item.key()))throw std::runtime_error("Unknown event weapon");Integer(ammo,item.key().c_str(),0,mwcombat::AmmoLimit);
  }
 }
 if(authored.contains("policeRacerWeapons")){
  const auto& weapons=authored.at("policeRacerWeapons");
  if(authored.value("campaign",std::string())!="rpd"||combat!=mwcombat::Mode::HotPursuit||!weapons.is_object()||weapons.size()>4)
   throw std::runtime_error("Invalid RPD racer weapon settings");
  for(const auto& item:weapons.items()){
   const auto& row=item.value();
   const auto known=std::any_of(std::begin(mwcombat::RacerWeaponNames),std::end(mwcombat::RacerWeaponNames),[&](auto name){return item.key()==name;});
   if(!known||!row.is_object()||row.size()!=row.count("ammo")+row.count("cooldown"))throw std::runtime_error("Invalid RPD racer weapon");
   if(row.contains("ammo"))Integer(row,"ammo",0,mwcombat::AmmoLimit);
   if(row.contains("cooldown"))Integer(row,"cooldown",1,600);
  }
 }
 if(!Correction(authored)||!authored.contains("blacklistNo")||!authored.at("blacklistNo").is_null())Integer(authored,"blacklistNo",1,15);
 Integer(authored,"cashValue",0,INT_MAX);Integer(authored,"racerCount",1,16);Integer(authored,"laps",1,10);
 if(authored.contains("carbonIntro")&&!authored.at("carbonIntro").is_string())throw std::runtime_error("Invalid intro selection type");
 const auto intro=authored.value("carbonIntro",std::string("auto"));
 if(!mwel_carbon::Valid(intro,authored.at("racerCount").get<unsigned>()))throw std::runtime_error("Intro selection does not match participants");
 if(mwel_carbon::RpdScene(intro)&&!(authored.value("campaign",std::string())=="rpd"&&authored.value("policeMission",std::string())=="interceptor"))throw std::runtime_error("Police rolling intro requires an Interceptor event");
 const bool policeSprint=authored.value("campaign",std::string())=="rpd"&&authored.value("policeMission",std::string())=="hotpursuit"&&mode=="sprint"&&mwel_carbon::RpdSprintScene(intro);
 if(mwel_carbon::HprScene(intro)&&!mwel_carbon::RpdScene(intro)&&!policeSprint&&(authored.value("campaign",std::string())=="rpd"||!mwel_carbon::HprRacerAllowed(intro,mode,authored.value("combatMode",std::string("none")))))throw std::runtime_error("HP intro is incompatible with this event mode");
 if(package.contains("carbonIntro")&&package.at("carbonIntro")!=intro)throw std::runtime_error("Intro selection mirror mismatch");
 if(mode=="lapknockout"&&authored.at("laps").get<int>()<std::max(1,authored.at("racerCount").get<int>()-1))throw std::runtime_error("Knockout laps must cover all eliminations");
 Number(authored,"traffic",0,100);Number(authored,"copDensity",0,100);
 if(authored.contains("startHeatLevel")&&!authored.at("startHeatLevel").is_null())Integer(authored,"startHeatLevel",0,10);
 if(authored.contains("checkpointsVisible")&&!authored.at("checkpointsVisible").is_boolean())throw std::runtime_error("Checkpoint display setting");
 for(const auto* flag:{"outsideWorldMap","returnToSafehouse"})
  if(authored.contains(flag)&&!authored.at(flag).is_boolean())throw std::runtime_error("Invalid event map or safehouse setting");
 if(!authored.at("cops").is_boolean())throw std::runtime_error("Police setting");
 if(mode=="tollboothrace")Number(authored,"timeLimit",.001,86400);
 if((mode=="circuit"||mode=="lapknockout")&&authored.at("loop")!=true)throw std::runtime_error("Loop required");
 const bool startOnly=RpdStartOnly(authored);
 const auto& gates=authored.at("gates");if(!startOnly&&(!gates.is_array()||gates.size()<2||gates.size()>258||gates.front().at("kind")!="start"||gates.back().at("kind")!="finish"))throw std::runtime_error("Gate sequence");
 for(size_t index=1;index+1<gates.size();++index)if(gates[index].at("kind")!="checkpoint")throw std::runtime_error("Intermediate gate kind");
 std::set<std::string> names;unsigned starts=0,finishes=0;
 for(const char* field:{"gates","shortcuts","extras"}){
  auto list=authored.value(field,Json::array());if(!list.is_array())throw std::runtime_error("Invalid authored object list");
  for(const auto& gate:list){auto name=gate.at("name").get<std::string>();if(name.empty()||!names.insert(name).second)throw std::runtime_error("Duplicate authored object");
   if(std::string(field)=="gates"){starts+=gate.at("kind")=="start";finishes+=gate.at("kind")=="finish";}
   for(const char* vector:{"position","dimensions"}){const auto& values=gate.at(vector);if(!values.is_array()||values.size()!=3)throw std::runtime_error("Gate vector");
    for(const auto& value:values){if(!value.is_number())throw std::runtime_error("Gate component");auto n=value.get<double>();if(!std::isfinite(n)||std::abs(n)>1000000||(std::string(vector)=="dimensions"&&n<=0))throw std::runtime_error("Gate component range");}}
   Number(gate,"rotation",-360000,360000);if(gate.contains("timeBonus"))Number(gate,"timeBonus",0,86400);
  }
 }
 unsigned traps=0;for(const auto& item:authored.value("extras",Json::array()))if(item.value("nativeTemplate","")=="speedtrap"){
  if(item.at("kind")!="marker")throw std::runtime_error("Speed trap kind");
  if(item.contains("radius"))Number(item,"radius",1,2000);++traps;
 }
 if(traps>16)throw std::runtime_error("Speed trap limit");
 if(mode=="speedtraprace"&&!traps)throw std::runtime_error("Speed trap points required");
 if(starts!=1||finishes!=(startOnly?0u:1u))throw std::runtime_error("Start/finish counts");
 if(startOnly){
  const auto racer=RpdRacerStart(authored);
  if(!racer.is_object()||racer.value("kind",std::string())!="start")throw std::runtime_error("RPD racer start required");
  const auto& p=racer.at("position");if(!p.is_array()||p.size()!=3)throw std::runtime_error("RPD racer start position");
  for(const auto& n:p)if(!n.is_number()||!std::isfinite(n.get<double>())||std::abs(n.get<double>())>1000000)throw std::runtime_error("RPD racer start component");
  Number(racer,"rotation",-360000,360000);
  if(Feature(authored,"replaceMap")||!authored.value("route",Json::array()).empty()||!authored.value("shortcuts",Json::array()).empty()||!authored.value("extras",Json::array()).empty())throw std::runtime_error("RPD free-run must not carry a race course or map replacement");
 }

 ApplyFixedRaceRules(result.authoring);return result;
}
inline void AtomicWrite(const fs::path& destination,const std::string& bytes){
 fs::create_directories(destination.parent_path());
 auto temporary=destination;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetCurrentThreadId());
 HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,nullptr);
 if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Sidecar temporary file unavailable");
 DWORD written=0;bool ok=WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
 CloseHandle(file);if(ok)ok=MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
 if(!ok){DeleteFileW(temporary.c_str());throw std::runtime_error("Sidecar commit failed");}
}
struct FinishEvidence {bool nativeGoalCrossed=false,forcedRaceEnd=false;unsigned place=0;double elapsedSeconds=0,score=0;bool armsSurvivalVictory=false;event_preview_stats::Finish preview;bool interceptorVictory=false;double topSpeedMps=0;};
class ProfileSidecar {
 fs::path path;std::string profileKey;Json records=Json::object();
 public:
 static std::string Key(const EventPackage& event){return event.id+":"+std::to_string(event.revision);}
 ProfileSidecar(const fs::path& root,std::string key):profileKey(std::move(key)){
  if(profileKey.empty()||profileKey.size()>4096)throw std::runtime_error("Profile identity required");
  path=root/fs::path(Digest(profileKey))/L"progress.json";
  if(fs::exists(path)){auto saved=Json::parse(Read(path));
   if(saved.at("schema")!=1||saved.at("profileKeyHash").get<std::string>()!=Digest(profileKey)||Digest(saved.at("records").dump())!=saved.at("payloadSha256").get<std::string>())throw std::runtime_error("Sidecar identity/integrity");
   records=saved.at("records");if(!records.is_object())throw std::runtime_error("Sidecar records");}
 }
 bool Record(const EventPackage& event,const FinishEvidence& finish){
  const bool survival=finish.armsSurvivalVictory&&event.authoring.value("combatMode",std::string("none"))=="armsrace"&&finish.place==1;
  const bool interceptor=finish.interceptorVictory&&event.authoring.value("campaign",std::string())=="rpd"&&event.authoring.value("policeMission",std::string())=="interceptor"&&event.authoring.value("combatMode",std::string())=="hotpursuit"&&finish.place==1;
  if(!EventId(event.id)||!event.revision||(!finish.nativeGoalCrossed&&!survival&&!interceptor)||finish.forcedRaceEnd||finish.place<1||finish.place>16||
     !std::isfinite(finish.elapsedSeconds)||finish.elapsedSeconds<=0||!std::isfinite(finish.score)||finish.score<0)return false;
  auto candidate=records;auto key=event.id+":"+std::to_string(event.revision);auto& row=candidate[key];
  if(row.is_null())row=Json::object();
  if(!row.contains("attempts")){row["attempts"]=0;row["wins"]=0;}
  row["attempts"]=row.at("attempts").get<uint64_t>()+1;row["wins"]=row.at("wins").get<uint64_t>()+(finish.place==1||event.authoring.value("combatMode",std::string("none"))=="hotpursuit");
  if(survival){row["survivalWins"]=row.value("survivalWins",uint64_t(0))+1;row["lastSurvivalSeconds"]=finish.elapsedSeconds;}
  else{
   row["bestSeconds"]=std::min(row.value("bestSeconds",finish.elapsedSeconds),finish.elapsedSeconds);
   row["bestScore"]=std::max(row.value("bestScore",finish.score),finish.score);
  }
  if(interceptor){row["interceptorWins"]=row.value("interceptorWins",uint64_t(0))+1;row["completed"]=true;}
  if(std::isfinite(finish.topSpeedMps)&&finish.topSpeedMps>0&&finish.topSpeedMps<250)row["topSpeedMps"]=std::max(row.value("topSpeedMps",0.),finish.topSpeedMps);
  const auto mode=event.authoring.value("combatMode",std::string("none"));
  if(finish.nativeGoalCrossed&&mode=="drift"&&finish.preview.drift&&event_preview_stats::ValidAngle(finish.preview.angle)){
   auto& stats=row["previewDrift"];if(stats.is_null())stats=Json::object();
   stats["bestAngle"]=std::max(stats.value("bestAngle",0.),finish.preview.angle);
   stats["bestPoints"]=std::max(stats.value("bestPoints",0.),finish.score);
  }
  if(finish.nativeGoalCrossed&&mode=="hotpursuit"&&finish.preview.hotPursuit&&event_preview_stats::ValidHot(finish.preview.survivors,finish.place)){
   auto& stats=row["previewHotPursuit"];if(stats.is_null())stats=Json::object();
   stats["mostSurvivors"]=event_preview_stats::BestSurvivors(stats.value("mostSurvivors",0u),finish.preview.survivors);
   stats["bestPlace"]=event_preview_stats::BestPlace(stats.value("bestPlace",0u),finish.place);
  }
  Json saved{{"schema",1},{"profileKeyHash",Digest(profileKey)},{"records",candidate},{"payloadSha256",Digest(candidate.dump())}};
  AtomicWrite(path,saved.dump()+"\n");records=std::move(candidate);return true;
 }
 // A police sprint is evaluated by arrest count, without a career win/loss.
 // The runtime calls this only after its completed roster and native result
 // are confirmed. Keep these statistics separate from course goal evidence.
 bool RecordRpdArrests(const EventPackage& event,unsigned arrested,unsigned suspects){
  const auto& a=event.authoring;
  if(!EventId(event.id)||!event.revision||a.value("campaign",std::string())!="rpd"||
     a.value("combatMode",std::string())!="hotpursuit"||a.value("policeMission",std::string())!="hotpursuit"||
     a.value("policeRule",std::string())!="sprint"||a.value("mode",std::string())!="sprint"||
     a.value("racerCount",0u)!=suspects+1||!event_preview_stats::ValidArrests(arrested,suspects))return false;
  auto candidate=records;auto& stats=candidate[Key(event)]["previewRpdHotPursuit"];
  if(stats.is_null()||!stats.is_object()||stats.value("suspects",0u)!=suspects)stats=Json::object();
  auto previous=stats.value("mostArrested",0u);
  stats["mostArrested"]=std::max(previous<=suspects?previous:0u,arrested);stats["suspects"]=suspects;
  if(candidate==records)return false;
  Json saved{{"schema",1},{"profileKeyHash",Digest(profileKey)},{"records",candidate},{"payloadSha256",Digest(candidate.dump())}};
  AtomicWrite(path,saved.dump()+"\n");records=std::move(candidate);return true;
 }
 bool SaveNativeProgress(const std::vector<std::pair<EventPackage,event_save::Record>>& progress){
  auto candidate=records;
  for(const auto& entry:progress){const auto& event=entry.first;const auto& value=entry.second;
   if(!EventId(event.id)||!event.revision||Correction(event.authoring)||!value.id)throw std::runtime_error("Additional native progress identity required");
   auto& row=candidate[Key(event)];if(row.is_null())row=Json::object();
   row["nativeProgress"]=Json::array({value.id,value.flags,value.value,value.auxiliary});
  }
  if(candidate==records)return false;
  Json saved{{"schema",1},{"profileKeyHash",Digest(profileKey)},{"records",candidate},{"payloadSha256",Digest(candidate.dump())}};
  AtomicWrite(path,saved.dump()+"\n");records=std::move(candidate);return true;
 }
 bool NativeProgress(const EventPackage& event,unsigned expectedId,event_save::Record& output)const{
  auto found=records.find(Key(event));if(found==records.end()||!found->contains("nativeProgress"))return false;
  const auto& value=found->at("nativeProgress");
  if(Correction(event.authoring)||!value.is_array()||value.size()!=4)throw std::runtime_error("Invalid native sidecar progress");
  unsigned words[4]{};for(unsigned i=0;i<4;++i){if(!value[i].is_number_unsigned()&&!value[i].is_number_integer())throw std::runtime_error("Invalid native progress word");auto n=value[i].get<int64_t>();if(n<0||n>UINT_MAX)throw std::runtime_error("Native progress word range");words[i]=unsigned(n);}
  if(words[0]!=expectedId||!expectedId)throw std::runtime_error("Native sidecar event identity mismatch");
  output={words[0],words[1],words[2],words[3]};return true;
 }
 const Json& Records()const{return records;}
 event_preview_stats::Record Preview(const EventPackage& event)const{
  event_preview_stats::Record result;auto row=records.find(Key(event));if(row==records.end())return result;
  auto drift=row->find("previewDrift");
  if(drift!=row->end()&&drift->is_object()){
   double angle=drift->value("bestAngle",-1.),points=drift->value("bestPoints",-1.);
   if(event_preview_stats::ValidAngle(angle)&&std::isfinite(points)&&points>=0&&points<=10000000){result.drift=true;result.angle=angle;result.points=unsigned(points);}
  }
  auto hot=row->find("previewHotPursuit");
  if(hot!=row->end()&&hot->is_object()){
   auto survivors=hot->value("mostSurvivors",0u),place=hot->value("bestPlace",0u);
   if(event_preview_stats::ValidHot(survivors,place)){result.hotPursuit=true;result.survivors=survivors;result.place=place;}
  }
  auto police=row->find("previewRpdHotPursuit");
  if(police!=row->end()&&police->is_object()){
   auto arrested=police->value("mostArrested",~0u),suspects=police->value("suspects",0u);
   if(event_preview_stats::ValidArrests(arrested,suspects)&&event.authoring.value("racerCount",0u)==suspects+1){
    result.policeSprint=true;result.arrested=arrested;result.suspects=suspects;
   }
  }
  return result;
 }
 const fs::path& Path()const{return path;}
};
}
