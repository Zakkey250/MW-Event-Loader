#include "RpdRetryBoundary.h"
#include <cassert>
#include <cstdio>
int main() {
 using namespace mwel_rpd_retry;
 unsigned checks=0;
 // An old result callback must never restart a newly entered dispatch.
 for (unsigned mask=0;mask<16;++mask) {
  const bool valid=SameOwner(!(mask&1),mask&2?0x200:0x100,mask&4?9:7,mask&8?4:3,0x100,7,3);
  assert(valid==(mask==0));++checks;
 }
 assert(!SameOwner(true,0,7,3,0,7,3));++checks;
 // Destructor scheduling is not completion: both ownership and acknowledgment
 // must be clear. Unreadable flow is also a wait, never permission to restart.
 for (unsigned mask=0;mask<16;++mask) {
  assert(SceneReleased(mask&1,mask&2,!(mask&4),mask&8)==(mask==0));++checks;
 }
 for (unsigned mode=0;mode<=6;++mode) {
  assert(SimulationResumed(true,0x100,mode)==(mode==3));++checks;
  assert(!SimulationResumed(false,0x100,mode));++checks;
  assert(!SimulationResumed(true,0,mode));++checks;
 }
 // Reproduce the actual failure: transition flow idle, NIS destroyed, world
 // still paused. Readiness alone must not allow the synchronous restart.
 assert(SceneReleased(0,0,true,0));++checks;
 assert(!SimulationResumed(true,0x100,4));++checks;
 assert(!SimulationResumed(true,0x100,0));++checks;
 assert(SimulationResumed(true,0x100,3));++checks;
 std::printf("PASS %u RPD retry boundary checks: owner changes, deferred NIS destruction, unreadable state, paused simulation and resume\n",checks);
}
