#include "EventCore.h"
#include "WorldCompatibility.h"
#include "NativeCashValue.h"
#include "NeoObstacleFile.h"
#include "../../integration/MWRaceBarrierPolicy.h"
#include "../../integration/MWNativeSpeechPolicy.h"
#include <iostream>
#include <set>
#include <algorithm>
static void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int wmain(int argc,wchar_t** argv){
 try{
  for(const char* name:{"replaceMap","nativeOpponentAssist","checkpointsVisible"}){
   Check(!mwel::Feature(mwel::Json::object(),name),"Omitted feature enabled");
   for(auto v:{mwel::Json(nullptr),mwel::Json(false),mwel::Json(1),mwel::Json("true")})Check(!mwel::Feature({{name,v}},name),"Non-true feature enabled");
   Check(mwel::Feature({{name,true}},name),"Explicit feature ignored");
  }
  Check(!mwel::NeoEnabled(mwel::Json::object())&&!mwel::NeoEnabled({{"aiRouteMode","native"}})&&!mwel::NeoEnabled({{"aiRouteMode",nullptr}})&&mwel::NeoEnabled({{"aiRouteMode","yellow"}}),"Per-event Neo opt-in failed");
  for(unsigned count=1;count<=16;++count){
   Check(mwel_carbon::Valid("auto",count)&&mwel_carbon::Valid("stock",count),"Default/native intro rejected");
   for(auto name:{"INTRO01NG02","INTRO02NG02","INTRO04NG02","INTRO05NG02","INTRO07B02NG","INTRO11B02NG","INTRO12B02NG","INTRO12NG","INTRODEFAULT"}){
    Check(mwel_carbon::Valid(name,count)==(count==mwel_carbon::Participants(name)),"Intro field mismatch accepted");
    Check(!mwel_carbon::Matches("stock",name,count,mwel_carbon::Participants(name)),"Stock intro replaced");
   }
  }
  Check(mwel_carbon::Valid("INTRO99NG02",8)&&!mwel_carbon::Valid("../INTRO01NG02",8),"Unavailable intro accepted");
  Check(!mwel_carbon::Matches("INTRO04NG02","INTRO05NG02",8,8)&&mwel_carbon::Matches("INTRO04NG02","INTRO04NG02",8,8),"Explicit intro selection changed");
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
