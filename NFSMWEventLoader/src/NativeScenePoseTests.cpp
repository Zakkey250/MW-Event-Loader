#include <Windows.h>
#include "CarbonIntroPose.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>
static unsigned checks=0;
static void Check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
using Matrix=std::array<float,16>;
static Matrix Source(float angle){
 const float a=angle*3.141592654f/180.f,c=std::cos(a),s=std::sin(a);
 return {c,s,0,0,-s,c,0,0,0,0,1,0,17,-23,2,1};
}
static Matrix Place(Matrix source,const float* pose){
 const float a=(pose[3]-90)*3.141592654f/180.f,c=std::cos(a),s=std::sin(a);
 Matrix result=source;
 for(unsigned row=0;row<4;++row){result[row*4]=source[row*4]*c-source[row*4+1]*s;
  result[row*4+1]=source[row*4]*s+source[row*4+1]*c;}
 result[12]+=pose[0];result[13]+=pose[1];result[14]+=pose[2];return result;
}
int main(int argc,char** argv){try{
 Check(argc==2,"supply the verified EN1.3 EXE");
 std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> exe((std::istreambuf_iterator<char>(file)),{});
 constexpr unsigned offset=0x5E1A0,bytes=0x9B;
 Check(exe.size()==6029312&&exe[offset]==0x83&&exe[offset+bytes-1]==0xC3,"native conversion body unavailable");
 // Exact EN1.3 render-to-physics conversion, copied without any relative calls.
 auto memory=VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Check(memory!=nullptr,"allocate native conversion fixture");
 std::memcpy(memory,exe.data()+offset,bytes);DWORD old=0;
 Check(VirtualProtect(memory,bytes,PAGE_EXECUTE_READ,&old)!=0,"seal native conversion fixture");
 FlushInstructionCache(GetCurrentProcess(),memory,bytes);
 auto convert=reinterpret_cast<void(__cdecl*)(float*,const float*)>(memory);
 const float position[]={125,34,-819};
 for(float sceneAngle:{90.f,-90.f,0.f,180.f,37.f,-123.f})for(float bodyAngle:{0.f,90.f,-90.f,180.f,23.f,-148.f}){
  auto source=Source(sceneAngle);const float a=bodyAngle*3.141592654f/180.f;
  const float heading[]={-std::sin(a),0,std::cos(a)};float pose[4]{};
  Check(mwel_carbon::NativeStartPose(pose,source.data(),position,heading),"valid scene pose rejected");
  auto render=Place(source,pose);Matrix physical{};convert(physical.data(),render.data());
  // Native conversion places the car's forward axis in physical row 2.
  const float dot=physical[8]*heading[0]+physical[10]*heading[2];
  if(dot<.9999f)std::printf("DIRECTION_MISMATCH scene_angle=%.1f body_angle=%.1f forward_dot=%.6f\n",sceneAngle,bodyAngle,dot);
  Check(dot>.9999f,"scene release reverses or rotates the native start heading");
  Check(std::abs(physical[12]-position[0])<.001f&&std::abs(physical[14]-position[2])<.001f,"scene release shifts native horizontal start position");
 }
 auto source=Source(90);float pose[]={11,12,13,14};const float heading[]={0,0,1};
 auto unchanged=[&](){return pose[0]==11&&pose[1]==12&&pose[2]==13&&pose[3]==14;};
 Check(!mwel_carbon::NativeStartPose(nullptr,source.data(),position,heading),"null output accepted");
 Check(!mwel_carbon::NativeStartPose(pose,nullptr,position,heading)&&unchanged(),"null source changed output");
 source[0]=source[1]=0;Check(!mwel_carbon::NativeStartPose(pose,source.data(),position,heading)&&unchanged(),"vertical source lost native fallback");
 source=Source(0);source[12]=std::numeric_limits<float>::quiet_NaN();Check(!mwel_carbon::NativeStartPose(pose,source.data(),position,heading)&&unchanged(),"invalid translation changed output");
 VirtualFree(memory,0,MEM_RELEASE);
 std::printf("PASS %u checks: native conversion preserves start direction and horizontal position across scene/body rotations; invalid inputs retain fallback\n",checks);return 0;
}catch(const std::exception& error){std::printf("FAIL %s\n",error.what());return 1;}}
