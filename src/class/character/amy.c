#include <jo/jo.h>
#include <string.h>
#include "amy.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "input_mapping.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *amy_ref = &player;
static jo_sidescroller_physics_params *amy_physics = &physics;

#define character_ref (*amy_ref)
#define physics (*amy_physics)

#define SPRITE_DIR "SPT"

static bool amy_loaded = false;
static bool amy_sheet_ready = false;
static bool amy_use_cart_ram = false;
static jo_img amy_sheet_img = {0};
static int amy_sheet_width = 0;
static int amy_sheet_height = 0;
static int amy_sprite_id = -1;
static int amy_punch_sprite_id = -1;
static int amy_damaged_sprite_id = -1;
static int amy_defeated_sprite_id = -1;

#define AMY_IDLE_FRAMES 8
#define AMY_WALK_FRAMES 8
#define AMY_RUN_FRAMES 6
#define AMY_JUMP_FRAMES 5
#define AMY_FALL_FRAMES 4
#define AMY_LAND_FRAMES 3
#define AMY_PUNCH_FRAMES 6
#define AMY_DAMAGED_FRAMES 3
#define AMY_LONG_FALL_MS 1000

typedef enum
{
    AmyAnimIdle = 0,
    AmyAnimWalk,
    AmyAnimRun,
    AmyAnimJump,
    AmyAnimFall,
    AmyAnimLand,
    AmyAnimPunch,
    AmyAnimDamaged
} amy_anim_mode_t;

static const jo_tile AmyDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void amy_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width)
{
    character_copy_sheet_frame_to_sprite_with_cart(sprite_id,
                                                  "AMY_FUL",
                                                  amy_sheet_width,
                                                  amy_sheet_height,
                                                  &amy_sheet_img,
                                                  amy_use_cart_ram,
                                                  frame_x,
                                                  frame_y,
                                                  frame_width,
                                                  target_width);
}

static int amy_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &amy_sprite_id);
}

static int amy_ensure_punch_wram_sprite(character_t *chr)
{
    return character_ensure_punch_wram_sprite(chr);
}

static int amy_ensure_damaged_wram_sprite(character_t *chr)
{
    return character_ensure_damaged_wram_sprite(chr);
}

static int amy_calc_frame(character_t *chr, int mode)
{
    return character_calc_frame_generic(&chr->amy_anim_mode,
                                         &chr->amy_anim_frame,
                                         &chr->amy_anim_ticks,
                                         mode,
                                         AMY_IDLE_FRAMES,
                                         AMY_WALK_FRAMES,
                                         AMY_RUN_FRAMES,
                                         AMY_JUMP_FRAMES,
                                         AMY_FALL_FRAMES,
                                         AMY_LAND_FRAMES,
                                         AMY_PUNCH_FRAMES,
                                         AMY_DAMAGED_FRAMES);
}

static void amy_render_current_frame_for(character_t *chr, int sprite_id)
{
    character_render_sheet_frame(chr,
                                 sprite_id,
                                 amy_sheet_ready,
                                 &chr->amy_anim_mode,
                                 &chr->amy_anim_frame,
                                 character_get_frame_coords,
                                 amy_copy_sheet_frame_to_sprite);
}

void amy_running_animation_handling(void)
{
    character_t *chr = &character_ref;
    character_running_animation_handling_sheet(chr,
                                               &physics,
                                               &chr->amy_anim_mode,
                                               &chr->amy_anim_frame,
                                               &chr->amy_anim_ticks,
                                               &chr->amy_fall_time_ms,
                                               &chr->amy_land_pending,
                                               AMY_LONG_FALL_MS,
                                               AMY_PUNCH_FRAMES,
                                               &chr->perform_punch2);
}

static void amy_draw_for_character(character_t *chr)
{
    character_draw_sheet_character(chr,
                                   amy_sheet_ready,
                                   &chr->amy_anim_mode,
                                   &chr->amy_anim_frame,
                                   amy_calc_frame,
                                   amy_ensure_wram_sprite,
                                   amy_ensure_punch_wram_sprite,
                                   amy_render_current_frame_for,
                                   chr->flip);
}

void amy_draw(character_t *chr)
{
    amy_draw_for_character(chr);
}

void display_amy(void)
{
    character_display(&character_ref, &physics, amy_draw_for_character);
}

void amy_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    character_set_current(chr, phy, &amy_ref, &amy_physics);
}

void load_amy(void)
{
    if (character_load_generic(&character_ref,
                               &amy_loaded,
                               &amy_sheet_ready,
                               &amy_use_cart_ram,
                               &amy_sheet_img,
                               &amy_sheet_width,
                               &amy_sheet_height,
                               &amy_sprite_id,
                               &amy_punch_sprite_id,
                               &amy_damaged_sprite_id,
                               &amy_defeated_sprite_id,
                               SPRITE_DIR,
                               "AMY_FUL.TGA",
                               "AMY_DFT.TGA",
                               AmyDefeatedTile,
                               JO_TILE_COUNT(AmyDefeatedTile),
                               UiCharacterAmy))
    {
        amy_physics = &physics;
        character_ref.amy_anim_mode = AmyAnimIdle;
        character_ref.amy_anim_frame = 0;
        character_ref.amy_anim_ticks = 0;
        character_ref.amy_fall_time_ms = 0;
        character_ref.amy_land_pending = false;
    }
}

void unload_amy(void)
{
    character_unload_generic(&character_ref,
                             &amy_loaded,
                             &amy_sheet_ready,
                             &amy_use_cart_ram,
                             &amy_sheet_img,
                             &amy_sheet_width,
                             &amy_sheet_height,
                             &amy_sprite_id,
                             "AMY_FUL");
}
