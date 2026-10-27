#include <Windows.h>
#include "CarbonIntroPose.h"
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
int main(int argc,char** argv){try{
 Check(argc==2,"supply verified EN1.3 EXE");
 std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> exe((std::istreambuf_iterator<char>(file)),{});
 constexpr unsigned offset=0x5E1A0,bytes=0x9B;
 Check(exe.size()==6029312&&exe[offset]==0x83&&exe[offset+bytes-1]==0xC3,"native conversion unavailable");
 auto memory=VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Check(memory!=nullptr,"allocate conversion fixture");
 memcpy(memory,exe.data()+offset,bytes);DWORD old=0;Check(VirtualProtect(memory,bytes,PAGE_EXECUTE_READ,&old)!=0,"seal fixture");
 FlushInstructionCache(GetCurrentProcess(),memory,bytes);
 auto convert=reinterpret_cast<void(__cdecl*)(float*,const float*)>(memory);
 constexpr float pi=3.141592654f;
 for(float yaw:{-180.f,-90.f,0.f,37.f,90.f,158.f,360.f}){
  const float position[]={1980.25f,737.125f,113.5f};float pose[4]{};
  Check(mwel_carbon::StartMarkerPose(pose,position,&yaw),"valid marker refused");
  Check(memcmp(pose,position,12)==0&&memcmp(pose+3,&yaw,4)==0,"native marker must retain exact exported values");
  const float angle=(pose[3]-90)*pi/180,c=std::cos(angle),s=std::sin(angle);
  for(float sourceYaw:{-180.f,-90.f,0.f,37.f,90.f,158.f})for(float x:{-12.f,0.f,12.f}){
   const float a=sourceYaw*pi/180,sc=std::cos(a),ss=std::sin(a);Matrix m={sc,ss,0,0,-ss,sc,0,0,0,0,1,0,x,-3,0,1};
   for(unsigned r=0;r<4;++r){float sx=m[r*4],sy=m[r*4+1];m[r*4]=sx*c-sy*s;m[r*4+1]=sx*s+sy*c;}
   m[12]+=pose[0];m[13]+=pose[1];m[14]+=pose[2];Matrix physics{};convert(physics.data(),m.data());
   Check(std::abs(physics[12]+m[13])<.001f&&std::abs(physics[14]-m[12])<.001f,"map marker changed physical XY placement");
   const float actual=std::atan2(-physics[8],physics[10]),expected=std::atan2(m[1],m[0]);
   Check(std::cos(actual-expected)>.99999f,"shared marker rotates physical heading incorrectly");
  }
 }
 float position[]={1,2,3},yaw=90,pose[]={11,12,13,14};const auto unchanged=[&](){return pose[0]==11&&pose[1]==12&&pose[2]==13&&pose[3]==14;};
 Check(!mwel_carbon::StartMarkerPose(nullptr,position,&yaw),"null output accepted");
 Check(!mwel_carbon::StartMarkerPose(pose,nullptr,&yaw)&&unchanged(),"missing position must not mutate output");
 Check(!mwel_carbon::StartMarkerPose(pose,position,nullptr)&&unchanged(),"missing rotation must not mutate output");
 for(unsigned i=0;i<4;++i)for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),100001.f}){
  float input[]={1,2,3,90};input[i]=invalid;
  Check(!mwel_carbon::StartMarkerPose(pose,input,input+3)&&unchanged(),"invalid marker must fail before publishing pose");
 }
 VirtualFree(memory,0,MEM_RELEASE);printf("PASS %u checks: exact marker values, native physical conversion, fail-closed missing/invalid attributes\n",checks);return 0;
}catch(const std::exception& e){printf("FAIL %s\n",e.what());return 1;}}
