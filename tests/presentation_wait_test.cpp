#include "PresentationWait.h"
#include <cassert>
#include <iostream>

int main(){
 ttcg::PresentationWait wait;
 // No time limit while choosing a card, browsing, or collecting a prize.
 assert(!wait.expired(12,4,0,100));
 assert(!wait.expired(12,4,0,3600100));
 // Repeated publications must not keep a stalled capture alive forever.
 assert(!wait.expired(12,4,1,3600100));
 assert(!wait.expired(12,4,1,3630099));
 assert(wait.expired(12,4,1,3630100));
 // A completed animation followed by AI search gets its own deadline.
 assert(!wait.expired(12,4,2,3630100));
 assert(!wait.expired(12,4,2,3660099));
 assert(wait.expired(12,4,2,3660100));
 // Progress and rematches cannot inherit the preceding wait's timer.
 assert(!wait.expired(12,5,1,3660100));
 assert(!wait.expired(14,5,1,3690100));
 assert(!wait.expired(14,5,1,3720099));
 // Returning to a player's turn cancels the pending timeout.
 assert(!wait.expired(14,5,0,3720100));
 assert(!wait.expired(14,5,1,4000000));
 // A close/load reset cancels it even if the restored IDs happen to match.
 wait.reset();
 assert(!wait.expired(14,5,1,4030000));
 assert(!wait.expired(14,5,1,4029999)); // A clock underflow cannot fire it.
 assert(!wait.expired(14,5,1,4059999));
 assert(wait.expired(14,5,1,4060000));
 std::cout<<"Presentation wait: idle turns, stalled captures/AI, repeated packets, progress, rematches and reset passed\n";
}
