#include <jo/jo.h>
#include "game_flow.h"
#include "game_constants.h"
#include "player.h"
#include "character_registry.h"
#include "bot.h"
#include "tails.h"
#include "world_physics.h"
#include "damage_fx.h"
#include "world_background.h"
#include "world_map.h"
#include "game_loop.h"
#include "runtime_log.h"
#include "rotating_sprite_pool.h"

static character_handlers_t active_handlers;
static ui_character_choice_t active_character = UiCharacterSonic;
static bool active_character_loaded = false;
static character_handlers_t player2_handlers;
static ui_character_choice_t player2_active_character = UiCharacterAmy;
static bool player2_character_loaded = false;
static bool player2_runtime_anims_created = false;
static int player2_tail_base_id = -1;
static int player2_kick_sprite_id = -1;
static bool loading_pending = false;
static bool loading_assets_ready = false;
static ui_character_choice_t loading_selected_character = UiCharacterSonic;
static ui_character_choice_t loading_selected_bot_character = UiCharacterAmy;
static int battle_sprite_start_id = -1;
static bool cdda_start_pending = false;
static int cdda_start_delay_frames = 0;
static int cdda_debug_state = 0;
static int cdda_attempts_left = 0;

#define CDDA_START_DELAY_FRAMES 20
#define CDDA_RETRY_DELAY_FRAMES 60
#define CDDA_MAX_ATTEMPTS 8

static int game_flow_anim_base_id_from_anim(int anim_id)
{
    int frame;
    int sprite_id;

    if (anim_id < 0)
        return -1;

    frame = jo_get_sprite_anim_frame(anim_id);
    sprite_id = jo_get_anim_sprite(anim_id);
    return sprite_id - frame;
}

static void game_flow_remove_player2_runtime_anims(void)
{
    if (!player2_runtime_anims_created)
        return;

    if (player2.walking_anim_id >= 0) jo_remove_sprite_anim(player2.walking_anim_id);
    if (player2.running1_anim_id >= 0) jo_remove_sprite_anim(player2.running1_anim_id);
    if (player2.running2_anim_id >= 0) jo_remove_sprite_anim(player2.running2_anim_id);
    if (player2.stand_sprite_id >= 0) jo_remove_sprite_anim(player2.stand_sprite_id);
    if (player2.punch_anim_id >= 0) jo_remove_sprite_anim(player2.punch_anim_id);
    if (player2.kick_anim_id >= 0) jo_remove_sprite_anim(player2.kick_anim_id);

    player2_runtime_anims_created = false;
}

static void unload_battle_assets(void)
{
    bot_unload();

    game_flow_remove_player2_runtime_anims();

    if (player2_character_loaded)
    {
        player2_handlers.unload();
        player2_character_loaded = false;
    }

    if (active_character_loaded)
    {
        active_handlers.unload();
        active_character_loaded = false;
    }

    if (battle_sprite_start_id >= 0)
    {
        jo_sprite_free_from(battle_sprite_start_id);
        rotating_sprite_pool_release_freed_sprites(battle_sprite_start_id);
        bot_invalidate_asset_cache();
    }

    player2_tail_base_id = -1;
    player2_kick_sprite_id = -1;
}

static void reset_fight(int *map_pos_x, int *map_pos_y)
{
    /* Player screen position (where the player is drawn on-screen) */
    player.x = 160 - CHARACTER_WIDTH / 2;
    player.y = 70;
    player.angle = 0;
    player.flip = false;
    player.spin = false;
    player.can_jump = true;
    player.life = 50;

     /* Center camera on configured world coordinate where the player should appear. */
     *map_pos_x = WORLD_CAMERA_TARGET_X - player.x;
     *map_pos_y = WORLD_CAMERA_TARGET_Y - player.y;
}

static void select_active_character(ui_character_choice_t selected_character)
{
    active_handlers = character_registry_get(selected_character);
}

static void setup_player2_character(ui_character_choice_t selected_character)
{
    character_animation_profile_t anim_profile;
    int move_count;
    int stand_count;
    int punch_count;
    int kick_count;
    int walking_base_id;
    int running1_base_id;
    int running2_base_id;
    int stand_base_id;
    int punch_base_id;
    int kick_base_id;
    character_t saved_player;

    if (player2_character_loaded && selected_character != player2_active_character)
    {
        player2_handlers.unload();
        player2_character_loaded = false;
    }

    saved_player = player;
    player2_handlers = character_registry_get(selected_character);
    player2_handlers.load();

    /* Ensure player2 combat stats (including charged kick) match the selected character. */
    character_registry_apply_combat_profile(&player2, selected_character);

    player2_character_loaded = true;
    player2_active_character = selected_character;

    anim_profile = character_registry_get_animation_profile(selected_character);
    move_count = anim_profile.move_count;
    stand_count = anim_profile.stand_count;
    punch_count = anim_profile.punch_count;
    kick_count = anim_profile.kick_count;

    walking_base_id = game_flow_anim_base_id_from_anim(player.walking_anim_id);
    running1_base_id = game_flow_anim_base_id_from_anim(player.running1_anim_id);
    running2_base_id = game_flow_anim_base_id_from_anim(player.running2_anim_id);
    stand_base_id = game_flow_anim_base_id_from_anim(player.stand_sprite_id);
    punch_base_id = game_flow_anim_base_id_from_anim(player.punch_anim_id);
    kick_base_id = game_flow_anim_base_id_from_anim(player.kick_anim_id);

    game_flow_remove_player2_runtime_anims();

    player2.walking_anim_id = sprite_safe_create_anim(walking_base_id, move_count, DEFAULT_SPRITE_FRAME_DURATION);
    player2.running1_anim_id = sprite_safe_create_anim(running1_base_id, move_count, DEFAULT_SPRITE_FRAME_DURATION);
    player2.running2_anim_id = sprite_safe_create_anim(running2_base_id, move_count, DEFAULT_SPRITE_FRAME_DURATION);
    player2.stand_sprite_id = sprite_safe_create_anim(stand_base_id, stand_count, DEFAULT_SPRITE_FRAME_DURATION);
    player2.punch_anim_id = sprite_safe_create_anim(punch_base_id, punch_count, DEFAULT_SPRITE_FRAME_DURATION);
    player2.kick_anim_id = sprite_safe_create_anim(kick_base_id, kick_count, DEFAULT_SPRITE_FRAME_DURATION);
    player2_runtime_anims_created = true;

    player2.spin_sprite_id = player.spin_sprite_id;
    player2.jump_sprite_id = player.jump_sprite_id;
    player2.damage_sprite_id = player.damage_sprite_id;
    player2.defeated_sprite_id = player.defeated_sprite_id;
    player2.hit_range_punch1 = player.hit_range_punch1;
    player2.hit_range_punch2 = player.hit_range_punch2;
    player2.hit_range_kick1 = player.hit_range_kick1;
    player2.hit_range_kick2 = player.hit_range_kick2;
    player2.attack_forward_impulse_light = player.attack_forward_impulse_light;
    player2.attack_forward_impulse_heavy = player.attack_forward_impulse_heavy;
    player2.knockback_punch1 = player.knockback_punch1;
    player2.knockback_punch2 = player.knockback_punch2;
    player2.knockback_kick1 = player.knockback_kick1;
    player2.knockback_kick2 = player.knockback_kick2;
    
    player2.charged_kick_ready = false;
    player2.charged_kick_active = false;
    player2.charged_kick_phase = 0;
    player2.charged_kick_phase_timer = 0;
    player2.character_id = selected_character;
    player2.hit_done_punch1 = false;
    player2.hit_done_punch2 = false;
    player2.hit_done_kick1 = false;
    player2.hit_done_kick2 = false;
    player2.walk = false;
    player2.run = 0;
    player2.flip = true;
    player2.spin = false;
    player2.jump = false;
    player2.falling = false;
    player2.punch = false;
    player2.punch2 = false;
    player2.punch2_requested = false;
    player2.perform_punch2 = false;
    player2.kick = false;
    player2.kick2 = false;
    player2.kick2_requested = false;
    player2.perform_kick2 = false;
    player2.air_kick_used = false;
    player2.attack_cooldown = 0;
    player2.stun_timer = 0;
    player2.can_jump = true;
    player2.angle = 0;
    player2.speed = 0;
    player2.life = 50;

    if (selected_character == UiCharacterTails)
    {
        player2_tail_base_id = tails_get_tail_base_id();
        player2_kick_sprite_id = tails_get_kick_sprite_id();
    }
    else
    {
        player2_tail_base_id = -1;
        player2_kick_sprite_id = -1;
    }

    player = saved_player;
}

static void ensure_active_character_loaded(ui_character_choice_t selected_character)
{
    bool changed = (selected_character != active_character);

    if (active_character_loaded && changed)
        active_handlers.unload();

    select_active_character(selected_character);

    active_handlers.load();
    active_character_loaded = true;

    active_character = selected_character;
}

void game_flow_start_from_menu(ui_character_choice_t selected_character,
                               ui_character_choice_t selected_bot_character,
                               void *user_data)
{
    game_flow_context_t *ctx = (game_flow_context_t *)user_data;

    cdda_start_pending = false;
    cdda_start_delay_frames = 0;
    cdda_attempts_left = 0;
    jo_audio_stop_cd();
    ui_control_reset_menu_bgm_state();

    loading_selected_character = selected_character;
    loading_selected_bot_character = selected_bot_character;
    loading_pending = true;
    loading_assets_ready = false;
    ui_control_reset_menu_sprites();
    world_map_load();
    ctx->ui_state->game_paused = false;
    ctx->ui_state->current_game_state = UiGameStateLoading;
}

void game_flow_process_loading(void *user_data)
{
    game_flow_context_t *ctx = (game_flow_context_t *)user_data;
    int bot_count;

    //jo_printf(0, 20, "game_flow: process_loading start");
    runtime_log("game_flow: process_loading start");

    if (!loading_pending)
        return;

    if (!loading_assets_ready)
    {
        //jo_printf(0, 200, "game_flow: beginning asset preparation");
        runtime_log("game_flow: beginning asset preparation");
        if (battle_sprite_start_id < 0)
            battle_sprite_start_id = jo_get_last_sprite_id() + 1;

            
        runtime_log("game_flow: calling unload_battle_assets");
        //jo_printf(0, 201, "game_flow: calling unload_battle_assets");
        unload_battle_assets();
        //jo_printf(0, 202, "game_flow: unload_battle_assets returned");
        runtime_log("game_flow: unload_battle_assets returned");

        player = (character_t){0};
        player2 = (character_t){0};
        //jo_printf(0, 203, "game_flow: ensure_active_character_loaded(%d)", (int)loading_selected_character);
        runtime_log("game_flow: ensure_active_character_loaded(%d)", (int)loading_selected_character);
        ensure_active_character_loaded(loading_selected_character);        
        runtime_log("game_flow: ensure_active_character_loaded returned");
        //jo_printf(0, 204, "game_flow: ensure_active_character_loaded returned");

        if (ctx->ui_state->menu_multiplayer_versus)
        {
            //jo_printf(0, 205, "game_flow: calling setup_player2_character(%d)", (int)ctx->ui_state->menu_selected_player2_character);
            runtime_log("game_flow: calling setup_player2_character(%d)", (int)ctx->ui_state->menu_selected_player2_character);
            setup_player2_character(ctx->ui_state->menu_selected_player2_character);
            //jo_printf(0, 206, "game_flow: setup_player2_character returned");
            runtime_log("game_flow: setup_player2_character returned");
        }
        else
            player2 = (character_t){0};

        runtime_log("game_flow: world_physics_init_character");
        world_physics_init_character(ctx->physics);
        runtime_log("game_flow: reset_fight");
        reset_fight(ctx->map_pos_x, ctx->map_pos_y);
        runtime_log("game_flow: damage_fx_reset");
        damage_fx_reset();
        *ctx->player_defeated = false;

        if (ctx->ui_state->menu_multiplayer_versus)
        {
            bot_count = ctx->ui_state->menu_bot_count;
            if (bot_count > 0)
            {
                //jo_printf(0, 210, "game_flow: calling bot_init_versus selected_char=%d player2_char=%d bot_count=%d", (int)loading_selected_character, (int)ctx->ui_state->menu_selected_player2_character, bot_count);
                runtime_log("game_flow: calling bot_init_versus selected_char=%d player2_char=%d bot_count=%d", (int)loading_selected_character, (int)ctx->ui_state->menu_selected_player2_character, bot_count);
                bot_init_versus((int)loading_selected_character,
                                (int)ctx->ui_state->menu_selected_player2_character,
                                bot_count,
                                *ctx->map_pos_x + player.x,
                                *ctx->map_pos_y + player.y);
                runtime_log("game_flow: bot_init_versus returned");
            }
            else
            {
                bot_set_active_count(0);
                bot_unload();
            }
        }
        else
        {
            int cpu_characters[UiCharacterCount];
            int cpu_groups[UiCharacterCount];
            int cpu_count = 0;
            int player_character = loading_selected_character;

            for (int i = 0; i < UiCharacterCount; ++i)
            {
                if (ctx->ui_state->menu_character_controller[i] == UiControllerCpu)
                {
                    cpu_characters[cpu_count] = i;
                    cpu_groups[cpu_count] = ctx->ui_state->menu_character_group[i];
                    cpu_count++;
                }
                else if (ctx->ui_state->menu_character_controller[i] == UiControllerPlayer)
                {
                    player_character = (ui_character_choice_t)i;
                }
            }

            player.group = ctx->ui_state->menu_character_group[player_character];

            if (cpu_count > 0)
            {
                //jo_printf(0, 212, "game_flow: calling bot_init_multi player=%d cpu_count=%d", player_character, cpu_count);
                runtime_log("game_flow: calling bot_init_multi player=%d cpu_count=%d", player_character, cpu_count);
                bot_init_multi(player_character,
                               cpu_characters,
                               cpu_groups,
                               cpu_count,
                               player.group,
                               *ctx->map_pos_x + player.x,
                               *ctx->map_pos_y + player.y);
            }
            else
            {
                bot_unload();
            }

            loading_selected_character = player_character;
        }

        loading_assets_ready = true;
        //jo_printf(0, 21, "game_flow: assets ready");
        runtime_log("game_flow: assets ready");
    }

    if (!world_map_is_ready())
        return;

    loading_pending = false;
    loading_assets_ready = false;
    world_background_load();
    runtime_log("game_flow: world_background_load called");
    ctx->ui_state->game_paused = false;
    ctx->ui_state->current_game_state = UiGameStatePlaying;
    runtime_log("game_flow: switching to playing");
}

void game_flow_reset_fight(void *user_data)
{
    game_flow_context_t *ctx = (game_flow_context_t *)user_data;
    int bot_count;

    player = (character_t){0};
    player2 = (character_t){0};
    ensure_active_character_loaded(ctx->ui_state->menu_selected_character);
    if (ctx->ui_state->menu_multiplayer_versus)
        setup_player2_character(ctx->ui_state->menu_selected_player2_character);
    reset_fight(ctx->map_pos_x, ctx->map_pos_y);
    if (ctx->ui_state->menu_multiplayer_versus)
    {
        player2.x = player.x + 96;
        player2.y = player.y;
        player2.flip = true;
        player2.can_jump = true;
        player2.life = 50;
        player2.walk = false;
        player2.run = 0;
        player2.spin = false;
        player2.jump = false;
        player2.punch = false;
        player2.punch2 = false;
        player2.kick = false;
        player2.kick2 = false;
        game_loop_reset_player2_runtime();
    }
    damage_fx_reset();
    *ctx->player_defeated = false;

    if (ctx->ui_state->menu_multiplayer_versus)
    {
        bot_count = ctx->ui_state->menu_bot_count;
        if (bot_count > 0)
        {
            bot_init_versus((int)ctx->ui_state->menu_selected_character,
                            (int)ctx->ui_state->menu_selected_player2_character,
                            bot_count,
                            *ctx->map_pos_x + player.x,
                            *ctx->map_pos_y + player.y);
        }
        else
        {
            bot_set_active_count(0);
            bot_unload();
        }
    }
    else
    {
        int cpu_characters[UiCharacterCount];
        int cpu_groups[UiCharacterCount];
        int cpu_count = 0;
        int player_character = ctx->ui_state->menu_selected_character;

        for (int i = 0; i < UiCharacterCount; ++i)
        {
            if (ctx->ui_state->menu_character_controller[i] == UiControllerCpu)
            {
                cpu_characters[cpu_count] = i;
                cpu_groups[cpu_count] = ctx->ui_state->menu_character_group[i];
                cpu_count++;
            }
            else if (ctx->ui_state->menu_character_controller[i] == UiControllerPlayer)
            {
                player_character = (ui_character_choice_t)i;
            }
        }

        player.group = ctx->ui_state->menu_character_group[player_character];

        if (cpu_count > 0)
            bot_init_multi(player_character,
                           cpu_characters,
                           cpu_groups,
                           cpu_count,
                           player.group,
                           *ctx->map_pos_x + player.x,
                           *ctx->map_pos_y + player.y);
        else
            bot_unload();

        /* Ensure the selected character for player matches the actual player selection. */
        ctx->ui_state->menu_selected_character = player_character;
    }
}

void game_flow_return_to_character_select(void *user_data)
{
    (void)user_data;
    damage_fx_reset();
    cdda_start_pending = false;
    cdda_start_delay_frames = 0;
    cdda_attempts_left = 0;
    jo_audio_stop_cd();
    ui_control_reset_menu_bgm_state();
    cdda_debug_state = 5;
}

void game_flow_runtime_tick(void)
{
    if (!cdda_start_pending)
        return;

    if (cdda_start_delay_frames > 0)
    {
        cdda_debug_state = 2;
        --cdda_start_delay_frames;
        return;
    }

    jo_audio_stop_cd();
    jo_audio_play_cd_track(2, 99, true);
    cdda_debug_state = 4;

    --cdda_attempts_left;
    if (cdda_attempts_left > 0)
    {
        cdda_debug_state = 3;
        cdda_start_delay_frames = CDDA_RETRY_DELAY_FRAMES;
        return;
    }

    cdda_start_pending = false;
    cdda_debug_state = 4;
}

int game_flow_cdda_debug_state(void)
{
    return cdda_debug_state;
}

int game_flow_cdda_debug_delay_frames(void)
{
    return cdda_start_delay_frames;
}

int game_flow_cdda_debug_attempts_left(void)
{
    return cdda_attempts_left;
}

void game_flow_debug_force_cdda_play(void)
{
    cdda_start_pending = false;
    cdda_start_delay_frames = 0;
    cdda_attempts_left = 0;
    jo_audio_stop_cd();
    jo_audio_play_cd_track(2, 99, true);
    cdda_debug_state = 6;
}

void game_flow_debug_force_cdda_stop(void)
{
    cdda_start_pending = false;
    cdda_start_delay_frames = 0;
    cdda_attempts_left = 0;
    jo_audio_stop_cd();
    cdda_debug_state = 7;
}

void game_flow_update_animation(void)
{
    active_handlers.update_animation();
}

void game_flow_display_character(void)
{
    active_handlers.display();
}

int game_flow_get_player2_tail_base_id(void)
{
    return player2_tail_base_id;
}

int game_flow_get_player2_kick_sprite_id(void)
{
    return player2_kick_sprite_id;
}