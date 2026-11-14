#include "RpdControlBindings.h"
#include <cstdio>
#include <stdexcept>
using namespace mwel_rpd_controls;
unsigned checks=0;void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void Apex5DumpRegression(){
 // Actual native globals in World2MW.exe.33696.dmp, 2026-11-03 18:15 JST.
 // This is a binding-table regression, not hardware or gameplay acceptance.
 const InputTable active{0x33A38390u};
 const Rows captured{{
  Row{0x8A5A98u,{1,0xC8,0},{},{4,0,2},{}},
  Row{0x8A5A84u,{1,0xD0,0},{},{5,0,2},{}},
  Row{0x8A5A6Cu,{1,0xCB,0},{},{4,0,0},{}},
  Row{0x8A5A54u,{1,0xCD,0},{},{5,0,0},{}},
  Row{0x8A5A3Cu,{1,0x39,0},{},{2,0,2},{}},
  Row{0x8A5A24u,{1,0x9D,0},{1,0x22,0},{2,0,3},{}},
  Row{0x8A5A14u,{1,0x38,0},{1,0x2D,0},{2,0,0},{}},
  Row{0x8A59FCu,{1,0x1D,0},{},{2,0,4},{}},
  Row{0x8A59E8u,{1,0x2A,0},{},{2,0,5},{}},
  Row{0x8A59D4u,{1,0x13,0},{},{},{}},
  Row{0x8A62A8u,{1,0x48,0},{},{2,0,8},{}},
  Row{0x8A6280u,{1,0x50,0},{},{2,0,9},{}},
  Row{0x8A6294u,{1,0x4B,0},{},{},{}},
  Row{0x8A626Cu,{1,0x4D,0},{},{},{}},
  Row{0x8A59C4u,{1,0x14,0},{},{},{}},
  Row{0x8A59B0u,{1,0x2E,0},{},{},{}},
  Row{0x8A599Cu,{1,0x26,0},{},{2,0,6},{}},
  Row{0x8A5988u,{1,0x19,0},{},{},{}},
  Row{0x8A62C0u,{1,1,0},{},{2,0,7},{}}
 }};
 Lease lease;auto live=captured;
 Check(lease.Capture(live,active),"APEX5 dump: slot zero alone must allow configured pad preservation");
 live[0].padPrimary={2,0,0};live[6].padPrimary={2,0,6};live[9].padSecondary={2,0,1};
 live[0].keyboardPrimary={1,0x11,0};live[6].keyboardSecondary={1,0x12,0};
 Check(lease.Restore(live,active)==3,"APEX5 dump: changed accelerator, nitro and secondary pad bindings restored");
 Check(live[0].keyboardPrimary==std::array<unsigned,3>{1,0x11,0}&&live[6].keyboardSecondary==std::array<unsigned,3>{1,0x12,0},"APEX5 dump: current keyboard bindings retained");
 for(unsigned i=0;i<live.size();++i)Check(live[i].padPrimary==captured[i].padPrimary&&live[i].padSecondary==captured[i].padSecondary,"APEX5 dump: all nineteen pad rows match the captured assignments");
 live[4].padPrimary={2,0,1};const auto changed=live[4].padPrimary;
 Check(lease.Restore(live,InputTable{0x33A38394u})==-1&&!lease.held&&live[4].padPrimary==changed,"APEX5 dump: replaced slot-zero identity cancels restoration without writes");
 Check(lease.Capture(captured,active),"APEX5 dump: transition can capture again");lease.Clear();
 Check(lease.Restore(live,active)==0&&live[4].padPrimary==changed,"APEX5 dump: edits after the transition remain untouched");
}
int main(){try{
 Apex5DumpRegression();
 Rows maps;for(unsigned i=0;i<maps.size();++i){maps[i].name=0x8A0000+i;maps[i].keyboardPrimary={1,0xC8+i,0};maps[i].padPrimary={3,8+i,0};maps[i].padSecondary={4,i,0x3F800000};}
 const auto original=maps;InputTable devices{0x1000,0x2000};Lease lease;
 Check(lease.Capture(maps,devices),"capture configured pad bindings with a populated input table");
 maps[0].padPrimary={3,1,0};maps[7].padSecondary={};maps[3].keyboardPrimary={1,48,0};
 Check(lease.Restore(maps,devices)==2,"restore only the changed pad assignments");
 Check(maps[3].keyboardPrimary==std::array<unsigned,3>{1,48,0},"keyboard changes must survive pad restoration");
 for(unsigned i=0;i<maps.size();++i)Check(maps[i].padPrimary==original[i].padPrimary&&maps[i].padSecondary==original[i].padSecondary,"all custom pad actions retained");
 Check(lease.Restore(maps,devices)==0,"unchanged map must not request a restore");
 auto replaced=devices;replaced[1]=0x3000;maps[0].padPrimary={3,2,0};const auto newMap=maps[0].padPrimary;
 Check(lease.Restore(maps,replaced)==-1&&!lease.held&&maps[0].padPrimary==newMap,"input-table identity change must cancel the lease without writes");
 Check(lease.Capture(original,devices),"recapture at retry");maps=original;maps[2].name++;
 Check(lease.Restore(maps,devices)==-1&&!lease.held,"different action definitions must not be overwritten");
 Check(!lease.Capture(original,InputTable{}),"absent native input table must never enable pad writes");
 Check(!lease.Capture(Rows{},devices),"unconfigured pad must not acquire a defaults lease");
 Check(lease.Capture(original,devices),"capture for possession transition");lease.Clear();maps=original;maps[1].padPrimary={3,9,7};
 Check(lease.Restore(maps,devices)==0&&maps[1].padPrimary==std::array<unsigned,3>{3,9,7},"user changes after transition completion must remain untouched");
 std::printf("RPD_CONTROL_BINDINGS_PASS %u checks; captured APEX5 table, keyboard isolation, input-table changes, retry and lease release\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
