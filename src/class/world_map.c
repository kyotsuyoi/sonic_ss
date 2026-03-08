#include <jo/jo.h>
#include "game_constants.h"
#include "world_map.h"

void world_map_load(void)
{
    jo_sprite_add_image_pack("BLK", "BLK.TEX", JO_COLOR_Red);
    jo_map_load_from_file(WORLD_MAP_ID, 500, "MAP", "DEMO2.MAP");
}