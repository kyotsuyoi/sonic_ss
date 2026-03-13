#include <jo/jo.h>
#include "world_collision.h"
#include "debug.h"
#include "game_constants.h"
#include "jo_ext/jo_map_ext.h"

static inline float world_collision_fabs(float x)
{
    return (x < 0.0f) ? -x : x;
}

static inline float world_collision_round(float x)
{
    if (x >= 0.0f)
        return (float)((int)(x + 0.5f));
    return (float)((int)(x - 0.5f));
}

#define PLATFORM_GROUNDED_SNAP_MAX_PIXELS (4)

static bool has_vertical_collision(jo_sidescroller_physics_params *physics,
                                   character_t *controlled_character,
                                   int map_pos_x,
                                   int map_pos_y)
{
    int dist;
    int attr;
    bool was_in_air;
    int foot_x;
    int foot_y;

    was_in_air = physics->is_in_air;
    controlled_character->can_jump = false;
    if (physics->speed_y < 0.0f)
    {
        physics->is_in_air = true;
        return false;
    }

    foot_x = map_pos_x + controlled_character->x + CHARACTER_WIDTH_2;
    foot_y = map_pos_y + controlled_character->y + CHARACTER_HEIGHT - 1;

    dist = jo_map_per_pixel_vertical_collision_ext(
        WORLD_MAP_ID,
        foot_x,
        map_pos_y + controlled_character->y + CHARACTER_HEIGHT,
        JO_NULL);

    /* Determine interaction type from the tile directly under character center feet. */
    attr = jo_map_hitbox_detection_custom_boundaries_ext(
        WORLD_MAP_ID,
        foot_x,
        foot_y,
        1,
        1);

    /* Only block attr and platform attr can be used as ground.
       Attr 0 / omitted attribute are treated as no interaction. */
    if (attr != MAP_TILE_BLOCK_ATTR && attr != MAP_TILE_PLATFORM_ATTR)
    {
        physics->is_in_air = true;
        return false;
    }

    /* One-way platform behavior: attr 2 should not grab from the side while airborne. */
    if (attr == MAP_TILE_PLATFORM_ATTR)
    {
        if (was_in_air)
        {
            /* While airborne, platform accepts only downward contact from above. */
            if (physics->speed_y <= 0.0f || dist < -CHARACTER_JUMP_PER_PIXEL_TOLERANCE)
            {
                physics->is_in_air = true;
                return false;
            }
        }
        else
        {
            /* When already grounded, avoid large side-contact snap to platform top. */
            if (dist < -PLATFORM_GROUNDED_SNAP_MAX_PIXELS)
            {
                physics->is_in_air = true;
                return false;
            }
        }
    }

    if (dist == JO_MAP_NO_COLLISION || dist > 0)
    {
        if (dist != JO_MAP_NO_COLLISION && dist < CHARACTER_JUMP_PER_PIXEL_TOLERANCE)
            controlled_character->can_jump = true;
        physics->is_in_air = true;
        return false;
    }

    if (dist < 0)
    {
        if (attr == MAP_TILE_PLATFORM_ATTR)
        {
            if (was_in_air || dist >= -PLATFORM_GROUNDED_SNAP_MAX_PIXELS)
                controlled_character->y += dist;
        }
        else if (jo_is_float_equals_zero(physics->speed_y))
            controlled_character->y += dist;
    }

    controlled_character->can_jump = true;
    physics->is_in_air = false;
    return true;
}

static bool has_horizontal_collision(jo_sidescroller_physics_params *physics,
                                     const character_t *controlled_character,
                                     int map_pos_x,
                                     int map_pos_y)
{
    int probe_x;
    int attr;

    if (jo_physics_is_going_on_the_right(physics))
        probe_x = map_pos_x + controlled_character->x + CHARACTER_WIDTH - 2;
    else if (jo_physics_is_going_on_the_left(physics))
        probe_x = map_pos_x + controlled_character->x + 1;
    else
        return false;

    attr = jo_map_hitbox_detection_custom_boundaries_ext(
        WORLD_MAP_ID,
        probe_x,
        map_pos_y + controlled_character->y,
        2,
        20);

    /* Only block attribute should block horizontal movement. */
    if (attr == JO_MAP_NO_COLLISION)
        return false;
    return (attr == MAP_TILE_BLOCK_ATTR);
}


void world_handle_character_collision(jo_sidescroller_physics_params *physics,
                                      character_t *controlled_character,
                                      int *map_pos_x,
                                      int map_pos_y)
{
    int prev_map_x = *map_pos_x;

    (void)controlled_character;

    if (has_vertical_collision(physics, controlled_character, *map_pos_x, map_pos_y))
        physics->speed_y = 0.0f;
    else
    {
        jo_physics_apply_gravity(physics);
        controlled_character->y += physics->speed_y;
    }

    if (has_horizontal_collision(physics, controlled_character, *map_pos_x, map_pos_y))
        physics->speed = 0.0f;
    else
    {
        float delta = world_collision_round(physics->speed);
        if (delta == 0.0f && physics->speed != 0.0f)
        {
            // Guarantee at least 1 pixel movement when there is non-zero speed.
            delta = (physics->speed > 0.0f) ? 1.0f : -1.0f;
        }
        *map_pos_x += (int)delta;
    }

    g_dbg_knock_dx = *map_pos_x - prev_map_x;
}

void world_handle_player_collision(jo_sidescroller_physics_params *physics,
                                   character_t *player,
                                   int *map_pos_x,
                                   int map_pos_y)
{
    world_handle_character_collision(physics, player, map_pos_x, map_pos_y);
}