#include "EventCore.h"
#include "WorldCompatibility.h"
#include "NativeCashValue.h"
#include "NeoObstacleFile.h"
#include "TougeFallPolicy.h"
#include "../../integration/MWRaceBarrierPolicy.h"
#include "NativeRaceGuardGroups.h"
#include "../../integration/MWNativeSpeechPolicy.h"
#include <iostream>
#include <set>
#include <algorithm>
static void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int wmain(int argc,wchar_t** argv){
 try{
  {
   using namespace touge_fall_policy;Gate gate{{0,0,0},40,20,0};
   Check(Continuous({0,0,0},{40,0,0})&&!Continuous({0,0,0},{0,0,-100}),"Respawn continuity guard");
   Check(DownCrossing(gate,{0,0,10},{0,0,-10}),"Downward horizontal gate miss");
   Check(!DownCrossing(gate,{0,0,-10},{0,0,10}),"Upward gate contact crashed");
   Check(!DownCrossing(gate,{21,0,10},{21,0,-10}),"Outside rectangle crashed");
   Check(!DownCrossing(gate,{0,0,100},{0,0,-100}),"Respawn warp became a fall");
   Check(DownCrossing(gate,{-30,0,10},{30,0,-10}),"Swept high-speed gate contact missed");
   Check(!DownCrossing(gate,{0,0,-12},{0,0,-14}),"Lower road triggered upper plane");
   Check(BelowRoad(100,87,-3,20)&&!BelowRoad(100,101,-3,20)&&!BelowRoad(100,87,0,20),"Cliff fall policy");
   Check(mwcombat::Parse("touge")==mwcombat::Mode::Touge&&!mwcombat::Weapons(mwcombat::Mode::Touge),"Touge mode identity");
  }
  for(const char* name:{"replaceMap","nativeOpponentAssist","checkpointsVisible"}){
   Check(!mwel::Feature(mwel::Json::object(),name),"Omitted feature enabled");
   for(auto v:{mwel::Json(nullptr),mwel::Json(false),mwel::Json(1),mwel::Json("true")})Check(!mwel::Feature({{name,v}},name),"Non-true feature enabled");
   Check(mwel::Feature({{name,true}},name),"Explicit feature ignored");
  }
  Check(!mwel::NeoEnabled(mwel::Json::object())&&!mwel::NeoEnabled({{"aiRouteMode","native"}})&&!mwel::NeoEnabled({{"aiRouteMode",nullptr}})&&mwel::NeoEnabled({{"aiRouteMode","yellow"}}),"Per-event Neo opt-in failed");
  for(const char* base:{"sprint","circuit","speedtraprace","lapknockout","drag","tollboothrace"}){
   Check(mwcombat::Compatible(mwcombat::Mode::Drift,base,6)==(!strcmp(base,"sprint")||!strcmp(base,"circuit")),"Drift accepted unsupported base rules");
  }
  for(const char* mode:{"none","armsrace","gauntlet","hotpursuit","drift"}){
   mwel::Json event={{"combatMode",mode},{"cops",true},{"copDensity",100},{"startHeatLevel",5}};auto original=event;
   mwel::ApplyFixedRaceRules(event);
   if(!strcmp(mode,"drift"))Check(event.at("cops")==false&&event.at("copDensity")==0&&event.at("startHeatLevel").is_null(),"Drift retained police or starting heat");
   else Check(event==original,"Drift policy changed another race mode");
  }
  std::cout<<"DRIFT_FIXED_RULES_PASS sprint/circuit only, police and heat disabled; other modes unchanged\n";
  for(unsigned count=1;count<=16;++count){
   Check(mwel_carbon::Valid("auto",count)&&mwel_carbon::Valid("stock",count),"Default/native intro rejected");
   for(auto name:{"INTRO01NG02","INTRO02NG02","INTRO04NG02","INTRO05NG02","INTRO07B02NG","INTRO11B02NG","INTRO12B02NG","INTRO12NG","INTRODEFAULT"}){
    Check(mwel_carbon::Valid(name,count)==(count==mwel_carbon::Participants(name)),"Intro field mismatch accepted");
    Check(!mwel_carbon::Matches("stock",name,count,mwel_carbon::Participants(name)),"Stock intro replaced");
   }
  }
  Check(mwel_carbon::Valid("INTRO99NG02",8)&&!mwel_carbon::Valid("../INTRO01NG02",8),"Unavailable intro accepted");
  Check(!mwel_carbon::Matches("INTRO04NG02","INTRO05NG02",8,8)&&mwel_carbon::Matches("INTRO04NG02","INTRO04NG02",8,8),"Explicit intro selection changed");
  for(auto name:{"HPR_INTERCEPTOR_ONLINE_10SEC","HPR_INTERCEPTOR_NO_SLOWMO","HPR_INTERCEPTOR_HANG_TOUGH","HPR_RACE_ROLLING_2","HPR_RACE_ALIGN_8","HPR_RACE_ALIGN_ONLINE_8","HPR_RACE_DRIVE_ONLINE_8"}){
   const auto count=mwel_carbon::Participants(name);
   Check(count==2||count==8,"HP scene field size missing");
   Check(mwel_carbon::Valid(name,count)&&!mwel_carbon::Valid(name,count+1),"HP scene field mismatch accepted");
   Check(!mwel_carbon::Matches("auto",name,count,count)&&mwel_carbon::Matches(name,name,count,count),"HP scene became an automatic intro");
  }
  Check(mwel_carbon::HprRaceMode("sprint")&&mwel_carbon::HprRaceMode("circuit")&&!mwel_carbon::HprRaceMode("drag")&&!mwel_carbon::HprRaceMode("tollboothrace"),"HP race intro mode policy");
   Check(mwel_carbon::Valid("UG2_INTRONIS20",6)&&!mwel_carbon::Valid("UG2_INTRONIS20",4)&&!mwel_carbon::Valid("UG2_INTRONIS20",12),"UG2 six-car scene field mismatch");
   Check(mwel_carbon::Valid("UG2_INTRONIS08",4)&&mwel_carbon::Valid("UG2_INTRONIS09",4)&&mwel_carbon::Valid("UG2_INTRONIS14",2),"Unavailable UG2 intro accepted");
   std::cout<<"CARBON_SELECTION_PASS auto/native, exact names and participant validation\n";
  for(const char* mode:{"sprint","speedtraprace","drag","tollboothrace"})Check(mwel::RaceLaps({{"mode",mode},{"laps",3}})==1,"Sprint-derived mode retained copied lap count");
  for(const char* mode:{"circuit","lapknockout"})Check(mwel::RaceLaps({{"mode",mode},{"laps",3}})==3,"Loop race lost authored lap count");
  {const char* empty=nullptr;const char* blank="";const char* valid="AnytimeEvents_DispTimeExpired";auto read=[](const void* p,void* out,size_t n){memcpy(out,p,n);return true;};
   Check(!mw_native_speech::Available(nullptr,read)&&!mw_native_speech::Available(&empty,read)&&!mw_native_speech::Available(&blank,read),"Unavailable native speech entered string hashing");
   Check(mw_native_speech::Available(&valid,read),"Valid native speech was suppressed");
   Check(!mw_native_speech::Available(&valid,[](const void*,void*,size_t){return false;}),"Unreadable native event accepted");
   std::cout<<"NATIVE_SPEECH_POLICY_PASS null/empty/unreadable declarations refused; valid events preserved\n";
  }
  for(unsigned driver=0;driver<7;++driver){
   const mw_race_barriers::Query query{0,driver,123};
   Check(mw_race_barriers::Collides(512,query)==(driver!=1&&driver!=2),"Event guard traffic/police permissions differ");
  }
  Check(!mw_race_barriers::Collides(512,{2,mw_race_barriers::UnknownDriver,0}),"Native barrier exclusion lost");
  Check(mw_race_barriers::Collides(512,{1,mw_race_barriers::UnknownDriver,0}),"Unrelated native exclusion removed guard");
  Check(mw_race_barriers::Collides(512,{}),"Unknown owner disabled event guard");
  {
   using namespace mwel_native_guards;
   std::vector<unsigned char> bytes(8+60);auto put=[&](size_t o,unsigned v,size_t n){memcpy(bytes.data()+o,&v,n);};
   put(0,0x34109,4);put(4,60,4);
   put(16,Key("BARRIER_SPLINE_730"),4);put(20,730,2);
   put(36,Key("SHOP_GARAGE"),4);put(40,731,2);
   put(56,Key("BARRIER_SPLINE_07"),4);put(60,732,2);
   auto groups=Decode(bytes);
   Check(!Retain(730,groups,true)&&!Retain(732,groups,true),"Named native closures retained for RPD officer");
   Check(Retain(0,groups,true)&&Retain(731,groups,true)&&Retain(4096,groups,true),"Native floor, scenery or unknown group removed");
   Check(Retain(730,groups,false),"Native closure filtering escaped verified officer query");
   bool refused=false;auto truncated=bytes;truncated.pop_back();try{Decode(truncated);}catch(...){refused=true;}Check(refused,"Truncated native groups accepted");
   refused=false;auto invalid=bytes;invalid[20]=0xff;invalid[21]=0xff;try{Decode(invalid);}catch(...){refused=true;}Check(refused,"Invalid native group index accepted");
   std::cout<<"NATIVE_GUARD_GROUP_POLICY_PASS named_closures_only=1 world_and_unknown_retained=1 officer_scope=1 bounded_records=1\n";
  }

  {
   unsigned char manager[0x98]{},body[0x180]{};auto vehicle=body+0x40;
   auto write=[](void* target,unsigned value){memcpy(target,&value,4);};
   write(vehicle,0x8AA828);write(vehicle-0x20,42);write(manager+0x90,42);
   unsigned char* vehicles[]={vehicle};
   struct List {unsigned char** data;unsigned capacity,count;} list{vehicles,1,1};
   for(unsigned driver:{0u,1u,2u,3u}){
    write(vehicle+0x94,driver);auto query=mw_race_barriers::ForCollision(manager,&list);
    Check(query.driver==driver&&query.worldId==42,"Native collision owner lookup differs");
    Check(mw_race_barriers::Collides(512,query)==(driver!=1&&driver!=2),"Owner collision permission differs");
   }
   write(vehicle-0x20,43);write(manager+0x94,2);
   auto stale=mw_race_barriers::ForCollision(manager,&list);
   Check(stale.driver==mw_race_barriers::UnknownDriver&&stale.exclusions==2,"Recycled owner retained old driver");
   list.count=2;
   Check(mw_race_barriers::ForCollision(manager,&list).driver==mw_race_barriers::UnknownDriver,"Invalid vehicle list accepted");
   std::cout<<"GUARDWALL_POLICY_PASS traffic_cop_pass=1 racers_block=1 native_mask=1 stale_owner=1\n";
  }
  for(unsigned cash:{0u,1u,12345u,25000u,150000u,1000000u,2147483520u}){
   const auto bits=native_cash::Bits(cash);float native=0;memcpy(&native,&bits,4);
   Check(native==float(cash)&&int(native)==int(cash),"Native float reward/payout conversion differs");
  }
  unsigned oldBits=25000;float oldCash=0;memcpy(&oldCash,&oldBits,4);
  Check(int(oldCash)==0,"Old integer-bit reward regression fixture did not reproduce zero payout");
  bool overflow=false;try{native_cash::Bits(2147483647u);}catch(...){overflow=true;}
  Check(overflow,"Float rounding overflow accepted");
  if(argc!=3)throw std::runtime_error("Provide package fixture and fresh sidecar test directory");
  auto package=mwel::ParsePackage(argv[1]);auto root=mwel::fs::path(argv[2]);
  if(package.authoring.value("combatMode",std::string("none"))=="drift")Check(package.authoring.at("cops")==false&&package.authoring.at("copDensity")==0&&package.authoring.at("startHeatLevel").is_null(),"Parsed Drift package retained police settings");
  if(mwel::fs::exists(root))throw std::runtime_error("Use a fresh sidecar test directory");
  mwel::fs::create_directories(root/"world");world_compatibility::Context world(root/"world");auto portable=package;
  portable.authoring["worldCompatibility"]=world_compatibility::Current(root/"world");
  Check(world.Allows(portable),"matching Rockport event refused in another installation");
  portable.authoring["worldCompatibility"]["kind"]="w2c";Check(!world.Allows(portable),"custom-world event loaded into Rockport");
  portable.authoring.erase("worldCompatibility");portable.worldSnapshotId="A3E60AC6CD62C3590A2C106107BF13F63C91CA0CA14FA35BA6353F29ED3688D8";Check(!world.Allows(portable),"package missing explicit world identity accepted");
  portable.worldSnapshotId=std::string(64,'F');Check(!world.Allows(portable),"unknown legacy world accepted without source identity");
  Check(mwel_paths::Settings(root/"world")==root/"world/scripts/EventLoader/EventLoader.ini","new installation uses canonical settings path");
  mwel::AtomicWrite(root/"world/NFSMWEventLoader.ini","legacy");Check(mwel_paths::Settings(root/"world")==root/"world/scripts/EventLoader/EventLoader.ini","old root settings unexpectedly used");
  mwel::AtomicWrite(root/"world/scripts/EventLoader/EventLoader.ini","canonical");Check(mwel_paths::Settings(root/"world")==root/"world/scripts/EventLoader/EventLoader.ini","canonical settings did not take precedence");
  std::cout<<"WORLD_COMPATIBILITY_PASS independent Rockport installations, custom-world refusal, explicit world identity, canonical-only paths\n";
  {
   using namespace event_neo_obstacles;
   std::vector<Obstacle> obstacles(1);
   Check(Load(nullptr,0x0611591Bu,obstacles)&&obstacles.empty(),"No-obstacle event rejected optional absent asset");
   Check(!Load("missing-declared-neo-obstacles.bin",0x0611591Bu,obstacles),"Unreadable declared obstacles accepted");
   mwel::fs::create_directories(root);
   auto path=(root/"obstacles.bin").string();unsigned header[]={0x424F3257,1,0x0611591Bu,1};
   Obstacle fixture{{1,2,3},{0,0,1},1,2,3};FILE* file=nullptr;fopen_s(&file,path.c_str(),"wb");
   fwrite(header,sizeof(header),1,file);fwrite(&fixture,sizeof(fixture),1,file);fclose(file);
   Check(Load(path.c_str(),0x0611591Bu,obstacles)&&obstacles.size()==1,"Valid authored obstacle rejected");
   Check(!Load(path.c_str(),0x0611591Bu^1,obstacles),"Other event obstacle accepted");
   fopen_s(&file,path.c_str(),"wb");fwrite(header,sizeof(header),1,file);fclose(file);
   Check(!Load(path.c_str(),0x0611591Bu,obstacles)&&obstacles.size()==1,"Truncated obstacles accepted or replaced valid state");
   std::cout<<"NEO_OPTIONAL_OBSTACLES_PASS absent=1 declared_missing_rejected=1 valid=1 corrupt_rejected=1\n";
  }
  mwel::ProfileSidecar alice(root,"test-installation/profile-A"),bob(root,"test-installation/profile-B");
  {
   auto mission=package;mission.revision+=120;
   mission.authoring["campaign"]="rpd";mission.authoring["policeMission"]="interceptor";mission.authoring["combatMode"]="hotpursuit";
   mwel::FinishEvidence arrest{false,false,1,37.25,0};arrest.interceptorVictory=true;arrest.topSpeedMps=55.5;
   Check(alice.Record(mission,arrest),"Native arrest without course goal refused");
   auto bad=arrest;bad.forcedRaceEnd=true;Check(!alice.Record(mission,bad),"Forced arrest saved");
   bad=arrest;bad.place=2;Check(!alice.Record(mission,bad),"Escaped suspect saved as mission win");
   auto ordinary=mission;ordinary.authoring["campaign"]="racer";Check(!alice.Record(ordinary,arrest),"Arrest bypassed racer goal proof");
   ordinary=mission;ordinary.authoring["policeMission"]="rapidresponse";Check(!alice.Record(ordinary,arrest),"Wrong mission accepted arrest proof");
   arrest.elapsedSeconds=40;arrest.topSpeedMps=60;Check(alice.Record(mission,arrest),"Second arrest refused");
   mwel::ProfileSidecar disk(root,"test-installation/profile-A");auto id=mwel::ProfileSidecar::Key(mission);const auto& r=disk.Records().at(id);
   Check(r.at("completed")==true&&r.at("interceptorWins")==2&&r.at("attempts")==2,"Arrest completion/counters did not survive reload");
   Check(r.at("bestSeconds")==37.25&&r.at("topSpeedMps")==60,"Mission time/speed maxima lost on reload");
   Check(!bob.Records().contains(id),"Mission completion crossed profiles");
   std::cout<<"RPD_PROGRESS_PASS native arrest; no fake goal; loss/forced/wrong mission refused; disk reload; speed/time; profile isolation\n";
  }
  {
   auto drift=package;drift.revision+=110;drift.authoring["combatMode"]="drift";
   auto hot=package;hot.revision+=111;hot.authoring["combatMode"]="hotpursuit";
   mwel::FinishEvidence first{true,false,3,100,12000};first.preview={true,false,42.5,0};
   Check(alice.Record(drift,first),"Preview Drift finish refused");
   auto second=first;second.place=2;second.score=9000;second.preview.angle=65;
   Check(alice.Record(drift,second),"Preview second Drift finish refused");
   auto bad=second;bad.forcedRaceEnd=true;bad.score=999999;bad.preview.angle=79;
   Check(!alice.Record(drift,bad),"Forced Drift finish supplied a preview record");
   mwel::FinishEvidence survivor{true,false,4,120,0};survivor.preview={false,true,0,6};
   Check(alice.Record(hot,survivor),"Preview Hot Pursuit survivor refused");
   survivor.place=2;survivor.preview.survivors=1;
   Check(alice.Record(hot,survivor),"Preview Hot Pursuit placement refused");
   mwel::ProfileSidecar reread(root,"test-installation/profile-A");
   auto d=reread.Preview(drift),h=reread.Preview(hot);
   Check(d.drift&&d.angle==65&&d.points==12000&&!d.hotPursuit,"Drift angle/points did not retain independent maxima");
   Check(h.hotPursuit&&h.survivors==6&&h.place==2&&!h.drift,"Hot Pursuit maxima/minimum rank came from the same attempt");
   Check(event_preview_stats::Angle(d)==L"65.0\u00b0"&&event_preview_stats::Points(d)==L"12000 Pts","Drift record formatting failed");
   Check(event_preview_stats::Hot(h,true)==L"6\u53f0 / 2\u4f4d"&&event_preview_stats::Hot(h,false)==L"6 cars / #2","Hot Pursuit merged display failed");
   Check(!bob.Preview(drift).drift&&!bob.Preview(hot).hotPursuit,"Preview records crossed profile identities");
   auto revision=drift;++revision.revision;Check(!reread.Preview(revision).drift,"Preview records crossed event revisions");
   Check(event_preview_stats::Angle({})==L"--"&&event_preview_stats::Points({})==L"--"&&event_preview_stats::Hot({},true)==L"-- / --","Missing records fabricated values");
   Check(!event_preview_stats::ValidAngle(NAN)&&!event_preview_stats::ValidAngle(81)&&!event_preview_stats::ValidHot(0,1)&&!event_preview_stats::ValidHot(2,17),"Invalid preview metrics accepted");
   const auto hotKey=mwel::ProfileSidecar::Key(hot);
   auto before=reread.Records().at(hotKey);(void)reread.Preview(hot);
   Check(reread.Records().at(hotKey)==before&&!before.contains("nativeProgress"),"Preview lookup rewrote completion data");
   std::cout<<"PREVIEW_STATS_PASS independent records, genuine finishes, profiles, revisions, missing values, Japanese/English display, read-only lookup\n";
  }
  {
   auto sprint=package;sprint.revision+=121;
   sprint.authoring["campaign"]="rpd";sprint.authoring["policeMission"]="hotpursuit";
   sprint.authoring["combatMode"]="hotpursuit";sprint.authoring["policeRule"]="sprint";
   sprint.authoring["mode"]="sprint";sprint.authoring["racerCount"]=9;
   const auto id=mwel::ProfileSidecar::Key(sprint);
   Check(event_preview_stats::Arrests(alice.Preview(sprint),8)==L"--/8","Unplayed police sprint fabricated an arrest record");
   Check(alice.RecordRpdArrests(sprint,0,8),"Zero-arrest police sprint was not recorded");
   Check(event_preview_stats::Arrests(alice.Preview(sprint),8)==L"0/8","Played zero arrests indistinguishable from unplayed");
   Check(alice.RecordRpdArrests(sprint,4,8),"Improved police sprint record was not saved");
   Check(!alice.RecordRpdArrests(sprint,2,8)&&!alice.RecordRpdArrests(sprint,4,8),"Lower/equal police sprint rewrote best record");
   Check(!alice.RecordRpdArrests(sprint,9,8)&&!alice.RecordRpdArrests(sprint,4,7),"Invalid arrest count/roster accepted");
   auto ordinary=sprint;ordinary.authoring["campaign"]="racer";
   Check(!alice.RecordRpdArrests(ordinary,8,8),"Racer event supplied police arrest record");
   ordinary=sprint;ordinary.authoring["policeMission"]="interceptor";
   Check(!alice.RecordRpdArrests(ordinary,8,8),"Interceptor supplied sprint arrest record");
   mwel::ProfileSidecar disk(root,"test-installation/profile-A");
   auto record=disk.Preview(sprint);Check(record.policeSprint&&record.arrested==4&&record.suspects==8,"Police sprint record lost on disk reload");
   Check(event_preview_stats::Arrests(record,8)==L"4/8","Arrest numerator/denominator display incorrect");
   Check(!disk.Records().at(id).contains("wins")&&!disk.Records().at(id).contains("completed")&&!disk.Records().at(id).contains("bestSeconds"),"Arrest statistics fabricated a win/completion/time");
   Check(!bob.Preview(sprint).policeSprint,"Police sprint records crossed profiles");
   auto newer=sprint;++newer.revision;Check(!disk.Preview(newer).policeSprint,"Police sprint records crossed revisions");
   newer=sprint;newer.authoring["racerCount"]=7;
   Check(!disk.Preview(newer).policeSprint&&event_preview_stats::Arrests(record,6)==L"--/6","Changed roster reused an incompatible denominator");
   Check(alice.RecordRpdArrests(sprint,8,8),"Full arrest record refused");
   std::cout<<"RPD_PREVIEW_ARRESTS_PASS zero/improved/lower/equal/invalid; disk persistence; no career win; profiles/revisions/rosters; count formatting\n";
  }
  mwel::FinishEvidence finish{true,false,1,105.5,0};
  Check(alice.Record(package,finish),"Valid finish refused");
  finish.nativeGoalCrossed=false;Check(!alice.Record(package,finish),"False goal accepted");
  finish.nativeGoalCrossed=true;finish.forcedRaceEnd=true;Check(!alice.Record(package,finish),"Forced end accepted");
  finish.forcedRaceEnd=false;finish.elapsedSeconds=95;Check(alice.Record(package,finish),"Second finish refused");
  mwel::ProfileSidecar reopened(root,"test-installation/profile-A");
  auto key=package.id+":"+std::to_string(package.revision);
  Check(reopened.Records().at(key).at("bestSeconds")==95,"Best time did not persist");
  Check(reopened.Records().at(key).at("attempts")==2,"Rejected finishes were counted");
  Check(bob.Records().empty()&&alice.Path()!=bob.Path(),"Profile isolation failed");
  {
   auto drift=package;drift.revision+=101;drift.authoring["combatMode"]="drift";
   mwel::FinishEvidence scored{true,false,3,125,3400};Check(alice.Record(drift,scored),"Drift lower score rank finish refused");
   auto driftKey=mwel::ProfileSidecar::Key(drift);Check(alice.Records().at(driftKey).at("wins")==0&&alice.Records().at(driftKey).at("bestScore")==3400,"Drift score or losing result lost");
   scored.place=1;scored.score=10000;Check(alice.Record(drift,scored),"Drift score victory refused");
   scored.forcedRaceEnd=true;Check(!alice.Record(drift,scored),"Forced drift goal accepted");
   Check(alice.Records().at(driftKey).at("wins")==1&&alice.Records().at(driftKey).at("bestScore")==10000,"Drift highest score did not persist");
  }
  {
   auto survivalPackage=package;survivalPackage.revision+=100;
   mwel::FinishEvidence survival{false,false,1,12.5,0,true};
   survivalPackage.authoring["combatMode"]="none";
   Check(!alice.Record(survivalPackage,survival),"Survival evidence leaked into ordinary races");
   survivalPackage.authoring["combatMode"]="armsrace";
   survival.place=2;Check(!alice.Record(survivalPackage,survival),"Lower rank accepted as last survivor");
   survival.place=1;survival.forcedRaceEnd=true;Check(!alice.Record(survivalPackage,survival),"Forced end accepted as survival victory");
   survival.forcedRaceEnd=false;Check(alice.Record(survivalPackage,survival),"Verified Arms survival win refused");
   auto survivalKey=mwel::ProfileSidecar::Key(survivalPackage);
   const auto& row=alice.Records().at(survivalKey);
   Check(row.at("wins")==1&&row.at("survivalWins")==1&&!row.contains("bestSeconds"),"Survival win fabricated a finish time");
   survival.nativeGoalCrossed=true;survival.armsSurvivalVictory=false;survival.elapsedSeconds=99;
   Check(alice.Record(survivalPackage,survival)&&alice.Records().at(survivalKey).at("bestSeconds")==99,"Later genuine finish lost its independent course record");
  }
  event_save::Record native{0x12345678,0x80000001,0xFFFFFFFF,0x87654321};
  if(mwel::Correction(package.authoring)){
   bool refused=false;try{reopened.SaveNativeProgress({{package,native}});}catch(...){refused=true;}
   Check(refused,"Correction progress was captured as an additional native event");
   Check(!reopened.Records().at(key).contains("nativeProgress"),"Correction leaked opaque native progress into sidecar");
   std::cout<<"PASS correction package parsing, profile isolation, finish evidence, native progress capture refused\n";return 0;
  }
  Check(reopened.SaveNativeProgress({{package,native}}),"Native progress capture failed");
  mwel::ProfileSidecar nativeReload(root,"test-installation/profile-A");event_save::Record restored{};
  Check(nativeReload.NativeProgress(package,native.id,restored)&&memcmp(&native,&restored,sizeof(native))==0,"Opaque native progress did not round-trip");
  Check(nativeReload.Records().at(key).at("attempts")==2,"Capture changed finish counters");
  Check(!bob.NativeProgress(package,native.id,restored),"Native progress leaked to another profile");
  auto revision=package;++revision.revision;Check(!nativeReload.NativeProgress(revision,native.id,restored),"Progress leaked to another course revision");
  bool wrongId=false;try{nativeReload.NativeProgress(package,1,restored);}catch(...){wrongId=true;}Check(wrongId,"Native identity mismatch accepted");
  mwel::ProfileSidecar firstCapture(root,"profile-C");firstCapture.SaveNativeProgress({{package,native}});Check(firstCapture.Record(package,finish),"Finish after initial opaque capture failed");
  auto corrupted=mwel::Json::parse(mwel::Read(alice.Path()));corrupted["records"][key]["wins"]=99;
  mwel::AtomicWrite(alice.Path(),corrupted.dump());bool rejected=false;
  try{mwel::ProfileSidecar bad(root,"test-installation/profile-A");}catch(...){rejected=true;}
  Check(rejected,"Corrupt sidecar accepted");
  std::cout<<"PASS package/Python interoperability, profile isolation, atomic persistence, finish evidence, integrity\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
