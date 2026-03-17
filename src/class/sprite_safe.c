#include "sprite_safe.h"
#include <jo/jo.h>
#include <stdio.h>

static bool sprite_safe_anim_available = true;

int sprite_safe_create_anim(int base_id, int count, int duration)
{
    if (!sprite_safe_anim_available)
        return -1;

    if (base_id < 0 || count <= 0) {
        jo_printf(0, 2, "sprite_safe: invalid base_id=%d count=%d", base_id, count);
        return -1;
    }

    int last = jo_get_last_sprite_id();
    // base_id refers to a first sprite id; ensure base_id+count-1 <= last
    if (base_id + count - 1 > last) {
        jo_printf(0, 2, "sprite_safe: not enough sprites registered: base=%d count=%d last=%d", base_id, count, last);
        return -1;
    }

    int id = jo_create_sprite_anim(base_id, count, duration);
    if (id < 0)
    {
        jo_printf(0, 2, "sprite_safe: jo_create_sprite_anim failed for base=%d count=%d", base_id, count);
        sprite_safe_anim_available = false; // disable future attempts to avoid spamming failures
    }

    return id;
}

void sprite_safe_reset(void)
{
    sprite_safe_anim_available = true;
}


bool sprite_safe_can_create_anim(void)
{
    return sprite_safe_anim_available;
}
