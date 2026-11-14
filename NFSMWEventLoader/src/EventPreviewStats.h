#pragma once
// EN1.3 00547730 refreshes the native SafeHouseRaceSheet selection. ECX is
// its list subobject (+2c), whose screen package is at -1c and selected item
// at +18. The item owns GRaceParameters at +18 (00547828). Native values are
// formatted at 00547A91/00547B88/00547BA9; the FNG's existing animations cycle
// those three rows. FrontB.lzc -> SafeHouseRaceSheet.fng verifies label IDs.
namespace event_preview_stats_runtime {
using expanded_leaderboard::Read;
using expanded_leaderboard::Object;
void(__thiscall* originalRefresh)(void*)=nullptr;
void*(__thiscall* originalDetails)(void*,void*)=nullptr;
bool installed=false;
constexpr unsigned TimeLabel=0xDEDC8803,TopLabel=0xD18D600A;
// Native 00547B88 writes AvgSpeed; 00547BA9 writes TopSpeed. Verified
// against the displayed FNG cycle, not the order of the two speed fields.
constexpr unsigned TimeValue=0x8FD41BB4,TopValue=0xDE9145FB;
constexpr unsigned NativeTime=0x5D40B809,NativeTop=0x79CE88AF;
unsigned angleKey=0,pointsKey=0,hotKey=0;
std::map<std::pair<std::string,unsigned>,unsigned> rpdOriginalLabels;
struct RpdVisibility{Object object=nullptr;bool visible=false;};
std::map<std::pair<std::string,unsigned>,RpdVisibility> rpdVisibility;
struct RpdCycle {
 Object panel=nullptr;ULONGLONG started=0;bool alternate=false;
 std::array<std::array<std::string,2>,5> primary{},secondary{};
};
std::map<std::string,RpdCycle> rpdCycles;
unsigned RpdRowKey(unsigned row,unsigned part){char key[64];sprintf_s(key,"MWEL_RPD_PREVIEW_%u_%u",row,part);return expanded_leaderboard::Hash(key);}
Object AnyObject(const char* screen,unsigned key){return reinterpret_cast<Object(__cdecl*)(const char*,unsigned)>(0x524850)(screen,key);}
void RestoreRpd(const char* screen){
 if(!screen)return;
 rpdCycles.erase(screen);
 for(auto& entry:rpdVisibility)if(entry.first.first==screen){auto object=AnyObject(screen,entry.first.second);if(object&&object==entry.second.object)expanded_leaderboard::Show(object,entry.second.visible);}
 for(unsigned row=0;row<5;++row)for(unsigned part=0;part<4;++part)expanded_leaderboard::Show(AnyObject(screen,RpdRowKey(row,part)),false);
}
std::string NativeTimeText(Object object,unsigned depth=0){
 if(!object||depth>8)return {};
 if(object[6]==5){auto child=reinterpret_cast<Object>(object[25]);for(unsigned n=0;child&&n<object[24]&&n<128;++n,child=reinterpret_cast<Object>(child[1])){auto text=NativeTimeText(child,depth+1);if(!text.empty())return text;}return {};}
 if(object[6]!=2)return {};
 auto text=Read<const wchar_t*>(object,0x64);std::string result;
 for(unsigned i=0;text&&i<63;++i){auto c=Read<wchar_t>(text,i*2);if(!c)return result;if(c>127)return {};result+=char(c);}return {};
}
Object ObjectFor(const char* package,unsigned key){
 auto object=reinterpret_cast<Object(__cdecl*)(const char*,unsigned)>(0x524850)(package,key);
 return object&&Read<unsigned>(object,0x18)==2?object:nullptr;
}
std::string Encode(const std::wstring& text,const std::string& path){
 auto& codec=event_titles::NativeCodec(path);std::string result;
 for(auto c:text){auto it=codec.find(c);if(it==codec.end()){
   if(c==L'\u00b0'){result+=" deg";continue;}
   throw std::runtime_error("Preview record glyph unavailable");
  }result+=it->second;
 }return result;
}
void RpdFields(const char* screen,const mwel::EventPackage* package){
 if(!package||!mwel_rpd::CaseFile(package->authoring))return;
 const bool inGame=_stricmp(screen,"InGameRaceSheet.fng")==0;
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto panel=engine?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,screen):nullptr;
 if(!panel)return;
 // Both shipped FNGs have identical cash-row parents. Clone their actual
 // visible/shadow strings, keeping the installation's font and alignment.
 const unsigned sourceKeys[]={0x0B27902C,inGame?0x140A9331u:0x612D0E96u,0x03A80495,inGame?0xB5D80380u:0xFFBBBF24u};
 Object sources[4]{};float x[4]{},y[4]{};
 auto pos=reinterpret_cast<void(__cdecl*)(void*,float*,float*)>(0x514FD0);
 auto move=reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x524CC0);
 for(unsigned part=0;part<4;++part){sources[part]=ObjectFor(screen,sourceKeys[part]);if(!sources[part])return;pos(sources[part],&x[part],&y[part]);}
 auto best=NativeTimeText(AnyObject(screen,TimeValue));if(best.empty())best="-:--.--";
 const auto& a=package->authoring;const auto path=event_titles::NativeLanguagePath();
 auto lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
 const bool ja=lower.find("japanese")!=std::string::npos,free=mwel_rpd::FreeRun(a),sprint=mwel_rpd::PoliceSprint(a);
 const wchar_t* en[]={L"BOUNTY",L"VEHICLE",L"BEST TIME",L"EVENT",L"RACERS"};
 const wchar_t* jp[]={L"\u30d0\u30a6\u30f3\u30c6\u30a3",L"\u4f7f\u7528\u8eca\u4e21",L"\u30d9\u30b9\u30c8\u30bf\u30a4\u30e0",L"\u30a4\u30d9\u30f3\u30c8",L"\u51fa\u73fe\u53f0\u6570"};
 const auto preset=a.value("policeVehicle",std::string());
 auto vehicle=MWELRpdPresetLabel(preset);if(vehicle.empty())vehicle=rpd_interceptor_policy::VehicleName(preset);
 if(mwel_rpd::InGameSelected(a))vehicle=Encode(ja?L"\u51fa\u52d5\u6642\u306b\u9078\u629e":L"At dispatch",path);
 const auto rule=Encode(ja?(free?L"\u30d5\u30ea\u30fc\u30e9\u30f3":L"\u30b9\u30d7\u30ea\u30f3\u30c8"):(free?L"FREE RUN":L"SPRINT"),path);
 const std::string values[]={std::to_string(a.at("cashValue").get<unsigned>()),vehicle,best,rule,std::to_string(a.at("racerCount").get<unsigned>()-1)};
 RpdCycle cycle;cycle.panel=panel;cycle.started=GetTickCount64();
 for(unsigned row=0;row<5;++row)cycle.primary[row]={Encode(ja?jp[row]:en[row],path),values[row]};
 cycle.secondary=cycle.primary;
 if(sprint){
  event_preview_stats::Record record;const auto identity=profile_progress::CurrentIdentity();
  if(!identity.empty()){mwel::ProfileSidecar sidecar(mwel_paths::Profiles(game),identity);record=sidecar.Preview(*package);}
  cycle.secondary[2]={Encode(ja?L"\u30d9\u30b9\u30c8\u902e\u6355\u6570":L"BEST ARRESTS",path),
   Encode(event_preview_stats::Arrests(record,a.at("racerCount").get<unsigned>()-1),path)};
 }
 cycle.secondary[4]={Encode(ja?L"\u5473\u65b9\u8ffd\u8de1\u8eca\u4e21\u6570":L"ALLIED PURSUIT CARS",path),std::to_string(mwel_rpd::Allies(a))};
 for(unsigned row=0;row<5;++row)for(unsigned part=0;part<4;++part){
  auto object=expanded_leaderboard::Clone(panel,sources[part],RpdRowKey(row,part));if(!object)return;
  // Five rows occupy the former four-row height; native list and frame stay put.
  move(object,x[part],y[part]+row*20.f);
  const auto& label=cycle.primary[row][part<2?0:1];
  expanded_leaderboard::Text(object,label.c_str());expanded_leaderboard::Show(object,true);
 }
 // Hide the old animated records and distance/lap alternatives at their
 // parents, so a later animation cannot draw over our five fixed rows.
 const unsigned hidden[]={sourceKeys[0],sourceKeys[1],sourceKeys[2],sourceKeys[3],0x9CA11B4A,0x00009B21,0xB3962C89,0xABB70392};
 for(auto key:hidden)if(auto object=AnyObject(screen,key)){
  auto identity=std::make_pair(std::string(screen),key);auto& saved=rpdVisibility[identity];
  if(saved.object!=object)saved={object,!(object[7]&1)};
  expanded_leaderboard::Show(object,false);
 }
 rpdCycles[screen]=std::move(cycle);
 Log("RPD_PREVIEW event=%s bounty=%u vehicle=%s rule=%s racers=%u allies=%u fixed_rows=5 cycle_ms=3000 best_arrests=%u",package->id.c_str(),a.at("cashValue").get<unsigned>(),vehicle.c_str(),free?"freerun":"sprint",a.at("racerCount").get<unsigned>()-1,mwel_rpd::Allies(a),unsigned(sprint));
}

void Frame(){
 if(rpdCycles.empty())return;
 const auto now=GetTickCount64();
 for(auto it=rpdCycles.begin();it!=rpdCycles.end();){
  auto& cycle=it->second;const bool alternate=((now-cycle.started)/3000)%2!=0;
  if(alternate==cycle.alternate){++it;continue;}
  // Resolve the live screen and cloned objects at each change. Never retain
  // writable FNG object pointers across a close/reopen or selection change.
  auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
  auto panel=engine?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,it->first.c_str()):nullptr;
  if(!panel||panel!=cycle.panel){it=rpdCycles.erase(it);continue;}
  bool valid=true;
  for(unsigned row:{2u,4u})if(cycle.primary[row]!=cycle.secondary[row]){
   Object objects[4]{};
   for(unsigned part=0;part<4;++part){objects[part]=ObjectFor(it->first.c_str(),RpdRowKey(row,part));if(!objects[part])valid=false;}
   if(!valid)break;
   const auto& text=alternate?cycle.secondary[row]:cycle.primary[row];
   for(unsigned part=0;part<4;++part)expanded_leaderboard::Text(objects[part],text[part<2?0:1].c_str());
  }
  if(!valid){it=rpdCycles.erase(it);continue;}
  cycle.alternate=alternate;++it;
 }
}

void Apply(const char* packageName,void* params){
 // Other panels share this native list method. Do not rewrite milestone,
 // bounty or third-party screens merely because they reuse field hashes.
 char name[64]{};if(!packageName||!Copy(packageName,name,63))return;
 const bool safeHouse=_stricmp(name,"SafeHouseRaceSheet.fng")==0;
 const bool inGame=_stricmp(name,"InGameRaceSheet.fng")==0;
 if(!safeHouse&&!inGame)return;
 const auto selected=params?event_presentation::PackageFor(params):nullptr;
 RpdFields(packageName,selected);
 if(selected&&mwel_rpd::CaseFile(selected->authoring))return;
 // InGameB.lzc has separate visible/shadow labels, unlike FrontB.lzc.
 // Its native constructor 00597DB0 writes the same value object hashes.
 const unsigned timeLabels[]={safeHouse?TimeLabel:0x24ED7328u,inGame?0xBA6D88C2u:0u};
 const unsigned topLabels[]={safeHouse?TopLabel:0x33081D4Fu,inGame?0xDF195EA4u:0u};
 auto timeValue=ObjectFor(packageName,TimeValue),topValue=ObjectFor(packageName,TopValue);
 if(!timeValue||!topValue)return;
 for(auto key:timeLabels)if(key&&!ObjectFor(packageName,key))return;
 for(auto key:topLabels)if(key&&!ObjectFor(packageName,key))return;
 const auto package=params?event_presentation::PackageFor(params):nullptr;
 const auto mode=package?package->authoring.value("combatMode",std::string("none")):"none";
 const bool rpd=package&&mwel_rpd::CaseFile(package->authoring);
 const bool drift=mode=="drift",hot=mode=="hotpursuit"&&!rpd;
 // Always reset the native labels on selection. A normal race must not
 // inherit the preceding Drift/Hot Pursuit label in this persistent FNG.
 auto label=reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x525220);
 for(auto key:timeLabels)if(key)label(packageName,key,drift?angleKey:hot?hotKey:NativeTime);
 for(auto key:topLabels)if(key)label(packageName,key,drift?pointsKey:NativeTop);
 if(!drift&&!hot)return;
 const auto path=event_titles::NativeLanguagePath();auto lower=path;
 std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
 event_preview_stats::Record record;auto identity=profile_progress::CurrentIdentity();
 if(!identity.empty()){
  // Sidecar reads are selection-driven, never diagnostic-logger driven and
  // never per frame. No native save, native rank or completion flag is written.
  mwel::ProfileSidecar sidecar(mwel_paths::Profiles(game),identity);record=sidecar.Preview(*package);
 }
 if(drift){
  const auto angle=Encode(event_preview_stats::Angle(record),path),points=Encode(event_preview_stats::Points(record),path);
  expanded_leaderboard::Text(timeValue,angle.c_str());expanded_leaderboard::Text(topValue,points.c_str());
 }else{
  const auto value=Encode(event_preview_stats::Hot(record,lower.find("japanese")!=std::string::npos),path);
  expanded_leaderboard::Text(timeValue,value.c_str());
 }
 Log("EVENT_PREVIEW_STATS screen=%s event=%s mode=%s drift_record=%u hot_record=%u native_average_speed_retained=1 completion_flags_written=0",name,package->id.c_str(),mode.c_str(),unsigned(record.drift),unsigned(record.hotPursuit));
}
void __fastcall Refresh(void* self,void*){
 RestoreRpd(Read<const char*>(static_cast<BYTE*>(self)-0x1C));
 originalRefresh(self);
 try{auto item=Read<BYTE*>(self,0x18);Apply(Read<const char*>(static_cast<BYTE*>(self)-0x1C),Read<void*>(item,0x18));}
 catch(const std::exception& error){Log("EVENT_PREVIEW_STATS_FAILED reason=%s",error.what());}
}
void* __fastcall Details(void* self,void*,void* argument){
 RestoreRpd(Read<const char*>(self,0x10));
 auto result=originalDetails(self,argument);
 try{
  // Native 00597DEA/00597DED/00597DF8: self+2c owns the selected event
  // key; GRaceDatabase::GetRaceParameters resolves it through the database.
  auto database=Read<void*>(reinterpret_cast<void*>(0x91E004));
  const auto key=Read<unsigned>(self,0x2C);
  auto params=database&&key?reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(0x6022D0)(database,key):nullptr;
  Apply(Read<const char*>(self,0x10),params);
 }catch(const std::exception& error){Log("EVENT_PREVIEW_STATS_FAILED screen=InGameRaceSheet reason=%s",error.what());}
 return result;
}
bool Install(){
 if(installed)return true;
 if(!std::any_of(packages.begin(),packages.end(),[](const auto& p){auto mode=p.authoring.value("combatMode",std::string("none"));return mode=="drift"||mode=="hotpursuit";}))return true;
 const BYTE guard[]={0x83,0xEC,0x4C,0x53,0x56,0x57,0x8B,0xF1,0xE8,0xD3,0xC1,4,0};
 auto site=reinterpret_cast<void*>(0x547730);if(memcmp(site,guard,sizeof(guard)))return false;
 const BYTE detailsGuard[]={0x6A,0xFF,0x68,0x2C,0x2D,0x87,0,0x64,0xA1,0,0,0,0};
 auto detailsSite=reinterpret_cast<void*>(0x597DB0);if(memcmp(detailsSite,detailsGuard,sizeof(detailsGuard)))return false;
 struct Label {const char* id;const wchar_t* en;const wchar_t* ja;unsigned* key;};
 const Label labels[]={
  {"MWEL_PREVIEW_DRIFT_ANGLE",L"BEST ANGLE",L"\u30d9\u30b9\u30c8\u30a2\u30f3\u30b0\u30eb",&angleKey},
  {"MWEL_PREVIEW_DRIFT_POINTS",L"BEST POINTS",L"\u30d9\u30b9\u30c8\u30dd\u30a4\u30f3\u30c8",&pointsKey},
  {"MWEL_PREVIEW_HOT_SURVIVORS",L"BEST SURV./RANK",L"\u30d9\u30b9\u30c8\u751f\u5b58/\u9806\u4f4d",&hotKey}
 };
 for(const auto& item:labels){*item.key=event_presentation::Hash(item.id);if(!event_titles::titles.emplace(*item.key,event_titles::Title{item.en,item.id,{},item.ja}).second)return false;}
 if(MH_CreateHook(site,Refresh,reinterpret_cast<void**>(&originalRefresh))!=MH_OK)return false;
 if(MH_CreateHook(detailsSite,Details,reinterpret_cast<void**>(&originalDetails))!=MH_OK){MH_RemoveHook(site);originalRefresh=nullptr;return false;}
 if(MH_EnableHook(site)!=MH_OK||MH_EnableHook(detailsSite)!=MH_OK){MH_DisableHook(site);MH_DisableHook(detailsSite);MH_RemoveHook(site);MH_RemoveHook(detailsSite);originalRefresh=nullptr;originalDetails=nullptr;return false;}
 installed=true;return true;
}
}
