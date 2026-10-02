#pragma once
#include <vector>

struct KeyCap { unsigned vk; const wchar_t* label; float x, y, w, h; };
inline const std::vector<KeyCap>& keyLayout() {
    static const auto keys = [] {
        std::vector<KeyCap> v;
        auto row = [&v](int index, std::initializer_list<std::pair<unsigned, std::pair<const wchar_t*, float>>> list) {
            float x = 205;
            for (const auto& item : list) {
                float w = item.second.second * 31.6f;
                v.push_back({item.first, item.second.first, x, 304.f + index * 22.f, w - 2.f, 19.f});
                x += w;
            }
        };
        row(0, {{0x1B,{L"Esc",1}}, {'1',{L"1",1}}, {'2',{L"2",1}}, {'3',{L"3",1}}, {'4',{L"4",1}}, {'5',{L"5",1}}, {'6',{L"6",1}}, {'7',{L"7",1}}, {'8',{L"8",1}}, {'9',{L"9",1}}, {'0',{L"0",1}}, {0x08,{L"Back",2}}});
        row(1, {{0x09,{L"Tab",1.5}}, {'Q',{L"Q",1}}, {'W',{L"W",1}}, {'E',{L"E",1}}, {'R',{L"R",1}}, {'T',{L"T",1}}, {'Y',{L"Y",1}}, {'U',{L"U",1}}, {'I',{L"I",1}}, {'O',{L"O",1}}, {'P',{L"P",1}}, {0x2E,{L"Del",1.5}}});
        row(2, {{0x14,{L"Caps",1.75}}, {'A',{L"A",1}}, {'S',{L"S",1}}, {'D',{L"D",1}}, {'F',{L"F",1}}, {'G',{L"G",1}}, {'H',{L"H",1}}, {'J',{L"J",1}}, {'K',{L"K",1}}, {'L',{L"L",1}}, {0x0D,{L"Enter",2.25}}});
        row(3, {{0xA0,{L"Shift",1.5}}, {'Z',{L"Z",1}}, {'X',{L"X",1}}, {'C',{L"C",1}}, {'V',{L"V",1}}, {'B',{L"B",1}}, {'N',{L"N",1}}, {'M',{L"M",1}}, {0xBC,{L",",1}}, {0xBE,{L".",1}}, {0xBF,{L"/",1}}, {0xA1,{L"Shift",1.5}}});
        row(4, {{0xA2,{L"Ctrl",1.25}}, {0x5B,{L"Win",1}}, {0xA4,{L"Alt",1.25}}, {0x20,{L"Space",5.5}}, {0xA5,{L"Alt",1.25}}, {0x5D,{L"Menu",1}}, {0xA3,{L"Ctrl",1.75}}});
        return v;
    }();
    return keys;
}
inline const KeyCap* findKey(unsigned vk) {
    for (const auto& k : keyLayout()) if (k.vk == vk) return &k;
    return nullptr;
}
inline bool leftHand(const KeyCap& key) { return key.x + key.w / 2 < 412; }
