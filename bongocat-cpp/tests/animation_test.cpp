#include "../src/animation.h"
#include <cstdlib>
#include <iostream>

void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    Animation a;
    require(a.frame(0) == 0, "starts idle");
    a.tap(true, 10);
    require(a.frame(11) == 1, "left tap selects left paw");
    require(a.frame(149) == 1, "short tap remains visible for 140ms");
    require(a.frame(150) == 0, "tap returns to idle");
    a.tap(false, 200);
    require(a.frame(201) == 2, "right tap selects right paw");
    a.tap(true, 210);
    require(a.frame(211) == 1, "newest tap wins simultaneous input");
    a.paused = true;
    require(a.frame(220) == 0, "pause displays idle");
    a.tap(false, 220);
    a.paused = false;
    require(a.frame(341) == 1, "paused tap does not alter last animation");
    require(a.frame(350) == 0, "animation expires normally");
    std::cout << "PASS: animation timing, simultaneous input and pause\n";
}
