#include <jo/jo.h>
#include <string.h>
#include "bot.h"
#include "player.h"
#include "game_constants.h"
#include "debug.h"
#include "ai_control.h"
#include "game_audio.h"
#include "damage_fx.h"
#include "world_collision.h"
#include "world_physics.h"
#include "world_map.h"
#include "character_registry.h"
#include "character/tails.h"
#include "character/sonic.h"
#include "character/amy.h"
#include "runtime_log.h"

#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 40
#define DEFEATED_SPRITE_HEIGHT 32

// Amy WRAM sheet support
static bool bot_amy_sheet_ready = false;
static jo_img bot_amy_sheet = {0};

static bool bot_sonic_sheet_ready = false;
static jo_img bot_sonic_sheet = {0};

/* Combat tuning */
#define BOT_ENGAGE_RANGE 9
#define BOT_COMBO_PUNCH2_RANGE 8
#define BOT_COMBO_KICK2_RANGE 9
#define BOT_PUNCH_START_RANGE 8
#define BOT_APPROACH_DISTANCE 16
#define BOT_TAKEN_STUN_LIGHT_FRAMES 28
#define BOT_TAKEN_STUN_HEAVY_FRAMES 36
#define BOT_KICK1_ATTACK_TIME 16
#define BOT_KICK2_ATTACK_TIME 20
#define BOT_TAIL_FRAME_COUNT 4
#define BOT_TAIL_FRAME_DURATION 5
#define BOT_TAIL_OFFSET_X 14
#define BOT_TAIL_Z CHARACTER_SPRITE_Z
#define KNUCKLES_KICK_PART3_INDEX 3
#define KNUCKLES_KICK_PART4_INDEX 2
#define KNUCKLES_KICK_PART3_WIDTH_PIXELS CHARACTER_WIDTH

#include "sprite_safe.h"
#define KNUCKLES_BOT_CHARGED_HOLD_FRAMES 60
#define BOT_AI_WARMUP_FRAMES 60

extern jo_sidescroller_physics_params physics;

typedef enum
{
    BotCharacterSonic = 0,
    BotCharacterAmy,
    BotCharacterTails,
    BotCharacterKnuckles,
    BotCharacterShadow,
    BotCharacterCount
} bot_character_t;

struct bot_instance
{
    character_t bot;
    jo_sound bot_jump_sfx;
    int bot_character;
    bool bot_loaded;
    bool bot_defeated;
    float bot_world_x;
    float bot_world_y;
    jo_sidescroller_physics_params bot_physics;
    int bot_walking_base_id;
    int bot_running1_base_id;
    int bot_running2_base_id;
    int bot_stand_base_id;
    int bot_punch_base_id;
    int bot_kick_base_id;
    int bot_walking_anim_id;
    int bot_running1_anim_id;
    int bot_running2_anim_id;
    int bot_stand_anim_id;
    int bot_punch_anim_id;
    int bot_kick_anim_id;
    int bot_punch_frame_count;
    int bot_kick_frame_count;
    int bot_stand_frame_count;
    int bot_punch_combo2_start_frame;
    int bot_kick_combo2_start_frame;
    int bot_jump_sprite_id;
    int bot_spin_sprite_id;
    int bot_damage_sprite_id;
    int bot_defeated_sprite_id;

    // When using the Amy WRAM sheet (AMY_FUL.TGA), each bot instance gets its own VRAM sprite
    // so multiple Amy bots can animate independently.
    int bot_wram_sheet_sprite_id;

    int bot_tail_base_id;
    int bot_tail_frame;
    int bot_tail_timer;
    int bot_attack_cooldown;
    int bot_attack_step;
    int bot_jump_cooldown;
    int bot_jump_hold_ms;
    int bot_jump_hold_target_ms;
    bool bot_jump_cut_applied;
    ai_bot_attack_t bot_current_attack;
    int bot_attack_timer;
    bool bot_attack_damage_done;
    bool bot_combo_requested;
    int bot_warmup_frames;
    bool prev_player_punch;
    bool prev_player_punch2;
    bool prev_player_kick;
    bool prev_player_kick2;
    bool prev_player2_punch;
    bool prev_player2_punch2;
    bool prev_player2_kick;
    bool prev_player2_kick2;
    bool bot_uses_shared_player_sprites;
    bool bot_use_player_flow;
    bool bot_is_ally;
    int bot_airborne_time_ms;
    bool bot_show_jump_sprite;
};

static bot_instance_t default_bot_instances[BOT_MAX_DEFAULT_COUNT] = {0};
static int default_bot_active_count = 1;
static bot_instance_t *ctx = &default_bot_instances[0];

typedef struct
{
    bool loaded;
    int instance_users;
    int walking_base_id;
    int running1_base_id;
    int running2_base_id;
    int stand_base_id;
    int punch_base_id;
    int kick_base_id;
    int jump_sprite_id;
    int spin_sprite_id;
    int damage_sprite_id;
    int defeated_sprite_id;
    int tail_base_id;
    int move_count;
    int stand_count;
    int punch_count;
    int kick_count;
    int punch_combo2_start_frame;
    int kick_combo2_start_frame;
} bot_character_assets_t;

static bot_character_assets_t bot_assets_cache[BotCharacterCount] = {0};

static int dbg_center_dx = 0;
static int dbg_center_dy = 0;
static int dbg_hreach_p1 = 0;
static int dbg_hreach_punch2 = 0;
static int dbg_hreach_k1 = 0;
static int dbg_hreach_k2 = 0;
static int dbg_vreach = 0;
static bool dbg_hit_p1 = false;
static bool dbg_hit_punch2 = false;
static bool dbg_hit_k1 = false;
static bool dbg_hit_k2 = false;
static jo_sidescroller_physics_params *target_player_physics_ctx = &physics;

#define bot (ctx->bot)
#define bot_jump_sfx (ctx->bot_jump_sfx)
#define bot_character (ctx->bot_character)
#define bot_loaded (ctx->bot_loaded)
#define bot_defeated (ctx->bot_defeated)
#define bot_world_x (ctx->bot_world_x)
#define bot_world_y (ctx->bot_world_y)
#define bot_physics (ctx->bot_physics)
#define bot_walking_base_id (ctx->bot_walking_base_id)
#define bot_running1_base_id (ctx->bot_running1_base_id)
#define bot_running2_base_id (ctx->bot_running2_base_id)
#define bot_stand_base_id (ctx->bot_stand_base_id)
#define bot_punch_base_id (ctx->bot_punch_base_id)
#define bot_kick_base_id (ctx->bot_kick_base_id)
#define bot_walking_anim_id (ctx->bot_walking_anim_id)
#define bot_running1_anim_id (ctx->bot_running1_anim_id)
#define bot_running2_anim_id (ctx->bot_running2_anim_id)
#define bot_stand_anim_id (ctx->bot_stand_anim_id)
#define bot_punch_anim_id (ctx->bot_punch_anim_id)
#define bot_kick_anim_id (ctx->bot_kick_anim_id)
#define bot_punch_frame_count (ctx->bot_punch_frame_count)
#define bot_kick_frame_count (ctx->bot_kick_frame_count)
#define bot_stand_frame_count (ctx->bot_stand_frame_count)
#define bot_punch_combo2_start_frame (ctx->bot_punch_combo2_start_frame)
#define bot_kick_combo2_start_frame (ctx->bot_kick_combo2_start_frame)
#define bot_jump_sprite_id (ctx->bot_jump_sprite_id)
#define bot_spin_asset_sprite_id (ctx->bot_spin_sprite_id)
#define bot_damage_sprite_id (ctx->bot_damage_sprite_id)
#define bot_defeated_sprite_id (ctx->bot_defeated_sprite_id)
#define bot_tail_base_id (ctx->bot_tail_base_id)
#define bot_tail_frame (ctx->bot_tail_frame)
#define bot_tail_timer (ctx->bot_tail_timer)
#define bot_attack_cooldown (ctx->bot_attack_cooldown)
#define bot_attack_step (ctx->bot_attack_step)
#define bot_jump_cooldown (ctx->bot_jump_cooldown)
#define bot_jump_hold_ms (ctx->bot_jump_hold_ms)
#define bot_jump_hold_target_ms (ctx->bot_jump_hold_target_ms)
#define bot_jump_cut_applied (ctx->bot_jump_cut_applied)
#define bot_current_attack (ctx->bot_current_attack)
#define bot_attack_timer (ctx->bot_attack_timer)
#define bot_attack_damage_done (ctx->bot_attack_damage_done)
#define bot_combo_requested (ctx->bot_combo_requested)
#define bot_warmup_frames (ctx->bot_warmup_frames)
#define prev_player_punch (ctx->prev_player_punch)
#define prev_player_punch2 (ctx->prev_player_punch2)
#define prev_player_kick (ctx->prev_player_kick)
#define prev_player_kick2 (ctx->prev_player_kick2)
#define prev_player2_punch (ctx->prev_player2_punch)
#define prev_player2_punch2 (ctx->prev_player2_punch2)
#define prev_player2_kick (ctx->prev_player2_kick)
#define prev_player2_kick2 (ctx->prev_player2_kick2)
#define bot_uses_shared_player_sprites (ctx->bot_uses_shared_player_sprites)
#define bot_use_player_flow (ctx->bot_use_player_flow)
#define bot_is_ally_flag (ctx->bot_is_ally)
#define bot_airborne_time_ms (ctx->bot_airborne_time_ms)
#define bot_show_jump_sprite (ctx->bot_show_jump_sprite)

// Bot sprite tile definitions are owned by the character system (character_sprites.c).
// Bot should not hardcode character-specific sprite layouts.

static int bot_punch_combo2_start_frame_for_count(int frame_count)
{
    if (frame_count >= 10)
        return 5;
    if (frame_count >= 4)
        return 2;
    if (frame_count >= 5)
        return 0;
    return -1;
}

static int bot_last_frame_for_player_attack(const character_t *player_ref, int attack_kind)
{
    int cid = player_ref->character_id;

    if (attack_kind == 0)
        /* punch1: second frame */
        return 1;
    if (attack_kind == 1)
        /* punch2: final frame */
        return 3;
    if (attack_kind == 2)
    {
        if (cid == CHARACTER_ID_KNUCKLES)
            return -1;
        if (cid == CHARACTER_ID_TAILS)
            return 0; /* Tails kick is single-frame; hit should occur immediately */
        return 1;
    }

    if (cid == CHARACTER_ID_TAILS)
        return 0; /* Tails kick2 also should hit immediately */

    if (cid == CHARACTER_ID_KNUCKLES && (player_ref->charged_kick_active || player_ref->charged_kick_phase > 0))
        return 2;
    return 2;
}

static bool bot_player_attack_reached_hit_frame(const character_t *player_ref, int attack_kind)
{
    int anim_id;
    int last_frame;

    if (attack_kind == 0 || attack_kind == 1)
        anim_id = player_ref->punch_anim_id;
    else
        anim_id = player_ref->kick_anim_id;

    if (anim_id < 0)
        return false;

    last_frame = bot_last_frame_for_player_attack(player_ref, attack_kind);
    if (last_frame < 0)
        return false;

    return jo_get_sprite_anim_frame(anim_id) >= last_frame;
}

static int bot_kick_combo2_start_frame_for_count(int frame_count)
{
    if (frame_count >= 13)
        return 7;
    if (frame_count >= 8)
        return 5;
    if (frame_count >= 4)
        return 2;
    return -1;
}

static void bot_reset_asset_ids(void)
{
    bot_walking_base_id = -1;
    bot_running1_base_id = -1;
    bot_running2_base_id = -1;
    bot_stand_base_id = -1;
    bot_punch_base_id = -1;
    bot_kick_base_id = -1;
    bot_walking_anim_id = -1;
    bot_running1_anim_id = -1;
    bot_use_player_flow = false;
    bot_running2_anim_id = -1;
    bot_stand_anim_id = -1;
    bot_punch_anim_id = -1;
    bot_kick_anim_id = -1;
    bot_punch_frame_count = 0;
    bot_kick_frame_count = 0;
    bot_punch_combo2_start_frame = -1;
    bot_kick_combo2_start_frame = -1;
    bot_jump_sprite_id = -1;
    bot_spin_asset_sprite_id = -1;
    bot_damage_sprite_id = -1;
    bot_defeated_sprite_id = -1;
    ctx->bot_wram_sheet_sprite_id = -1;
    bot_tail_base_id = -1;
    bot_tail_frame = 0;
    bot_tail_timer = 0;
    bot_is_ally_flag = false;
}

static void bot_update_tail_loop(void)
{
    if (bot_character != BotCharacterTails || bot_tail_base_id < 0)
        return;

    ++bot_tail_timer;
    if (bot_tail_timer >= BOT_TAIL_FRAME_DURATION)
    {
        bot_tail_timer = 0;
        bot_tail_frame = (bot_tail_frame + 1) % BOT_TAIL_FRAME_COUNT;
    }
}

static bool bot_should_draw_tail(void)
{
    if (bot_character != BotCharacterTails)
        return false;
    if (bot_tail_base_id < 0)
        return false;
    if (bot_defeated)
        return false;
    if (bot.stun_timer > 0)
        return false;
    if (bot.spin)
        return false;
    if (bot.kick || bot.kick2)
        return false;

    return true;
}

static bool bot_uses_player_flow(void)
{
    return bot_character == BotCharacterSonic || bot_character == BotCharacterAmy;
}

static void bot_draw_tail(void)
{
    int tail_x;

    if (!bot_should_draw_tail())
        return;

    tail_x = bot.x + (bot.flip ? BOT_TAIL_OFFSET_X : -BOT_TAIL_OFFSET_X);
    jo_sprite_draw3D2(bot_tail_base_id + bot_tail_frame, tail_x, bot.y, BOT_TAIL_Z);
}

static inline void bot_use_instance(bot_instance_t *instance)
{
    ctx = instance;
}

static void bot_reset_cached_assets(bot_character_assets_t *assets)
{
    assets->loaded = false;
    assets->instance_users = 0;
    assets->walking_base_id = -1;
    assets->running1_base_id = -1;
    assets->running2_base_id = -1;
    assets->stand_base_id = -1;
    assets->punch_base_id = -1;
    assets->kick_base_id = -1;
    assets->jump_sprite_id = -1;
    assets->spin_sprite_id = -1;
    assets->damage_sprite_id = -1;
    assets->defeated_sprite_id = -1;
    assets->tail_base_id = -1;
    assets->move_count = 0;
    assets->punch_count = 0;
    assets->kick_count = 0;
    assets->punch_combo2_start_frame = -1;
    assets->kick_combo2_start_frame = -1;
}

static bot_character_assets_t *bot_get_assets_cache(int character)
{
    if (character < BotCharacterSonic || character >= BotCharacterCount)
        character = BotCharacterSonic;

    return &bot_assets_cache[character];
}

static ui_character_choice_t bot_ui_character_from_bot_character(int character)
{
    switch (character)
    {
    case BotCharacterAmy:
        return UiCharacterAmy;
    case BotCharacterTails:
        return UiCharacterTails;
    case BotCharacterKnuckles:
        return UiCharacterKnuckles;
    case BotCharacterShadow:
        return UiCharacterShadow;
    case BotCharacterSonic:
    default:
        return UiCharacterSonic;
    }
}

static int anim_base_id_from_anim(int anim_id)
{
    int frame;
    int sprite_id;

    if (anim_id < 0)
        return -1;

    frame = jo_get_sprite_anim_frame(anim_id);
    sprite_id = jo_get_anim_sprite(anim_id);
    return sprite_id - frame;
}

static bool bot_validate_anim_set(void)
{
    return bot_walking_anim_id >= 0
        && bot_running1_anim_id >= 0
        && bot_running2_anim_id >= 0
        && bot_stand_anim_id >= 0
        && bot_punch_anim_id >= 0
        && bot_kick_anim_id >= 0;
}

static void bot_init_amy_sheet(void)
{
    if (bot_amy_sheet_ready)
        return;

    char *sheet_data = jo_fs_read_file_in_dir("AMY_FUL.TGA", SPRITE_DIR, JO_NULL);
    if (sheet_data == JO_NULL)
        return;

    if (jo_tga_loader_from_stream(&bot_amy_sheet, sheet_data, JO_COLOR_Green) == JO_TGA_OK)
        bot_amy_sheet_ready = true;
    jo_free(sheet_data);
}

static void bot_init_sonic_sheet(void)
{
    if (bot_sonic_sheet_ready)
        return;

    char *sheet_data = jo_fs_read_file_in_dir("SNC_FUL.TGA", SPRITE_DIR, JO_NULL);
    if (sheet_data == JO_NULL)
        return;

    if (jo_tga_loader_from_stream(&bot_sonic_sheet, sheet_data, JO_COLOR_Green) == JO_TGA_OK)
        bot_sonic_sheet_ready = true;
    jo_free(sheet_data);
}

static int bot_try_add_tga_tileset(const char *filename, const jo_tile *tiles, int count)
{
    char *data = jo_fs_read_file_in_dir(filename, SPRITE_DIR, JO_NULL);
    if (data == JO_NULL)
        return -1;
    jo_free(data);
    return jo_sprite_add_tga_tileset(SPRITE_DIR, filename, JO_COLOR_Green, tiles, count);
}

static int bot_try_add_tga(const char *filename)
{
    char *data = jo_fs_read_file_in_dir(filename, SPRITE_DIR, JO_NULL);
    if (data == JO_NULL)
        return -1;
    jo_free(data);
    return jo_sprite_add_tga(SPRITE_DIR, filename, JO_COLOR_Green);
}

static int bot_create_blank_sprite(void)
{
    jo_img blank;
    blank.width = CHARACTER_WIDTH;
    blank.height = CHARACTER_HEIGHT;
    blank.data = (unsigned short *)jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
    if (blank.data == JO_NULL)
        return -1;

    for (size_t i = 0; i < (size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT; ++i)
        ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

    int id = jo_sprite_add(&blank);
    jo_free_img(&blank);
    return id;
}

static void bot_copy_sheet_frame_to_sprite(const jo_img *sheet, int sprite_id, int frame_x, int frame_y)
{
    if (sheet == JO_NULL || sheet->data == JO_NULL || sprite_id < 0)
        return;

    static jo_img tmp = {0};
    if (tmp.data == JO_NULL)
    {
        tmp.width = CHARACTER_WIDTH;
        tmp.height = CHARACTER_HEIGHT;
        tmp.data = jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
        if (tmp.data == JO_NULL)
            return;
    }

    unsigned short *dst = (unsigned short *)tmp.data;
    unsigned short *src = (unsigned short *)sheet->data;
    int sheet_width = sheet->width;

    for (int y = 0; y < CHARACTER_HEIGHT; ++y)
    {
        unsigned short *src_row = src + (frame_y + y) * sheet_width + frame_x;
        unsigned short *dst_row = dst + y * CHARACTER_WIDTH;
        memcpy(dst_row, src_row, CHARACTER_WIDTH * sizeof(unsigned short));
    }

    jo_sprite_replace(&tmp, sprite_id);
}

#undef bot
#undef bot_punch_anim_id
#undef bot_kick_anim_id
#undef bot_walking_anim_id
#undef bot_running1_anim_id
#undef bot_running2_anim_id
#undef bot_stand_anim_id

static void bot_wram_render_current_frame(bot_instance_t *instance, const jo_img *sheet, bool sheet_ready, int jump_col, int damage_col, int spin_col)
{
    if (!sheet_ready || instance == JO_NULL || instance->bot_wram_sheet_sprite_id < 0)
        return;

    character_t *bot_ref = &instance->bot;

    int row = 0;
    int col = 0;

    if (bot_ref->spin)
    {
        row = 0;
        col = spin_col;
    }
    else if (bot_ref->life <= 0)
    {
        return; // defeated uses separate sprite
    }
    else if (bot_ref->stun_timer > 0)
    {
        row = 0;
        col = damage_col;
    }
    else if (bot_ref->punch || bot_ref->punch2)
    {
        row = 4;
        col = jo_get_sprite_anim_frame(instance->bot_punch_anim_id);
    }
    else if (bot_ref->kick || bot_ref->kick2)
    {
        row = 5;
        col = jo_get_sprite_anim_frame(instance->bot_kick_anim_id);
    }
    else if (bot_ref->jump)
    {
        row = 0;
        col = jump_col;
    }
    else
    {
        if (bot_ref->walk && bot_ref->run == 0)
        {
            row = 1;
            col = jo_get_sprite_anim_frame(instance->bot_walking_anim_id);
        }
        else if (bot_ref->walk && bot_ref->run == 1)
        {
            row = 2;
            col = jo_get_sprite_anim_frame(instance->bot_running1_anim_id);
        }
        else if (bot_ref->walk && bot_ref->run == 2)
        {
            row = 3;
            col = jo_get_sprite_anim_frame(instance->bot_running2_anim_id);
        }
        else
        {
            row = 0;
            col = jo_get_sprite_anim_frame(instance->bot_stand_anim_id);
        }
    }

    // Copy from the selected WRAM sheet into the per-bot VRAM sprite
    bot_copy_sheet_frame_to_sprite(sheet, instance->bot_wram_sheet_sprite_id, col * CHARACTER_WIDTH, row * CHARACTER_HEIGHT);
}

#define bot_punch_anim_id (ctx->bot_punch_anim_id)
#define bot_kick_anim_id (ctx->bot_kick_anim_id)
#define bot_walking_anim_id (ctx->bot_walking_anim_id)
#define bot_running1_anim_id (ctx->bot_running1_anim_id)
#define bot_running2_anim_id (ctx->bot_running2_anim_id)
#define bot_stand_anim_id (ctx->bot_stand_anim_id)
#define bot (ctx->bot)

static void bot_rollback_anim_set(void)
{
    if (bot_walking_anim_id >= 0) jo_remove_sprite_anim(bot_walking_anim_id);
    if (bot_running1_anim_id >= 0) jo_remove_sprite_anim(bot_running1_anim_id);
    if (bot_running2_anim_id >= 0) jo_remove_sprite_anim(bot_running2_anim_id);
    if (bot_stand_anim_id >= 0) jo_remove_sprite_anim(bot_stand_anim_id);
    if (bot_punch_anim_id >= 0) jo_remove_sprite_anim(bot_punch_anim_id);
    if (bot_kick_anim_id >= 0) jo_remove_sprite_anim(bot_kick_anim_id);
    bot_reset_asset_ids();
}

static bool bot_bind_shared_player_assets(int character)
{
    character_animation_profile_t anim_profile;
    int move_count;
    int stand_count;
    int punch_count;
    int kick_count;

    anim_profile = character_registry_get_animation_profile(bot_ui_character_from_bot_character(character));
    move_count = anim_profile.move_count;
    stand_count = anim_profile.stand_count;
    punch_count = anim_profile.punch_count;
    kick_count = anim_profile.kick_count;

    bot_walking_base_id = anim_base_id_from_anim(player.walking_anim_id);
    bot_running1_base_id = anim_base_id_from_anim(player.running1_anim_id);
    bot_running2_base_id = anim_base_id_from_anim(player.running2_anim_id);
    bot_stand_base_id = anim_base_id_from_anim(player.stand_sprite_id);
    bot_punch_base_id = anim_base_id_from_anim(player.punch_anim_id);
    bot_kick_base_id = anim_base_id_from_anim(player.kick_anim_id);
    bot_jump_sprite_id = player.jump_sprite_id;
    bot_spin_asset_sprite_id = player.spin_sprite_id;
    bot_damage_sprite_id = player.damage_sprite_id;
    bot_defeated_sprite_id = player.defeated_sprite_id;
    bot_tail_base_id = (character == BotCharacterTails) ? tails_get_tail_base_id() : -1;

    bot_walking_anim_id = jo_create_sprite_anim(bot_walking_base_id, move_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_running1_anim_id = jo_create_sprite_anim(bot_running1_base_id, move_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_running2_anim_id = jo_create_sprite_anim(bot_running2_base_id, move_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_stand_anim_id = jo_create_sprite_anim(bot_stand_base_id, stand_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_punch_anim_id = jo_create_sprite_anim(bot_punch_base_id, punch_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_kick_anim_id = jo_create_sprite_anim(bot_kick_base_id, kick_count, DEFAULT_SPRITE_FRAME_DURATION);

    if (!bot_validate_anim_set())
    {
        bot_rollback_anim_set();
        return false;
    }

    bot_punch_frame_count = punch_count;
    bot_kick_frame_count = kick_count;
    bot_stand_frame_count = stand_count;
    bot_punch_combo2_start_frame = bot_punch_combo2_start_frame_for_count(punch_count);
    bot_kick_combo2_start_frame = bot_kick_combo2_start_frame_for_count(kick_count);
    bot_uses_shared_player_sprites = true;
    return true;
}

static inline int bot_screen_x(int map_pos_x)
{
    return (int)bot_world_x - map_pos_x;
}

static inline int bot_screen_y(int map_pos_y)
{
    return (int)bot_world_y - map_pos_y;
}

static bool is_close_enough(int x1, int y1, int x2, int y2, int range)
{
    int center_x_1 = x1 + (CHARACTER_WIDTH / 2);
    int center_x_2 = x2 + (CHARACTER_WIDTH / 2);
    int center_y_1 = y1 + (CHARACTER_HEIGHT / 2);
    int center_y_2 = y2 + (CHARACTER_HEIGHT / 2);
    int horizontal_reach = (CHARACTER_WIDTH / 2) + range;
    int vertical_reach = (CHARACTER_HEIGHT / 2) + 12;

    return JO_ABS(center_x_1 - center_x_2) <= horizontal_reach
        && JO_ABS(center_y_1 - center_y_2) <= vertical_reach;
}

static bool is_attack_in_range(int attacker_x, int attacker_y, bool attacker_flip, int target_x, int target_y, int range)
{
    int attacker_left = attacker_x;
    int attacker_right = attacker_x + CHARACTER_WIDTH;
    int attacker_top = attacker_y;
    int attacker_bottom = attacker_y + CHARACTER_HEIGHT;
    int attack_left = attacker_left;
    int attack_right = attacker_right;
    int target_left = target_x;
    int target_right = target_x + CHARACTER_WIDTH;
    int target_top = target_y;
    int target_bottom = target_y + CHARACTER_HEIGHT;

    if (attacker_flip)
        attack_left -= range;
    else
        attack_right += range;

    return target_right >= attack_left
        && target_left <= attack_right
        && target_bottom >= attacker_top
        && target_top <= attacker_bottom;
}

static void bot_debug_update_hitbox(int player_world_x, int player_world_y, const character_t *player_ref)
{
    int center_x_player = player_world_x + (CHARACTER_WIDTH / 2);
    int center_x_bot = (int)bot_world_x + (CHARACTER_WIDTH / 2);
    int center_y_player = player_world_y + (CHARACTER_HEIGHT / 2);
    int center_y_bot = (int)bot_world_y + (CHARACTER_HEIGHT / 2);
    int player_hit_range_punch1 = player_ref->hit_range_punch1;
    int player_hit_range_punch2 = player_ref->hit_range_punch2;
    int player_hit_range_kick1 = player_ref->hit_range_kick1;
    int player_hit_range_kick2 = player_ref->hit_range_kick2;

    dbg_center_dx = JO_ABS(center_x_player - center_x_bot);
    dbg_center_dy = JO_ABS(center_y_player - center_y_bot);
    dbg_hreach_p1 = (CHARACTER_WIDTH / 2) + player_hit_range_punch1;
    dbg_hreach_punch2 = (CHARACTER_WIDTH / 2) + player_hit_range_punch2;
    dbg_hreach_k1 = (CHARACTER_WIDTH / 2) + player_hit_range_kick1;
    dbg_hreach_k2 = (CHARACTER_WIDTH / 2) + player_hit_range_kick2;
    dbg_vreach = (CHARACTER_HEIGHT / 2) + 12;

    dbg_hit_p1 = is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_punch1);
    dbg_hit_punch2 = is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_punch2);
    dbg_hit_k1 = is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_kick1);
    dbg_hit_k2 = is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_kick2);
}

static void bot_apply_physics(int map_pos_x, int map_pos_y)
{
    int bot_map_pos_x;

    bot_map_pos_x = map_pos_x;
    bot.x = (int)bot_world_x - map_pos_x;
    bot.y = (int)bot_world_y - map_pos_y;

    world_handle_character_collision(&bot_physics, &bot, &bot_map_pos_x, map_pos_y);

    if (bot_physics.is_in_air)
    {
        bot_airborne_time_ms += GAME_FRAME_MS;
        if (bot_physics.speed_y < 0.0f)
            bot_show_jump_sprite = true;
        else if (bot_physics.speed_y >= AIRBORNE_FALL_SPEED_THRESHOLD)
            bot_show_jump_sprite = true;
        else if (bot_airborne_time_ms >= AIRBORNE_SPRITE_DELAY_MS)
            bot_show_jump_sprite = true;
        else
            bot_show_jump_sprite = false;
    }
    else
    {
        bot_airborne_time_ms = 0;
        bot_show_jump_sprite = false;
    }

    bot_world_x = (float)(bot_map_pos_x + bot.x);
    bot_world_y = (float)(map_pos_y + bot.y);
}

static void reset_non_attack_anims(void)
{
    jo_reset_sprite_anim(bot.walking_anim_id);
    jo_reset_sprite_anim(bot.running1_anim_id);
    jo_reset_sprite_anim(bot.running2_anim_id);
    jo_reset_sprite_anim(bot.stand_sprite_id);
}

static void bot_reset_animation_lists_except(int active_anim_id)
{
    if (bot.walking_anim_id >= 0 && bot.walking_anim_id != active_anim_id)
        jo_reset_sprite_anim(bot.walking_anim_id);
    if (bot.running1_anim_id >= 0 && bot.running1_anim_id != active_anim_id)
        jo_reset_sprite_anim(bot.running1_anim_id);
    if (bot.running2_anim_id >= 0 && bot.running2_anim_id != active_anim_id)
        jo_reset_sprite_anim(bot.running2_anim_id);
    if (bot.stand_sprite_id >= 0 && bot.stand_sprite_id != active_anim_id)
        jo_reset_sprite_anim(bot.stand_sprite_id);
    if (bot.punch_anim_id >= 0 && bot.punch_anim_id != active_anim_id)
        jo_reset_sprite_anim(bot.punch_anim_id);
    if (bot.kick_anim_id >= 0 && bot.kick_anim_id != active_anim_id)
        jo_reset_sprite_anim(bot.kick_anim_id);
}

/*
Speed -> Animation mapping (bot):
- `speed_step = (int)JO_ABS(bot_physics.speed)` converts physics speed to a discrete level.
- The bot code maps speed ranges to an animation variant and a sprite frame rate (thresholds differ per implementation),
  but the principle is the same:
  * higher `speed_step` selects faster running variants and lower `frame_rate` values (faster animation)
  * lower `speed_step` selects walking and slower animation frame rates
- Lower `frame_rate` values advance animation frames faster (e.g. `3` animates quicker than `9`).
- `run` (or equivalent) selects which animation variant is drawn.
This keeps visual animation speed consistent with the bot's physical speed.
*/
static void bot_update_animation(void)
{
    int speed_step;

    if (bot_physics.is_in_air && bot_show_jump_sprite)
    {
        reset_non_attack_anims();

        if (bot_current_attack == AiBotAttackPunch1 || bot_current_attack == AiBotAttackPunch2)
        {
            jo_reset_sprite_anim(bot.kick_anim_id);
            if (jo_is_sprite_anim_stopped(bot.punch_anim_id))
                jo_start_sprite_anim(bot.punch_anim_id);
            return;
        }

        if (bot_current_attack == AiBotAttackKick1 || bot_current_attack == AiBotAttackKick2)
        {
            jo_reset_sprite_anim(bot.punch_anim_id);
            if (bot_character != BotCharacterTails && jo_is_sprite_anim_stopped(bot.kick_anim_id))
                jo_start_sprite_anim(bot.kick_anim_id);
            return;
        }

        jo_reset_sprite_anim(bot.punch_anim_id);
        jo_reset_sprite_anim(bot.kick_anim_id);
        return;
    }

    if (bot_current_attack == AiBotAttackPunch1 || bot_current_attack == AiBotAttackPunch2)
    {
        reset_non_attack_anims();
        jo_reset_sprite_anim(bot.kick_anim_id);
        if (jo_is_sprite_anim_stopped(bot.punch_anim_id))
            jo_start_sprite_anim(bot.punch_anim_id);
        return;
    }

    if (bot_current_attack == AiBotAttackKick1 || bot_current_attack == AiBotAttackKick2)
    {
        reset_non_attack_anims();
        jo_reset_sprite_anim(bot.punch_anim_id);
        if (bot_character != BotCharacterTails && jo_is_sprite_anim_stopped(bot.kick_anim_id))
            jo_start_sprite_anim(bot.kick_anim_id);
        return;
    }

    jo_reset_sprite_anim(bot.punch_anim_id);
    jo_reset_sprite_anim(bot.kick_anim_id);

    /* Derive run state from physics speed, so walk/run sprite selection works. */
    speed_step = (int)JO_ABS(bot_physics.speed);

    if (bot.walk)
    {
        if (speed_step >= 6)
        {
            bot.run = 2;
            if (bot.running2_anim_id >= 0)
                jo_set_sprite_anim_frame_rate(bot.running2_anim_id, 3);
        }
        else if (speed_step >= 5)
        {
            bot.run = 1;
            if (bot.running1_anim_id >= 0)
                jo_set_sprite_anim_frame_rate(bot.running1_anim_id, 4);
        }
        else if (speed_step >= 4)
        {
            bot.run = 1;
            if (bot.running1_anim_id >= 0)
                jo_set_sprite_anim_frame_rate(bot.running1_anim_id, 5);
        }
        else if (speed_step >= 3)
        {
            bot.run = 1;
            if (bot.running1_anim_id >= 0)
                jo_set_sprite_anim_frame_rate(bot.running1_anim_id, 6);
        }
        else if (speed_step >= 2)
        {
            bot.run = 1;
            if (bot.running1_anim_id >= 0)
                jo_set_sprite_anim_frame_rate(bot.running1_anim_id, 7);
        }
        else
        {
            bot.run = 0;
            if (bot.walking_anim_id >= 0)
                jo_set_sprite_anim_frame_rate(bot.walking_anim_id, 8);
        }
    }
    else
    {
        bot.run = 0;
    }

    int movement_anim_id = -1;
    if (bot.walk)
    {
        if (bot.run == 2)
            movement_anim_id = bot.running2_anim_id;
        else if (bot.run == 1)
            movement_anim_id = bot.running1_anim_id;
        else
            movement_anim_id = bot.walking_anim_id;
    }

    bot_reset_animation_lists_except(movement_anim_id);

    if (movement_anim_id >= 0 && jo_is_sprite_anim_stopped(movement_anim_id))
        jo_start_sprite_anim_loop(movement_anim_id);
}


static void bot_start_attack(ai_bot_attack_t attack, bool request_combo)
{
    int punch2_start_frame;
    int kick2_start_frame;

    bot_current_attack = attack;
    bot_attack_damage_done = false;
    bot_combo_requested = false;

    punch2_start_frame = (bot_punch_combo2_start_frame >= 0 && bot_punch_combo2_start_frame < bot_punch_frame_count)
        ? bot_punch_combo2_start_frame
        : 0;
    kick2_start_frame = (bot_kick_combo2_start_frame >= 0 && bot_kick_combo2_start_frame < bot_kick_frame_count)
        ? bot_kick_combo2_start_frame
        : 0;

    if (attack == AiBotAttackPunch1)
    {
        bot_attack_timer = 14;
        bot_combo_requested = request_combo;
        bot.punch = true;
        bot.punch2 = false;
        game_audio_play_sfx_next_channel(game_audio_get_punch_sfx());
        jo_set_sprite_anim_frame(bot.punch_anim_id, 0);
        jo_start_sprite_anim(bot.punch_anim_id);
    }
    else if (attack == AiBotAttackPunch2)
    {
        bot_attack_timer = 18;
        bot.punch = false;
        bot.punch2 = true;
        game_audio_play_sfx_next_channel(game_audio_get_kick_sfx());
        jo_set_sprite_anim_frame(bot.punch_anim_id, punch2_start_frame);
        jo_start_sprite_anim(bot.punch_anim_id);
    }
    else if (attack == AiBotAttackKick1)
    {
        bool knuckles_charge = (bot_character == BotCharacterKnuckles && bot.charged_kick_enabled);
        bot_attack_timer = BOT_KICK1_ATTACK_TIME;
        bot_combo_requested = knuckles_charge ? false : request_combo;
        bot.kick = true;
        bot.kick2 = false;
        bot.charged_kick_hold_ms = 0;
        bot.charged_kick_ready = false;
        bot.charged_kick_active = false;
        bot.charged_kick_phase = 0;
        bot.charged_kick_phase_timer = 0;
        if (knuckles_charge)
            bot_attack_timer = KNUCKLES_BOT_CHARGED_HOLD_FRAMES;
        game_audio_play_sfx_next_channel(game_audio_get_punch_sfx());
        if (bot_character != BotCharacterTails)
        {
            jo_set_sprite_anim_frame(bot.kick_anim_id, 0);
            jo_start_sprite_anim(bot.kick_anim_id);
        }
    }
    else if (attack == AiBotAttackKick2)
    {
        bot_attack_timer = BOT_KICK2_ATTACK_TIME;
        bot.kick = false;
        bot.kick2 = true;
        if (bot_character == BotCharacterKnuckles && bot.charged_kick_enabled)
        {
            /* Only treat as charged if the bot actually charged it (charging step completed). */
            bot.charged_kick_active = bot.charged_kick_ready;
            bot.charged_kick_ready = false;
            bot.charged_kick_phase = bot.charged_kick_active ? 2 : 0;
            bot.charged_kick_phase_timer = 0;
        }
        game_audio_play_sfx_next_channel(game_audio_get_kick_sfx());
        if (bot_character != BotCharacterTails)
        {
            jo_set_sprite_anim_frame(bot.kick_anim_id, kick2_start_frame);
            jo_start_sprite_anim(bot.kick_anim_id);
        }
    }
}

static void bot_end_attack(void)
{
    bot_current_attack = AiBotAttackNone;
    bot_attack_timer = 0;
    bot_combo_requested = false;
    bot.punch = false;
    bot.punch2 = false;
    bot.kick = false;
    bot.kick2 = false;
}

static void bot_load_character_assets(bot_character_assets_t *assets, int character)
{
    character_animation_profile_t anim_profile;
    const jo_tile *move_tiles;
    const jo_tile *stand_tiles;
    const jo_tile *punch_tiles;
    const jo_tile *kick_tiles;
    int move_count;
    int stand_count;
    int punch_count;
    int kick_count;

    if (assets->loaded)
        return;

    ui_character_choice_t choice = bot_ui_character_from_bot_character(character);

    anim_profile = character_registry_get_animation_profile(choice);
    move_count = anim_profile.move_count;
    stand_count = anim_profile.stand_count;
    punch_count = anim_profile.punch_count;
    kick_count = anim_profile.kick_count;

    move_tiles = character_get_move_tiles(choice, &move_count);
    stand_tiles = character_get_stand_tiles(choice, &stand_count);
    punch_tiles = character_get_punch_tiles(choice, &punch_count);
    kick_tiles = character_get_kick_tiles(choice, &kick_count);

    const char *wlk = character_get_walk_tga(choice);
    const char *run1 = character_get_run1_tga(choice);
    const char *run2 = character_get_run2_tga(choice);
    const char *std = character_get_stand_tga(choice);
    const char *jmp = character_get_jump_tga(choice);
    const char *spn = character_get_spin_tga(choice);
    const char *dmg = character_get_damage_tga(choice);
    const char *dft = character_get_defeated_tga(choice);
    const char *pnc = character_get_punch_tga(choice);
    const char *kck = character_get_kick_tga(choice);

    /* Bot WRAM-sheet support (Sonic/Amy) is still handled by bot-specific code. */

    assets->walking_base_id = bot_try_add_tga_tileset(wlk, move_tiles, move_count);
    assets->running1_base_id = bot_try_add_tga_tileset(run1, move_tiles, move_count);

    if (character == BotCharacterTails || character == BotCharacterKnuckles)
        assets->running2_base_id = assets->running1_base_id;
    else
        assets->running2_base_id = bot_try_add_tga_tileset(run2, move_tiles, move_count);

    assets->stand_base_id = bot_try_add_tga_tileset(std, stand_tiles, stand_count);

    assets->jump_sprite_id = bot_try_add_tga(jmp);
    assets->spin_sprite_id = bot_try_add_tga(spn);
    assets->damage_sprite_id = bot_try_add_tga(dmg);
    {
        int defeated_count;
        const jo_tile *defeated_tiles = character_get_defeated_tiles(&defeated_count);
        assets->defeated_sprite_id = bot_try_add_tga_tileset(dft, defeated_tiles, defeated_count);
    }

    assets->punch_base_id = bot_try_add_tga_tileset(pnc, punch_tiles, punch_count);
    assets->kick_base_id = bot_try_add_tga_tileset(kck, kick_tiles, kick_count);

    assets->move_count = move_count;
    assets->stand_count = stand_count;
    assets->punch_count = punch_count;
    assets->kick_count = kick_count;
    assets->punch_combo2_start_frame = bot_punch_combo2_start_frame_for_count(punch_count);
    assets->kick_combo2_start_frame = bot_kick_combo2_start_frame_for_count(kick_count);

    if (character == BotCharacterTails)
    {
        int tail_count;
        const jo_tile *tail_tiles = character_get_tail_tiles(bot_ui_character_from_bot_character(character), &tail_count);
        const char *tail_tga = character_get_tail_tga(bot_ui_character_from_bot_character(character));
        assets->tail_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, tail_tga, JO_COLOR_Green, tail_tiles, tail_count);
    }
    else
    {
        assets->tail_base_id = -1;
    }

    // Ensure all required assets were loaded successfully.
    if (assets->walking_base_id < 0 ||
        assets->running1_base_id < 0 ||
        assets->running2_base_id < 0 ||
        assets->stand_base_id < 0 ||
        assets->punch_base_id < 0 ||
        assets->kick_base_id < 0 ||
        assets->jump_sprite_id < 0 ||
        assets->spin_sprite_id < 0 ||
        assets->damage_sprite_id < 0 ||
        assets->defeated_sprite_id < 0)
    {
        // Asset load incomplete (missing files); clear cache so we can retry later.
        bot_reset_cached_assets(assets);
        return;
    }

    assets->loaded = true;
}

static bool bot_bind_character_assets_to_instance(bot_character_assets_t *assets)
{
    bot_walking_base_id = assets->walking_base_id;
    bot_running1_base_id = assets->running1_base_id;
    bot_running2_base_id = assets->running2_base_id;
    bot_stand_base_id = assets->stand_base_id;
    bot_punch_base_id = assets->punch_base_id;
    bot_kick_base_id = assets->kick_base_id;
    bot_jump_sprite_id = assets->jump_sprite_id;
    bot_spin_asset_sprite_id = assets->spin_sprite_id;
    bot_damage_sprite_id = assets->damage_sprite_id;
    bot_defeated_sprite_id = assets->defeated_sprite_id;
    bot_tail_base_id = assets->tail_base_id;

    bot_walking_anim_id = jo_create_sprite_anim(bot_walking_base_id, assets->move_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_running1_anim_id = jo_create_sprite_anim(bot_running1_base_id, assets->move_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_running2_anim_id = jo_create_sprite_anim(bot_running2_base_id, assets->move_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_stand_anim_id = jo_create_sprite_anim(bot_stand_base_id, assets->stand_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_punch_anim_id = jo_create_sprite_anim(bot_punch_base_id, assets->punch_count, DEFAULT_SPRITE_FRAME_DURATION);
    bot_kick_anim_id = jo_create_sprite_anim(bot_kick_base_id, assets->kick_count, DEFAULT_SPRITE_FRAME_DURATION);

    if (!bot_validate_anim_set())
    {
        bot_rollback_anim_set();
        return false;
    }

    bot_punch_frame_count = assets->punch_count;
    bot_kick_frame_count = assets->kick_count;
    bot_stand_frame_count = assets->stand_count;
    bot_punch_combo2_start_frame = assets->punch_combo2_start_frame;
    bot_kick_combo2_start_frame = assets->kick_combo2_start_frame;

    assets->instance_users++;
    return true;
}

static void apply_damage_to_bot(int damage)
{
    if (bot_defeated)
        return;

    bot.life -= damage;
    if (bot.life <= 0)
    {
        bot.life = 0;
        bot_defeated = true;
        bot.walk = false;
        bot.run = 0;
        bot.punch = false;
        bot.kick = false;
        bot.punch2 = false;
        bot.kick2 = false;
    }
}

static void apply_hit_effect_to_bot(bool attacker_flip, float knockback_force, int stun_frames)
{
    int direction = attacker_flip ? -1 : 1;
    int recovery_frames = stun_frames + 4;

    bot_physics.speed = (float)direction * knockback_force;
    bot.stun_timer = stun_frames;
    bot.attack_cooldown = recovery_frames;
    bot_attack_cooldown = recovery_frames;
    bot_end_attack();
}

static void apply_spin_knockback_to_bot(bool attacker_flip, float knockback_force)
{
    int direction = attacker_flip ? -1 : 1;
    bot_physics.speed = (float)direction * knockback_force;
}

static void apply_hit_effect_to_character(character_t *player_ref,
                                          jo_sidescroller_physics_params *player_physics,
                                          bool attacker_flip,
                                          float knockback_force,
                                          int stun_frames)
{
    int direction = attacker_flip ? -1 : 1;
    jo_sidescroller_physics_params *target_physics = player_physics;

    if (target_physics == JO_NULL)
        target_physics = &physics;

    target_physics->speed = (float)direction * knockback_force;
    if (stun_frames <= 0)
        return;

    player_ref->stun_timer = stun_frames;
    player_ref->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
    player_ref->walk = false;
    player_ref->run = 0;
    player_ref->spin = false;
    player_ref->punch = false;
    player_ref->punch2 = false;
    player_ref->punch2_requested = false;
    player_ref->perform_punch2 = false;
    player_ref->kick = false;
    player_ref->kick2 = false;
    player_ref->kick2_requested = false;
    player_ref->perform_kick2 = false;
    player_ref->charged_kick_hold_ms = 0;
    player_ref->charged_kick_ready = false;
    player_ref->charged_kick_active = false;
    player_ref->charged_kick_phase = 0;
    player_ref->charged_kick_phase_timer = 0;
    player_ref->hit_done_punch1 = false;
    player_ref->hit_done_punch2 = false;
    player_ref->hit_done_kick1 = false;
    player_ref->hit_done_kick2 = false;
}

static void apply_hit_effect_to_player(character_t *player_ref, bool attacker_flip, float knockback_force, int stun_frames)
{
    apply_hit_effect_to_character(player_ref,
                                  target_player_physics_ctx,
                                  attacker_flip,
                                  knockback_force,
                                  stun_frames);
}

static void process_player_hits(character_t *player_ref,
                                jo_sidescroller_physics_params *player_physics,
                                int player_world_x,
                                int player_world_y,
                                bool *prev_punch,
                                bool *prev_punch2,
                                bool *prev_kick,
                                bool *prev_kick2)
{
    character_combat_profile_t combat_profile;
    (void)prev_punch;
    (void)prev_punch2;
    (void)prev_kick;
    (void)prev_kick2;
    int player_hit_range_punch1 = player_ref->hit_range_punch1;
    int player_hit_range_punch2 = player_ref->hit_range_punch2;
    int player_hit_range_kick1 = player_ref->hit_range_kick1;
    int player_hit_range_kick2 = player_ref->hit_range_kick2;
    if (player_ref->character_id == CHARACTER_ID_TAILS)
    {
        /* Tails has extra reach due to tail length */
        player_hit_range_kick1 += 10;
        player_hit_range_kick2 += 10;
    }
    float player_knockback_punch1 = debug_balance_get_knockback(player_ref->character_id, DebugBalanceAttackPunch1);
    float player_knockback_punch2 = debug_balance_get_knockback(player_ref->character_id, DebugBalanceAttackPunch2);
    float player_knockback_kick1 = debug_balance_get_knockback(player_ref->character_id, DebugBalanceAttackKick1);
    float player_knockback_kick2 = debug_balance_get_knockback(player_ref->character_id, DebugBalanceAttackKick2);

    /* Use the attacker's combat profile for non-adjusted values (e.g., charged-kick multiplier).
       Use the debug balance profile for damage/knockback values when the attacker is a player. */
    combat_profile = character_registry_get_combat_profile_by_character_id(player_ref->character_id);

    if (bot_defeated)
        return;

    if (player_ref->group != 0 && player_ref->group == bot.group)
        return;

    if (!player_ref->punch)
        player_ref->hit_done_punch1 = false;
    if (!player_ref->punch2)
        player_ref->hit_done_punch2 = false;
    if (!player_ref->kick)
        player_ref->hit_done_kick1 = false;
    if (!player_ref->kick2)
        player_ref->hit_done_kick2 = false;

    if (player_ref->punch && !player_ref->hit_done_punch1 && bot_player_attack_reached_hit_frame(player_ref, 0)
        && is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_punch1))
    {
        player_ref->hit_done_punch1 = true;
        if (bot.spin)
        {
            int spin_damage = debug_balance_get_damage(bot.character_id, DebugBalanceAttackSpin);
            float spin_knockback = debug_balance_get_knockback(bot.character_id, DebugBalanceAttackSpin);
            int spin_stun = debug_balance_get_stun(bot.character_id, DebugBalanceAttackSpin);

            if (player_ref == &player)
            {
                debug_track_player_damage_received(bot.character_id, spin_damage);
                debug_track_player_knockback_received(spin_knockback);
                debug_track_player_stun_received(bot.character_id, spin_stun);
            }

            if (spin_damage > 0)
                player_ref->life -= spin_damage;

            apply_spin_knockback_to_bot(player_ref->flip, spin_knockback * 2.0f);
            apply_hit_effect_to_character(player_ref, player_physics, !player_ref->flip, spin_knockback, spin_stun);
            damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            return;
        }

        {
            int damage = debug_balance_get_damage(player_ref->character_id, DebugBalanceAttackPunch1);
            int stun = debug_balance_get_stun(player_ref->character_id, DebugBalanceAttackPunch1);
            debug_battle_add_damage(player_ref->character_id, damage);
            player_ref->battle_damage_dealt += damage;
            apply_damage_to_bot(damage);
            if (player_ref == &player)
            {
                debug_track_player_damage_dealt(bot.character_id, damage);
                debug_track_player_stun_dealt(bot.character_id, stun);
            }
        }
        if (player_ref == &player)
            debug_track_player_knockback_dealt(player_knockback_punch1);
        apply_hit_effect_to_bot(player_ref->flip, player_knockback_punch1, debug_balance_get_stun(player_ref->character_id, DebugBalanceAttackPunch1));
        damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
        game_audio_play_sfx_next_channel(game_audio_get_damage_low_sfx());
    }

    if (player_ref->punch2 && !player_ref->hit_done_punch2 && bot_player_attack_reached_hit_frame(player_ref, 1)
        && is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_punch2))
    {
        player_ref->hit_done_punch2 = true;
        if (bot.spin)
        {
            int spin_damage = debug_balance_get_damage(bot.character_id, DebugBalanceAttackSpin);
            float spin_knockback = debug_balance_get_knockback(bot.character_id, DebugBalanceAttackSpin);
            int spin_stun = debug_balance_get_stun(bot.character_id, DebugBalanceAttackSpin);

            if (player_ref == &player)
            {
                debug_track_player_damage_received(bot.character_id, spin_damage);
                debug_track_player_knockback_received(spin_knockback);
                debug_track_player_stun_received(bot.character_id, spin_stun);
            }

            if (spin_damage > 0)
                player_ref->life -= spin_damage;

            apply_spin_knockback_to_bot(player_ref->flip, spin_knockback * 2.0f);
            apply_hit_effect_to_character(player_ref, player_physics, !player_ref->flip, spin_knockback, spin_stun);
            damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            return;
        }

        {
            int damage = debug_balance_get_damage(player_ref->character_id, DebugBalanceAttackPunch2);
            int stun = debug_balance_get_stun(player_ref->character_id, DebugBalanceAttackPunch2);
            debug_battle_add_damage(player_ref->character_id, damage);
            player_ref->battle_damage_dealt += damage;
            apply_damage_to_bot(damage);
            if (player_ref == &player)
            {
                debug_track_player_damage_dealt(bot.character_id, damage);
                debug_track_player_stun_dealt(bot.character_id, stun);
            }
        }
        if (player_ref == &player)
            debug_track_player_knockback_dealt(player_knockback_punch2);
        apply_hit_effect_to_bot(player_ref->flip, player_knockback_punch2, debug_balance_get_stun(player_ref->character_id, DebugBalanceAttackPunch2));
        damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
        game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
    }

    if (player_ref->kick && player_ref->character_id != CHARACTER_ID_KNUCKLES
        && !player_ref->hit_done_kick1 && bot_player_attack_reached_hit_frame(player_ref, 2)
        && is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_kick1))
    {
        player_ref->hit_done_kick1 = true;
        if (bot.spin)
        {
            if (player_ref == &player)
            {
                debug_track_player_stun_received(bot.character_id, COUNTER_STUN_FRAMES);
            }
            apply_spin_knockback_to_bot(player_ref->flip, player_knockback_kick1 * 2.0f);
            apply_hit_effect_to_character(player_ref, player_physics, !player_ref->flip, player_knockback_kick1, COUNTER_STUN_FRAMES);
            damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            return;
        }

        {
            int damage = debug_balance_get_damage(player_ref->character_id, DebugBalanceAttackKick1);
            int stun = debug_balance_get_stun(player_ref->character_id, DebugBalanceAttackKick1);
            debug_battle_add_damage(player_ref->character_id, damage);
            player_ref->battle_damage_dealt += damage;
            apply_damage_to_bot(damage);
            if (player_ref == &player)
            {
                debug_track_player_damage_dealt(bot.character_id, damage);
                debug_track_player_stun_dealt(bot.character_id, stun);
            }
        }
        if (player_ref == &player)
            debug_track_player_knockback_dealt(player_knockback_kick1);
        apply_hit_effect_to_bot(player_ref->flip, player_knockback_kick1, debug_balance_get_stun(player_ref->character_id, DebugBalanceAttackKick1));
        damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
        game_audio_play_sfx_next_channel(game_audio_get_damage_low_sfx());
    }

    if (player_ref->kick2
        && (player_ref->character_id != CHARACTER_ID_KNUCKLES
            || player_ref->charged_kick_active
            || player_ref->charged_kick_phase > 0
            || player_physics->is_in_air)
        && !player_ref->hit_done_kick2 && bot_player_attack_reached_hit_frame(player_ref, 3)
        && is_attack_in_range(player_world_x, player_world_y, player_ref->flip, (int)bot_world_x, (int)bot_world_y, player_hit_range_kick2))
    {
        player_ref->hit_done_kick2 = true;
        bool charged_kick = combat_profile.charged_kick_enabled
            && (player_ref->charged_kick_active || player_ref->charged_kick_phase > 0);
        float kick2_knockback = charged_kick
            ? (player_knockback_kick2 * debug_balance_get_knockback(player_ref->character_id, DebugBalanceAttackCharged))
            : player_knockback_kick2;
        int kick2_stun = debug_balance_get_stun(player_ref->character_id, charged_kick ? DebugBalanceAttackCharged : DebugBalanceAttackKick2);
        int kick2_damage = debug_balance_get_damage(player_ref->character_id, charged_kick ? DebugBalanceAttackCharged : DebugBalanceAttackKick2);

        if (bot.spin)
        {
            if (player_ref == &player)
            {
                debug_track_player_stun_received(bot.character_id, COUNTER_STUN_FRAMES);
            }
            apply_spin_knockback_to_bot(player_ref->flip, kick2_knockback * 2.0f);
            apply_hit_effect_to_character(player_ref, player_physics, !player_ref->flip, kick2_knockback, COUNTER_STUN_FRAMES);
            damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            return;
        }

        {
            int damage = kick2_damage;
            int stun = kick2_stun;
            debug_battle_add_damage(player_ref->character_id, damage);
            player_ref->battle_damage_dealt += damage;
            apply_damage_to_bot(damage);
            if (player_ref == &player)
            {
                debug_track_player_damage_dealt(bot.character_id, damage);
                debug_track_player_stun_dealt(bot.character_id, stun);
            }
        }
        if (player_ref == &player)
            debug_track_player_knockback_dealt(kick2_knockback);
        apply_hit_effect_to_bot(player_ref->flip, kick2_knockback, kick2_stun);
        damage_fx_trigger_world((int)bot_world_x, (int)bot_world_y);
        game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
        player_ref->charged_kick_active = false;
    }
}

static void remember_player_attack_states(const character_t *player_ref,
                                          bool *prev_punch,
                                          bool *prev_punch2,
                                          bool *prev_kick,
                                          bool *prev_kick2)
{
    (void)player_ref;
    *prev_punch = false;
    *prev_punch2 = false;
    *prev_kick = false;
    *prev_kick2 = false;
}

#undef bot
#undef bot_loaded
#undef bot_defeated
#undef bot_world_x
#undef bot_world_y
#undef bot_physics

static void bot_select_target(bot_instance_t *self_instance,
                              character_t *player_ref,
                              bool *player_defeated,
                              jo_sidescroller_physics_params *player_physics,
                              int player_world_x,
                              int player_world_y,
                              character_t *player2_ref,
                              bool *player2_defeated,
                              jo_sidescroller_physics_params *player2_physics,
                              int player2_world_x,
                              int player2_world_y,
                              bool versus_mode,
                              character_t **out_target_character,
                              bool **out_target_defeated,
                              jo_sidescroller_physics_params **out_target_physics,
                              int *out_target_world_x,
                              int *out_target_world_y)
{
    int self_group = self_instance->bot.group;
    int self_world_x = (int)self_instance->bot_world_x;
    int self_world_y = (int)self_instance->bot_world_y;
    int best_score = 0x7fffffff;
    character_t *best_target_character = JO_NULL;
    bool *best_target_defeated = JO_NULL;
    jo_sidescroller_physics_params *best_target_physics = JO_NULL;
    int best_target_world_x = self_world_x;
    int best_target_world_y = self_world_y;
    int index;

    /* If either party is in group 0 (no group), treat as enemies (free-for-all). */
    bool player_is_enemy = (player_ref != JO_NULL)
        && (self_group == 0 || player_ref->group == 0 || self_group != player_ref->group);
    if (player_is_enemy && player_ref->life > 0 && (player_defeated == JO_NULL || !(*player_defeated)))
    {
        best_target_character = player_ref;
        best_target_defeated = player_defeated;
        best_target_physics = player_physics;
        best_target_world_x = player_world_x;
        best_target_world_y = player_world_y;
        best_score = JO_ABS(self_world_x - player_world_x) + JO_ABS(self_world_y - player_world_y);
    }

    bool player2_is_enemy = (player2_ref != JO_NULL)
        && (self_group == 0 || player2_ref->group == 0 || self_group != player2_ref->group);
    if (player2_ref != JO_NULL && player2_is_enemy && player2_ref->life > 0 && (player2_defeated == JO_NULL || !(*player2_defeated)))
    {
        int score = JO_ABS(self_world_x - player2_world_x) + JO_ABS(self_world_y - player2_world_y);
        if (best_target_character == JO_NULL || score < best_score)
        {
            best_target_character = player2_ref;
            best_target_defeated = player2_defeated;
            best_target_physics = player2_physics;
            best_target_world_x = player2_world_x;
            best_target_world_y = player2_world_y;
            best_score = score;
        }
    }

    for (index = 0; index < default_bot_active_count; ++index)
    {
        bot_instance_t *candidate = &default_bot_instances[index];
        int candidate_world_x;
        int candidate_world_y;
        int score;

        if (candidate == self_instance)
            continue;
        if (!candidate->bot_loaded || candidate->bot_defeated)
            continue;

        int candidate_group = candidate->bot.group;
        bool candidate_is_enemy = (self_group == 0 || candidate_group == 0 || self_group != candidate_group);
        if (!candidate_is_enemy)
            continue;

        candidate_world_x = (int)candidate->bot_world_x;
        candidate_world_y = (int)candidate->bot_world_y;
        score = JO_ABS(self_world_x - candidate_world_x) + JO_ABS(self_world_y - candidate_world_y);

        if (best_target_character == JO_NULL || score < best_score)
        {
            best_target_character = &candidate->bot;
            best_target_defeated = &candidate->bot_defeated;
            best_target_physics = &candidate->bot_physics;
            best_target_world_x = candidate_world_x;
            best_target_world_y = candidate_world_y;
            best_score = score;
        }
    }

    if (best_target_character == JO_NULL)
    {
        if (player_is_enemy && player_ref != JO_NULL)
        {
            best_target_character = player_ref;
            best_target_defeated = player_defeated;
            best_target_physics = player_physics;
            best_target_world_x = player_world_x;
            best_target_world_y = player_world_y;
        }
        else if (player2_ref != JO_NULL && player2_is_enemy)
        {
            best_target_character = player2_ref;
            best_target_defeated = player2_defeated;
            best_target_physics = player2_physics;
            best_target_world_x = player2_world_x;
            best_target_world_y = player2_world_y;
        }
        /* If neither player is an enemy (same group or no group), leave target NULL so the bot can idle.
           This prevents allied bots from chasing the player even when they cannot hit. */
    }

    *out_target_character = best_target_character;
    *out_target_defeated = best_target_defeated;
    *out_target_physics = best_target_physics;
    *out_target_world_x = best_target_world_x;
    *out_target_world_y = best_target_world_y;
}

#define bot (ctx->bot)
#define bot_loaded (ctx->bot_loaded)
#define bot_defeated (ctx->bot_defeated)
#define bot_world_x (ctx->bot_world_x)
#define bot_world_y (ctx->bot_world_y)
#define bot_physics (ctx->bot_physics)

void bot_instance_init(bot_instance_t *instance,
                       int selected_player_character,
                       int selected_bot_character,
                       int selected_bot_group,
                       int player_world_x,
                       int player_world_y)
{
    //LOG
    if (runtime_log_is_enabled())
        jo_printf(0, 242, "bot_instance_init: entry selected_player=%d selected_bot=%d", selected_player_character, selected_bot_character);
    runtime_log("bot_instance_init: entry selected_player=%d selected_bot=%d", selected_player_character, selected_bot_character);
    
    bot_character_assets_t *assets;
    int desired_bot_character;
    bool use_shared_player_assets;

    bot_use_instance(instance);
    desired_bot_character = selected_bot_character;
    if (desired_bot_character < BotCharacterSonic || desired_bot_character >= BotCharacterCount)
        desired_bot_character = BotCharacterSonic;

    bool player_flow = (desired_bot_character == BotCharacterSonic || desired_bot_character == BotCharacterAmy);
    use_shared_player_assets = (selected_player_character == desired_bot_character);

    if (player_flow)
    {
        if (!bot_loaded || bot_character != desired_bot_character || !bot_uses_shared_player_sprites)
        {
            bot_instance_unload(instance);
            bot_reset_asset_ids();
            bot_character = desired_bot_character;
            bot_uses_shared_player_sprites = false;
            bot_use_player_flow = true;

            ui_character_choice_t ui_choice = bot_ui_character_from_bot_character(bot_character);
            character_handlers_t handlers = character_registry_get(ui_choice);

            if (bot_character == BotCharacterSonic)
                sonic_set_current(&bot, &bot_physics);
            else if (bot_character == BotCharacterAmy)
                amy_set_current(&bot, &bot_physics);

            handlers.load();

            if (bot_character == BotCharacterSonic)
                sonic_set_current(&player, NULL);
            else if (bot_character == BotCharacterAmy)
                amy_set_current(&player, NULL);

            bot_loaded = true;
        }
    }
    else if (use_shared_player_assets)
    {
        if (!bot_loaded || bot_character != desired_bot_character || !bot_uses_shared_player_sprites)
        {
            bot_instance_unload(instance);
            bot_reset_asset_ids();
            bot_character = desired_bot_character;
            if (!bot_bind_shared_player_assets(bot_character))
            {
                bot_loaded = false;
                bot_defeated = false;
                bot_uses_shared_player_sprites = false;
                return;
            }
        }
    }
    else if (!bot_loaded || bot_character != desired_bot_character || bot_uses_shared_player_sprites)
    {
        bot_instance_unload(instance);
        bot_reset_asset_ids();
        bot_character = desired_bot_character;
        assets = bot_get_assets_cache(bot_character);
        bot_load_character_assets(assets, bot_character);
        if (!bot_bind_character_assets_to_instance(assets))
        {
            bot_loaded = false;
            bot_defeated = false;
            bot_uses_shared_player_sprites = false;
            return;
        }

        // If we have an Amy WRAM sprite sheet, allocate an instance-level VRAM sprite
        // so each bot can animate independently.
if (bot_character == BotCharacterAmy && bot_amy_sheet_ready && instance->bot_wram_sheet_sprite_id < 0)
        instance->bot_wram_sheet_sprite_id = bot_create_blank_sprite();
    if (bot_character == BotCharacterSonic && bot_sonic_sheet_ready && instance->bot_wram_sheet_sprite_id < 0)
        instance->bot_wram_sheet_sprite_id = bot_create_blank_sprite();

        bot_uses_shared_player_sprites = false;
    }

    if (!bot_use_player_flow)
    {
        bot = (character_t){0};
        bot.wram_sprite_id = -1;
        bot.walking_anim_id = bot_walking_anim_id;
        bot.running1_anim_id = bot_running1_anim_id;
        bot.running2_anim_id = bot_running2_anim_id;
        bot.stand_sprite_id = bot_stand_anim_id;
        bot.jump_sprite_id = bot_jump_sprite_id;
        bot.spin_sprite_id = bot_spin_asset_sprite_id;
        bot.punch_anim_id = bot_punch_anim_id;
        bot.kick_anim_id = bot_kick_anim_id;
        bot.life = 50;
        bot.flip = true;
        bot.can_jump = true;
        bot.stun_timer = 0;
    }

    bot_jump_sfx = *game_audio_get_jump_sfx();
    bot_world_x = player_world_x;
    bot_world_y = player_world_y;
    bot.x = bot_screen_x(0);
    bot.y = bot_screen_y(0);
    character_registry_apply_combat_profile(&bot, bot_ui_character_from_bot_character(bot_character));
    bot.group = selected_bot_group;

    bot_defeated = false;
    world_physics_init_character(&bot_physics);
    bot_physics.speed = 0.0f;
    bot_physics.speed_y = 0.0f;
    bot_physics.is_in_air = false;
    bot_attack_cooldown = 0;
    bot_attack_step = 0;
    bot_jump_cooldown = 0;
    bot_jump_hold_ms = 0;
    bot_jump_hold_target_ms = 0;
    bot_jump_cut_applied = false;
    bot_airborne_time_ms = 0;
    bot_show_jump_sprite = false;
    bot_warmup_frames = BOT_AI_WARMUP_FRAMES;
    bot_end_attack();

    prev_player_punch = false;
    prev_player_punch2 = false;
    prev_player_kick = false;
    prev_player_kick2 = false;
    prev_player2_punch = false;
    prev_player2_punch2 = false;
    prev_player2_kick = false;
    prev_player2_kick2 = false;

    bot_loaded = true;

    //LOG
    if (runtime_log_is_enabled())
        jo_printf(0, 243, "bot_instance_init: exit bot_character=%d loaded=%d", bot_character, bot_loaded);
    runtime_log("bot_instance_init: exit bot_character=%d loaded=%d", bot_character, bot_loaded);
}

void bot_instance_unload(bot_instance_t *instance)
{
    bot_character_assets_t *assets;
    bot_use_instance(instance);

    if (!bot_loaded)
    {
        bot_reset_asset_ids();
        bot_uses_shared_player_sprites = false;
        bot_use_player_flow = false;
        return;
    }

    if (bot_use_player_flow)
    {
        bot_reset_asset_ids();
        bot_uses_shared_player_sprites = false;
        bot_use_player_flow = false;
        bot_loaded = false;
        bot_defeated = false;
        return;
    }

    if (bot_walking_anim_id >= 0) jo_remove_sprite_anim(bot_walking_anim_id);
    if (bot_running1_anim_id >= 0) jo_remove_sprite_anim(bot_running1_anim_id);
    if (bot_running2_anim_id >= 0) jo_remove_sprite_anim(bot_running2_anim_id);
    if (bot_stand_anim_id >= 0) jo_remove_sprite_anim(bot_stand_anim_id);
    if (bot_punch_anim_id >= 0) jo_remove_sprite_anim(bot_punch_anim_id);
    if (bot_kick_anim_id >= 0) jo_remove_sprite_anim(bot_kick_anim_id);

    if (!bot_uses_shared_player_sprites)
    {
        assets = bot_get_assets_cache(bot_character);
        if (assets->instance_users > 0)
            assets->instance_users--;
    }

    bot_loaded = false;
    bot_defeated = false;
    bot_uses_shared_player_sprites = false;
    bot_is_ally_flag = false;
    bot_reset_asset_ids();
}

void bot_instance_update(bot_instance_t *instance,
                         character_t *player_ref,
                         bool *player_defeated,
                         jo_sidescroller_physics_params *player_physics,
                         character_t *player2_ref,
                         bool *player2_defeated,
                         jo_sidescroller_physics_params *player2_physics,
                         bool versus_mode,
                         int map_pos_x,
                         int map_pos_y)
{
    int player_world_x;
    int player_world_y;
    int player2_world_x = 0;
    int player2_world_y = 0;
    bool local_player2_defeated = false;
    character_t *target_player_ref = player_ref;
    bool *target_player_defeated = player_defeated;
    jo_sidescroller_physics_params *target_player_physics = player_physics;
    ai_bot_context_t ai_ctx;

    bot_use_instance(instance);

    if (!bot_loaded)
        return;

    player_world_x = map_pos_x + player_ref->x;
    player_world_y = map_pos_y + player_ref->y;

    if (versus_mode && player2_ref != JO_NULL)
    {
        player2_world_x = map_pos_x + player2_ref->x;
        player2_world_y = map_pos_y + player2_ref->y;
        if (player2_defeated == JO_NULL)
        {
            local_player2_defeated = (player2_ref->life <= 0);
            player2_defeated = &local_player2_defeated;
        }
    }

    bot.x = bot_screen_x(map_pos_x);
    bot.y = bot_screen_y(map_pos_y);

    bool hit_player1 = true;
    bool hit_player2 = true;

    if (player_ref->group != 0 && player_ref->group == bot.group)
        hit_player1 = false;

    if (versus_mode && player2_ref != JO_NULL)
    {
        if (player2_ref->group != 0 && player2_ref->group == bot.group)
            hit_player2 = false;
    }

    if (hit_player1)
    {
        process_player_hits(player_ref,
                            player_physics,
                            player_world_x,
                            player_world_y,
                            &prev_player_punch,
                            &prev_player_punch2,
                            &prev_player_kick,
                            &prev_player_kick2);
    }

    if (versus_mode && player2_ref != JO_NULL && hit_player2)
    {
        process_player_hits(player2_ref,
                            player2_physics,
                            player2_world_x,
                            player2_world_y,
                            &prev_player2_punch,
                            &prev_player2_punch2,
                            &prev_player2_kick,
                            &prev_player2_kick2);
    }

    if (instance == &default_bot_instances[0])
        bot_debug_update_hitbox(player_world_x, player_world_y, player_ref);

    bot_select_target(instance,
                      player_ref,
                      player_defeated,
                      player_physics,
                      player_world_x,
                      player_world_y,
                      player2_ref,
                      player2_defeated,
                      player2_physics,
                      player2_world_x,
                      player2_world_y,
                      versus_mode,
                      &target_player_ref,
                      &target_player_defeated,
                      &target_player_physics,
                      &ai_ctx.player_world_x,
                      &ai_ctx.player_world_y);

    bool has_target = (target_player_ref != JO_NULL);

    if (!has_target)
    {
        /* No enemies remain (all allies). Stop movement and reset actions, but keep physics running. */
        bot_end_attack();
        bot_attack_cooldown = 0;
        bot.walk = false;
        bot.run = 0;
        bot.spin = false;
        bot.stun_timer = 0;
        bot_physics.speed = 0.0f;
    }
    else if (target_player_defeated != JO_NULL && *target_player_defeated)
    {
        bot_end_attack();
        bot_attack_cooldown = 0;
        bot.walk = false;
        bot.run = 0;
        bot.spin = false;
        bot.stun_timer = 0;
        bot_physics.speed = 0.0f;
    }
    else if (bot_defeated)
    {
        bot.walk = false;
        bot.run = 0;
        bot.punch = false;
        bot.punch2 = false;
        bot.kick = false;
        bot.kick2 = false;
        bot.stun_timer = 0;
        jo_physics_apply_friction(&bot_physics);
    }
    else
    {
        if (bot_warmup_frames > 0)
            --bot_warmup_frames;

        ai_ctx.player = target_player_ref;
        ai_ctx.player_defeated = target_player_defeated;
        ai_ctx.player_physics = target_player_physics;
        ai_ctx.bot_ref = &bot;
        ai_ctx.bot_defeated_flag = bot_defeated;
        ai_ctx.bot_in_air_flag = bot_physics.is_in_air;
        ai_ctx.bot_physics_ref = &bot_physics;
        ai_ctx.bot_world_x_ref = &bot_world_x;
        ai_ctx.bot_world_y_ref = &bot_world_y;
        ai_ctx.bot_speed_x_ref = &bot_physics.speed;
        ai_ctx.bot_speed_y_ref = &bot_physics.speed_y;
        ai_ctx.bot_attack_cooldown_ref = &bot_attack_cooldown;
        ai_ctx.bot_attack_step_ref = &bot_attack_step;
        ai_ctx.bot_jump_cooldown_ref = &bot_jump_cooldown;
        ai_ctx.bot_jump_hold_ms_ref = &bot_jump_hold_ms;
        ai_ctx.bot_jump_hold_target_ms_ref = &bot_jump_hold_target_ms;
        ai_ctx.bot_jump_cut_applied_ref = &bot_jump_cut_applied;
        ai_ctx.bot_current_attack_ref = &bot_current_attack;
        ai_ctx.bot_attack_timer_ref = &bot_attack_timer;
        ai_ctx.bot_attack_damage_done_ref = &bot_attack_damage_done;
        ai_ctx.bot_combo_requested_ref = &bot_combo_requested;
        ai_ctx.warmup_frames_left = bot_warmup_frames;
        ai_ctx.jump_sfx = &bot_jump_sfx;
        ai_ctx.jump_sfx_channel = 0;
        ai_ctx.spin_sfx = game_audio_get_spin_sfx();
        ai_ctx.punch_sfx = game_audio_get_punch_sfx();
        ai_ctx.kick_sfx = game_audio_get_kick_sfx();
        ai_ctx.is_close_enough = is_close_enough;
        ai_ctx.is_attack_in_range = is_attack_in_range;
        ai_ctx.start_attack = bot_start_attack;
        ai_ctx.apply_hit_effect_to_player = apply_hit_effect_to_player;

        target_player_physics_ctx = target_player_physics;

        ai_control_handle_bot_commands(&ai_ctx);
    }

    bot_apply_physics(map_pos_x, map_pos_y);

    /* Keep jump state synced with physics (same as player runtime path). */
    bot.jump = bot_show_jump_sprite;
    bot.falling = bot_show_jump_sprite && (bot_physics.speed_y > 0.0f);

    if (bot_use_player_flow)
    {
        player_update_animation_state(&bot, &bot_physics);
    }
    else
    {
        bot_update_animation();
        bot_update_tail_loop();
    }

    remember_player_attack_states(player_ref,
                                  &prev_player_punch,
                                  &prev_player_punch2,
                                  &prev_player_kick,
                                  &prev_player_kick2);

    if (versus_mode && player2_ref != JO_NULL)
    {
        remember_player_attack_states(player2_ref,
                                      &prev_player2_punch,
                                      &prev_player2_punch2,
                                      &prev_player2_kick,
                                      &prev_player2_kick2);
    }

    bot.x = bot_screen_x(map_pos_x);
    bot.y = bot_screen_y(map_pos_y);
}

void bot_instance_draw(bot_instance_t *instance, int map_pos_x, int map_pos_y)
{
    int life_percent;
    int bar_max_width;
    int bar_width;
    char bar[21];
    int i;
    int anim_sprite_id;

    bot_use_instance(instance);

    // Ensure WRAM sprite sheets are loaded and per-bot VRAM sprite is allocated.
    // This guards against scenarios where the sheet becomes available after the bot
    // was initially loaded (e.g., player sheet load happens later).
    if (!bot_loaded)
        return;

    bot.x = bot_screen_x(map_pos_x);
    bot.y = bot_screen_y(map_pos_y);

    if (bot.x < -64 || bot.x > 384 || bot.y < -64 || bot.y > 288)
        return;

    if (bot_use_player_flow)
    {
        player_draw(&bot, &bot_physics);
        return;
    }

    if (bot_character == BotCharacterAmy)
    {
        bot_init_amy_sheet();
        if (bot_amy_sheet_ready && ctx->bot_wram_sheet_sprite_id < 0)
            ctx->bot_wram_sheet_sprite_id = bot_create_blank_sprite();
    }
    else if (bot_character == BotCharacterSonic)
    {
        bot_init_sonic_sheet();
        if (bot_sonic_sheet_ready && ctx->bot_wram_sheet_sprite_id < 0)
            ctx->bot_wram_sheet_sprite_id = bot_create_blank_sprite();
    }

    if (bot.x < -64 || bot.x > 384 || bot.y < -64 || bot.y > 288)
        return;

    if (bot.flip)
        jo_sprite_enable_horizontal_flip();

    bot_draw_tail();

    if (bot_character == BotCharacterAmy && bot_amy_sheet_ready && ctx->bot_wram_sheet_sprite_id >= 0)
    {
        if ((bot_defeated || bot.life <= 0) && bot_defeated_sprite_id >= 0)
        {
            bot_reset_animation_lists_except(-1);
            jo_sprite_draw3D2(bot_defeated_sprite_id, bot.x, bot.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
        }
        else
        {
            bot_wram_render_current_frame(instance, &bot_amy_sheet, bot_amy_sheet_ready, 4, 5, 6);
            jo_sprite_draw3D2(ctx->bot_wram_sheet_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
        }

        if (bot.flip)
            jo_sprite_disable_horizontal_flip();
        return;
    }

    if (bot_character == BotCharacterSonic && bot_sonic_sheet_ready && ctx->bot_wram_sheet_sprite_id >= 0)
    {
        if ((bot_defeated || bot.life <= 0) && bot_defeated_sprite_id >= 0)
        {
            bot_reset_animation_lists_except(-1);
            jo_sprite_draw3D2(bot_defeated_sprite_id, bot.x, bot.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
        }
        else
        {
            bot_wram_render_current_frame(instance, &bot_sonic_sheet, bot_sonic_sheet_ready, 4, 5, 6);
            jo_sprite_draw3D2(ctx->bot_wram_sheet_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
        }

        if (bot.flip)
            jo_sprite_disable_horizontal_flip();
        return;
    }

    /* If bot is defeated, immediately draw defeated sprite (skip charged kick special-case). */
    if ((bot_defeated || bot.life <= 0) && bot_defeated_sprite_id >= 0)
    {
        bot_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(bot_defeated_sprite_id, bot.x, bot.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
        if (bot.flip)
            jo_sprite_disable_horizontal_flip();
        return;
    }

    /* If bot is Knuckles and actively holding Kick1 for charged kick, always show charged sprite part1. */
    if (bot_character == BotCharacterKnuckles && bot.charged_kick_enabled && bot_current_attack == AiBotAttackKick1 && bot_attack_timer > 0)
    {
        int base_id = (bot.kick_anim_id >= 0) ? (jo_get_anim_sprite(bot.kick_anim_id) - jo_get_sprite_anim_frame(bot.kick_anim_id)) : -1;
        if (base_id >= 0)
        {
            jo_sprite_draw3D2(base_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
            if (bot.flip)
                jo_sprite_disable_horizontal_flip();
            return;
        }
    }

    if (bot.spin && bot.spin_sprite_id >= 0)
    {
        bot_reset_animation_lists_except(-1);
        jo_sprite_draw3D_and_rotate2(bot.spin_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z, bot.angle);
        if (bot.flip)
            bot.angle -= CHARACTER_SPIN_SPEED;
        else
            bot.angle += CHARACTER_SPIN_SPEED;
    }
    else if ((bot_defeated || bot.life <= 0) && bot_defeated_sprite_id >= 0)
    {
        bot_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(bot_defeated_sprite_id, bot.x, bot.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    }
    else if (bot.stun_timer > 0 && bot_damage_sprite_id >= 0)
    {
        bot_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(bot_damage_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
    }
    else if (bot.punch || bot.punch2)
    {
        bot_reset_animation_lists_except(bot.punch_anim_id);
        anim_sprite_id = (bot.punch_anim_id >= 0) ? jo_get_anim_sprite(bot.punch_anim_id) : -1;
        if (anim_sprite_id >= 0)
            jo_sprite_draw3D2(anim_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
    }
    else if (bot.kick || bot.kick2)
    {
        if (bot_character == BotCharacterTails)
            bot_reset_animation_lists_except(-1);
        else
            bot_reset_animation_lists_except(bot.kick_anim_id);

        if (bot_character == BotCharacterTails)
        {
            int total_time = bot.kick2 ? BOT_KICK2_ATTACK_TIME : BOT_KICK1_ATTACK_TIME;
            int total_degrees = bot.kick2 ? 360 : 180;
            int progress = total_time - bot_attack_timer;
            int kick_angle;

            if (progress < 0)
                progress = 0;
            if (progress > total_time)
                progress = total_time;

            kick_angle = (total_degrees * progress) / total_time;
            if (bot.flip)
                kick_angle = -kick_angle;

            jo_sprite_draw3D_and_rotate2(bot_kick_base_id, bot.x, bot.y, CHARACTER_SPRITE_Z, kick_angle);
        }
        else if (bot_character == BotCharacterKnuckles)
        {
            int base_id = (bot.kick_anim_id >= 0) ? (jo_get_anim_sprite(bot.kick_anim_id) - jo_get_sprite_anim_frame(bot.kick_anim_id)) : -1;

            if (base_id >= 0)
            {
                if (bot.kick && !bot.kick2)
                {
                    /* Show charged sprite when the bot is actively in the Kick1 attack period
                       (use bot_current_attack for robustness against animation resets). */
                    if (bot_character == BotCharacterKnuckles && bot.charged_kick_enabled && bot_current_attack == AiBotAttackKick1 && bot_attack_timer > 0)
                        jo_sprite_draw3D2(base_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
                    else
                        jo_sprite_draw3D2(base_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
                }
                else if (bot.kick2)
                {
                    int front_offset = bot.flip ? -KNUCKLES_KICK_PART3_WIDTH_PIXELS : KNUCKLES_KICK_PART3_WIDTH_PIXELS;
                    if (bot.charged_kick_active && bot.charged_kick_phase <= 1)
                        jo_sprite_draw3D2(base_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
                    else
                    {
                        jo_sprite_draw3D2(base_id + KNUCKLES_KICK_PART4_INDEX, bot.x, bot.y, CHARACTER_SPRITE_Z);
                        jo_sprite_draw3D2(base_id + KNUCKLES_KICK_PART3_INDEX,
                                          bot.x + front_offset,
                                          bot.y,
                                          CHARACTER_SPRITE_Z);
                    }
                }
            }
        }
        else
        {
            anim_sprite_id = (bot.kick_anim_id >= 0) ? jo_get_anim_sprite(bot.kick_anim_id) : -1;
            if (anim_sprite_id >= 0)
            {
                jo_sprite_draw3D2(anim_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
            }
            else if (bot.kick2)
            {
                int base_id = (bot.kick_anim_id >= 0) ? (jo_get_anim_sprite(bot.kick_anim_id) - jo_get_sprite_anim_frame(bot.kick_anim_id)) : -1;
                int desired_frame = (bot_kick_combo2_start_frame >= 0 && bot_kick_combo2_start_frame < bot_kick_frame_count) ? bot_kick_combo2_start_frame : (bot_kick_frame_count > 0 ? (bot_kick_frame_count - 1) : 0);
                if (base_id >= 0)
                    jo_sprite_draw3D2(base_id + desired_frame, bot.x, bot.y, CHARACTER_SPRITE_Z);
            }
        }
    }
    else if (bot_physics.is_in_air && bot_show_jump_sprite)
    {
        bot_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(bot.jump_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
    }
    else
    {
        if (bot.walk && bot.run == 0)
        {
            bot_reset_animation_lists_except(bot.walking_anim_id);
            anim_sprite_id = (bot.walking_anim_id >= 0) ? jo_get_anim_sprite(bot.walking_anim_id) : -1;
        }
        else if (bot.walk && bot.run == 1)
        {
            bot_reset_animation_lists_except(bot.running1_anim_id);
            anim_sprite_id = (bot.running1_anim_id >= 0) ? jo_get_anim_sprite(bot.running1_anim_id) : -1;
        }
        else if (bot.walk && bot.run == 2)
        {
            bot_reset_animation_lists_except(bot.running2_anim_id);
            anim_sprite_id = (bot.running2_anim_id >= 0) ? jo_get_anim_sprite(bot.running2_anim_id) : -1;
        }
        else
        {
            bot_reset_animation_lists_except(bot.stand_sprite_id);
            anim_sprite_id = (bot.stand_sprite_id >= 0) ? jo_get_anim_sprite(bot.stand_sprite_id) : -1;
        }

        if (anim_sprite_id >= 0)
            jo_sprite_draw3D2(anim_sprite_id, bot.x, bot.y, CHARACTER_SPRITE_Z);
    }

    if (bot.flip)
        jo_sprite_disable_horizontal_flip();

    life_percent = (bot.life * 100) / 50;
    bar_max_width = 20;
    bar_width = (life_percent * bar_max_width) / 100;
    for (i = 0; i < bar_max_width; ++i)
        bar[i] = (i < bar_width) ? '#' : '-';
    bar[bar_max_width] = '\0';

    /*
    if (instance == &default_bot_instances[0] && !bot_is_ally_flag)
    {
        jo_printf(0,
                  27,
                  "%s : [%s] %d%%",
                  "Bot",
                  bar,
                  life_percent);
        if (bot_defeated)
            jo_printf(0, 28, "BOT KO");
    }
    */
}

bool bot_instance_is_defeated(bot_instance_t *instance)
{
    bot_use_instance(instance);
    return bot_defeated;
}

bot_instance_t *bot_default_instance(void)
{
    return &default_bot_instances[0];
}

void bot_set_active_count(int count)
{
    if (count < 0)
        count = 0;
    if (count > BOT_MAX_DEFAULT_COUNT)
        count = BOT_MAX_DEFAULT_COUNT;
    default_bot_active_count = count;
}

int bot_get_active_count(void)
{
    return default_bot_active_count;
}

void bot_init(int selected_player_character, int selected_bot_character, int player_world_x, int player_world_y)
{
    int selected_bot_characters[1];
    int selected_bot_groups[1];
    selected_bot_characters[0] = selected_bot_character;
    selected_bot_groups[0] = 0;
    bot_init_multi(selected_player_character, selected_bot_characters, selected_bot_groups, 1, 0, player_world_x, player_world_y);
}

void bot_init_multi(int selected_player_character, const int selected_bot_characters[], const int selected_bot_groups[], int bot_count, int selected_player_group, int player_world_x, int player_world_y)
{
    int index;
    int spacing = 64;

    if (bot_count < 0)
        bot_count = 0;
    if (bot_count > BOT_MAX_DEFAULT_COUNT)
        bot_count = BOT_MAX_DEFAULT_COUNT;

    default_bot_active_count = bot_count;

    for (index = BOT_MAX_DEFAULT_COUNT - 1; index >= default_bot_active_count; --index)
        bot_instance_unload(&default_bot_instances[index]);

    for (index = 0; index < default_bot_active_count; ++index)
    {
        int char_index = selected_bot_characters[index % bot_count];
        int group = selected_bot_groups[index % bot_count];

        /* Use map-defined bot start positions when available. */
        int bot_start_world_x = world_map_get_bot_start_x(index, group);
        int bot_start_world_y = world_map_get_bot_start_y(index, group);
        if (bot_start_world_x == WORLD_CAMERA_TARGET_X && bot_start_world_y == WORLD_CAMERA_TARGET_Y)
        {
            /* Fallback: if map doesn't define a bot marker, fall back to the player group start position (so bots spawn near the selected group). */
            int base_x = world_map_get_player_start_x(0, group);
            int base_y = world_map_get_player_start_y(0, group);
            bot_start_world_x = base_x + (spacing * index);
            bot_start_world_y = base_y;
        }

        bot_instance_init(&default_bot_instances[index],
                          selected_player_character,
                          char_index,
                          group,
                          bot_start_world_x,
                          bot_start_world_y);

        default_bot_instances[index].bot_is_ally = (group != 0 && group == selected_player_group);
    }

    //LOG
    if (runtime_log_is_enabled())
        jo_printf(0, 241, "bot_init_multi: done count=%d\n", default_bot_active_count);
}

void bot_reposition_from_map_start(void)
{
    int spacing = 64;
    for (int index = 0; index < default_bot_active_count; ++index)
    {
        bot_use_instance(&default_bot_instances[index]);
        int group = bot.group;

        int bot_start_world_x = world_map_get_bot_start_x(index, group);
        int bot_start_world_y = world_map_get_bot_start_y(index, group);
        if (bot_start_world_x == WORLD_CAMERA_TARGET_X && bot_start_world_y == WORLD_CAMERA_TARGET_Y)
        {
            /* Fallback: if map doesn't define a bot marker, fall back to the player group start position. */
            int base_x = world_map_get_player_start_x(0, group);
            int base_y = world_map_get_player_start_y(0, group);
            bot_start_world_x = base_x + (spacing * index);
            bot_start_world_y = base_y;
        }

        bot_world_x = (float)bot_start_world_x;
        bot_world_y = (float)bot_start_world_y;
        bot.x = bot_screen_x(0);
        bot.y = bot_screen_y(0);
    }
}

void bot_init_versus(int selected_player_character,
                     int selected_player2_character,
                     int clones_per_side,
                     int player_world_x,
                     int player_world_y)
{
    int index;
    int total_count;
    int ally_index;
    int enemy_index;

    if (clones_per_side < 0)
        clones_per_side = 0;
    if (clones_per_side > 2)
        clones_per_side = 2;

    total_count = clones_per_side * 2;
    if (total_count > BOT_MAX_DEFAULT_COUNT)
        total_count = BOT_MAX_DEFAULT_COUNT;

    default_bot_active_count = total_count;

    for (index = BOT_MAX_DEFAULT_COUNT - 1; index >= default_bot_active_count; --index)
        bot_instance_unload(&default_bot_instances[index]);

    ally_index = 0;
    enemy_index = 1;

    for (index = 0; index < clones_per_side; ++index)
    {
        if (ally_index < default_bot_active_count)
        {
            bot_instance_init(&default_bot_instances[ally_index],
                              selected_player_character,
                              selected_player_character,
                              1,
                              player_world_x - (96 * (index + 1)),
                              player_world_y);
            default_bot_instances[ally_index].bot_is_ally = true;
            ally_index += 2;
        }

        if (enemy_index < default_bot_active_count)
        {
            bot_instance_init(&default_bot_instances[enemy_index],
                              selected_player_character,
                              selected_player2_character,
                              2,
                              player_world_x + (96 * (index + 1)),
                              player_world_y);
            default_bot_instances[enemy_index].bot_is_ally = false;
            enemy_index += 2;
        }
    }
}

void bot_unload(void)
{
    int index;
    for (index = BOT_MAX_DEFAULT_COUNT - 1; index >= 0; --index)
        bot_instance_unload(&default_bot_instances[index]);
}

void bot_invalidate_asset_cache(void)
{
    int index;
    for (index = 0; index < BotCharacterCount; ++index)
        bot_reset_cached_assets(&bot_assets_cache[index]);
}

void bot_update(character_t *player_ref,
                bool *player_defeated,
                jo_sidescroller_physics_params *player_physics,
                character_t *player2_ref,
                bool *player2_defeated,
                jo_sidescroller_physics_params *player2_physics,
                bool versus_mode,
                int map_pos_x,
                int map_pos_y)
{
    int index;
    for (index = 0; index < default_bot_active_count; ++index)
        bot_instance_update(&default_bot_instances[index],
                            player_ref,
                            player_defeated,
                            player_physics,
                            player2_ref,
                            player2_defeated,
                            player2_physics,
                            versus_mode,
                            map_pos_x,
                            map_pos_y);
}

void bot_draw(int map_pos_x, int map_pos_y)
{
    int index;
    for (index = 0; index < default_bot_active_count; ++index)
        bot_instance_draw(&default_bot_instances[index], map_pos_x, map_pos_y);
}

bool bot_is_defeated(void)
{
    int index;
    bool has_enemy = false;

    if (default_bot_active_count <= 0)
        return false;

    for (index = 0; index < default_bot_active_count; ++index)
    {
        bot_instance_t *instance = &default_bot_instances[index];

        if (instance->bot_is_ally)
            continue;

        has_enemy = true;
        if (!bot_instance_is_defeated(&default_bot_instances[index]))
            return false;
    }

    return has_enemy;
}

void bot_debug_draw_hitbox(void)
{
    if (runtime_log_is_enabled())
    {
        jo_printf(26, 14, "HITBOX DBG");
        jo_printf(26, 15, "DX:%3d DY:%3d", dbg_center_dx, dbg_center_dy);
        jo_printf(26, 16, "P1:%3d/%3d %s", dbg_center_dx, dbg_hreach_p1, dbg_hit_p1 ? "HIT" : "---");
        jo_printf(26, 17, "P2:%3d/%3d %s", dbg_center_dx, dbg_hreach_punch2, dbg_hit_punch2 ? "HIT" : "---");
        jo_printf(26, 18, "K1:%3d/%3d %s", dbg_center_dx, dbg_hreach_k1, dbg_hit_k1 ? "HIT" : "---");
        jo_printf(26, 19, "K2:%3d/%3d %s", dbg_center_dx, dbg_hreach_k2, dbg_hit_k2 ? "HIT" : "---");
        jo_printf(26, 20, "V :%3d/%3d", dbg_center_dy, dbg_vreach);
    }
}

bool bot_debug_get_hitbox_snapshot(debug_hitbox_snapshot_t *snapshot)
{
    if (snapshot == JO_NULL)
        return false;

    snapshot->center_dx = dbg_center_dx;
    snapshot->center_dy = dbg_center_dy;
    snapshot->hreach_p1 = dbg_hreach_p1;
    snapshot->hreach_p2 = dbg_hreach_punch2;
    snapshot->hreach_k1 = dbg_hreach_k1;
    snapshot->hreach_k2 = dbg_hreach_k2;
    snapshot->vreach = dbg_vreach;
    snapshot->hit_p1 = dbg_hit_p1;
    snapshot->hit_p2 = dbg_hit_punch2;
    snapshot->hit_k1 = dbg_hit_k1;
    snapshot->hit_k2 = dbg_hit_k2;

    return true;
}

int bot_get_life(int index)
{
    if (index < 0 || index >= default_bot_active_count)
        return 0;

    /* bot_instance_t starts with a character_t, so we can read life directly. */
    character_t *bot_chr = (character_t *)&default_bot_instances[index];
    return bot_chr->life;
}

int bot_get_character_id(int index)
{
    if (index < 0 || index >= default_bot_active_count)
        return CHARACTER_ID_SONIC;

    character_t *bot_chr = (character_t *)&default_bot_instances[index];
    return bot_chr->character_id;
}

int bot_get_group(int index)
{
    if (index < 0 || index >= default_bot_active_count)
        return 0;

    character_t *bot_chr = (character_t *)&default_bot_instances[index];
    return bot_chr->group;
}
