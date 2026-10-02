#pragma once
#include <cstdint>

struct Animation {
    bool paused = false;
    int active = 0;
    std::uint64_t until = 0;
    void tap(bool left, std::uint64_t now) {
        if (paused) return;
        active = left ? 1 : 2;
        until = now + 140;
    }
    int frame(std::uint64_t now) const {
        return !paused && now < until ? active : 0;
    }
};
