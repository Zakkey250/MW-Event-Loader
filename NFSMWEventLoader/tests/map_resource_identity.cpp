#include "../src/MapResourceIdentity.h"
#include <cassert>
#include <iostream>
unsigned Hash(const std::string& text){unsigned h=~0u;for(unsigned char c:text)h=h*33+c;return h;}
int main(){
 const std::string id="15.2.1.r";
 assert(event_resource::RadarPrefix(id)=="MINI_MAP_15_2_1_R");
 assert(Hash(event_resource::RadarPrefix(id)+"_CHOP0")==0xF3D29999u);
 assert(event_resource::Token("e12ab34Cd")=="E12AB34CD");
 assert(event_resource::Token("15.2.1")=="15_2_1");
 assert(event_resource::Token("abc.X-1")=="ABC_X-1");
 assert(id=="15.2.1.r");
 std::cout<<"MAP_RESOURCE_IDENTITY_PASS canonical radar keys; event identity preserved\n";
}
