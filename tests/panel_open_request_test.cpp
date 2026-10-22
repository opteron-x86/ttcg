#include "PanelOpenRequest.h"
#include <cassert>
#include <iostream>

int main() {
 using Request=ttcg::PanelOpenRequest;
 using enum Request::Step;
 Request r;
 assert(!r.active(r.token())&&r.poll(r.token(),0,false)==Idle);
 // Dialogue can close before or after the Papyrus callback. Neither ordering
 // consumes the request until menus have stayed clear for a whole interval.
 for(bool dialogueFirst:{false,true}) {
  const auto token=r.begin(1000);
  if(!dialogueFirst)assert(r.poll(token,1100,true)==Waiting);
  assert(r.poll(token,1200,false)==Waiting);
  assert(r.poll(token,1299,false)==Waiting);
  assert(r.poll(token,1300,false)==Open);
  assert(!r.current(token)&&!r.active(r.token()));
  assert(r.poll(token,1400,false)==Idle); // Only one window per request.
 }
 // A brief clear frame while a menu or scene is closing is not sufficient.
 auto token=r.begin(2000);
 assert(r.poll(token,2100,false)==Waiting);
 assert(r.poll(token,2150,true)==Waiting);
 assert(r.poll(token,2200,false)==Waiting);
 assert(r.poll(token,2250,false)==Waiting);
 assert(r.poll(token,2300,false)==Open);
 // A persistent blocker and a delayed task both expire. They cannot open
 // later simply because the player eventually closes an unrelated menu.
 for(bool blocked:{false,true}) {
  token=r.begin(3000);
  assert(r.poll(token,3000,true)==Waiting);
  assert(r.poll(token,3000+Request::timeoutMs,blocked)==Expired);
  assert(r.poll(token,20000,false)==Idle&&!r.current(token));
 }
 // Load/close/disable cancels queued callbacks. A replacement has its own
 // timeout and must be unaffected by callbacks for the previous NPC.
 token=r.begin(30000);r.cancel();
 assert(r.poll(token,30100,false)==Idle&&!r.active(r.token()));
 const auto old=r.begin(40000),next=r.begin(40010);
 assert(r.poll(old,50000,true)==Idle&&r.active(next));
 assert(r.poll(next,40010,false)==Waiting);
 assert(r.poll(next,40110,false)==Open);
 std::cout<<"Panel handoff: both dialogue orderings, focus settling, flicker, timeout, cancellation, replacement and single opening passed\n";
}
