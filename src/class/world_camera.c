#include <jo/jo.h>
#include "world_camera.h"

void world_camera_handle(character_t *player, int prev_x, int prev_y, int *map_pos_x, int *map_pos_y)
{
    int delta_x;
    int delta_y;

    delta_x = JO_ABS(player->x - prev_x);
    if (player->x > 220)
    {
        *map_pos_x += delta_x;
        player->x -= delta_x;
    }
    else if (player->x < 40)
    {
        *map_pos_x -= delta_x;
        player->x += delta_x;
    }

    delta_y = JO_ABS(player->y - prev_y);
    if (player->y > 100)
    {
        *map_pos_y += delta_y;
        player->y -= delta_y;
    }
    else if (player->y < 50)
    {
        *map_pos_y -= delta_y;
        player->y += delta_y;
    }
}