#include <jo/jo.h>
#include "world_collision.h"
#include "game_constants.h"

static bool has_vertical_collision(jo_sidescroller_physics_params *physics,
                                   character_t *controlled_character,
                                   int map_pos_x,
                                   int map_pos_y)
{
    int dist;

    controlled_character->can_jump = false;
    if (physics->speed_y < 0.0f)
    {
        physics->is_in_air = true;
        return false;
    }

    dist = jo_map_per_pixel_vertical_collision(
        WORLD_MAP_ID,
        map_pos_x + controlled_character->x + CHARACTER_WIDTH_2,
        map_pos_y + controlled_character->y + CHARACTER_HEIGHT,
        JO_NULL);

    if (dist == JO_MAP_NO_COLLISION || dist > 0)
    {
        if (dist != JO_MAP_NO_COLLISION && dist < CHARACTER_JUMP_PER_PIXEL_TOLERANCE)
            controlled_character->can_jump = true;
        physics->is_in_air = true;
        return false;
    }

    if (dist < 0 && jo_is_float_equals_zero(physics->speed_y))
        controlled_character->y += dist;

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

    attr = jo_map_hitbox_detection_custom_boundaries(
        WORLD_MAP_ID,
        probe_x,
        map_pos_y + controlled_character->y,
        2,
        20);

    if (attr == JO_MAP_NO_COLLISION)
        return false;
    if (attr != MAP_TILE_BLOCK_ATTR)
        return false;
    return true;
}

void world_handle_character_collision(jo_sidescroller_physics_params *physics,
                                      character_t *controlled_character,
                                      int *map_pos_x,
                                      int map_pos_y)
{
    if (has_vertical_collision(physics, controlled_character, *map_pos_x, map_pos_y))
        physics->speed_y = 0.0f;
    else
    {
        jo_physics_apply_gravity(physics);
        controlled_character->y += physics->speed_y;
    }

    if (has_horizontal_collision(physics, controlled_character, *map_pos_x, map_pos_y))
        physics->speed = 0.0f;
    else if (physics->speed > 0.0f)
        *map_pos_x += physics->speed < 1.0f ? 1.0f : physics->speed;
    else if (physics->speed < 0.0f)
        *map_pos_x += physics->speed > -1.0f ? -1.0f : physics->speed;
}

void world_handle_player_collision(jo_sidescroller_physics_params *physics,
                                   character_t *player,
                                   int *map_pos_x,
                                   int map_pos_y)
{
    world_handle_character_collision(physics, player, map_pos_x, map_pos_y);
}