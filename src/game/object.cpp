#include "object.hpp"
#include "globals.hpp"

// Run every active object slot in order.
// Mirrors the Obj_Run loop in s2disasm (the part that iterates $FFFFD000–end).
void objects_run_all()
{
    for (auto &obj : v_objects) {
        if (obj.active() && obj.update)
            obj.update(obj);
    }
}

// Find the first free (inactive) object slot.
// Returns nullptr if all slots are full.
Object *object_alloc()
{
    for (auto &obj : v_objects) {
        if (!obj.active()) {
            obj = Object{};  // clear the slot
            return &obj;
        }
    }
    return nullptr;
}
