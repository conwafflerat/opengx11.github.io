#pragma once
#include "object.hpp"
#include <cstdint>

// Ring collection radius (pixels) — matches s2disasm's ring proximity check
static constexpr int RING_COLLECT_RADIUS = 12;

// Scatter ring: bounced out when Sonic is hurt; these rings can be re-collected.
// In s2disasm they are a separate 'Scattered Ring' object type.
struct ScatterRingData {
    uint8_t timer;      // frames until despawn (s2disasm: 256 frames)
    int8_t  bounce_left; // bounce count remaining
};

void ring_init(Object &obj, int16_t world_x, int16_t world_y);
void ring_update(Object &obj);

// Spawn 'count' scatter rings from a hurt Sonic
void ring_scatter_all(int16_t from_x, int16_t from_y, uint8_t count);

// Called by sonic_hurt — spawns up to 16 scatter rings
void scatter_ring_init(Object &obj, int16_t wx, int16_t wy,
                       int16_t vx, int16_t vy);
void scatter_ring_update(Object &obj);
