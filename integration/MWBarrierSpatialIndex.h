#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

// Immutable broad phase only. Callers keep their live activation, ownership,
// vertical/contact tests and output limits. Sorted IDs preserve source order.
namespace mw_barrier_index {
struct Bounds {float minX,minZ,maxX,maxZ;};
class Index {
 static constexpr double cellSize=64.;
 static constexpr float padding=.05f;
 std::map<std::pair<int,int>,std::vector<unsigned>> cells;
 std::vector<unsigned> broad;
 std::size_t count=0;
 static bool Cell(double value,int& result){
  if(!std::isfinite(value)||value<-1000001.||value>1000001.)return false;
  result=static_cast<int>(std::floor(value/cellSize));return true;
 }
public:
 template<class Get>void Build(std::size_t size,Get bounds){
  cells.clear();broad.clear();count=size;if(size<=32)return;
  for(unsigned i=0;i<size;++i){auto b=bounds(i);int x0,z0,x1,z1;
   if(!Cell(double(b.minX)-padding,x0)||!Cell(double(b.minZ)-padding,z0)||
      !Cell(double(b.maxX)+padding,x1)||!Cell(double(b.maxZ)+padding,z1)||
      x1<x0||z1<z0||std::size_t(x1-x0+1)*std::size_t(z1-z0+1)>256){broad.push_back(i);continue;}
   for(int x=x0;x<=x1;++x)for(int z=z0;z<=z1;++z)cells[{x,z}].push_back(i);
  }
 }
 // false requests the original full scan; malformed or unusually broad
 // requests must never turn into missing contacts or unbounded grid work.
 bool Query(float x,float z,float radius,std::vector<unsigned>& out)const{
  out.clear();if(count<=32)return false;
  int x0,z0,x1,z1;
  if(!std::isfinite(radius)||radius<0||!Cell(double(x)-radius-padding,x0)||
     !Cell(double(z)-radius-padding,z0)||!Cell(double(x)+radius+padding,x1)||
     !Cell(double(z)+radius+padding,z1)||std::size_t(x1-x0+1)*std::size_t(z1-z0+1)>4096)return false;
  out.insert(out.end(),broad.begin(),broad.end());
  for(int cx=x0;cx<=x1;++cx)for(int cz=z0;cz<=z1;++cz){auto it=cells.find({cx,cz});if(it!=cells.end())out.insert(out.end(),it->second.begin(),it->second.end());}
  std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return true;
 }
};
}
