#pragma once
#include <array>
#include <cstdint>
#include "key_layout.h"

struct InputState {
    bool paused = false;
    unsigned holdMs = 50;
    std::array<bool, 256> down{};
    std::array<bool, 256> unpresented{};
    std::array<std::uint64_t, 256> until{}, order{};
    std::uint64_t sequence = 0;
    void key(unsigned vk, bool pressed, std::uint64_t now) {
        if (vk >= 256 || (!findKey(vk) && vk != 1 && vk != 2 && vk != 4)) return;
        if (pressed && !down[vk]) {
            until[vk] = now + holdMs; order[vk] = ++sequence; unpresented[vk] = !paused;
        }
        if (!pressed && paused) { unpresented[vk] = false; until[vk] = 0; }
        down[vk] = pressed;
    }
    bool visible(unsigned vk, std::uint64_t now) const {
        return vk < 256 && !paused && (down[vk] || unpresented[vk] || now < until[vk]);
    }
    bool pending(std::uint64_t now) const {
        if (paused) return false;
        for (unsigned i=0; i<256; ++i) if (!down[i] && (unpresented[i] || now < until[i])) return true;
        return false;
    }
    int latest(bool left, std::uint64_t now) const {
        int vk = -1; std::uint64_t newest = 0;
        for (const auto& k : keyLayout()) {
            if (leftHand(k) == left && visible(k.vk, now) && order[k.vk] > newest) {
                newest = order[k.vk]; vk = static_cast<int>(k.vk);
            }
        }
        return vk;
    }
    int latestAny(std::uint64_t now) const {
        int vk=-1; std::uint64_t newest=0;
        for (const auto& k : keyLayout()) if (visible(k.vk,now) && order[k.vk]>newest) {
            newest=order[k.vk]; vk=static_cast<int>(k.vk);
        }
        return vk;
    }
    void presented(std::uint64_t now) {
        if (paused) return;
        for (unsigned vk=0;vk<256;++vk) if (unpresented[vk]) {
            unpresented[vk]=false;
            until[vk]=now+holdMs;
        }
    }
    void clear() { down.fill(false); unpresented.fill(false); until.fill(0); order.fill(0); }
};
