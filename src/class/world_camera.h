#ifndef WORLD_CAMERA_H
#define WORLD_CAMERA_H

#include "character.h"

void world_camera_handle(character_t *player, int prev_x, int prev_y, int *map_pos_x, int *map_pos_y);

#endif