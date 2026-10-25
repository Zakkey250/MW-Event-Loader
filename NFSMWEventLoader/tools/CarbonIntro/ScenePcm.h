#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
// C++17 counterpart of NFRR's EncounterWave parser. Private PCM16 only.
namespace scene_pcm {
struct Format { unsigned rate=0,channels=0;size_t offset=0,bytes=0; };
inline bool Parse(const std::vector<unsigned char>& b,Format& out){
 out={};
 auto u16=[&](size_t p){return unsigned(b[p])|(unsigned(b[p+1])<<8);};
 auto u32=[&](size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);};
 if(b.size()<44||b.size()>8*1024*1024||memcmp(b.data(),"RIFF",4)||memcmp(b.data()+8,"WAVE",4))return false;
 const uint64_t end=uint64_t(u32(4))+8;if(end>b.size()||end<12)return false;
 bool format=false,data=false;
 for(size_t p=12;p+8<=end;){auto n=u32(p+4);auto start=p+8;if(n>end-start)return false;
  if(!memcmp(b.data()+p,"fmt ",4)){
   if(format||n<16||u16(start)!=1)return false;
   out.channels=u16(start+2);out.rate=u32(start+4);
   if((out.channels!=1&&out.channels!=2)||u16(start+14)!=16||out.rate<8000||out.rate>96000||u16(start+12)!=out.channels*2||u32(start+8)!=out.rate*out.channels*2)return false;
   format=true;
  }else if(!memcmp(b.data()+p,"data",4)){
   if(data||!n)return false;out.offset=start;out.bytes=n;data=true;
  }
  p=start+n+(n&1);if(p>end)return false;
 }
 return format&&data&&out.bytes%(out.channels*2)==0&&out.bytes<=out.rate*out.channels*2*60;
}
}
