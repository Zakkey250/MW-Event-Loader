#pragma once
#include "RpdIntroMusicPolicy.h"
// Native PC SndStrmWrapper, verified against the EN1.3 executable and its
// original call sites. The private wrapper owns one native stream and buffer.
// No waveOut, external player, PATH bank edits, user settings or audio worker.
namespace rpd_intro_music {
using combat_health_runtime::Read;
struct Track {std::string relative;unsigned duration=0;};
std::vector<Track> tracks;
bool prepared=false,surface=false,started=false,done=false,handoff=false,streamPaused=false;
unsigned raceOwner=0,generationOwner=0,musicWrapper=0;int musicHandle=-1,lastGain=-1;
ULONGLONG handoffAt=0,nextMusicHold=0,nextStatus=0;size_t selected=0;
std::vector<size_t> bag;std::mt19937 random{unsigned(GetTickCount64())^GetCurrentProcessId()};
alignas(16) std::array<unsigned,8> stream{};
bool Surface(){
 struct Code {unsigned at;std::initializer_list<BYTE> bytes;};
 const Code codes[]={
  {0x4B2100,{0x8B,0xC1,0xC7,0x40,0x14,0xFF,0xFF,0xFF,0xFF}},
  {0x4BAAF0,{0x83,0xEC,0x18,0x53,0x55,0x56,0x57}},
  {0x4B2230,{0x8B,0x44,0x24,0x08,0x8B,0x54,0x24,0x04}},
  {0x4B21B0,{0x83,0xEC,0x1C,0x56,0x8B,0xF1}},
  {0x4B22E0,{0x8B,0x44,0x24,0x04,0x85,0xC0,0x56,0x57}},
  {0x4BADC0,{0x56,0x8B,0xF1,0x8B,0x46,0x14,0x85,0xC0}},
  {0x4B24F0,{0x56,0x8B,0xF1,0xE8,0xD3,0x97,0x36,0}},
  {0x4B2510,{0x56,0x8B,0xF1,0xE8,0xB3,0x97,0x36,0}},
  {0x4B2450,{0x56,0x8B,0xF1,0xE8,0x73,0x98,0x36,0}},
  {0x4B2480,{0x56,0xE8,0x45,0x98,0x36,0}},
  {0x4C7D53,{0xC7,0x00,0x94,0x74,0x89,0}},
  {0x4D0E17,{0x8B,0x0D,0xA8,0x1F,0x91,0,0x8B,0x81,0x80,0,0,0}},
 };
 for(const auto& code:codes)if(memcmp(reinterpret_cast<void*>(code.at),code.bytes.begin(),code.bytes.size()))return false;
 return true;
}
unsigned MusicWrapper(){
 unsigned sound=0,manager=0,channel=0;int handle=-1;
 if(!Read(0x911FA8,sound)||!sound||!Read(sound+0x80,manager)||!manager||!Read(manager+0x14,channel)||!gauntlet_runtime::Table(channel,0x897494)||!Read(channel+4+0x14,handle)||handle<0)return 0;
 return channel+4; // EAXS_StreamChannel has its SndStrmWrapper at +4.
}
bool MusicPausedByGame(){unsigned mask=0;return !Read(0x911FF0,mask)||(mask&2)!=0;}
void ReleaseMusic(){
 int current=-1;
 if(musicWrapper&&MusicWrapper()==musicWrapper&&Read(musicWrapper+0x14,current)&&current==musicHandle){
  // An independent native pause owns resumption if it is still in effect.
  const bool nativePause=MusicPausedByGame();
  if(!nativePause)reinterpret_cast<void(__thiscall*)(unsigned)>(0x4B2510)(musicWrapper);
  Log("RPD_INTRO_MUSIC_RELEASE native_music_handle=%d resumed=%u native_pause_retained=%u settings_written=0",musicHandle,unsigned(!nativePause),unsigned(nativePause));
 }
 musicWrapper=0;musicHandle=-1;nextMusicHold=0;
}
void HoldMusic(){
 const auto wrapper=MusicWrapper();int handle=-1;
 if(!wrapper||!Read(wrapper+0x14,handle))return;
 if(musicWrapper!=wrapper||musicHandle!=handle){musicWrapper=0;musicHandle=-1;nextMusicHold=0;}
 if(MusicPausedByGame())return;
 // Native stage/countdown may change its own pause. Reassert only while
 // this scoped jingle owns the channel, without writing shared pause flags.
 if(GetTickCount64()<nextMusicHold)return;nextMusicHold=GetTickCount64()+50;
 reinterpret_cast<void(__thiscall*)(unsigned)>(0x4B24F0)(wrapper);
 if(!musicWrapper)Log("RPD_INTRO_MUSIC_HELD native_music_handle=%d stream_only=1 settings_written=0",handle);
 musicWrapper=wrapper;musicHandle=handle;
}
void CloseStream(){
 if(started){reinterpret_cast<void(__thiscall*)(void*)>(0x4BADC0)(stream.data());started=false;}
 stream={};stream[5]=~0u;streamPaused=false;lastGain=-1;
}
void Reset(){CloseStream();ReleaseMusic();raceOwner=generationOwner=0;done=handoff=false;handoffAt=nextStatus=0;}
void Status(){
 if(!worldLogEnabled||!started||GetTickCount64()<nextStatus)return;nextStatus=GetTickCount64()+1000;
 struct StreamStatus {int outstanding,current,buffered;} status{};
 struct RequestStatus {int state,current,remaining,buffered;} request{};
 const int result=reinterpret_cast<int(__thiscall*)(void*,StreamStatus*)>(0x4B2450)(stream.data(),&status);
 int requestResult=-1;if(result>=0&&status.current>=0)requestResult=reinterpret_cast<int(__thiscall*)(void*,int,RequestStatus*)>(0x4B2480)(stream.data(),status.current,&request);
 unsigned mask=0;Read(0x911FF0,mask);
 Log("RPD_INTRO_JINGLE_STATUS track=%u result=%d request_result=%d request=%d state=%d current_ms=%d remaining_ms=%d buffered_ms=%d music_wrapper=%08X music_pause_mask=%X owned_music_pause=%u",unsigned(selected+1),result,requestResult,status.current,request.state,request.current,request.remaining,request.buffered,MusicWrapper(),mask,unsigned(musicWrapper!=0));
}
bool Prepare(){
 if(prepared)return !tracks.empty();prepared=true;surface=Surface();
 if(!surface){Log("RPD_INTRO_JINGLE_UNAVAILABLE native_stream_surface=0 music_unchanged=1");return false;}
 try{
  const auto root=mwel_paths::Root(game)/L"Runtime"/L"RpdJingles";
  if(!mwel::fs::exists(root/L"catalog.json"))return false;
  const auto catalog=mwel::Json::parse(mwel::Read(root/L"catalog.json",32768));
  if(catalog.at("version")!=1||!catalog.at("tracks").is_array()||catalog.at("tracks").empty()||catalog.at("tracks").size()>64)throw std::runtime_error("Invalid RPD jingle catalog");
  for(const auto& row:catalog.at("tracks")){
   const auto name=row.at("file").get<std::string>();const auto duration=row.at("durationMs").get<unsigned>();
   if(!rpd_intro_music_policy::FileName(name)||duration<1000||duration>120000)throw std::runtime_error("Invalid RPD jingle entry");
   const auto path=root/name;const auto size=mwel::fs::file_size(path);
   if(size<20||size>8*1024*1024)throw std::runtime_error("Invalid RPD jingle size");
   std::ifstream input(path,std::ios::binary);char header[12]{};input.read(header,12);
   if(input.gcount()!=12||memcmp(header,"SCHl",4)||memcmp(header+8,"PT\0\0",4))throw std::runtime_error("Invalid RPD native jingle header");
   tracks.push_back({"scripts\\EventLoader\\Runtime\\RpdJingles\\"+name,duration});
  }
  Log("RPD_INTRO_JINGLES_READY tracks=%u native_stream=1 shared_rpd_start_hook=1",unsigned(tracks.size()));
 }catch(const std::exception& e){tracks.clear();Log("RPD_INTRO_JINGLE_UNAVAILABLE reason=%s music_unchanged=1",e.what());}
 return !tracks.empty();
}
bool Queue(){
 const int request=reinterpret_cast<int(__thiscall*)(void*,const char*,int,int)>(0x4B2230)(stream.data(),tracks[selected].relative.c_str(),0,0);
 Log("RPD_INTRO_JINGLE_QUEUE track=%u native_stream_handle=%d request=%d",unsigned(selected+1),int(stream[5]),request);
 return request>=0;
}
bool Begin(){
 if(!Prepare())return false;
 if(bag.empty()){for(size_t i=0;i<tracks.size();++i)bag.push_back(i);std::shuffle(bag.begin(),bag.end(),random);}
 selected=bag.back();bag.pop_back();stream={};
 reinterpret_cast<void(__thiscall*)(void*)>(0x4B2100)(stream.data());
 const int handle=reinterpret_cast<int(__thiscall*)(void*,int,int,int)>(0x4BAAF0)(stream.data(),8,32,0x10000);
 if(handle<0){Log("RPD_INTRO_JINGLE_UNAVAILABLE native_create_result=%d music_unchanged=1",handle);return false;}
 started=true;
 if(!Queue()){CloseStream();return false;}
 HoldMusic();Log("RPD_INTRO_JINGLE_BEGIN track=%u duration_ms=%u native_stream_handle=%d external_player=0",unsigned(selected+1),tracks[selected].duration,handle);return true;
}
float UserGain(){
 unsigned sound=0,settings=0;float master=0,music=0;
 if(!Read(0x911FA8,sound)||!Read(sound+0x24,settings)||!Read(settings,master)||!Read(settings+12,music)||!std::isfinite(master)||!std::isfinite(music))return 0;
 return std::clamp(master,0.f,1.f)*std::clamp(music,0.f,1.f);
}
void Go(){
 if(!raceOwner||done)return;
 if(started){reinterpret_cast<int(__thiscall*)(void*,int,int)>(0x4B22E0)(stream.data(),0,0);Log("RPD_INTRO_JINGLE_FADE_DONE native_go=1 gain=0");}
 CloseStream();done=true;handoff=true;handoffAt=GetTickCount64();
}
void AfterGo(){
 if(!handoff)return;
 unsigned race=0;if(!Read(0x91E000,race)||race!=raceOwner){Reset();return;}
 unsigned sound=0,music=0,mode=0;
 if(Read(0x911FA8,sound)&&sound)music=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x4AEAA0)(sound,0x40010010);
 const bool pursuit=gauntlet_runtime::Table(music,0x899B80)&&Read(music+0x148,mode)&&mode==1;
 if(pursuit||!rpd_mission::Current()||GetTickCount64()-handoffAt>=2000){
  ReleaseMusic();handoff=false;Log("RPD_INTRO_JINGLE_HANDOFF native_pursuit_music=%u wait_ms=%llu forced_music_event=0",unsigned(pursuit),GetTickCount64()-handoffAt);
 }else HoldMusic();
}
void Frame(unsigned race,unsigned generation,float elapsed,float countdown,float go,bool skipped){
 if(combat_health_runtime::gameThread&&GetCurrentThreadId()!=combat_health_runtime::gameThread)return;
 const auto policy=event_race_policies::Current();
 if(!policy||!mwel_rpd::CaseFile(policy->authoring)){if(raceOwner)Reset();return;}
 if(!race||!std::isfinite(elapsed)||!std::isfinite(countdown)||!std::isfinite(go)||go<=countdown||countdown<0){if(raceOwner)Reset();return;}
 if(raceOwner!=race||generationOwner!=generation){Reset();raceOwner=race;generationOwner=generation;done=skipped;if(!done&&!Begin())done=true;}
 if(handoff){AfterGo();return;}if(done||!started)return;
 if(elapsed>=go){Go();return;}
 HoldMusic();
 Status();
 BYTE paused=0;Read(0x91CAE4,paused);
 if(bool(paused)!=streamPaused){reinterpret_cast<void(__thiscall*)(void*)>(paused?0x4B24F0:0x4B2510)(stream.data());streamPaused=bool(paused);}
 const int gain=int(std::lround(100*UserGain()*rpd_intro_music_policy::Gain(elapsed,countdown,go)));
 if(gain!=lastGain){reinterpret_cast<int(__thiscall*)(void*,int,int)>(0x4B22E0)(stream.data(),gain,0);lastGain=gain;
  if(elapsed>=countdown)Log("RPD_INTRO_JINGLE_FADE elapsed=%.3f countdown=%.3f go=%.3f gain=%d",elapsed,countdown,go,gain);
 }
 if(elapsed<countdown&&!reinterpret_cast<bool(__thiscall*)(void*)>(0x4B21B0)(stream.data()))Queue();
}
}
