#pragma once
#include "object.hpp"
#include <cstdint>

// Monitor item types — matches s2disasm's MonitorType_* equates
enum class MonitorItem : uint8_t {
    Rings      = 0,  // +10 rings
    Shield     = 1,
    SpeedShoes = 2,
    Invincible = 3,
    ExtraLife  = 4,
    Continue   = 5,
    S_Monitor  = 6,  // Super Sonic (needs 50 rings)
};

struct MonitorData {
    MonitorItem item;
    uint8_t     break_timer;  // countdown once broken, before applying item
    bool        broken;
};

void monitor_init(Object &obj, MonitorItem item, int16_t wx, int16_t wy);
void monitor_update(Object &obj);
