#include <jo/jo.h>
#include "damage_fx.h"
#include "game_constants.h"

#define SPRITE_DIR "SPT"
#define DAMAGE_FX_FRAME_WIDTH 48
#define DAMAGE_FX_FRAME_HEIGHT 48
#define DAMAGE_FX_FRAME_COUNT 4
#define DAMAGE_FX_TOTAL_TICKS 16

static int damage_fx_base_id = -1;
static bool damage_fx_loaded = false;
static int damage_fx_world_x = 0;
static int damage_fx_world_y = 0;
static int damage_fx_timer = 0;

static const jo_tile DamageFxTiles[] =
{
    {DAMAGE_FX_FRAME_WIDTH * 0, 0, DAMAGE_FX_FRAME_WIDTH, DAMAGE_FX_FRAME_HEIGHT},
    {DAMAGE_FX_FRAME_WIDTH * 1, 0, DAMAGE_FX_FRAME_WIDTH, DAMAGE_FX_FRAME_HEIGHT},
    {DAMAGE_FX_FRAME_WIDTH * 2, 0, DAMAGE_FX_FRAME_WIDTH, DAMAGE_FX_FRAME_HEIGHT},
    {DAMAGE_FX_FRAME_WIDTH * 3, 0, DAMAGE_FX_FRAME_WIDTH, DAMAGE_FX_FRAME_HEIGHT}
};

void damage_fx_setup(void)
{
    if (damage_fx_loaded)
        return;

    damage_fx_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR,
                                                  "DMG_FXS.TGA",
                                                  JO_COLOR_Green,
                                                  DamageFxTiles,
                                                  JO_TILE_COUNT(DamageFxTiles));
    damage_fx_loaded = true;
    damage_fx_timer = 0;
}

void damage_fx_reset(void)
{
    damage_fx_timer = 0;
}

void damage_fx_trigger_world(int world_x, int world_y)
{
    if (!damage_fx_loaded || damage_fx_base_id < 0)
        return;

    damage_fx_world_x = world_x;
    damage_fx_world_y = world_y;
    damage_fx_timer = DAMAGE_FX_TOTAL_TICKS;
}

void damage_fx_draw(int map_pos_x, int map_pos_y)
{
    int elapsed;
    int frame;
    int screen_x;
    int screen_y;

    if (!damage_fx_loaded || damage_fx_base_id < 0 || damage_fx_timer <= 0)
        return;

    elapsed = DAMAGE_FX_TOTAL_TICKS - damage_fx_timer;
    frame = elapsed / 4;
    if (frame >= DAMAGE_FX_FRAME_COUNT)
        frame = DAMAGE_FX_FRAME_COUNT - 1;

    if (((damage_fx_timer / 2) & 1) == 0)
    {
        screen_x = (damage_fx_world_x - map_pos_x) + (CHARACTER_WIDTH / 2) - (DAMAGE_FX_FRAME_WIDTH / 2);
        screen_y = (damage_fx_world_y - map_pos_y) + (CHARACTER_HEIGHT / 2) - (DAMAGE_FX_FRAME_HEIGHT / 2);
        jo_sprite_draw3D2(damage_fx_base_id + frame, screen_x, screen_y, DAMAGE_FX_SPRITE_Z);
    }

    --damage_fx_timer;
}
