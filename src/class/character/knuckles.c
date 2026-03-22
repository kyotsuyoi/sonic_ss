#include <jo/jo.h>
#include <string.h>
#include "knuckles.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "input_mapping.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *knuckles_ref = &player;
static jo_sidescroller_physics_params *knuckles_physics = &physics;

#define character_ref (*knuckles_ref)
#define physics (*knuckles_physics)

#define SPRITE_DIR "SPT"

static bool knuckles_loaded = false;
static bool knuckles_sheet_ready = false;
static bool knuckles_use_cart_ram = false;
static jo_img knuckles_sheet_img = {0};
static int knuckles_sheet_width = 0;
static int knuckles_sheet_height = 0;
static int knuckles_sprite_id = -1;
static int knuckles_punch_sprite_id = -1;
static int knuckles_damaged_sprite_id = -1;
static int knuckles_defeated_sprite_id = -1;

#define KNUCKLES_IDLE_FRAMES 8
#define KNUCKLES_WALK_FRAMES 8
#define KNUCKLES_RUN_FRAMES 6
#define KNUCKLES_JUMP_FRAMES 5
#define KNUCKLES_FALL_FRAMES 4
#define KNUCKLES_LAND_FRAMES 3
#define KNUCKLES_PUNCH_FRAMES 6
#define KNUCKLES_DAMAGED_FRAMES 3
#define KNUCKLES_LONG_FALL_MS 1000

typedef enum
{
    KnucklesAnimIdle = 0,
    KnucklesAnimWalk,
    KnucklesAnimRun,
    KnucklesAnimJump,
    KnucklesAnimFall,
    KnucklesAnimLand,
    KnucklesAnimPunch,
    KnucklesAnimDamaged
} knuckles_anim_mode_t;

static const jo_tile KnucklesDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void knuckles_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width)
{
    character_copy_sheet_frame_to_sprite_with_cart(sprite_id,
                                                  "KNK_FUL",
                                                  knuckles_sheet_width,
                                                  knuckles_sheet_height,
                                                  &knuckles_sheet_img,
                                                  knuckles_use_cart_ram,
                                                  frame_x,
                                                  frame_y,
                                                  frame_width,
                                                  target_width);
}

static int knuckles_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &knuckles_sprite_id);
}

static int knuckles_ensure_punch_wram_sprite(character_t *chr)
{
    return character_ensure_punch_wram_sprite(chr);
}

static int knuckles_ensure_damaged_wram_sprite(character_t *chr)
{
    return character_ensure_damaged_wram_sprite(chr);
}

static int knuckles_calc_frame(character_t *chr, int mode)
{
    return character_calc_frame_generic(&chr->knuckles_anim_mode,
                                         &chr->knuckles_anim_frame,
                                         &chr->knuckles_anim_ticks,
                                         mode,
                                         KNUCKLES_IDLE_FRAMES,
                                         KNUCKLES_WALK_FRAMES,
                                         KNUCKLES_RUN_FRAMES,
                                         KNUCKLES_JUMP_FRAMES,
                                         KNUCKLES_FALL_FRAMES,
                                         KNUCKLES_LAND_FRAMES,
                                         KNUCKLES_PUNCH_FRAMES,
                                         KNUCKLES_DAMAGED_FRAMES);
}

static void knuckles_render_current_frame_for(character_t *chr, int sprite_id)
{
    character_render_sheet_frame(chr,
                                 sprite_id,
                                 knuckles_sheet_ready,
                                 &chr->knuckles_anim_mode,
                                 &chr->knuckles_anim_frame,
                                 character_get_frame_coords,
                                 knuckles_copy_sheet_frame_to_sprite);
}

void knuckles_running_animation_handling(void)
{
    character_t *chr = &character_ref;
    character_running_animation_handling_sheet(chr,
                                               &physics,
                                               &chr->knuckles_anim_mode,
                                               &chr->knuckles_anim_frame,
                                               &chr->knuckles_anim_ticks,
                                               &chr->knuckles_fall_time_ms,
                                               &chr->knuckles_land_pending,
                                               KNUCKLES_LONG_FALL_MS,
                                               KNUCKLES_PUNCH_FRAMES,
                                               &chr->perform_kick2);
}

static void knuckles_draw_for_character(character_t *chr)
{
    character_draw_sheet_character(chr,
                                   knuckles_sheet_ready,
                                   &chr->knuckles_anim_mode,
                                   &chr->knuckles_anim_frame,
                                   knuckles_calc_frame,
                                   knuckles_ensure_wram_sprite,
                                   knuckles_ensure_punch_wram_sprite,
                                   knuckles_render_current_frame_for,
                                   chr->flip);
}

void knuckles_draw(character_t *chr)
{
    knuckles_draw_for_character(chr);
}

void display_knuckles(void)
{
    character_display(&character_ref, &physics, knuckles_draw_for_character);
}

void knuckles_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    character_set_current(chr, phy, &knuckles_ref, &knuckles_physics);
}

void load_knuckles(void)
{
    if (character_load_generic(&character_ref,
                               &knuckles_loaded,
                               &knuckles_sheet_ready,
                               &knuckles_use_cart_ram,
                               &knuckles_sheet_img,
                               &knuckles_sheet_width,
                               &knuckles_sheet_height,
                               &knuckles_sprite_id,
                               &knuckles_punch_sprite_id,
                               &knuckles_damaged_sprite_id,
                               &knuckles_defeated_sprite_id,
                               SPRITE_DIR,
                               "KNK_FUL.TGA",
                               "KNK_DFT.TGA",
                               KnucklesDefeatedTile,
                               JO_TILE_COUNT(KnucklesDefeatedTile),
                               UiCharacterKnuckles))
    {
        knuckles_physics = &physics;
        character_ref.knuckles_anim_mode = KnucklesAnimIdle;
        character_ref.knuckles_anim_frame = 0;
        character_ref.knuckles_anim_ticks = 0;
        character_ref.knuckles_fall_time_ms = 0;
        character_ref.knuckles_land_pending = false;
    }
}

void unload_knuckles(void)
{
    character_unload_generic(&character_ref,
                             &knuckles_loaded,
                             &knuckles_sheet_ready,
                             &knuckles_use_cart_ram,
                             &knuckles_sheet_img,
                             &knuckles_sheet_width,
                             &knuckles_sheet_height,
                             &knuckles_sprite_id,
                             "KNK_FUL");
}