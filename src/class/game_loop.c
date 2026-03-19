#include <jo/jo.h>
#include <stdlib.h>
#include <time.h>
#include "player.h"

/* Ensure srand is declared even if some platform headers are missing it. */
extern void srand(unsigned int);
#include "game_constants.h"
#include "bot.h"
#include "control.h"
#include "ui_control.h"
#include "game_flow.h"
#include "world_collision.h"
#include "world_camera.h"
#include "world_background.h"
#include "world_physics.h"
#include "game_audio.h"
#include "input_mapping.h"
#include "debug.h"
#include "game_loop.h"
#include "damage_fx.h"
#include "character_registry.h"
#include "vram_cache.h"
#include "runtime_log.h"
#include "world_map.h"
#include "jo_ext/jo_map_ext.h"

static game_loop_context_t *g_ctx;

static int g_dbg_pvp_center_dx = 0;
static int g_dbg_pvp_center_dy = 0;
static int g_dbg_pvp_hreach_p1 = 0;
static int g_dbg_pvp_hreach_punch2 = 0;
static int g_dbg_pvp_hreach_k1 = 0;
static int g_dbg_pvp_hreach_k2 = 0;
static int g_dbg_pvp_vreach = 0;
static bool g_dbg_pvp_hit_p1 = false;
static bool g_dbg_pvp_hit_punch2 = false;
static bool g_dbg_pvp_hit_k1 = false;
static bool g_dbg_pvp_hit_k2 = false;
static bool g_dbg_pvp_available = false;
static int g_runtime_playing_update_logs = 0;
static int g_runtime_playing_draw_logs = 0;
static bool g_ui_text_dirty = true;

/* Camera world position (what gets rendered). */
static float g_camera_world_x = 0.0f;
static float g_camera_world_y = 0.0f;
static bool g_camera_world_initialized = false;

/* Player world position used for physics/collision (may diverge from camera during free cam). */
static int g_player_map_pos_x = 0;
static int g_player_map_pos_y = 0;

/* Screen location where the player should appear when the camera is following normally. */
static const int CAMERA_TARGET_SCREEN_X = 160; /* center of 320px width */
static const int CAMERA_TARGET_SCREEN_Y = 75;

/* When true, camera follows Player 2 instead of Player 1. */
static bool g_camera_follow_player2 = false;

static const float CAMERA_FREE_SPEED = 4.0f;
static const float CAMERA_RETURN_SPEED = 0.12f; /* slower for smoother return */
static bool g_prev_free_cam = false;

/* Free cam cooldown (frames) after taking damage. */
static int g_free_cam_lock_frames = 0;
static const int FREE_CAM_LOCK_FRAMES = 60; /* ~1 second at 60fps */

/* Which player is currently controlling free cam (0 = P1, 1 = P2, -1 = none) */
static int g_free_cam_player = -1;


static bool game_loop_is_attack_in_range(int attacker_x,
                                         int attacker_y,
                                         bool attacker_flip,
                                         int target_x,
                                         int target_y,
                                         int range);

static int game_loop_last_frame_for_attack(const character_t *attacker, int attack_kind)
{
    int cid = attacker->character_id;

    if (attack_kind == 0)
        /* punch1: hit occurs on second frame (index 1) */
        return 1;
    if (attack_kind == 1)
        /* punch2: hit on final frame (index 3) */
        return 3;
    if (attack_kind == 2)
    {
        if (cid == CHARACTER_ID_TAILS)
            return 0; /* Tails has only 1 kick frame, so hit should occur immediately */
        if (cid == CHARACTER_ID_KNUCKLES)
            return -1;
        return 1;
    }

    if (attack_kind == 3)
    {
        if (cid == CHARACTER_ID_TAILS)
            return 0; /* Tails combo kick uses the same single-frame animation */
        if (cid == CHARACTER_ID_KNUCKLES && (attacker->charged_kick_active || attacker->charged_kick_phase > 0))
            return 2;
        return 2;
    }

    return 2;
}

static bool game_loop_attack_reached_hit_frame(const character_t *attacker, int attack_kind)
{
    int anim_id;
    int last_frame;

    if (attack_kind == 0 || attack_kind == 1)
        anim_id = attacker->punch_anim_id;
    else
        anim_id = attacker->kick_anim_id;

    if (anim_id < 0)
        return false;

    last_frame = game_loop_last_frame_for_attack(attacker, attack_kind);
    if (last_frame < 0)
        return false;

    return jo_get_sprite_anim_frame(anim_id) >= last_frame;
}

static void game_loop_update_pvp_hitbox_debug(int p1_world_x,
                                              int p1_world_y,
                                              int p2_world_x,
                                              int p2_world_y)
{
    int center_x_p1 = p1_world_x + (CHARACTER_WIDTH / 2);
    int center_x_p2 = p2_world_x + (CHARACTER_WIDTH / 2);
    int center_y_p1 = p1_world_y + (CHARACTER_HEIGHT / 2);
    int center_y_p2 = p2_world_y + (CHARACTER_HEIGHT / 2);

    g_dbg_pvp_center_dx = JO_ABS(center_x_p1 - center_x_p2);
    g_dbg_pvp_center_dy = JO_ABS(center_y_p1 - center_y_p2);
    g_dbg_pvp_hreach_p1 = (CHARACTER_WIDTH / 2) + player.hit_range_punch1;
    g_dbg_pvp_hreach_punch2 = (CHARACTER_WIDTH / 2) + player.hit_range_punch2;
    g_dbg_pvp_hreach_k1 = (CHARACTER_WIDTH / 2) + player.hit_range_kick1;
    g_dbg_pvp_hreach_k2 = (CHARACTER_WIDTH / 2) + player.hit_range_kick2;
    g_dbg_pvp_vreach = (CHARACTER_HEIGHT / 2) + 12;

    g_dbg_pvp_hit_p1 = game_loop_is_attack_in_range(p1_world_x,
                                                     p1_world_y,
                                                     player.flip,
                                                     p2_world_x,
                                                     p2_world_y,
                                                     player.hit_range_punch1);
    g_dbg_pvp_hit_punch2 = game_loop_is_attack_in_range(p1_world_x,
                                                         p1_world_y,
                                                         player.flip,
                                                         p2_world_x,
                                                         p2_world_y,
                                                         player.hit_range_punch2);
    g_dbg_pvp_hit_k1 = game_loop_is_attack_in_range(p1_world_x,
                                                     p1_world_y,
                                                     player.flip,
                                                     p2_world_x,
                                                     p2_world_y,
                                                     player.hit_range_kick1);
    g_dbg_pvp_hit_k2 = game_loop_is_attack_in_range(p1_world_x,
                                                     p1_world_y,
                                                     player.flip,
                                                     p2_world_x,
                                                     p2_world_y,
                                                     player.hit_range_kick2);
    g_dbg_pvp_available = true;
}

static bool game_loop_is_attack_in_range(int attacker_x,
                                         int attacker_y,
                                         bool attacker_flip,
                                         int target_x,
                                         int target_y,
                                         int range)
{
    int forward_range = range;
    int attacker_left = attacker_x;
    int attacker_right = attacker_x + CHARACTER_WIDTH;
    int attacker_top = attacker_y;
    int attacker_bottom = attacker_y + CHARACTER_HEIGHT;
    int attack_left;
    int attack_right;
    int target_left = target_x;
    int target_right = target_x + CHARACTER_WIDTH;
    int target_top = target_y;
    int target_bottom = target_y + CHARACTER_HEIGHT;

    if (forward_range > 1)
        forward_range = (forward_range + 1) / 2;

    if (attacker_flip)
    {
        attack_left = attacker_left - forward_range;
        attack_right = attacker_left;
    }
    else
    {
        attack_left = attacker_right;
        attack_right = attacker_right + forward_range;
    }

    return target_right >= attack_left
        && target_left <= attack_right
        && target_bottom >= attacker_top
        && target_top <= attacker_bottom;
}

static void game_loop_center_camera_on_current_target(void)
{
    float player_world_x;
    float player_world_y;

    if (g_camera_follow_player2 && player2_is_active())
    {
        player_world_x = (float)(*g_ctx->map_pos_x + player2.x + (CHARACTER_WIDTH / 2));
        player_world_y = (float)(*g_ctx->map_pos_y + player2.y);
    }
    else
    {
        player_world_x = (float)(*g_ctx->map_pos_x + player.x + (CHARACTER_WIDTH / 2));
        player_world_y = (float)(*g_ctx->map_pos_y + player.y);
    }

    g_camera_world_x = player_world_x - CAMERA_TARGET_SCREEN_X;
    g_camera_world_y = player_world_y - CAMERA_TARGET_SCREEN_Y;
}

static void game_loop_exit_free_cam_and_lock(void)
{
    g_prev_free_cam = false;
    g_free_cam_player = -1;
    g_free_cam_lock_frames = FREE_CAM_LOCK_FRAMES;
    game_loop_center_camera_on_current_target();
}

static void game_loop_apply_hit_effect(character_t *target,
                                       jo_sidescroller_physics_params *target_physics,
                                       bool attacker_flip,
                                       float knockback_force,
                                       int stun_frames)
{
    int direction = attacker_flip ? -1 : 1;

    /* If the currently free-cam controlling player got hit, exit free cam. */
    if (g_free_cam_player >= 0)
    {
        if ((g_free_cam_player == 0 && target == &player) ||
            (g_free_cam_player == 1 && target == &player2))
        {
            game_loop_exit_free_cam_and_lock();
        }
    }

    
    //jo_printf(1, 20, "KB dir=%d f=%.2f flip=%d", direction, knockback_force, attacker_flip ? 1 : 0);
    //LOG
    g_dbg_knock_dir = direction;
    g_dbg_knock_force = knockback_force;        

    if (target_physics != JO_NULL)
        target_physics->speed = (float)direction * knockback_force;

    // if (direction == -1)
    //     target_physics->speed *= 0.65;          
    
    g_dbg_knock_speed = target_physics->speed;  

    if (stun_frames <= 0)
        return;

    target->stun_timer = stun_frames;
    target->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
    target->walk = false;
    target->run = 0;
    target->spin = false;
    target->punch = false;
    target->punch2 = false;
    target->punch2_requested = false;
    target->perform_punch2 = false;
    target->kick = false;
    target->kick2 = false;
    target->kick2_requested = false;
    target->perform_kick2 = false;
    target->charged_kick_hold_ms = 0;
    target->charged_kick_ready = false;
    target->charged_kick_active = false;
    target->charged_kick_phase = 0;
    target->charged_kick_phase_timer = 0;
    target->hit_done_punch1 = false;
    target->hit_done_punch2 = false;
    target->hit_done_kick1 = false;
    target->hit_done_kick2 = false;
}

static void game_loop_process_player_attack(character_t *attacker,
                                            jo_sidescroller_physics_params *attacker_physics,
                                            character_t *target,
                                            jo_sidescroller_physics_params *target_physics,
                                            bool *target_defeated,
                                            int attacker_world_x,
                                            int attacker_world_y,
                                            int target_world_x,
                                            int target_world_y,
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

    if (target_defeated != JO_NULL && *target_defeated)
        return;
    if (attacker->life <= 0 || target->life <= 0)
        return;
    if (attacker->group != 0 && attacker->group == target->group)
        return;

    if (!attacker->punch)
        attacker->hit_done_punch1 = false;
    if (!attacker->punch2)
        attacker->hit_done_punch2 = false;
    if (!attacker->kick)
        attacker->hit_done_kick1 = false;
    if (!attacker->kick2)
        attacker->hit_done_kick2 = false;

    combat_profile = character_registry_get_combat_profile_by_character_id(attacker->character_id);

    /* Use debug balance values for players to keep in-sync with the in-game tuning menu. */
    float attacker_knockback_punch1 = debug_balance_get_knockback(attacker->character_id, DebugBalanceAttackPunch1);
    float attacker_knockback_punch2 = debug_balance_get_knockback(attacker->character_id, DebugBalanceAttackPunch2);
    float attacker_knockback_kick1 = debug_balance_get_knockback(attacker->character_id, DebugBalanceAttackKick1);
    float attacker_knockback_kick2 = debug_balance_get_knockback(attacker->character_id, DebugBalanceAttackKick2);
    (void)attacker_knockback_kick2;

    if (attacker->punch
        && !attacker->hit_done_punch1
        && game_loop_attack_reached_hit_frame(attacker, 0)
        && game_loop_is_attack_in_range(attacker_world_x,
                                        attacker_world_y,
                                        attacker->flip,
                                        target_world_x,
                                        target_world_y,
                                        attacker->hit_range_punch1))
    {
        attacker->hit_done_punch1 = true;
        if (target->spin)
        {
            int spin_damage = debug_balance_get_damage(target->character_id, DebugBalanceAttackSpin);
            float spin_knockback = debug_balance_get_knockback(target->character_id, DebugBalanceAttackSpin);
            int spin_stun = debug_balance_get_stun(target->character_id, DebugBalanceAttackSpin);

            if (attacker == &player)
            {
                debug_track_player_damage_dealt(target->character_id, spin_damage);
                debug_track_player_knockback_dealt(spin_knockback);
                debug_track_player_stun_dealt(target->character_id, spin_stun);
            }
            if (target == &player)
            {
                debug_track_player_damage_received(attacker->character_id, spin_damage);
                debug_track_player_knockback_received(spin_knockback);
                debug_track_player_stun_received(attacker->character_id, spin_stun);
            }

            if (spin_damage > 0)
            {
                debug_battle_add_damage(target->character_id, spin_damage);
                attacker->life -= spin_damage;
            }

            game_loop_apply_hit_effect(attacker, attacker_physics, !attacker->flip, spin_knockback, spin_stun);
            game_loop_apply_hit_effect(target, target_physics, attacker->flip, spin_knockback * 2.0f, 0);
            damage_fx_trigger_world(target_world_x, target_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
        }
        else
        {
            int damage = debug_balance_get_damage(attacker->character_id, DebugBalanceAttackPunch1);
            debug_battle_add_damage(attacker->character_id, damage);
            attacker->battle_damage_dealt += damage;
            target->life -= damage;
            if (attacker == &player)
            {
                debug_track_player_damage_dealt(target->character_id, damage);
                debug_track_player_knockback_dealt(attacker_knockback_punch1);
                debug_track_player_stun_dealt(target->character_id, STUN_LIGHT_FRAMES);
            }
            if (target == &player)
            {
                debug_track_player_damage_received(attacker->character_id, damage);
                debug_track_player_knockback_received(attacker_knockback_punch1);
                debug_track_player_stun_received(attacker->character_id, STUN_LIGHT_FRAMES);
            }
            game_loop_apply_hit_effect(target, target_physics, attacker->flip, attacker_knockback_punch1, debug_balance_get_stun(attacker->character_id, DebugBalanceAttackPunch1));
            damage_fx_trigger_world(target_world_x, target_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_low_sfx());
        }
    }

    if (attacker->punch2
        && !attacker->hit_done_punch2
        && game_loop_attack_reached_hit_frame(attacker, 1)
        && game_loop_is_attack_in_range(attacker_world_x,
                                        attacker_world_y,
                                        attacker->flip,
                                        target_world_x,
                                        target_world_y,
                                        attacker->hit_range_punch2))
    {
        attacker->hit_done_punch2 = true;
        if (target->spin)
        {
            int spin_damage = debug_balance_get_damage(target->character_id, DebugBalanceAttackSpin);
            float spin_knockback = debug_balance_get_knockback(target->character_id, DebugBalanceAttackSpin);
            int spin_stun = debug_balance_get_stun(target->character_id, DebugBalanceAttackSpin);

            if (attacker == &player)
            {
                debug_track_player_damage_dealt(target->character_id, spin_damage);
                debug_track_player_knockback_dealt(spin_knockback);
                debug_track_player_stun_dealt(target->character_id, spin_stun);
            }
            if (target == &player)
            {
                debug_track_player_damage_received(attacker->character_id, spin_damage);
                debug_track_player_knockback_received(spin_knockback);
                debug_track_player_stun_received(attacker->character_id, spin_stun);
            }

            if (spin_damage > 0)
            {
                debug_battle_add_damage(target->character_id, spin_damage);
                attacker->life -= spin_damage;
            }

            game_loop_apply_hit_effect(attacker, attacker_physics, !attacker->flip, spin_knockback, spin_stun);
            game_loop_apply_hit_effect(target, target_physics, attacker->flip, spin_knockback * 2.0f, 0);
            damage_fx_trigger_world(target_world_x, target_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
        }
        else
        {
            int damage = debug_balance_get_damage(attacker->character_id, DebugBalanceAttackPunch2);
            debug_battle_add_damage(attacker->character_id, damage);
            attacker->battle_damage_dealt += damage;
            target->life -= damage;
            if (attacker == &player)
            {
                debug_track_player_damage_dealt(target->character_id, damage);
                debug_track_player_stun_dealt(target->character_id, STUN_HEAVY_FRAMES);
            }
            if (target == &player)
            {
                debug_track_player_damage_received(attacker->character_id, damage);
                debug_track_player_stun_received(attacker->character_id, STUN_HEAVY_FRAMES);
            }
            game_loop_apply_hit_effect(target, target_physics, attacker->flip, attacker_knockback_punch2, debug_balance_get_stun(attacker->character_id, DebugBalanceAttackPunch2));
            damage_fx_trigger_world(target_world_x, target_world_y);
            game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
        }
    }

    {
        int range_k1 = attacker->hit_range_kick1;
        if (attacker->character_id == CHARACTER_ID_TAILS)
            range_k1 += 10; /* Tails kick extends farther due to tail reach */

        if (attacker->kick
            && attacker->character_id != CHARACTER_ID_KNUCKLES
            && !attacker->hit_done_kick1
            && game_loop_attack_reached_hit_frame(attacker, 2)
            && game_loop_is_attack_in_range(attacker_world_x,
                                            attacker_world_y,
                                            attacker->flip,
                                            target_world_x,
                                            target_world_y,
                                            range_k1))
        {
            attacker->hit_done_kick1 = true;
            if (target->spin)
            {
                int spin_damage = debug_balance_get_damage(target->character_id, DebugBalanceAttackSpin);
                float spin_knockback = debug_balance_get_knockback(target->character_id, DebugBalanceAttackSpin);
                int spin_stun = debug_balance_get_stun(target->character_id, DebugBalanceAttackSpin);

                if (attacker == &player)
                {
                    debug_track_player_damage_dealt(target->character_id, spin_damage);
                    debug_track_player_knockback_dealt(spin_knockback);
                    debug_track_player_stun_dealt(target->character_id, spin_stun);
                }
                if (target == &player)
                {
                    debug_track_player_damage_received(attacker->character_id, spin_damage);
                    debug_track_player_knockback_received(spin_knockback);
                    debug_track_player_stun_received(attacker->character_id, spin_stun);
                }

                if (spin_damage > 0)
                {
                    debug_battle_add_damage(target->character_id, spin_damage);
                    attacker->life -= spin_damage;
                }

                game_loop_apply_hit_effect(attacker, attacker_physics, !attacker->flip, spin_knockback, spin_stun);
                game_loop_apply_hit_effect(target, target_physics, attacker->flip, spin_knockback * 2.0f, 0);
                damage_fx_trigger_world(target_world_x, target_world_y);
                game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            }
            else
            {
                int damage = debug_balance_get_damage(attacker->character_id, DebugBalanceAttackKick1);
                debug_battle_add_damage(attacker->character_id, damage);
                attacker->battle_damage_dealt += damage;
                target->life -= damage;
                if (attacker == &player)
                {
                    debug_track_player_damage_dealt(target->character_id, damage);
                    debug_track_player_knockback_dealt(attacker_knockback_kick1);
                    debug_track_player_stun_dealt(target->character_id, STUN_LIGHT_FRAMES);
                }
                if (target == &player)
                {
                    debug_track_player_damage_received(attacker->character_id, damage);
                    debug_track_player_knockback_received(attacker_knockback_kick1);
                    debug_track_player_stun_received(attacker->character_id, STUN_LIGHT_FRAMES);
                }
                game_loop_apply_hit_effect(target, target_physics, attacker->flip, attacker_knockback_kick1, debug_balance_get_stun(attacker->character_id, DebugBalanceAttackKick1));
                damage_fx_trigger_world(target_world_x, target_world_y);
                game_audio_play_sfx_next_channel(game_audio_get_damage_low_sfx());
            }
        }
    }

    {
        int range_k2 = attacker->hit_range_kick2;
        if (attacker->character_id == CHARACTER_ID_TAILS)
            range_k2 += 10; /* Tails second kick also needs extra reach */

        if (attacker->kick2
            && (attacker->character_id != CHARACTER_ID_KNUCKLES
                || attacker->charged_kick_active
                || attacker->charged_kick_phase > 0
                || attacker_physics->is_in_air)
            && !attacker->hit_done_kick2
            && game_loop_attack_reached_hit_frame(attacker, 3)
            && game_loop_is_attack_in_range(attacker_world_x,
                                            attacker_world_y,
                                            attacker->flip,
                                            target_world_x,
                                            target_world_y,
                                            range_k2))
        {
            attacker->hit_done_kick2 = true;
            bool charged_kick = combat_profile.charged_kick_enabled
                && (attacker->charged_kick_active || attacker->charged_kick_phase > 0);
            float kick2_knockback = charged_kick
                ? (attacker->knockback_kick2 * combat_profile.charged_kick_knockback_mult)
                : attacker->knockback_kick2;
            int kick2_stun = debug_balance_get_stun(attacker->character_id, charged_kick ? DebugBalanceAttackCharged : DebugBalanceAttackKick2);
            int kick2_damage = debug_balance_get_damage(attacker->character_id, charged_kick ? DebugBalanceAttackCharged : DebugBalanceAttackKick2);

            if (target->spin)
            {
                int spin_damage = debug_balance_get_damage(target->character_id, DebugBalanceAttackSpin);
                float spin_knockback = debug_balance_get_knockback(target->character_id, DebugBalanceAttackSpin);
                int spin_stun = debug_balance_get_stun(target->character_id, DebugBalanceAttackSpin);

                if (attacker == &player)
                {
                    debug_track_player_damage_dealt(target->character_id, spin_damage);
                    debug_track_player_knockback_dealt(spin_knockback);
                    debug_track_player_stun_dealt(target->character_id, spin_stun);
                }
                if (target == &player)
                {
                    debug_track_player_damage_received(attacker->character_id, spin_damage);
                    debug_track_player_knockback_received(spin_knockback);
                    debug_track_player_stun_received(attacker->character_id, spin_stun);
                }

                if (spin_damage > 0)
                {
                    debug_battle_add_damage(target->character_id, spin_damage);
                    attacker->life -= spin_damage;
                }

                game_loop_apply_hit_effect(attacker, attacker_physics, !attacker->flip, spin_knockback, spin_stun);
                game_loop_apply_hit_effect(target, target_physics, attacker->flip, spin_knockback * 2.0f, 0);
                damage_fx_trigger_world(target_world_x, target_world_y);
                game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            }
            else
            {
                debug_battle_add_damage(attacker->character_id, kick2_damage);
                attacker->battle_damage_dealt += kick2_damage;
                target->life -= kick2_damage;
                if (attacker == &player)
                {
                    debug_track_player_damage_dealt(target->character_id, kick2_damage);
                    debug_track_player_knockback_dealt(kick2_knockback);
                    debug_track_player_stun_dealt(target->character_id, kick2_stun);
                }
                if (target == &player)
                {
                    debug_track_player_damage_received(attacker->character_id, kick2_damage);
                    debug_track_player_knockback_received(kick2_knockback);
                    debug_track_player_stun_received(attacker->character_id, kick2_stun);
                }
                game_loop_apply_hit_effect(target, target_physics, attacker->flip, kick2_knockback, debug_balance_get_stun(attacker->character_id, charged_kick ? DebugBalanceAttackCharged : DebugBalanceAttackKick2));
                damage_fx_trigger_world(target_world_x, target_world_y);
                game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            }
        }

        attacker->charged_kick_active = false;
    }

    if (target->life <= 0)
    {
        target->life = 0;
        if (target_defeated != JO_NULL)
            *target_defeated = true;
        target->walk = false;
        target->run = 0;
        target->punch = false;
        target->kick = false;
        target->punch2 = false;
        target->kick2 = false;
        target->spin = false;
    }
}

static void game_loop_process_player_vs_player_hits(void)
{
    int p1_world_x;
    int p1_world_y;
    int p2_world_x;
    int p2_world_y;

    if (!player2_is_active())
    {
        g_dbg_pvp_available = false;
        return;
    }

    p1_world_x = *g_ctx->map_pos_x + player.x;
    p1_world_y = *g_ctx->map_pos_y + player.y;
    p2_world_x = *g_ctx->map_pos_x + player2.x;
    p2_world_y = *g_ctx->map_pos_y + player2.y;

    game_loop_update_pvp_hitbox_debug(p1_world_x, p1_world_y, p2_world_x, p2_world_y);

    bool player2_defeated = (player2.life <= 0);

    {
        player_instance_t *p1 = player_get_default_player();
        player_instance_t *p2 = player_get_default_player2();

        game_loop_process_player_attack(p1->character,
                                        &p1->physics,
                                        p2->character,
                                        &p2->physics,
                                        &player2_defeated,
                                        p1_world_x,
                                        p1_world_y,
                                        p2_world_x,
                                        p2_world_y,
                                        &p1->prev_punch,
                                        &p1->prev_punch2,
                                        &p1->prev_kick,
                                        &p1->prev_kick2);

        game_loop_process_player_attack(p2->character,
                                        &p2->physics,
                                        p1->character,
                                        &p1->physics,
                                        g_ctx->player_defeated,
                                        p2_world_x,
                                        p2_world_y,
                                        p1_world_x,
                                        p1_world_y,
                                        &p2->prev_punch,
                                        &p2->prev_punch2,
                                        &p2->prev_kick,
                                        &p2->prev_kick2);

        p1->prev_punch = false;
        p1->prev_punch2 = false;
        p1->prev_kick = false;
        p1->prev_kick2 = false;
        p2->prev_punch = false;
        p2->prev_punch2 = false;
        p2->prev_kick = false;
        p2->prev_kick2 = false;
    }
}

static const char *character_short_name(int character_id)
{
    switch (character_id)
    {
    case CHARACTER_ID_AMY:
        return "AMY";
    case CHARACTER_ID_TAILS:
        return "TLS";
    case CHARACTER_ID_KNUCKLES:
        return "KNK";
    case CHARACTER_ID_SHADOW:
        return "SDW";
    case CHARACTER_ID_SONIC:
    default:
        return "SNC";
    }
}

static void game_loop_draw_battle_end_stats(void)
{
    typedef struct {
        const char *name;
        int life;
        int damage;
        int group;
    } battle_stat_t;

    battle_stat_t stats[2 + BOT_MAX_DEFAULT_COUNT];
    int stat_count = 0;

    stats[stat_count++] = (battle_stat_t){character_short_name(player.character_id), player.life, player.battle_damage_dealt, player.group};

    if (player2_is_active())
    {
        stats[stat_count++] = (battle_stat_t){character_short_name(player2.character_id), player2.life, player2.battle_damage_dealt, player2.group};
    }

    int bot_count = bot_get_active_count();
    int max_stats = (int)(sizeof(stats) / sizeof(stats[0]));
    for (int i = 0; i < bot_count && stat_count < max_stats; ++i)
    {
        const char *name = character_short_name(bot_get_character_id(i));
        int life = bot_get_life(i);
        int group = bot_get_group(i);
        int damage = debug_battle_damage_dealt(bot_get_character_id(i));
        stats[stat_count++] = (battle_stat_t){name, life, damage, group};
    }

    bool g1_alive = false;
    bool g2_alive = false;
    int total_g1 = 0;
    int total_g2 = 0;

    for (int i = 0; i < stat_count; ++i)
    {
        if (stats[i].group == 1)
        {
            total_g1 += stats[i].damage;
            if (stats[i].life > 0)
                g1_alive = true;
        }
        else if (stats[i].group == 2)
        {
            total_g2 += stats[i].damage;
            if (stats[i].life > 0)
                g2_alive = true;
        }
    }

    const char *result_label = "DRAW";
    if (g1_alive && !g2_alive)
        result_label = "G1 WINS";
    else if (!g1_alive && g2_alive)
        result_label = "G2 WINS";
    else if (!g1_alive && !g2_alive)
        result_label = "G1/G2 LOST";
    else if (total_g1 > total_g2)
        result_label = "G1 WINS";
    else if (total_g2 > total_g1)
        result_label = "G2 WINS";

    const char *clear_line = "                                        ";
    for (int y = 2; y < 2 + 6 + stat_count; ++y)
        jo_printf(0, y, "%s", clear_line);

    jo_printf(14, 2, "END BATTLE");
    jo_printf(2, 8, " #  NAME  LIFE  DMG  GRP");

    for (int i = 0; i < stat_count; ++i)
    {
        jo_printf(2, 9 + i, "%2d  %-4s  %3d   %4d  G%d", i + 1, stats[i].name, stats[i].life, stats[i].damage, stats[i].group);
    }

    jo_printf(2, 9 + stat_count + 1, "TOTAL -> G1:%d  G2:%d", total_g1, total_g2);
    jo_printf(2, 9 + stat_count + 2, "%s", result_label);
}

static void game_loop_draw_life_bar(int x, int y, const char *label, int life)
{
    int life_percent = (life * 100) / 50;
    int bar_max_width = 10;
    int bar_width = (life_percent * bar_max_width) / 100;
    char bar[bar_max_width + 1];
    for (int i = 0; i < bar_max_width; ++i)
        bar[i] = (i < bar_width) ? '#' : '-';
    bar[bar_max_width] = '\0';

    jo_printf(x, y, "%s[%s] %d%%", label, bar, life_percent);
}

static void game_loop_draw_health_bars(void)
{
    /* Draw up to 5 life bars (player + player2 + up to 3 bots), using 3-letter codes. */
    game_loop_draw_life_bar(1, 24, character_short_name(player.character_id), player.life);

    if (player2_is_active() && g_ctx->ui_state->menu_multiplayer_versus)
        game_loop_draw_life_bar(1, 25, character_short_name(player2.character_id), player2.life);

    int bot_count = bot_get_active_count();
    if (bot_count > 0)
        game_loop_draw_life_bar(24, 24, character_short_name(bot_get_character_id(0)), bot_get_life(0));
    if (bot_count > 1)
        game_loop_draw_life_bar(24, 25, character_short_name(bot_get_character_id(1)), bot_get_life(1));
    if (bot_count > 2)
        game_loop_draw_life_bar(24, 26, character_short_name(bot_get_character_id(2)), bot_get_life(2));
    if (bot_count > 3)
        game_loop_draw_life_bar(24, 27, character_short_name(bot_get_character_id(3)), bot_get_life(3));
}

extern bool g_loading_just_started;

void game_loop_init(game_loop_context_t *ctx)
{
    g_ctx = ctx;
    /* Seed RNG for AI randomness. */
    srand((unsigned)time(NULL));
    /* Inicializa o cache VRAM/WRAM para evitar uso de APIs não inicializadas. */
    vram_cache_init();
}

void game_loop_update(void)
{
    if (g_ctx->ui_state->diag_menu_only && g_ctx->ui_state->current_game_state == UiGameStateMenu)
        return;

    *g_ctx->total_pcm = game_audio_total_pcm();
    game_flow_runtime_tick();

    if (g_ctx->ui_state->current_game_state == UiGameStateLoading)
    {
        if (g_loading_just_started)
        {
            /* Let the next frame draw the loading screen before doing the heavy loading work. */
            g_loading_just_started = false;
            return;
        }

        g_runtime_playing_update_logs = 0;
        g_runtime_playing_draw_logs = 0;
        player2_sync_mode(false);
        player2_reset_runtime(g_ctx->map_pos_x, g_ctx->map_pos_y);
        game_flow_process_loading(g_ctx->game_flow_ctx);
        return;
    }

    if (g_ctx->ui_state->current_game_state != UiGameStatePlaying || g_ctx->ui_state->game_paused)
    {
        g_runtime_playing_update_logs = 0;
        g_runtime_playing_draw_logs = 0;
        return;
    }

    if (g_runtime_playing_update_logs < 4)
    {
        runtime_log_verbose("upd: begin");
    }

    if (g_ctx->ui_state->diag_vram_uploads)
        world_map_prepare_visible_tiles(*g_ctx->map_pos_x, *g_ctx->map_pos_y);

    player2_sync_mode(g_ctx->ui_state->menu_multiplayer_versus);

    player.speed = (int)JO_ABS(g_ctx->physics->speed);

    game_loop_process_player_vs_player_hits();

    if (g_runtime_playing_update_logs < 5)
    {
        runtime_log_verbose("upd: hits done");
    }

    if (!g_ctx->ui_state->pause_bots && g_ctx->ui_state->diag_bot_updates)
    {
        bool player2_defeated = player2_is_defeated();
        bool player2_active = player2_is_active();

        bot_update(&player,
                   g_ctx->player_defeated,
                   g_ctx->physics,
                   player2_active ? &player2 : JO_NULL,
                   player2_active ? &player2_defeated : JO_NULL,
                   player2_active ? player2_get_physics() : JO_NULL,
                   player2_active,
                   *g_ctx->map_pos_x,
                   *g_ctx->map_pos_y);
    }

    if (g_runtime_playing_update_logs < 6)
    {
        runtime_log_verbose("upd: bot done");
        ++g_runtime_playing_update_logs;
    }
}

void game_loop_debug_frame(void)
{
    if (g_ctx->ui_state->debug_mode == UiDebugModeHardware)
        debug_set_display_mode(DebugDisplayHardware);
    else if (g_ctx->ui_state->debug_mode == UiDebugModeCamera)
        debug_set_display_mode(DebugDisplayCamera);
    else
        debug_set_display_mode(DebugDisplayPlayer);

    if (g_ctx->ui_state->current_game_state == UiGameStatePlaying && !g_ctx->ui_state->game_paused && g_ctx->ui_state->debug_enabled)
        debug_frame();
}

int game_loop_get_map_pos_x(void)
{
    if (g_ctx == JO_NULL || g_ctx->map_pos_x == JO_NULL)
        return 0;
    return *g_ctx->map_pos_x;
}

int game_loop_get_map_pos_y(void)
{
    if (g_ctx == JO_NULL || g_ctx->map_pos_y == JO_NULL)
        return 0;
    return *g_ctx->map_pos_y;
}

int game_loop_get_camera_pos_x(void)
{
    return (int)g_camera_world_x;
}

int game_loop_get_camera_pos_y(void)
{
    return (int)g_camera_world_y;
}

void game_loop_mark_ui_dirty(void)
{
    g_ui_text_dirty = true;
}

void game_loop_draw(void)
{
    static bool g_prev_need_text_layer = false;
    static ui_menu_screen_t g_prev_menu_screen = UiMenuScreenMain;
    static ui_game_state_t g_prev_game_state = UiGameStateMenu;
    static bool g_prev_game_paused = false;
    static bool g_prev_diag_menu_only = false;
    bool need_text_layer = (g_ctx->ui_state->current_game_state != UiGameStatePlaying)
        || g_ctx->ui_state->game_paused
        || g_ctx->ui_state->debug_enabled
        || (runtime_log_get_mode() != RuntimeLogModeOff)
        || g_ctx->ui_state->pause_bots;

    /* Force redraw when the need for a text layer changes (enter/exit pause/loading/logs). */
    if (need_text_layer != g_prev_need_text_layer)
        g_ui_text_dirty = true;

    /* Force redraw when entering/exiting pause or changing menu/game state. */
    if (g_ctx->ui_state->game_paused != g_prev_game_paused
        || g_ctx->ui_state->menu_screen != g_prev_menu_screen
        || g_ctx->ui_state->current_game_state != g_prev_game_state
        || g_ctx->ui_state->diag_menu_only != g_prev_diag_menu_only)
    {
        g_ui_text_dirty = true;
    }

    bool entering_playing = (g_prev_game_state != UiGameStatePlaying && g_ctx->ui_state->current_game_state == UiGameStatePlaying);

    g_prev_need_text_layer = need_text_layer;
    g_prev_menu_screen = g_ctx->ui_state->menu_screen;
    g_prev_game_state = g_ctx->ui_state->current_game_state;
    g_prev_game_paused = g_ctx->ui_state->game_paused;
    g_prev_diag_menu_only = g_ctx->ui_state->diag_menu_only;

    if (entering_playing || !g_camera_world_initialized)
    {
        g_camera_world_x = *g_ctx->map_pos_x;
        g_camera_world_y = *g_ctx->map_pos_y;
        g_player_map_pos_x = *g_ctx->map_pos_x;
        g_player_map_pos_y = *g_ctx->map_pos_y;
        g_camera_world_initialized = true;
        g_prev_free_cam = false;
    }


    if (g_ui_text_dirty)
    {
        ui_control_clear_text_layer();
        g_ui_text_dirty = false;
    }

    if (g_ctx->ui_state->current_game_state == UiGameStateLoading)
    {
        jo_core_set_screens_order(JO_NBG0_SCREEN, JO_NBG1_SCREEN, JO_SPRITE_SCREEN, JO_RBG0_SCREEN, JO_NBG2_SCREEN, JO_NBG3_SCREEN);
    }
    else
    {
        jo_core_set_screens_order(JO_NBG0_SCREEN, JO_SPRITE_SCREEN, JO_RBG0_SCREEN, JO_NBG1_SCREEN, JO_NBG2_SCREEN, JO_NBG3_SCREEN);
    }

    if (g_ctx->ui_state->current_game_state == UiGameStateMenu)
    {
        if (g_ctx->ui_state->diag_menu_only)
        {
            /* Avoid clearing every frame; only clear when needed to avoid VDP write cost. */
            if (g_ui_text_dirty)
            {
                jo_clear_background(JO_COLOR_Black);
                ui_control_clear_text_layer();
                jo_printf(0, 0, "MENU ONLY (diag_menu_only)");
                g_ui_text_dirty = false;
            }
            return;
        }

        if (g_ctx->ui_state->diag_draw_backgrounds)
        {
            world_background_set_menu();
            jo_move_background(0, 0);
        }
        else if (g_ui_text_dirty)
        {
            jo_clear_background(JO_COLOR_Black);
        }
        ui_control_draw_character_menu(g_ctx->ui_state);
        return;
    }

    if (g_ctx->ui_state->current_game_state == UiGameStateLoading)
    {
        // Ensure any WRAM->VRAM uploads advance even during loading.
        vram_cache_do_uploads();

        // Avança a carga do mapa (não-bloqueante). world_map_do_load_step
        // fará leituras incrementais via vram_cache e só chamará
        // jo_map_load_from_file quando o pack estiver pronto.
        world_map_do_load_step();
        jo_move_background(0, 0);
        ui_control_draw_loading();
        runtime_log_draw(0, 16);
        return;
    }

    if (g_ctx->ui_state->game_paused)
    {
        world_background_set_menu();
        jo_move_background(0, 0);
        ui_control_draw_pause_menu(g_ctx->ui_state);
        return;
    }

    if (g_ctx->ui_state->diag_disable_draw)
    {
        jo_clear_background(JO_COLOR_Black);
        return;
    }

    if (g_ctx->ui_state->diag_draw_backgrounds)
    {
        world_background_set_gameplay();
        if (g_runtime_playing_draw_logs < 1)
        {
            runtime_log_verbose("draw: bg mode");
            ++g_runtime_playing_draw_logs;
        }
        /* We'll apply camera offset to the background draw below. */
    }
    else
    {
        jo_clear_background(JO_COLOR_Black);
    }

    // Ensure any scheduled WRAM->VRAM uploads are performed every frame.
    // This is required so deferred loads (like SNC_FUL) can complete.
    vram_cache_do_uploads();

    player_instance_t *p1 = player_get_default_player();

    /* Camera free mode (R held) allows independent movement in world space. */
    bool free_cam_p1 = !g_camera_follow_player2 && jo_is_pad1_key_pressed(JO_KEY_R);
    bool free_cam_p2 = g_camera_follow_player2 && input_mapping_is_player2_key_pressed(JO_KEY_R);

    if (g_free_cam_lock_frames > 0)
        --g_free_cam_lock_frames;

    /* Prevent re-entering free cam while on cooldown. */
    if (g_free_cam_lock_frames > 0)
    {
        free_cam_p1 = false;
        free_cam_p2 = false;
    }

    bool camera_free_mode = free_cam_p1 || free_cam_p2;
    bool entering_free_cam = camera_free_mode && !g_prev_free_cam;

    /* If entering free cam, freeze the player whose input is controlling the camera. */
    if (entering_free_cam)
    {
        if (free_cam_p1)
        {
            g_player_map_pos_x = *g_ctx->map_pos_x;
            g_player_map_pos_y = *g_ctx->map_pos_y;
            p1->character->walk = false;
            p1->character->run = 0;
            p1->character->spin = false;
            p1->physics.speed = 0.0f;
            p1->physics.speed_y = 0.0f;
        }
        else if (free_cam_p2)
        {
            player_instance_t *p2 = player_get_default_player2();
            if (p2 != JO_NULL)
            {
                p2->character->walk = false;
                p2->character->run = 0;
                p2->character->spin = false;
                p2->physics.speed = 0.0f;
                p2->physics.speed_y = 0.0f;
            }
        }
    }

    /* Prevent P1 movement from affecting the global map scroll when camera is following P2. */
    int tmp_player1_map_pos_x = *g_ctx->map_pos_x;
    int tmp_player1_map_pos_y = *g_ctx->map_pos_y;

    int *player_map_pos_x = g_ctx->map_pos_x;
    int *player_map_pos_y = g_ctx->map_pos_y;

    if (g_camera_follow_player2 && player2_is_active())
    {
        player_map_pos_x = &tmp_player1_map_pos_x;
        player_map_pos_y = &tmp_player1_map_pos_y;
    }

    player_instance_update_runtime(p1, player_map_pos_x, player_map_pos_y);

    /* Keep the frozen map position used for drawing offsets in sync with the actual map_pos. */
    g_player_map_pos_x = *g_ctx->map_pos_x;
    g_player_map_pos_y = *g_ctx->map_pos_y;

    /* Update camera position after the player has moved. */
    if (camera_free_mode)
    {
        jo_printf(25, 1, "FREE CAM");

        if (free_cam_p1)
        {
            if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
                g_camera_world_x -= CAMERA_FREE_SPEED;
            if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
                g_camera_world_x += CAMERA_FREE_SPEED;
            if (jo_is_pad1_key_pressed(JO_KEY_UP))
                g_camera_world_y -= CAMERA_FREE_SPEED;
            if (jo_is_pad1_key_pressed(JO_KEY_DOWN))
                g_camera_world_y += CAMERA_FREE_SPEED;
        }
        else if (free_cam_p2)
        {
            if (input_mapping_is_player2_key_pressed(JO_KEY_LEFT))
                g_camera_world_x -= CAMERA_FREE_SPEED;
            if (input_mapping_is_player2_key_pressed(JO_KEY_RIGHT))
                g_camera_world_x += CAMERA_FREE_SPEED;
            if (input_mapping_is_player2_key_pressed(JO_KEY_UP))
                g_camera_world_y -= CAMERA_FREE_SPEED;
            if (input_mapping_is_player2_key_pressed(JO_KEY_DOWN))
                g_camera_world_y += CAMERA_FREE_SPEED;
        }
    }
    else
    {
        jo_printf(25, 1, "        ");

        float player_world_x;
        float player_world_y;

        if (g_camera_follow_player2 && player2_is_active())
        {
            player_world_x = (float)(*g_ctx->map_pos_x + player2.x + (CHARACTER_WIDTH / 2));
            player_world_y = (float)(*g_ctx->map_pos_y + player2.y);
        }
        else
        {
            player_world_x = (float)(g_player_map_pos_x + p1->character->x + (CHARACTER_WIDTH / 2));
            player_world_y = (float)(g_player_map_pos_y + p1->character->y);
        }

        float target_world_x = player_world_x - CAMERA_TARGET_SCREEN_X;
        float target_world_y = player_world_y - CAMERA_TARGET_SCREEN_Y;

        float dx = target_world_x - g_camera_world_x;
        float dy = target_world_y - g_camera_world_y;

        g_camera_world_x += dx * CAMERA_RETURN_SPEED;
        g_camera_world_y += dy * CAMERA_RETURN_SPEED;

        if (JO_ABS(dx) < 1.0f)
            g_camera_world_x = target_world_x;
        if (JO_ABS(dy) < 1.0f)
            g_camera_world_y = target_world_y;
    }

    g_prev_free_cam = camera_free_mode;

    /* Track which player is currently in free cam, for damage-triggered exit. */
    if (camera_free_mode)
        g_free_cam_player = free_cam_p1 ? 0 : 1;
    else
        g_free_cam_player = -1;

    /* Map position is authoritative for physics/collision; camera defines the visible portion of the world. */
    int render_map_x = (int)g_camera_world_x;
    int render_map_y = (int)g_camera_world_y;
    int screen_x = render_map_x - JO_TV_WIDTH_2;
    int screen_y = render_map_y - JO_TV_HEIGHT_2;

    if (g_ctx->ui_state->diag_draw_backgrounds)
    {
        world_background_draw_parallax(render_map_x, render_map_y);
        if (g_runtime_playing_draw_logs < 2)
        {
            runtime_log_verbose("draw: parallax");
            ++g_runtime_playing_draw_logs;
        }
    }

    if (g_ctx->ui_state->diag_vram_uploads)
    {
        /* Ensure tiles visible in the current camera view are prepared. */
        world_map_prepare_visible_tiles(screen_x, screen_y);

        jo_map_draw_ext(WORLD_MAP_ID, (short)screen_x, (short)screen_y);

        if (g_runtime_playing_draw_logs < 3)
        {
            runtime_log_verbose("draw: uploads");
            ++g_runtime_playing_draw_logs;
        }
        if (g_runtime_playing_draw_logs < 4)
        {
            runtime_log_verbose("draw: map");
            ++g_runtime_playing_draw_logs;
        }
    }
    else
    {
        jo_clear_background(JO_COLOR_Black);
    }
    if (g_runtime_playing_draw_logs < 4)
    {
        runtime_log_verbose("draw: map");
        ++g_runtime_playing_draw_logs;
    }

    if (g_runtime_playing_draw_logs < 5)
    {
        runtime_log_verbose("draw: camera");
        ++g_runtime_playing_draw_logs;
    }

    /* Draw player relative to the current camera position. */
    int saved_x = p1->character->x;
    int saved_y = p1->character->y;
    int player_offset_x = (int)(g_player_map_pos_x - g_camera_world_x);
    int player_offset_y = (int)(g_player_map_pos_y - g_camera_world_y);


    p1->character->x += player_offset_x;
    p1->character->y += player_offset_y;

    game_flow_display_character();

    p1->character->x = saved_x;
    p1->character->y = saved_y;

    player2_sync_mode(g_ctx->ui_state->menu_multiplayer_versus);
    /* Update player2 physics using the shared world origin (map_pos).
       We offset when drawing so P2 stays in sync with the camera even during free cam. */
    player2_update_runtime(g_ctx->map_pos_x, g_ctx->map_pos_y);
    game_flow_update_animation();

    /* Player2 uses the same draw path as Player1 (no special-case draw code). */
    if (player2_is_active())
    {
        int saved_x2 = player2.x;
        int saved_y2 = player2.y;
        player2.x += player_offset_x;
        player2.y += player_offset_y;
        player_draw(&player2, player2_get_physics());
        player2.x = saved_x2;
        player2.y = saved_y2;
    }

    bot_draw(render_map_x, render_map_y);
    damage_fx_draw(render_map_x, render_map_y);

    game_loop_draw_health_bars();

    if (g_runtime_playing_draw_logs < 6)
    {
        runtime_log_verbose("draw: chars");
        ++g_runtime_playing_draw_logs;
    }

    if (*g_ctx->player_defeated || bot_is_defeated())
        game_loop_draw_battle_end_stats();

    if (g_ctx->ui_state->debug_mode == UiDebugModeHardware)
        debug_set_display_mode(DebugDisplayHardware);
    else
        if (g_ctx->ui_state->debug_mode == UiDebugModeCamera)
        debug_set_display_mode(DebugDisplayCamera);
    else
        debug_set_display_mode(DebugDisplayPlayer);

    if (g_ctx->ui_state->debug_enabled)
        debug_frame();

    /* Draw runtime_log once at the end of the frame (bottom area) */
    runtime_log_draw(0, 16);
}

void game_loop_input(void)
{
    if (g_ctx->ui_state->current_game_state == UiGameStateMenu)
    {
        ui_control_handle_menu_input(g_ctx->ui_state, game_flow_start_from_menu, g_ctx->game_flow_ctx);
        return;
    }

    if (g_ctx->ui_state->current_game_state == UiGameStateLoading)
        return;

    ui_control_handle_start_toggle(g_ctx->ui_state);

    if (g_ctx->ui_state->game_paused)
    {
        ui_control_handle_pause_input(g_ctx->ui_state,
                                      game_flow_reset_fight,
                                      game_flow_return_to_character_select,
                                      g_ctx->game_flow_ctx);
        return;
    }

    if (*g_ctx->player_defeated)
    {
        player.walk = false;
        player.run = 0;
        player.spin = false;
        player.punch = false;
        player.punch2 = false;
        player.kick = false;
        player.kick2 = false;
        jo_physics_apply_friction(g_ctx->physics);
        return;
    }

    bool free_cam_p1 = !g_camera_follow_player2 && jo_is_pad1_key_pressed(JO_KEY_R);
    bool free_cam_p2 = g_camera_follow_player2 && input_mapping_is_player2_key_pressed(JO_KEY_R);
    bool camera_free_mode = free_cam_p1 || free_cam_p2;
    bool entering_free_cam = camera_free_mode && !g_prev_free_cam;

    if (entering_free_cam)
    {
        if (free_cam_p1)
        {
            player_instance_t *p1 = player_get_default_player();
            g_player_map_pos_x = *g_ctx->map_pos_x;
            g_player_map_pos_y = *g_ctx->map_pos_y;
            p1->character->walk = false;
            p1->character->run = 0;
            p1->character->spin = false;
            p1->physics.speed = 0.0f;
            p1->physics.speed_y = 0.0f;
        }
        else if (free_cam_p2)
        {
            player_instance_t *p2 = player_get_default_player2();
            if (p2 != JO_NULL)
            {
                p2->character->walk = false;
                p2->character->run = 0;
                p2->character->spin = false;
                p2->physics.speed = 0.0f;
                p2->physics.speed_y = 0.0f;
            }
        }
    }

    /* Camera focus toggle (L): swap focus between P1/P2 when the camera currently follows that player. */
    if (jo_is_pad1_key_pressed(JO_KEY_L) && !g_camera_follow_player2)
    {
        if (player2_is_active())
            g_camera_follow_player2 = true;
    }
    if (input_mapping_is_player2_key_pressed(JO_KEY_L) && g_camera_follow_player2)
    {
        g_camera_follow_player2 = false;
    }

    /* If free cam is active, block input only for the controlling player. */
    if (!camera_free_mode || free_cam_p2)
    {
        /* Player1 uses the same input flow as Player2 via the generic player instance API. */
        player_instance_t *p1 = player_get_default_player();
        if (p1 != JO_NULL && !free_cam_p1)
        {
            control_input_t player1_input;
            input_mapping_read_player1(&player1_input);
            player_instance_handle_input(p1,
                                         &player1_input,
                                         game_audio_get_jump_sfx(),
                                         1,
                                         game_audio_get_spin_sfx(),
                                         game_audio_get_punch_sfx(),
                                         game_audio_get_kick_sfx());
        }
    }

    player2_sync_mode(g_ctx->ui_state->menu_multiplayer_versus);
    if (player2_is_active() && !free_cam_p2)
    {
        control_input_t player2_input;
        input_mapping_read_player2(&player2_input);
        player2_handle_input(&player2_input);
    }
}

