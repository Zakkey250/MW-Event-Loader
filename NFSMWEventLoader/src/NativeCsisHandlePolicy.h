#pragma once
#include <cstdint>
namespace mwel_csis_handle {
enum class Membership { Unknown, Live, Retired };
// EN1.3's class-definition lookup (0083132B): linked bank node,
// WORD class count at -0x14, and 12-byte definition table at -8.
// Establish bank membership BEFORE reading the candidate pointer.
template<class Read> Membership ClassDefinition(std::uint32_t candidate,Read read){
 std::uint32_t node=0;if(!read(0x9C29E4,&node,4)||!node)return Membership::Unknown;
 for(unsigned n=0;node&&n<4096;++n){
  if(node<0x10020)return Membership::Unknown;
  std::uint32_t table=0,next=0;std::uint16_t count=0;
  if(!read(node-0x14,&count,2)||!read(node-8,&table,4)||!read(node,&next,4))return Membership::Unknown;
  if(count&&table<0x10000)return Membership::Unknown;
  const std::uint64_t end=std::uint64_t(table)+std::uint64_t(count)*12;
  if(end>0x100000000ull)return Membership::Unknown;
  if(candidate>=table&&std::uint64_t(candidate)<end&&(candidate-table)%12==0)return Membership::Live;
  if(next==node)return Membership::Unknown;
  node=next;
 }
 return node?Membership::Unknown:Membership::Retired;
}
}
