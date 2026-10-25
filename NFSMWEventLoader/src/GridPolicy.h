#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace mwel_grid {
constexpr unsigned Capacity=16;
using Position=std::array<float,3>; // Native world X/Y/Z.
struct Layout {std::array<Position,Capacity> positions{};unsigned count=0,columns=0;bool valid=false;};
template<class Fit> Layout Plan(unsigned count,Position origin,float rotation,Fit fit){
 if(!count||count>Capacity||!std::isfinite(rotation))return {};
 for(float n:origin)if(!std::isfinite(n))return {};
 const float radians=rotation*.017453292519943295f;
 // Editor tangent (cos,sin) becomes native (-sin,cos).
 const float dx=-std::sin(radians),dz=std::cos(radians),sx=-dz,sz=dx;
 for(unsigned columns:{2u,1u}){
  Layout out;out.count=count;out.columns=columns;bool complete=true;
  float previousBack=-5;
  for(unsigned row=0;row<(count+columns-1)/columns;++row){bool accepted=false;
   const float nominal=2+7.f*row;
   // Fit a whole row before advancing: neither column may reuse another row.
   float first=std::max(nominal,previousBack+7);
   for(unsigned attempt=0;attempt<9;++attempt){float back=first+3.f*attempt;if(back>160)break;bool good=true;Position staged[2]{};
    for(unsigned col=0;col<columns&&row*columns+col<count;++col){float side=columns==1||count==1?0:(col?1.5f:-1.5f);
     Position candidate{origin[0]-dx*back+sx*side,origin[1],origin[2]-dz*back+sz*side};
     if(!fit(candidate,dx,dz)){good=false;break;}staged[col]=candidate;
    }
    if(good){for(unsigned col=0;col<columns&&row*columns+col<count;++col)out.positions[row*columns+col]=staged[col];previousBack=back;accepted=true;break;}
   }
   if(!accepted){complete=false;break;}
  }
  if(complete){out.valid=true;return out;}
 }
 return {};
}
}
