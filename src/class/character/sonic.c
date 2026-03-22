#include <jo/jo.h>
#include <string.h>
#include "sonic.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "input_mapping.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *sonic_ref = &player;
static jo_sidescroller_physics_params *sonic_physics = &physics;

#define character_ref (*sonic_ref)
#define physics (*sonic_physics)

#define SPRITE_DIR "SPT"

static bool sonic_loaded = false;
static bool sonic_sheet_ready = false;
static bool sonic_use_cart_ram = false;
static jo_img sonic_sheet_img = {0};
static int sonic_sheet_width = 0;
static int sonic_sheet_height = 0;
static int sonic_sprite_id = -1;
static int sonic_punch_sprite_id = -1;
static int sonic_damaged_sprite_id = -1;
static int sonic_defeated_sprite_id = -1;

#define SONIC_IDLE_FRAMES 6
#define SONIC_WALK_FRAMES 8
#define SONIC_RUN_FRAMES 6
#define SONIC_JUMP_FRAMES 5
#define SONIC_FALL_FRAMES 4
#define SONIC_LAND_FRAMES 3
#define SONIC_PUNCH_FRAMES 6
#define SONIC_DAMAGED_FRAMES 3
#define SONIC_LONG_FALL_MS 1000

typedef enum
{
    SonicAnimIdle = 0,
    SonicAnimWalk,
    SonicAnimRun,
    SonicAnimJump,
    SonicAnimFall,
    SonicAnimLand,
    SonicAnimPunch,
    SonicAnimDamaged
} sonic_anim_mode_t;

static const jo_tile SonicDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void sonic_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width)
{
    character_copy_sheet_frame_to_sprite_with_cart(sprite_id,
                                                  "SNC_FUL",
                                                  sonic_sheet_width,
                                                  sonic_sheet_height,
                                                  &sonic_sheet_img,
                                                  sonic_use_cart_ram,
                                                  frame_x,
                                                  frame_y,
                                                  frame_width,
                                                  target_width);
}

static int sonic_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &sonic_sprite_id);
}

static int sonic_ensure_punch_wram_sprite(character_t *chr)
{
    return character_ensure_punch_wram_sprite(chr);
}

static int sonic_ensure_damaged_wram_sprite(character_t *chr)
{
    return character_ensure_damaged_wram_sprite(chr);
}

static int sonic_calc_frame(character_t *chr, int mode)
{
    return character_calc_frame_generic(&chr->sonic_anim_mode,
                                         &chr->sonic_anim_frame,
                                         &chr->sonic_anim_ticks,
                                         mode,
                                         SONIC_IDLE_FRAMES,
                                         SONIC_WALK_FRAMES,
                                         SONIC_RUN_FRAMES,
                                         SONIC_JUMP_FRAMES,
                                         SONIC_FALL_FRAMES,
                                         SONIC_LAND_FRAMES,
                                         SONIC_PUNCH_FRAMES,
                                         SONIC_DAMAGED_FRAMES);
}

static void sonic_render_current_frame_for(character_t *chr, int sprite_id)
{
    character_render_sheet_frame(chr,
                                 sprite_id,
                                 sonic_sheet_ready,
                                 &chr->sonic_anim_mode,
                                 &chr->sonic_anim_frame,
                                 character_get_frame_coords,
                                 sonic_copy_sheet_frame_to_sprite);
}

void sonic_running_animation_handling(void)
{
    character_t *chr = &character_ref;
    character_running_animation_handling_sheet(chr,
                                               &physics,
                                               &chr->sonic_anim_mode,
                                               &chr->sonic_anim_frame,
                                               &chr->sonic_anim_ticks,
                                               &chr->sonic_fall_time_ms,
                                               &chr->sonic_land_pending,
                                               SONIC_LONG_FALL_MS,
                                               SONIC_PUNCH_FRAMES,
                                               &chr->perform_punch2);
}

static void sonic_draw_for_character(character_t *chr)
{
    character_draw_sheet_character(chr,
                                   sonic_sheet_ready,
                                   &chr->sonic_anim_mode,
                                   &chr->sonic_anim_frame,
                                   sonic_calc_frame,
                                   sonic_ensure_wram_sprite,
                                   sonic_ensure_punch_wram_sprite,
                                   sonic_render_current_frame_for,
                                   chr->flip);
}

void sonic_draw(character_t *chr)
{
    sonic_draw_for_character(chr);
}

void display_sonic(void)
{
    character_display(&character_ref, &physics, sonic_draw_for_character);
}

void sonic_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    character_set_current(chr, phy, &sonic_ref, &sonic_physics);
}

void load_sonic(void)
{
    if (character_load_generic(&character_ref,
                               &sonic_loaded,
                               &sonic_sheet_ready,
                               &sonic_use_cart_ram,
                               &sonic_sheet_img,
                               &sonic_sheet_width,
                               &sonic_sheet_height,
                               &sonic_sprite_id,
                               &sonic_punch_sprite_id,
                               &sonic_damaged_sprite_id,
                               &sonic_defeated_sprite_id,
                               SPRITE_DIR,
                               "SNC_FUL.TGA",
                               "SNC_DFT.TGA",
                               SonicDefeatedTile,
                               JO_TILE_COUNT(SonicDefeatedTile),
                               UiCharacterSonic))
    {
        sonic_physics = &physics;
        character_ref.sonic_anim_mode = SonicAnimIdle;
        character_ref.sonic_anim_frame = 0;
        character_ref.sonic_anim_ticks = 0;
        character_ref.sonic_fall_time_ms = 0;
        character_ref.sonic_land_pending = false;
    }
}

void unload_sonic(void)
{
    character_unload_generic(&character_ref,
                             &sonic_loaded,
                             &sonic_sheet_ready,
                             &sonic_use_cart_ram,
                             &sonic_sheet_img,
                             &sonic_sheet_width,
                             &sonic_sheet_height,
                             &sonic_sprite_id,
                             "SNC_FUL");
}