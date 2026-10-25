#pragma once
#include <array>
namespace police_deployment {
enum class Owner {Any,None,Gauntlet,HotPursuit};
struct Limit {unsigned offset=0,original=0,written=0;};
struct Lease {
 unsigned manager=0;Owner owner=Owner::None;std::array<Limit,4> limits{};
 template<class Read,class Write>void Release(unsigned live,Owner expected,Read read,Write write){
  if(expected!=Owner::Any&&owner!=expected)return;
  if(manager&&manager==live)for(const auto& row:limits){unsigned value=0;
   if(read(manager,row.offset,value)&&value==row.written)write(manager,row.offset,row.original);
  }
  manager=0;owner=Owner::None;limits={};
 }
 template<class Read,class Write>bool Acquire(unsigned live,Owner next,unsigned cap,Read read,Write write){
  if(!live||next==Owner::None||next==Owner::Any||!cap||cap>16)return false;
  if(manager!=live){
   Release(live,Owner::Any,read,write);
   constexpr unsigned offsets[]={0x84,0x8C,0x90,0x98};std::array<Limit,4> captured{};
   for(unsigned i=0;i<4;++i){unsigned value=0;if(!read(live,offsets[i],value)||value>64)return false;captured[i]={offsets[i],value,cap};}
   manager=live;limits=captured;
  }
  // A mode handoff changes the holder, not the native baseline. A late Reset
  // from the outgoing mode cannot release limits now held by the next mode.
  owner=next;
  for(auto& row:limits){unsigned value=0;if(!read(live,row.offset,value))return false;row.written=cap;
   if(value!=cap)write(live,row.offset,cap);
  }return true;
 }
};
}
