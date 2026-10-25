#pragma once
#include <cstdio>
#include <cmath>
#include <vector>
#include <utility>
namespace event_neo_obstacles {
struct Obstacle {float p[3]{},heading[3]{},halfWidth=0,halfLength=0,halfHeight=0;};
static_assert(sizeof(Obstacle)==9*sizeof(float));
inline bool Load(const char* declaredPath,unsigned hash,std::vector<Obstacle>& output){
 if(!declaredPath){output.clear();return true;}
 FILE* file=nullptr;fopen_s(&file,declaredPath,"rb");if(!file)return false;
 unsigned header[4]{};
 bool valid=fread(header,4,4,file)==4&&header[0]==0x424F3257&&header[1]==1&&header[2]==hash&&header[3]<=256;
 std::vector<Obstacle> staged(valid?header[3]:0);
 for(auto& obstacle:staged){
  if(fread(&obstacle,sizeof(obstacle),1,file)!=1){valid=false;break;}
  for(float value:obstacle.p)if(!std::isfinite(value)||fabs(value)>100000)valid=false;
  for(float value:obstacle.heading)if(!std::isfinite(value)||fabs(value)>1.01f)valid=false;
  for(float value:{obstacle.halfWidth,obstacle.halfLength,obstacle.halfHeight})if(!std::isfinite(value)||value<=0||value>50)valid=false;
 }
 if(valid&&fgetc(file)!=EOF)valid=false;fclose(file);
 if(valid)output=std::move(staged);return valid;
}
}
