#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
namespace mwel_rpd_controls {
// EN1.3 UIGetControlText: 52-byte rows, four 12-byte input bindings.
// Native keyboard defaults (006284D0) bound the driving actions to 19 rows.
struct Row {unsigned name=0;std::array<unsigned,3> keyboardPrimary{},keyboardSecondary{},padPrimary{},padSecondary{};};
static_assert(sizeof(Row)==52&&offsetof(Row,padPrimary)==28);
constexpr unsigned DrivingActions=19;
using Rows=std::array<Row,DrivingActions>;
// Native input-table identity, not a keyboard/pad slot classification. The
// APEX5 runtime dump has configured pad bindings with only slot zero set.
using InputTable=std::array<unsigned,8>;
struct Lease {
 Rows saved{};InputTable inputTable{};bool held=false;
 void Clear(){held=false;}
 bool Capture(const Rows& rows,const InputTable& active){
  Clear();if(std::none_of(active.begin(),active.end(),[](unsigned p){return p!=0;}))return false;
  if(std::none_of(rows.begin(),rows.end(),[](const Row& r){return r.padPrimary[0]||r.padSecondary[0];}))return false;
  saved=rows;inputTable=active;held=true;return true;
 }
 int Restore(Rows& rows,const InputTable& active){
  if(!held)return 0;
  if(active!=inputTable){Clear();return -1;}
  for(unsigned i=0;i<rows.size();++i)if(rows[i].name!=saved[i].name){Clear();return -1;}
  int changed=0;for(unsigned i=0;i<rows.size();++i){
   if(rows[i].padPrimary!=saved[i].padPrimary){rows[i].padPrimary=saved[i].padPrimary;++changed;}
   if(rows[i].padSecondary!=saved[i].padSecondary){rows[i].padSecondary=saved[i].padSecondary;++changed;}
  }return changed;
 }
};
}
