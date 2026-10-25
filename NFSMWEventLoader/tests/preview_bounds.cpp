#include "../src/EventPreviewBounds.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

int main(){
 constexpr auto full=event_preview::ToNative(0,0,1,1);
 static_assert(full.lower[0]==-.125f && full.lower[1]==1.125f);
 static_assert(full.upper[0]==1.125f && full.upper[1]==-.125f);
 // Native auto-fit uses these exact signs, and centers via endpoint averages.
 for(auto b:{full,event_preview::ToNative(.1f,.2f,.9f,.7f)}){
  assert(b.upper[0]-b.lower[0]>0);
  assert(b.lower[1]-b.upper[1]>0);
 }
 assert(std::abs((full.lower[0]+full.upper[0])*.5f-.5f)<.0001f);
 assert(std::abs((full.lower[1]+full.upper[1])*.5f-.5f)<.0001f);
 std::puts("PREVIEW_BOUNDS_PASS native auto-fit positive width/height, centered full image and original UV margin");
}
