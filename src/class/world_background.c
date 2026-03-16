#include <jo/jo.h>
#include "world_background.h"

static jo_img bg_gameplay_image;
static jo_img bg_menu_image;
static bool bg_gameplay_loaded = false;
static bool bg_menu_loaded = false;

typedef enum
{
    WorldBackgroundGameplay = 0,
    WorldBackgroundMenu
} world_background_mode_t;

static world_background_mode_t current_mode = (world_background_mode_t)-1;

void world_background_load(void)
{
    world_background_set_gameplay();
}

void world_background_invalidate(void)
{
    current_mode = (world_background_mode_t)-1;
}

void world_background_set_gameplay(void)
{
    if (!bg_gameplay_loaded)
    {
        bg_gameplay_image.data = JO_NULL;
        jo_tga_loader(&bg_gameplay_image, "BG", "BGG_GH1.TGA", JO_COLOR_Black);
        bg_gameplay_loaded = true;
    }

    if (current_mode != WorldBackgroundGameplay)
    {
        jo_set_background_sprite(&bg_gameplay_image, 0, 0);
        current_mode = WorldBackgroundGameplay;
    }
}

void world_background_set_menu(void)
{
    if (!bg_menu_loaded)
    {
        bg_menu_image.data = JO_NULL;
        jo_tga_loader(&bg_menu_image, "BG", "BGG_MEN.TGA", JO_COLOR_Black);
        bg_menu_loaded = true;
    }

    if (current_mode != WorldBackgroundMenu)
    {
        jo_set_background_sprite(&bg_menu_image, 0, 0);
        current_mode = WorldBackgroundMenu;
    }
}

void world_background_draw_parallax(int map_pos_x, int map_pos_y)
{
    float parallax_factor = 0.1f;
    int bg_offset_x = (int)(map_pos_x * parallax_factor);
    int bg_width = 512;
    int tile_x = ((bg_offset_x % bg_width) + bg_width) % bg_width;
    (void)map_pos_y;

    jo_move_background(tile_x, 0);
}