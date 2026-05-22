#include "monitor.hpp"
#include "globals.hpp"
#include "sonic.hpp"
#include "ring.hpp"
#include <cstdlib>
#include <algorithm>

static MonitorData &md(Object &obj)
{
    return *reinterpret_cast<MonitorData *>(obj.scratch);
}

void monitor_init(Object &obj, MonitorItem item, int16_t wx, int16_t wy)
{
    obj = Object{};
    obj.x          = (int32_t)wx << 8;
    obj.y          = (int32_t)wy << 8;
    obj.halfWidth  = 15;
    obj.halfHeight = 15;
    obj.colType    = ColType::Item;
    // artTile: monitor frame tile — needs VRAM offset from art upload
    obj.artTile    = 0x0100;
    obj.update     = monitor_update;

    md(obj).item        = item;
    md(obj).break_timer = 0;
    md(obj).broken      = false;
}

// Apply the monitor's item to Sonic — translated from s2disasm's Monitor_GiveItem
static void give_item(const MonitorItem item)
{
    switch (item) {
    case MonitorItem::Rings:
        f_rings = (uint8_t)std::min((int)f_rings + 10, 99);
        break;
    case MonitorItem::Shield:
        // TODO: create shield object in Sonic's slot extra
        break;
    case MonitorItem::SpeedShoes:
        f_speedShoes = 60 * 20;  // 20 seconds at 60fps
        break;
    case MonitorItem::Invincible:
        f_invincible = 60 * 20;
        break;
    case MonitorItem::ExtraLife:
        if (f_lives < 9) ++f_lives;
        break;
    case MonitorItem::Continue:
        // TODO: increment continues counter
        break;
    case MonitorItem::S_Monitor:
        // TODO: trigger Super Sonic if rings >= 50
        break;
    }
}

// Axis-aligned overlap test between Sonic and a monitor box.
// Translated from s2disasm's monitor proximity test pattern.
static bool overlaps_sonic(const Object &mon)
{
    const Object &sonic = v_objects[OBJ_SLOT_PLAYER];
    int dx = std::abs(mon.pixel_x() - sonic.pixel_x());
    int dy = std::abs(mon.pixel_y() - sonic.pixel_y());
    return dx <= (mon.halfWidth  + sonic.halfWidth)  &&
           dy <= (mon.halfHeight + sonic.halfHeight);
}

void monitor_update(Object &obj)
{
    auto &m = md(obj);

    // Update screen position
    obj.screenX = (int16_t)(obj.pixel_x() - v_screenX);
    obj.screenY = (int16_t)(obj.pixel_y() - v_screenY);

    // Off-screen skip
    if (obj.screenX < -24 || obj.screenX > SCREEN_W + 24) return;
    if (obj.screenY < -24 || obj.screenY > SCREEN_H + 24) return;

    if (!m.broken) {
        // Break condition: Sonic hits the top of the monitor (jumping into it
        // from below with yvel < 0) or spin-dashes through it.
        const Object &sonic = v_objects[OBJ_SLOT_PLAYER];
        bool sonic_above = (sonic.pixel_y() + sonic.halfHeight) < obj.pixel_y();
        bool sonic_below = (sonic.pixel_y() - sonic.halfHeight) > obj.pixel_y();
        bool sonic_attacking = (sonic.status & (ST_JUMPING | ST_ROLL)) != 0;

        if (overlaps_sonic(obj)) {
            bool break_it = false;
            // Jumping from below (yvel > 0 = moving down onto monitor top)
            if (sonic_above && sonic.yvel > 0) break_it = true;
            // Attacking from the side or rolling through
            if (!sonic_above && !sonic_below && sonic_attacking) break_it = true;

            if (break_it) {
                m.broken      = true;
                m.break_timer = 30;  // 30 frames of break animation
                obj.artTile   = 0x0108;  // broken monitor sprite
                // Bounce Sonic upward slightly (s2disasm: yvel = -0x0400)
                Object &sv = v_objects[OBJ_SLOT_PLAYER];
                sv.yvel = -0x0400;
            } else if (!sonic_attacking) {
                // Solid collision — push Sonic out (simplified)
                // TODO: proper AABB resolution
            }
        }
    } else {
        // Break animation countdown then give item
        if (m.break_timer > 0) {
            --m.break_timer;
        } else {
            give_item(m.item);
            obj.deactivate();
        }
    }
}
