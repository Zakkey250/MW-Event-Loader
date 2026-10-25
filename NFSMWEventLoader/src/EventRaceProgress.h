#pragma once
#include "RouteProgress.h"
namespace event_race_progress {
struct Course {std::vector<mwel_progress::Leg> legs;unsigned laps=1;};
std::map<unsigned,Course> courses;
bool failed=false;
unsigned segmentsOffset=0x443C,segmentCapacity=18;
void* lengthSite=reinterpret_cast<void*>(0x5FEF30);
void(__thiscall* originalUpdate)(void*,float)=nullptr;
void(__thiscall* originalLength)(void*)=nullptr;
template<class T>T Read(void* p,unsigned offset=0){T v{};if(p)Copy(static_cast<BYTE*>(p)+offset,&v,sizeof(v));return v;}
struct Reader {
 std::string bytes;size_t cursor=0;
 template<class T>T Next(){T value{};if(sizeof(T)>bytes.size()-cursor)throw std::runtime_error("Truncated authored progress route");memcpy(&value,bytes.data()+cursor,sizeof(T));cursor+=sizeof(T);return value;}
 mwel_progress::Path Path(unsigned count){if(count<2||count>8192)throw std::runtime_error("Authored progress point count");mwel_progress::Path points;points.reserve(count);
  for(unsigned i=0;i<count;++i){mwel_progress::Point p{};for(auto& v:p.position){v=Next<float>();if(!std::isfinite(v)||fabs(v)>100000)throw std::runtime_error("Authored progress coordinate");}
   if(i){float d=0;for(unsigned k=0;k<3;++k){float x=p.position[k]-points.back().position[k];d+=x*x;}p.station=points.back().station+std::sqrt(d);}points.push_back(p);
  }if(points.back().station<=.001f)throw std::runtime_error("Zero length progress path");return points;
 }
 void End(){if(cursor!=bytes.size())throw std::runtime_error("Trailing progress route bytes");}
};
void Prepare(){
 for(auto& package:packages){
  unsigned key=mwel::Correction(package.authoring)?std::stoul(package.authoring.at("correctionTarget").at("hash").get<std::string>(),nullptr,16):registration_experiment::Hash(package.nativeNamespace.c_str());
  if(!package.assets.contains("course/ai-recovery.bin")){
   // Native-AI exports intentionally omit NeoAI navigation. The game's own
   // checkpoint path remains authoritative for these events. Requiring a
   // NeoAI recovery asset here silently disabled the entire loader before
   // corrections could be registered, including automatically selected intros.
   if(package.authoring.value("aiRouteMode",std::string("native"))!="native")throw std::runtime_error("Authored progress route missing");
   Log("EVENT_NATIVE_PROGRESS_RETAINED key=%08X reason=native_guidance_without_authored_route native_checkpoint_measurement=1",key);continue;
  }
  Reader r{mwel::Read(package.directory/L"course"/L"ai-recovery.bin",16*1024*1024)};
  if(r.Next<unsigned>()!=0x43523257||r.Next<unsigned>()!=1||r.Next<unsigned>()!=key)throw std::runtime_error("Authored progress route identity");
  unsigned count=r.Next<unsigned>();if(!count||count>segmentCapacity)throw std::runtime_error("Authored segments exceed this host's native checkpoint capacity");
  Course course;course.laps=mwel::RaceLaps(package.authoring);
  for(unsigned i=0;i<count;++i){mwel_progress::Leg leg;leg.main=r.Path(r.Next<unsigned>());course.legs.push_back(std::move(leg));}r.End();
  if(package.assets.contains("course/ai-branches.bin")){
   Reader branches{mwel::Read(package.directory/L"course"/L"ai-branches.bin",32*1024*1024)};
   if(branches.Next<unsigned>()!=0x56423257)throw std::runtime_error("Progress branch magic");unsigned version=branches.Next<unsigned>();
   if(version<1||version>3||branches.Next<unsigned>()!=key||branches.Next<unsigned>()!=count)throw std::runtime_error("Progress branch identity");
   for(unsigned leg=0;leg<count;++leg){unsigned n=branches.Next<unsigned>();if(n>4096)throw std::runtime_error("Progress branch count");
    for(unsigned i=0;i<n;++i){unsigned start=0,end=unsigned(course.legs[leg].main.size()-1),points=0;
     if(version==1){unsigned guides=branches.Next<unsigned>();points=branches.Next<unsigned>();if(guides>256)throw std::runtime_error("Progress guide count");for(unsigned g=0;g<guides*6;++g)branches.Next<float>();}
     else{start=branches.Next<unsigned>();end=branches.Next<unsigned>();points=branches.Next<unsigned>();if(version==3)branches.Next<unsigned>();}
     auto& main=course.legs[leg].main;if(start>=end||end>=main.size())throw std::runtime_error("Progress branch interval");
     auto path=branches.Path(points);float gap=0;for(unsigned k=0;k<3;++k){float a=path.front().position[k]-main[start].position[k],b=path.back().position[k]-main[end].position[k];gap+=a*a+b*b;}if(gap>.0001f)throw std::runtime_error("Progress branch endpoints");
     float length=path.back().station,begin=main[start].station,span=main[end].station-begin;for(auto& p:path)p.station=begin+span*p.station/length;
     course.legs[leg].alternatives.push_back(std::move(path));
    }
   }branches.End();
  }
  unsigned checkpoints=0;for(auto& gate:package.authoring.at("gates"))if(gate.at("kind")=="checkpoint")++checkpoints;
  if(count!=checkpoints+1)throw std::runtime_error("Progress legs do not match native checkpoint order");
  // Older packages predate the explicit preview distance. Their verified
  // navigation paths already carry the authored geometry, so derive the
  // menu attribute in memory without rewriting any installed event files.
  if(!package.authoring.contains("raceLengthMetres")){
   float metres=0;for(const auto& leg:course.legs)metres+=leg.Length();metres*=course.laps;
   if(!std::isfinite(metres)||metres<=0.f||metres>1000000.f)throw std::runtime_error("Authored progress distance invalid");
   package.authoring["raceLengthMetres"]=metres;
   Log("EVENT_AUTHORED_PREVIEW_DISTANCE_DERIVED key=%08X metres=%.3f installed_event_files_unchanged=1",key,double(metres));
  }
  Log("EVENT_AUTHORED_PROGRESS_PREPARED key=%08X legs=%u laps=%u controller_independent=1",key,count,course.laps);courses.emplace(key,std::move(course));
 }
}
void LengthUnsafe(void* self){
 if(self!=Read<void*>(reinterpret_cast<void*>(0x91E000)))return;auto found=courses.find(MWELNeoCurrentKey());if(found==courses.end())return;
 auto race=static_cast<BYTE*>(self);float total=0;unsigned i=0;for(auto& leg:found->second.legs){float length=leg.Length();memcpy(race+segmentsOffset+i++*4,&length,4);total+=length;}
 // Keep native setup/road flags and trigger registration. Replace only its
 // distance table with the actual authored road used by this owned event.
 memcpy(race+0x4488,&total,4);memcpy(race+0x448C,&total,4);float all=total*found->second.laps;memcpy(race+0x4484,&all,4);
 Log("EVENT_AUTHORED_RACE_LENGTH key=%08X lap_metres=%.3f total_metres=%.3f native_setup_preserved=1",found->first,total,all);
}
void UpdateUnsafe(void* self){
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto found=courses.find(MWELNeoCurrentKey());if(found==courses.end()||!race||Read<unsigned>(race,0x1960)!=1)return;
 unsigned count=Read<unsigned>(race,0x1958);if(!count||count>16)return;unsigned slot=16;
 for(unsigned i=0;i<count;++i)if(self==race+0x18+i*0x194){slot=i;break;}if(slot==16)return;
 auto info=static_cast<BYTE*>(self);if(Read<BYTE>(info,0x21)||Read<unsigned>(info,0x1C)||Read<BYTE>(info,0x190))return;
 NeoBodyMotion body{};if(!runtime_lifecycle::Body(Read<unsigned>(info),body))return;
 int cp=Read<int>(info,0x2C),lap=Read<int>(info,0x28);if(cp<0||lap<0)return;
 auto p=mwel_progress::Measure(found->second.legs,unsigned(cp),unsigned(lap),found->second.laps,{body.position[0],body.position[1],body.position[2]});if(!p.valid)return;
 // Native rank sorting and checkpoint/goal callbacks retain authority. These
 // three floats are measurements of the real body, never guessed CP or goals.
 memcpy(info+0xF8,&p.remaining,4);memcpy(info+0x24,&p.lapPercent,4);memcpy(info+0x18,&p.racePercent,4);
}
void Failure(){if(failed)return;failed=true;runtimeReady=false;runtime_lifecycle::enabled=false;race_start_experiment::aborted=true;Log("EXPERIMENT_RACE_ABORT authored_progress_fault");}
void __fastcall Length(void* self,void*){originalLength(self);__try{LengthUnsafe(self);}__except(EXCEPTION_EXECUTE_HANDLER){Failure();}}
void __fastcall Update(void* self,void*,float dt){originalUpdate(self,dt);__try{UpdateUnsafe(self);}__except(EXCEPTION_EXECUTE_HANDLER){Failure();}}
bool ResolveHost(){
 if(!MWELInstallCheckpointExtension())return false;
 mw_checkpoint::Runtime host{};
 if(MWEL_CheckpointRuntimeV1(&host,sizeof(host))){
  BYTE branch[5]{};if(!Copy(reinterpret_cast<void*>(0x5FEF30),branch,5)||branch[0]!=0xE9)return false;
  int displacement=0;memcpy(&displacement,branch+1,4);
  if(host.size!=sizeof(host)||unsigned(host.calculator)!=0x5FEF35+displacement||host.capacity<18||host.capacity>258||host.segmentsOffset<0x4558||host.segmentsOffset>1024*1024||host.raceBytes<host.segmentsOffset+host.capacity*4)return false;
  segmentsOffset=host.segmentsOffset;segmentCapacity=host.capacity;lengthSite=host.calculator;
 }
 Log("EVENT_PROGRESS_DISTANCE_HOST segments_offset=%X capacity=%u native_calculator=%08X world_dependency=0",segmentsOffset,segmentCapacity,unsigned(lengthSite));return true;
}
bool Install(){
 const BYTE updateGuard[]={0x83,0xEC,0x1C,0x56,0x8B,0xF1,0x8A,0x46,0x21};
 const BYTE lengthGuard[]={0x6A,0xFF,0x68,0x3B,0x61,0x87,0,0x64,0xA1,0,0,0,0};
 void* update=reinterpret_cast<void*>(0x5E92E0);void* length=lengthSite;
 if(memcmp(update,updateGuard,sizeof(updateGuard))||memcmp(length,lengthGuard,sizeof(lengthGuard)))return false;
 if(MH_CreateHook(update,Update,reinterpret_cast<void**>(&originalUpdate))!=MH_OK)return false;
 if(MH_CreateHook(length,Length,reinterpret_cast<void**>(&originalLength))!=MH_OK){MH_RemoveHook(update);return false;}
 if(MH_QueueEnableHook(update)!=MH_OK||MH_QueueEnableHook(length)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_DisableHook(update);MH_DisableHook(length);MH_RemoveHook(update);MH_RemoveHook(length);return false;}
 Log("EVENT_AUTHORED_PROGRESS_ACTIVE native_rank_sort_preserved=1 CP_writes=0 goal_writes=0 body_measurement=1 controller_independent=1");return true;
}
}
