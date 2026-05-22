#include "ring.hpp"
#include "globals.hpp"
#include "sonic.hpp"
#include "object.hpp"
#include <cstdlib>
#include <cmath>

// ---- Collect ring animation: 8 frames of spin ----------------------
static const int8_t ring_anim_offsets[] = { 0, 2, 4, 6, 4, 2, 0, -2 };

// ---- Ring object ---------------------------------------------------
void ring_init(Object &obj, int16_t world_x, int16_t world_y)
{
    obj = Object{};
    obj.x        = (int32_t)world_x << 8;
    obj.y        = (int32_t)world_y << 8;
    obj.halfWidth  = 8;
    obj.halfHeight = 8;
    obj.colType    = ColType::Ring;
    obj.artTile    = 0x0050;  // ring tile index — update after art upload
    obj.aniSpeed   = 4;       // advance frame every 4 game frames
    obj.update     = ring_update;
}

void ring_update(Object &obj)
{
    // Animation: cycle through 8 frames based on global frame count
    obj.aniFrame = (uint8_t)((f_frameCount / obj.aniSpeed) & 7);

    // Update screen position
    obj.screenX = (int16_t)(obj.pixel_x() - v_screenX);
    obj.screenY = (int16_t)(obj.pixel_y() - v_screenY);

    // Off-screen range check — skip collision test if not visible
    if (obj.screenX < -16 || obj.screenX > SCREEN_W + 16) return;
    if (obj.screenY < -16 || obj.screenY > SCREEN_H + 16) return;

    // Proximity check against Sonic (player slot 0)
    const Object &sonic = v_objects[OBJ_SLOT_PLAYER];
    int dx = std::abs(obj.pixel_x() - sonic.pixel_x());
    int dy = std::abs(obj.pixel_y() - sonic.pixel_y());

    if (dx <= RING_COLLECT_RADIUS && dy <= RING_COLLECT_RADIUS) {
        // Sonic collected this ring
        sonic_collect_ring(const_cast<Object &>(sonic));
        obj.deactivate();
    }
}

// ---- Scatter ring ---------------------------------------------------
// Scatter velocity table: 8 pairs of (vx, vy) for an arc effect.
// Mirrors s2disasm's ScatterRing_Vel table.
static const struct { int16_t vx, vy; } scatter_vel[16] = {
    { 0x0100, -0x0400}, { 0x0200, -0x0380},
    { 0x0300, -0x02C0}, { 0x0380, -0x0180},
    {-0x0100, -0x0400}, {-0x0200, -0x0380},
    {-0x0300, -0x02C0}, {-0x0380, -0x0180},
    { 0x0080, -0x0500}, { 0x0180, -0x04C0},
    { 0x02C0, -0x0440}, { 0x0400, -0x02C0},
    {-0x0080, -0x0500}, {-0x0180, -0x04C0},
    {-0x02C0, -0x0440}, {-0x0400, -0x02C0},
};

void ring_scatter_all(int16_t from_x, int16_t from_y, uint8_t count)
{
    if (count > 16) count = 16;
    for (int i = 0; i < count; ++i) {
        Object *slot = object_alloc();
        if (!slot) break;
        scatter_ring_init(*slot, from_x, from_y,
                          scatter_vel[i].vx, scatter_vel[i].vy);
    }
}

void scatter_ring_init(Object &obj, int16_t wx, int16_t wy,
                       int16_t vx, int16_t vy)
{
    obj = Object{};
    obj.x        = (int32_t)wx << 8;
    obj.y        = (int32_t)wy << 8;
    obj.xvel     = vx;
    obj.yvel     = vy;
    obj.halfWidth  = 8;
    obj.halfHeight = 8;
    obj.colType    = ColType::Ring;
    obj.artTile    = 0x0050;

    auto &sd = *reinterpret_cast<ScatterRingData *>(obj.scratch);
    sd.timer       = 255;   // 256 frames = ~4.3 seconds
    sd.bounce_left = 3;
    obj.update = scatter_ring_update;
}

void scatter_ring_update(Object &obj)
{
    auto &sd = *reinterpret_cast<ScatterRingData *>(obj.scratch);

    // Gravity
    obj.yvel += SONIC_GRAVITY;
    if (obj.yvel > SONIC_MAX_FALL) obj.yvel = SONIC_MAX_FALL;
    obj.apply_velocity();

    // Simple floor bounce: if below a rough ground estimate, flip yvel
    // (proper floor detection requires Level reference — wire up later)
    if (sd.bounce_left > 0 && obj.yvel > 0) {
        int floor_est = (EHZ_ACT1_H - 4) * 128;  // rough floor y
        if (obj.pixel_y() > floor_est) {
            obj.y   = (int32_t)floor_est << 8;
            obj.yvel = -(obj.yvel >> 1);
            --sd.bounce_left;
        }
    }

    // Despawn timer
    if (--sd.timer == 0) { obj.deactivate(); return; }

    // Blink in last 60 frames
    if (sd.timer < 60 && (sd.timer & 4)) return;

    // Update screen position
    obj.screenX = (int16_t)(obj.pixel_x() - v_screenX);
    obj.screenY = (int16_t)(obj.pixel_y() - v_screenY);

    // Collect check
    const Object &sonic = v_objects[OBJ_SLOT_PLAYER];
    int dx = std::abs(obj.pixel_x() - sonic.pixel_x());
    int dy = std::abs(obj.pixel_y() - sonic.pixel_y());
    if (dx <= RING_COLLECT_RADIUS && dy <= RING_COLLECT_RADIUS) {
        sonic_collect_ring(const_cast<Object &>(sonic));
        obj.deactivate();
    }
}
