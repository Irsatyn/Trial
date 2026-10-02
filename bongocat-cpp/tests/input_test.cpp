#include "../src/input_state.h"
#include <cstdlib>
#include <iostream>

void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    InputState s;
    s.key('A', true, 100);
    check(s.visible('A', 101), "real A press highlights A");
    check(!s.visible('B', 101), "A never highlights B");
    s.presented(100);
    s.key('J', true, 105);
    check(s.visible('A', 106) && s.visible('J', 106), "simultaneous keys remain highlighted");
    check(s.latest(true, 106) == 'A' && s.latest(false, 106) == 'J', "keys map to their physical hand region");
    check(s.latestAny(106)=='J', "single keyboard paw selects newest chord member");
    s.presented(105);
    s.key('A', false, 110);
    check(s.visible('A', 149) && !s.visible('A', 150), "quick press has 50ms minimum visibility");
    check(s.visible('J', 2000), "held key does not expire");
    check(!s.pending(2000), "steady held input needs no continuous timer");
    s.key('J', false, 2001);
    check(!s.visible('J', 2001), "long-held key releases immediately");
    s.key('D', true, 3000); s.key('F', true, 3010); s.key('D', true, 3020);
    s.presented(3020);
    check(s.latest(true, 3021) == 'F', "autorepeat does not steal paw from newer held key");
    s.paused = true;
    check(!s.visible('F', 3030) && s.latest(true, 3030) == -1, "pause displays neutral scene");
    s.key('F', false, 3040); s.key('D', false, 3040); s.paused = false;
    check(!s.visible('F', 3100), "release while paused cannot leave stuck key");
    s.key(255, true, 4000);
    check(!s.visible(255, 4001), "unmapped key is ignored");
    check(findKey(0x25)==nullptr && findKey(0x26)==nullptr && findKey(0x27)==nullptr && findKey(0x28)==nullptr,
          "arrow keys are not drawn or mapped");
    InputState quick;
    quick.holdMs=10;
    quick.key('A',true,5000); quick.key('A',false,5001);
    check(quick.visible('A',5033), "quick tap survives a delayed first frame");
    quick.presented(5033);
    check(quick.visible('A',5042) && !quick.visible('A',5043), "minimum hold starts at first presentation");
    std::cout << "PASS: mapping, chord, holds, release, autorepeat, pause and delayed presentation\n";
}
