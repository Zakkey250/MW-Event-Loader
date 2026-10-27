#include "../src/LeaderboardPhase.h"
#include <cassert>
#include <cstdio>
int main(){
 LeaderboardPhase board;
 // Full two cycles: native two-state flips must produce names / HP / score.
 const unsigned expected[]={0,1,2,0,1,2,0};
 for(unsigned i=0;i<7;++i)for(unsigned frame=0;frame<100;++frame)assert(board.Observe(true,i%2)==expected[i]);
 // Other event modes retain the native two-state sequence, including switches
 // in an FE package that survives returning to free roam.
 for(unsigned i=0;i<8;++i)assert(board.Observe(false,i%2)==i%2);
 assert(board.Observe(true,true)==1);
 assert(board.Observe(true,false)==2);
 board={};assert(board.Observe(true,false)==0);
 board={};assert(board.Observe(true,true)==1);
 std::puts("LEADERBOARD_PHASE_PASS 700 stable frames, three-stage cycles, mode switch and new-race reset");
}
