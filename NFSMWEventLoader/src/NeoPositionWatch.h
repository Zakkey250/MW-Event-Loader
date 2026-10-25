#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <atomic>
// Private diagnostic reference only. None of these values are car state.
// CDB watches the actual IRigidBody position, then reads these integer bounds;
// it does not evaluate floating point expressions on every physics write.
struct NeoPositionWatch {
 std::uint32_t lowKey=0,highKey=0;float time=0,until=0;volatile std::uint32_t valid=0;
 static std::uint32_t Key(float x){std::uint32_t u=0;std::memcpy(&u,&x,4);if((u&0x7FFFFFFFu)==0)u=0;return u&(1u<<31)?~u:u^(1u<<31);}
 void Invalidate(){valid=0;std::atomic_thread_fence(std::memory_order_seq_cst);}
 void Observe(float x,float seconds,bool sameBody){
  Invalidate();lowKey=highKey=0;time=until=0;
  if(!sameBody||!std::isfinite(x)||!std::isfinite(seconds)||seconds<=0)return;
  // A fifty-metre discontinuity is a diagnostic trigger, never a controller
  // permission, course threshold or physical recovery operation.
  lowKey=Key(x-50.f);highKey=Key(x+50.f);time=seconds;until=seconds+.25f;
  std::atomic_thread_fence(std::memory_order_release);valid=1;
 }
};
