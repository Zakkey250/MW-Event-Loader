#pragma once
#include "ScenePcm.h"
#include <mmsystem.h>
#include <array>
#pragma comment(lib,"winmm.lib")
// Optional, predecoded source track. No native sound slots, process-global
// PlaySound channel, device volume writes, logger or persistent worker.
struct SceneWave {
 struct Playback {
  HWAVEOUT device=nullptr;std::array<WAVEHDR,3> headers{};
  std::array<std::vector<unsigned char>,3> buffers;
  size_t cursor=0;bool paused=false;float lastScene=0;ULONGLONG advanced=0;
  ~Playback(){
   if(!device)return;waveOutReset(device);
   for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(device,&h,sizeof(h));
   waveOutClose(device);
  }
 };
 std::vector<unsigned char> source;scene_pcm::Format format;
 std::unique_ptr<Playback> playback;
 float start=0,end=0,gain=1;unsigned lane=0;bool attempted=false;
 // Only the main thread reads validated game settings. Never cache game pointers.
 static bool GameGain(unsigned lane,float& result) noexcept {
  __try{
   const unsigned char signature[]={0x8B,0x15,0xA8,0x1F,0x91,0,0x8B,0x42,0x24,0xD9,0x40,4,0xD8,8};
   if(memcmp(reinterpret_cast<void*>(0x4B5A7E),signature,sizeof(signature)))return false;
   auto manager=*reinterpret_cast<unsigned char**>(0x911FA8);if(!manager)return false;
   auto settings=*reinterpret_cast<unsigned char**>(manager+0x24);if(!settings)return false;
   float master=*reinterpret_cast<float*>(settings);
   float channel=*reinterpret_cast<float*>(settings+(lane==0?4:lane==1?16:28));
   if(!std::isfinite(master)||!std::isfinite(channel)||master<0||master>1||channel<0||channel>1)return false;
   result=master*channel;return true;
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
 void Load(const std::string& folder,float duration){
  Stop();source.clear();format={};
  const auto a=GetFileAttributesA((folder+"/audio.wav").c_str()),b=GetFileAttributesA((folder+"/audio-timing.bin").c_str());
  if(a==INVALID_FILE_ATTRIBUTES&&b==INVALID_FILE_ATTRIBUTES)return;
  if(a==INVALID_FILE_ATTRIBUTES||b==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Scene WAV/timing pair missing");
  std::ifstream timing(folder+"/audio-timing.bin",std::ios::binary);unsigned magic;
  SceneAssets::Read(timing,&magic);SceneAssets::Read(timing,&start);SceneAssets::Read(timing,&end);SceneAssets::Read(timing,&gain);SceneAssets::Read(timing,&lane);char extra;
  if(magic!=0x31554143||timing.read(&extra,1)||!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(gain)||start<0||end<=start||end>duration||gain<0||gain>1||lane>2)throw std::runtime_error("Invalid scene audio timing");
  std::ifstream f(folder+"/audio.wav",std::ios::binary|std::ios::ate);auto size=f.tellg();
  if(size<44||size>8*1024*1024)throw std::runtime_error("Scene PCM budget exceeded");
  source.resize(size_t(size));f.seekg(0);SceneAssets::Read(f,source.data(),source.size());
  if(!scene_pcm::Parse(source,format)||double(format.bytes)/(format.rate*format.channels*2)<end-start-.02)throw std::runtime_error("Invalid or short scene PCM16 WAV");
 }
 void Stop(){playback.reset();attempted=false;}
 bool Active()const{return bool(playback);}
 void Tick(float elapsed,float timeScale=100){
  if(source.empty())return;
  if(elapsed>=end){playback.reset();return;}
  if(timeScale!=100){playback.reset();attempted=true;return;} // unsupported source retiming stays silent
  if(elapsed<start)return;
  if(playback&&size_t((elapsed-start)*format.rate)*format.channels*2>playback->cursor+size_t(format.rate/4)*format.channels*2)Stop();
  float volume=0;GameGain(lane,volume);volume*=gain;
  if(!attempted){
   attempted=true;auto p=std::make_unique<Playback>();
   WAVEFORMATEX w{};w.wFormatTag=WAVE_FORMAT_PCM;w.nChannels=WORD(format.channels);w.nSamplesPerSec=format.rate;w.wBitsPerSample=16;w.nBlockAlign=WORD(format.channels*2);w.nAvgBytesPerSec=w.nSamplesPerSec*w.nBlockAlign;
   if(waveOutOpen(&p->device,WAVE_MAPPER,&w,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return;
   const size_t chunk=std::max(1u,format.rate/40)*w.nBlockAlign;
   for(unsigned i=0;i<3;i++){
    p->buffers[i].resize(chunk);auto& h=p->headers[i];h.lpData=reinterpret_cast<char*>(p->buffers[i].data());h.dwBufferLength=DWORD(chunk);
    if(waveOutPrepareHeader(p->device,&h,sizeof(h))!=MMSYSERR_NOERROR)return;
   }
   p->cursor=size_t((elapsed-start)*format.rate)*w.nBlockAlign;p->lastScene=elapsed;p->advanced=GetTickCount64();playback=std::move(p);
  }
  if(!playback)return;auto& p=*playback;auto now=GetTickCount64();
  if(elapsed>p.lastScene){p.lastScene=elapsed;p.advanced=now;if(p.paused){waveOutRestart(p.device);p.paused=false;}}
  else if(now-p.advanced>100&&!p.paused){waveOutPause(p.device);p.paused=true;}
  if(p.paused)return;
  const size_t limit=std::min(format.bytes,size_t((end-start)*format.rate)*format.channels*2);
  for(unsigned k=0;k<3;k++){
   auto& h=p.headers[k];if((h.dwFlags&WHDR_INQUEUE)||p.cursor>=limit)continue;
   auto bytes=std::min(p.buffers[k].size(),limit-p.cursor);
   for(size_t i=0;i<bytes;i+=2){int16_t sample;memcpy(&sample,source.data()+format.offset+p.cursor+i,2);sample=int16_t(std::lround(sample*volume));memcpy(p.buffers[k].data()+i,&sample,2);}
   h.dwBufferLength=DWORD(bytes);p.cursor+=bytes;
   if(waveOutWrite(p.device,&h,sizeof(h))!=MMSYSERR_NOERROR){playback.reset();return;}
  }
 }
};
