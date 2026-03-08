#include <jo/jo.h>
#include "world_camera.h"

void world_camera_handle(character_t *player, int prev_y, int *map_pos_y)
{
    int delta;

    delta = JO_ABS(player->y - prev_y);
    if (player->y > 100)
    {
        *map_pos_y += delta;
        player->y -= delta;
    }
    else if (player->y < 50)
    {
        *map_pos_y -= delta;
        player->y += delta;
    }
}