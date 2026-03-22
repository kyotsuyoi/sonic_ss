#include <jo/jo.h>
#include <string.h>
#include "tails.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "input_mapping.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *tails_ref = &player;
static jo_sidescroller_physics_params *tails_physics = &physics;

#define character_ref (*tails_ref)
#define physics (*tails_physics)

#define SPRITE_DIR "SPT"

static bool tails_loaded = false;
static bool tails_sheet_ready = false;
static bool tails_use_cart_ram = false;
static jo_img tails_sheet_img = {0};
static int tails_sheet_width = 0;
static int tails_sheet_height = 0;
static int tails_sprite_id = -1;
static int tails_punch_sprite_id = -1;
static int tails_damaged_sprite_id = -1;
static int tails_defeated_sprite_id = -1;
static int tails_tail_wram_sprite_id = -1;

#define TAILS_IDLE_FRAMES 8
#define TAILS_WALK_FRAMES 8
#define TAILS_RUN_FRAMES 8
#define TAILS_JUMP_FRAMES 5
#define TAILS_FALL_FRAMES 4
#define TAILS_LAND_FRAMES 3
#define TAILS_PUNCH_FRAMES 8
#define TAILS_DAMAGED_FRAMES 3
#define TAILS_LONG_FALL_MS 1000

typedef enum
{
    TailsAnimIdle = 0,
    TailsAnimWalk,
    TailsAnimRun,
    TailsAnimJump,
    TailsAnimFall,
    TailsAnimLand,
    TailsAnimPunch,
    TailsAnimDamaged
} tails_anim_mode_t;

static const jo_tile TailsDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void tails_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width)
{
    character_copy_sheet_frame_to_sprite_with_cart(sprite_id,
                                                  "TLS_FUL",
                                                  tails_sheet_width,
                                                  tails_sheet_height,
                                                  &tails_sheet_img,
                                                  tails_use_cart_ram,
                                                  frame_x,
                                                  frame_y,
                                                  frame_width,
                                                  target_width);
}

static int tails_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &tails_sprite_id);
}

static int tails_ensure_tail_wram_sprite(character_t *chr)
{
    (void)chr;
    if (tails_tail_wram_sprite_id < 0)
        tails_tail_wram_sprite_id = character_create_blank_sprite();
    return tails_tail_wram_sprite_id;
}

static int tails_ensure_punch_wram_sprite(character_t *chr)
{
    return character_ensure_punch_wram_sprite(chr);
}

static int tails_ensure_damaged_wram_sprite(character_t *chr)
{
    return character_ensure_damaged_wram_sprite(chr);
}

static int tails_calc_frame(character_t *chr, int mode)
{
    return character_calc_frame_generic(&chr->tails_anim_mode,
                                         &chr->tails_anim_frame,
                                         &chr->tails_anim_ticks,
                                         mode,
                                         TAILS_IDLE_FRAMES,
                                         TAILS_WALK_FRAMES,
                                         TAILS_RUN_FRAMES,
                                         TAILS_JUMP_FRAMES,
                                         TAILS_FALL_FRAMES,
                                         TAILS_LAND_FRAMES,
                                         TAILS_PUNCH_FRAMES,
                                         TAILS_DAMAGED_FRAMES);
}

static void tails_render_current_frame_for(character_t *chr, int sprite_id)
{
    character_render_sheet_frame(chr,
                                 sprite_id,
                                 tails_sheet_ready,
                                 &chr->tails_anim_mode,
                                 &chr->tails_anim_frame,
                                 character_get_frame_coords,
                                 tails_copy_sheet_frame_to_sprite);
}

static void tails_copy_tail_frame_to_sprite(int sprite_id, int tail_frame)
{
    if (sprite_id < 0 || tail_frame < 0 || tail_frame >= TAILS_TAIL_FRAME_COUNT)
        return;

    int frame_x = tail_frame * CHARACTER_WIDTH;
    int frame_y = CHARACTER_HEIGHT * 16;

    character_copy_sheet_frame_to_sprite_with_cart(sprite_id,
                                                  "TLS_FUL",
                                                  tails_sheet_width,
                                                  tails_sheet_height,
                                                  &tails_sheet_img,
                                                  tails_use_cart_ram,
                                                  frame_x,
                                                  frame_y,
                                                  CHARACTER_WIDTH,
                                                  CHARACTER_WIDTH);
}

static void tails_draw_tail_if_applicable(character_t *chr)
{
    if (!tails_sheet_ready)
        return;

    if (chr->tails_anim_mode != CHARACTER_ANIM_IDLE && chr->tails_anim_mode != CHARACTER_ANIM_WALK)
        return;

    int tail_sprite_id = tails_ensure_tail_wram_sprite(chr);
    if (tail_sprite_id < 0)
        return;

    tails_copy_tail_frame_to_sprite(tail_sprite_id, chr->tail_frame);

    int tail_offset = (chr->tails_anim_mode == CHARACTER_ANIM_IDLE ? 16 : 20);
    int draw_x = chr->x + (chr->flip ? tail_offset : -tail_offset);

    jo_sprite_draw3D2(tail_sprite_id, draw_x, chr->y, CHARACTER_SPRITE_Z);
}

void tails_running_animation_handling(void)
{
    character_t *chr = &character_ref;
    character_running_animation_handling_sheet(chr,
                                               &physics,
                                               &chr->tails_anim_mode,
                                               &chr->tails_anim_frame,
                                               &chr->tails_anim_ticks,
                                               &chr->tails_fall_time_ms,
                                               &chr->tails_land_pending,
                                               TAILS_LONG_FALL_MS,
                                               TAILS_PUNCH_FRAMES,
                                               &chr->perform_punch2);
}

static void tails_draw_for_character(character_t *chr)
{
    tails_draw_tail_if_applicable(chr);

    character_draw_sheet_character(chr,
                                   tails_sheet_ready,
                                   &chr->tails_anim_mode,
                                   &chr->tails_anim_frame,
                                   tails_calc_frame,
                                   tails_ensure_wram_sprite,
                                   tails_ensure_punch_wram_sprite,
                                   tails_render_current_frame_for,
                                   chr->flip);
}

void tails_draw(character_t *chr)
{
    tails_draw_for_character(chr);
}

void display_tails(void)
{
    character_display(&character_ref, &physics, tails_draw_for_character);
}

void tails_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    character_set_current(chr, phy, &tails_ref, &tails_physics);
}

void load_tails(void)
{
    if (character_load_generic(&character_ref,
                               &tails_loaded,
                               &tails_sheet_ready,
                               &tails_use_cart_ram,
                               &tails_sheet_img,
                               &tails_sheet_width,
                               &tails_sheet_height,
                               &tails_sprite_id,
                               &tails_punch_sprite_id,
                               &tails_damaged_sprite_id,
                               &tails_defeated_sprite_id,
                               SPRITE_DIR,
                               "TLS_FUL.TGA",
                               "TLS_DFT.TGA",
                               TailsDefeatedTile,
                               JO_TILE_COUNT(TailsDefeatedTile),
                               UiCharacterTails))
    {
        tails_physics = &physics;
        character_ref.tails_anim_mode = TailsAnimIdle;
        character_ref.tails_anim_frame = 0;
        character_ref.tails_anim_ticks = 0;
        character_ref.tails_fall_time_ms = 0;
        character_ref.tails_land_pending = false;
    }
}

void unload_tails(void)
{
    character_unload_generic(&character_ref,
                             &tails_loaded,
                             &tails_sheet_ready,
                             &tails_use_cart_ram,
                             &tails_sheet_img,
                             &tails_sheet_width,
                             &tails_sheet_height,
                             &tails_sprite_id,
                             "TLS_FUL");

    tails_tail_wram_sprite_id = -1;
}
