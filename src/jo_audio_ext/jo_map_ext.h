/* Header for map extension used by the game to avoid editing Jo Engine map.c */
#ifndef JO_MAP_EXT_H
#define JO_MAP_EXT_H

#include <stdbool.h>
#include "jo/jo.h"

bool jo_map_create_ext(const unsigned int layer, const unsigned short max_tile_count, const short depth);

void jo_map_add_tile_ext(const unsigned int layer, const short x, const short y,
#if JO_MAX_SPRITE > 255
                         const unsigned short sprite_id,
#else
                         const unsigned char sprite_id,
#endif
                         const unsigned char attribute);

void jo_map_add_animated_tile_ext(const unsigned int layer, const short x, const short y, const unsigned char anim_id, const unsigned char attribute);

bool jo_map_set_tile_sprite_ext(const unsigned int layer, const unsigned short tile_index,
#if JO_MAX_SPRITE > 255
                                const unsigned short sprite_id);
#else
                                const unsigned char sprite_id);
#endif

int jo_map_per_pixel_vertical_collision_ext(const unsigned int layer, int x, int y, unsigned char *attribute);
int jo_map_hitbox_detection_custom_boundaries_ext(const unsigned int layer, const int x, const int y, const int w, const int h);

void jo_map_draw_ext(const unsigned int layer, const short screen_x, const short screen_y);
void jo_map_free_ext(const unsigned int layer);

#endif