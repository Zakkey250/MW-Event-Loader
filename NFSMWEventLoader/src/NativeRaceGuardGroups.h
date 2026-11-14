#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace mwel_native_guards {
using Groups=std::array<bool,4096>;
inline unsigned Key(const std::string& name){unsigned h=~0u;for(unsigned char c:name)h=h*33+c;return h;}
// Scenery group number is also CARP instance +0x18. Match actual named
// BARRIER_SPLINE groups; CARP line bit 0x200 also marks ordinary world walls.
inline Groups Decode(const std::vector<unsigned char>& bytes){
 Groups out{};std::set<unsigned> names;for(unsigned i=0;i<4096;++i){names.insert(Key("BARRIER_SPLINE_"+std::to_string(i)));if(i<10)names.insert(Key("BARRIER_SPLINE_0"+std::to_string(i)));}
 auto read=[&](size_t o,size_t n){if(o>bytes.size()||n>bytes.size()-o)throw std::runtime_error("Native race guard group extent");unsigned v=0;memcpy(&v,bytes.data()+o,n);return v;};
 bool found=false;
 for(size_t o=0;o<bytes.size();){const auto tag=read(o,4),size=read(o+4,4);read(o+8,size?1:0);if(size>bytes.size()-(o+8))throw std::runtime_error("Native race guard chunk extent");
  if(tag==0x34109){if(found)throw std::runtime_error("Duplicate native race guard groups");found=true;const auto end=o+8+size;
   for(size_t p=o+8;p<end;){if(end-p<20)throw std::runtime_error("Native race guard record extent");auto key=read(p+8,4),group=read(p+12,2),count=read(p+14,2);const auto next=p+((20+count*2+3)&~size_t(3));if(group>=out.size()||next>end)throw std::runtime_error("Native race guard group binding");if(names.count(key))out[group]=true;p=next;}
  }o+=8+size;
 }if(!found)throw std::runtime_error("Native race guard group table absent");return out;
}
inline bool Retain(unsigned group,const Groups& guards,bool verifiedOfficer){return !verifiedOfficer||group==0||group>=guards.size()||!guards[group];}
}
