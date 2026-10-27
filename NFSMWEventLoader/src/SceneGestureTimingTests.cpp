#include "SceneGestureTiming.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
int main(){
 using namespace scene_gesture_timing;unsigned checks=0;
 auto check=[&](bool yes){++checks;if(!yes)throw std::runtime_error("Gesture timing policy regression");};
 constexpr auto primary="3E692EE7A71FFB74D6D839FD2FCACD25EDC5F1CA4CC14DA0EA6CC163C3E2655A";
 constexpr auto clock="5277FE07B2416346FC609D837158B637FC69E01DB602D0BD44ECD70B2B0313E4";
 constexpr auto second="157AB57A5D97EAD09E5D2DF9E687E01E2421BF0375DFAEA613FCAD357564858E";
 constexpr auto radio="563090B39EA93ED25F66503A37F8B443DFE41D6170DEBAA4E7CACAAF4B8EEF07";
 check(Identify(4,6,9.6f,415.f/30)==Clip::Paired);
 check(Correct(Clip::Paired,9.6f,primary,clock,second)==523.f/60);
 check(Identify(4,200.f/30,289.f/30,415.f/30)==Clip::Radio);
 check(std::abs(Correct(Clip::Radio,289.f/30,radio,clock,"")-10.6222222f)<.00001f);
 check(Identify(4,6,523.f/60,415.f/30)==Clip::None);
 check(Identify(4,200.f/30,10.6222222f,415.f/30)==Clip::None);
 for(unsigned n=0;n<=16;++n)if(n!=4)check(Identify(n,6,9.6f,415.f/30)==Clip::None);
 for(float go:{8.6666667f,8.8333333f,9.60001f,0.f,std::numeric_limits<float>::quiet_NaN()})check(Identify(4,6,go,415.f/30)==Clip::None);
 check(Identify(4,6.00001f,9.6f,415.f/30)==Clip::None);
 check(Identify(4,6,9.6f,14.3f)==Clip::None);
 for(unsigned index=0;index<64;++index){
  std::string a(primary),b(clock),c(second);a[index]=a[index]=='0'?'1':'0';b[index]=b[index]=='0'?'1':'0';c[index]=c[index]=='0'?'1':'0';
  check(Correct(Clip::Paired,9.6f,a,clock,second)==9.6f);
  check(Correct(Clip::Paired,9.6f,primary,b,second)==9.6f);
  check(Correct(Clip::Paired,9.6f,primary,clock,c)==9.6f);
 }
 check(Correct(Clip::None,9.6f,primary,clock,second)==9.6f);
 printf("PASS %u gesture timing checks: exact converted motion identity, custom timing preserved, old packs need no reconversion\n",checks);
}
