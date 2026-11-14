#pragma once
#include "WorldProviderModule.h"
// CGrd's pointer table may omit CGcn records for detached W2C roads. Native
// 00782CB0 silently skips these null cells when registering GTrigger shapes.
// Supply the verified 20-byte empty CGcn header; the native grid still owns
// dynamic lists, narrow-phase contact, enter/leave callbacks and CP ordering.
namespace touge_grid {
struct Cell {unsigned dynamic=0,index=0;unsigned char counts[4]{};unsigned short offsets[4]{};};
static_assert(sizeof(Cell)==20,"Native CGcn empty-cell header");
struct Storage {BYTE* grid=nullptr;Cell** table=nullptr;std::unique_ptr<Cell> cell;};
std::vector<Storage> owned;
bool Active(const mwel::EventPackage& p){return p.authoring.value("combatMode",std::string("none"))=="touge"&&p.authoring.value("worldCompatibility",mwel::Json::object()).value("kind",std::string())=="w2c"&&mwel_world_provider::Module();}
bool Ensure(const mwel::EventPackage& p){
 if(!Active(p))return true;
 const BYTE moveGuard[]={0x8B,0x4C,0x24,0x04,0x51,0x8D,0x41,0x30,0x50};
 if(memcmp(reinterpret_cast<void*>(0x783B60),moveGuard,sizeof(moveGuard)))return false;
 BYTE* grid=nullptr;if(!Copy(reinterpret_cast<void*>(0x9B38A8),&grid,4)||!grid)return false;
 float values[6]{};unsigned rows=0,columns=0;Cell** table=nullptr;
 if(!Copy(grid,values,sizeof(values))||!Copy(grid+24,&rows,4)||!Copy(grid+28,&columns,4)||!Copy(grid+32,&table,4)||!table||!rows||!columns||rows>1024||columns>1024)return false;
 if(!std::isfinite(values[0])||!std::isfinite(values[2])||!std::isfinite(values[4])||!std::isfinite(values[5])||values[4]<=0||std::abs(values[4]*values[5]-1)>1e-5)return false;
 std::set<unsigned> cells;
 for(const auto& gate:p.authoring.at("gates")){
  const auto& pos=gate.at("position");float x=-pos.at(1).get<float>(),z=pos.at(0).get<float>(),radius=0;
  for(const auto& v:gate.at("dimensions")){float n=v.get<float>();radius+=n*n;}
  radius=std::sqrt(radius)*.5f+2*values[4];
  int left=int(std::floor((x-radius-values[0])*values[5])),right=int(std::floor((x+radius-values[0])*values[5]));
  int top=int(std::floor((z-radius-values[2])*values[5])),bottom=int(std::floor((z+radius-values[2])*values[5]));
  if((right-left+1)*(bottom-top+1)>4096)return false;
  // 00763690 clamps its X/Z indices to the grid boundary. Use that same
  // rule for detached roads; never relocate the gate or expand global bounds.
  left=std::clamp(left,0,int(columns)-1);right=std::clamp(right,0,int(columns)-1);
  top=std::clamp(top,0,int(rows)-1);bottom=std::clamp(bottom,0,int(rows)-1);
  for(int r=top;r<=bottom;++r)for(int c=left;c<=right;++c)cells.insert(unsigned(r)*columns+unsigned(c));
 }
 std::vector<Storage> pending;
 for(auto index:cells){Cell* current=nullptr;if(!Copy(table+index,&current,4))return false;if(current)continue;
  if(owned.size()+pending.size()>=16384)return false;
  auto cell=std::make_unique<Cell>();cell->index=index;pending.push_back(Storage{grid,table,std::move(cell)});
 }
 owned.reserve(owned.size()+pending.size());
 for(auto& row:pending){row.table[row.cell->index]=row.cell.get();owned.push_back(std::move(row));}
 // CGcn headers have the world resource lifetime, as do the original headers.
 // Do not free a cell while native bodies/triggers can still reference its list.
 for(unsigned n=0;n<std::min(3u,unsigned(p.authoring.at("gates").size()));++n){const auto& pos=p.authoring.at("gates").at(n).at("position");int c=std::clamp(int(std::floor((-pos.at(1).get<float>()-values[0])*values[5])),0,int(columns)-1),r=std::clamp(int(std::floor((pos.at(0).get<float>()-values[2])*values[5])),0,int(rows)-1);unsigned index=unsigned(r)*columns+unsigned(c);Log("TOUGE_GRID_GATE_AUDIT gate=%u grid=%08X table=%08X index=%u pointer=%08X included=%u",n,unsigned(grid),unsigned(table),index,unsigned(table[index]),unsigned(cells.count(index)));}
 Log("TOUGE_NATIVE_GRID_READY cells=%u added=%u original_cells_replaced=0 native_contact_callbacks=1",unsigned(cells.size()),unsigned(pending.size()));return true;
}
}
