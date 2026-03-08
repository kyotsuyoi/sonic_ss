#ifndef WORLD_COLLISION_H
#define WORLD_COLLISION_H

#include <jo/jo.h>
#include "character.h"

void world_handle_character_collision(jo_sidescroller_physics_params *physics,
                                      character_t *character,
                                      int *map_pos_x,
                                      int map_pos_y);

void world_handle_player_collision(jo_sidescroller_physics_params *physics,
                                   character_t *player,
                                   int *map_pos_x,
                                   int map_pos_y);

#endif