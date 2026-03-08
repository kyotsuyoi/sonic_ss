#include <jo/jo.h>
#include "ai_control.h"
#include "control.h"
#include "game_audio.h"
#include "damage_fx.h"
#include "game_constants.h"

#define BOT_HIT_RANGE_PUNCH1 8
#define BOT_HIT_RANGE_PUNCH2 8
#define BOT_HIT_RANGE_KICK1 8
#define BOT_HIT_RANGE_KICK2 9
#define BOT_ENGAGE_RANGE 9
#define BOT_COMBO_PUNCH2_RANGE 8
#define BOT_COMBO_KICK2_RANGE 9
#define BOT_PUNCH_START_RANGE 8
#define BOT_APPROACH_DISTANCE 16
#define BOT_JUMP_MIN_VERTICAL_GAP 18
#define BOT_JUMP_MAX_VERTICAL_GAP 180
#define BOT_JUMP_MAX_HORIZONTAL_GAP 120
#define BOT_JUMP_VERTICAL_BONUS_DIV 2
#define BOT_JUMP_DISTANCE_BONUS_DIV 3
#define BOT_JUMP_PROFILE_LOW 0
#define BOT_JUMP_PROFILE_BALANCED 1
#define BOT_JUMP_PROFILE_AGGRESSIVE 2
#define BOT_JUMP_PROFILE BOT_JUMP_PROFILE_BALANCED

#if BOT_JUMP_PROFILE == BOT_JUMP_PROFILE_LOW
#define BOT_JUMP_BASE_CHANCE_PROFILE 16
#define BOT_JUMP_CHANCE_MAX_PROFILE 65
#define BOT_JUMP_MIN_HOLD_MS_PROFILE 17
#define BOT_JUMP_MAX_HOLD_MS_PROFILE 120
#define BOT_JUMP_HOLD_VERTICAL_BONUS_MAX_PROFILE 64
#define BOT_JUMP_HOLD_DISTANCE_BONUS_MAX_PROFILE 24
#elif BOT_JUMP_PROFILE == BOT_JUMP_PROFILE_AGGRESSIVE
#define BOT_JUMP_BASE_CHANCE_PROFILE 32
#define BOT_JUMP_CHANCE_MAX_PROFILE 95
#define BOT_JUMP_MIN_HOLD_MS_PROFILE 51
#define BOT_JUMP_MAX_HOLD_MS_PROFILE 170
#define BOT_JUMP_HOLD_VERTICAL_BONUS_MAX_PROFILE 112
#define BOT_JUMP_HOLD_DISTANCE_BONUS_MAX_PROFILE 56
#else
#define BOT_JUMP_BASE_CHANCE_PROFILE 24
#define BOT_JUMP_CHANCE_MAX_PROFILE 85
#define BOT_JUMP_MIN_HOLD_MS_PROFILE 34
#define BOT_JUMP_MAX_HOLD_MS_PROFILE 170
#define BOT_JUMP_HOLD_VERTICAL_BONUS_MAX_PROFILE 96
#define BOT_JUMP_HOLD_DISTANCE_BONUS_MAX_PROFILE 40
#endif

#define BOT_JUMP_RETRY_COOLDOWN_MIN 8
#define BOT_JUMP_RETRY_COOLDOWN_MAX 18
#define BOT_AIR_KICK_TRIGGER_CHANCE 48
#define BOT_NAV_MIN_VERTICAL_GAP 24
#define BOT_NAV_MAX_VERTICAL_GAP 220
#define BOT_NAV_SCAN_STEP 12
#define BOT_NAV_SCAN_MAX_DISTANCE 168
#define BOT_NAV_PROBE_WIDTH 6
#define BOT_NAV_PROBE_HEIGHT 72
#define BOT_NAV_LANE_TOLERANCE 8
#define BOT_NAV_MAX_GROUND_DELTA 20

static bool bot_nav_has_headroom_at_x(int world_x, int world_y)
{
    int attr;
    int probe_x = world_x + CHARACTER_WIDTH_2 - (BOT_NAV_PROBE_WIDTH / 2);
    int probe_y = world_y - BOT_NAV_PROBE_HEIGHT;

    if (probe_x < 0 || probe_y < 0)
        return false;

    attr = jo_map_hitbox_detection_custom_boundaries(
        WORLD_MAP_ID,
        probe_x,
        probe_y,
        BOT_NAV_PROBE_WIDTH,
        BOT_NAV_PROBE_HEIGHT);

    return attr == JO_MAP_NO_COLLISION;
}

static bool bot_nav_is_ground_reachable_at_x(int candidate_x, int bot_world_y)
{
    int dist = jo_map_per_pixel_vertical_collision(
        WORLD_MAP_ID,
        candidate_x + CHARACTER_WIDTH_2,
        bot_world_y + CHARACTER_HEIGHT,
        JO_NULL);

    if (candidate_x < 0 || bot_world_y < 0)
        return false;

    if (dist == JO_MAP_NO_COLLISION)
        return false;
    return dist >= -BOT_NAV_MAX_GROUND_DELTA && dist <= BOT_NAV_MAX_GROUND_DELTA;
}

static int bot_nav_find_jump_target_x(int bot_world_x, int bot_world_y, int player_world_x)
{
    bool found = false;
    int best_x = bot_world_x;
    int best_score = -2147483647;
    int preferred_dir = (player_world_x >= bot_world_x) ? 1 : -1;
    int offset;

    for (offset = -BOT_NAV_SCAN_MAX_DISTANCE; offset <= BOT_NAV_SCAN_MAX_DISTANCE; offset += BOT_NAV_SCAN_STEP)
    {
        int candidate_x = bot_world_x + offset;
        int score;

        if (!bot_nav_is_ground_reachable_at_x(candidate_x, bot_world_y))
            continue;
        if (!bot_nav_has_headroom_at_x(candidate_x, bot_world_y))
            continue;

        score = 500 - (JO_ABS(candidate_x - player_world_x) * 2) - JO_ABS(offset);
        if (score > best_score)
        {
            found = true;
            best_score = score;
            best_x = candidate_x;
        }
    }

    if (!found)
        return bot_world_x + (preferred_dir * (BOT_NAV_SCAN_MAX_DISTANCE / 2));

    return best_x;
}

void ai_control_handle_bot_commands(ai_bot_context_t *ctx)
{
    int distance_x;
    int distance_abs;
    int vertical_gap;
    bool allow_advanced;
    bool player_airborne;
    bool has_headroom_current;
    int nav_target_x;
    bool nav_mode;
    bool should_hold_jump;
    control_input_t input = {0};

    if (*ctx->player_defeated || ctx->bot_defeated_flag)
        return;

    if (*ctx->bot_jump_cooldown_ref > 0)
        --(*ctx->bot_jump_cooldown_ref);

    if (ctx->bot_ref->stun_timer > 0)
    {
        --ctx->bot_ref->stun_timer;
        *ctx->bot_speed_x_ref *= 0.82f;
        if (JO_ABS(*ctx->bot_speed_x_ref) < 0.05f)
            *ctx->bot_speed_x_ref = 0.0f;
    }

    distance_x = ctx->player_world_x - (int)*ctx->bot_world_x_ref;
    distance_abs = JO_ABS(distance_x);
    vertical_gap = (int)*ctx->bot_world_y_ref - ctx->player_world_y;
    allow_advanced = ctx->warmup_frames_left <= 0;
    player_airborne = ctx->player->jump || ctx->player->falling;
    nav_mode = allow_advanced && vertical_gap >= BOT_NAV_MIN_VERTICAL_GAP && vertical_gap <= BOT_NAV_MAX_VERTICAL_GAP;
    nav_target_x = (int)*ctx->bot_world_x_ref;
    should_hold_jump = false;
    has_headroom_current = true;
    if (!ctx->bot_in_air_flag)
        has_headroom_current = bot_nav_has_headroom_at_x((int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref);

    if (ctx->bot_in_air_flag
        && ctx->bot_jump_hold_ms_ref != JO_NULL
        && ctx->bot_jump_hold_target_ms_ref != JO_NULL
        && *ctx->bot_jump_hold_ms_ref < *ctx->bot_jump_hold_target_ms_ref)
    {
        should_hold_jump = true;
    }

    if (ctx->bot_ref->stun_timer == 0 && *ctx->bot_current_attack_ref == AiBotAttackNone)
    {
        if (nav_mode && !ctx->bot_in_air_flag)
        {
            if (distance_abs > BOT_JUMP_MAX_HORIZONTAL_GAP)
            {
                nav_target_x = ctx->player_world_x;
            }
            else if (!has_headroom_current)
            {
                nav_target_x = bot_nav_find_jump_target_x((int)*ctx->bot_world_x_ref,
                                                          (int)*ctx->bot_world_y_ref,
                                                          ctx->player_world_x);
            }
        }

        if (nav_mode)
        {
            if ((int)*ctx->bot_world_x_ref < nav_target_x - BOT_NAV_LANE_TOLERANCE)
                input.right_pressed = true;
            else if ((int)*ctx->bot_world_x_ref > nav_target_x + BOT_NAV_LANE_TOLERANCE)
                input.left_pressed = true;
            else if (distance_x > BOT_APPROACH_DISTANCE)
                input.right_pressed = true;
            else if (distance_x < -BOT_APPROACH_DISTANCE)
                input.left_pressed = true;
        }
        else if (distance_x > BOT_APPROACH_DISTANCE)
            input.right_pressed = true;
        else if (distance_x < -BOT_APPROACH_DISTANCE)
            input.left_pressed = true;

        if (ctx->bot_ref->can_jump && *ctx->bot_jump_cooldown_ref == 0 && !ctx->bot_in_air_flag)
        {
            int chance;
            int noise;
            int distance_bonus;

            if (vertical_gap >= BOT_JUMP_MIN_VERTICAL_GAP
                && vertical_gap <= BOT_JUMP_MAX_VERTICAL_GAP
                && distance_abs <= BOT_JUMP_MAX_HORIZONTAL_GAP)
            {
                if (nav_mode && ((int)*ctx->bot_world_x_ref < nav_target_x - BOT_NAV_LANE_TOLERANCE
                    || (int)*ctx->bot_world_x_ref > nav_target_x + BOT_NAV_LANE_TOLERANCE))
                {
                    *ctx->bot_jump_cooldown_ref = BOT_JUMP_RETRY_COOLDOWN_MAX;
                }
                else
                {
                if (!has_headroom_current)
                {
                    *ctx->bot_jump_cooldown_ref = BOT_JUMP_RETRY_COOLDOWN_MAX;
                }
                else
                {
                distance_bonus = (BOT_JUMP_MAX_HORIZONTAL_GAP - distance_abs) / BOT_JUMP_DISTANCE_BONUS_DIV;
                chance = BOT_JUMP_BASE_CHANCE_PROFILE + (vertical_gap / BOT_JUMP_VERTICAL_BONUS_DIV) + distance_bonus;
                if (chance > BOT_JUMP_CHANCE_MAX_PROFILE)
                    chance = BOT_JUMP_CHANCE_MAX_PROFILE;

                noise = JO_ABS((ctx->player_world_x * 3)
                    + (ctx->player_world_y * 5)
                    + ((int)*ctx->bot_world_x_ref * 7)
                    + ((int)*ctx->bot_world_y_ref * 11)
                    + (*ctx->bot_attack_step_ref * 13)) % 100;

                if (noise < chance)
                {
                    int vertical_hold_bonus = (vertical_gap * BOT_JUMP_HOLD_VERTICAL_BONUS_MAX_PROFILE) / BOT_JUMP_MAX_VERTICAL_GAP;
                    int distance_hold_bonus = ((BOT_JUMP_MAX_HORIZONTAL_GAP - distance_abs) * BOT_JUMP_HOLD_DISTANCE_BONUS_MAX_PROFILE) / BOT_JUMP_MAX_HORIZONTAL_GAP;
                    int hold_target_ms = BOT_JUMP_MIN_HOLD_MS_PROFILE + vertical_hold_bonus + distance_hold_bonus;

                    if (hold_target_ms > BOT_JUMP_MAX_HOLD_MS_PROFILE)
                        hold_target_ms = BOT_JUMP_MAX_HOLD_MS_PROFILE;
                    if (hold_target_ms < BOT_JUMP_MIN_HOLD_MS_PROFILE)
                        hold_target_ms = BOT_JUMP_MIN_HOLD_MS_PROFILE;

                    if (ctx->bot_jump_hold_target_ms_ref != JO_NULL)
                        *ctx->bot_jump_hold_target_ms_ref = hold_target_ms;

                    input.b_pressed = true;
                    input.b_down = true;
                }
                else
                {
                    *ctx->bot_jump_cooldown_ref = BOT_JUMP_RETRY_COOLDOWN_MIN
                        + (noise % (BOT_JUMP_RETRY_COOLDOWN_MAX - BOT_JUMP_RETRY_COOLDOWN_MIN + 1));
                }
                }
                }
            }
        }
    }

    if (should_hold_jump)
        input.b_down = true;

    control_handle_character_commands(ctx->bot_physics_ref,
                                      ctx->bot_ref,
                                      ctx->bot_jump_cooldown_ref,
                                      ctx->bot_jump_hold_ms_ref,
                                      ctx->bot_jump_cut_applied_ref,
                                      ctx->jump_sfx,
                                      ctx->jump_sfx_channel,
                                      ctx->spin_sfx,
                                      ctx->punch_sfx,
                                      ctx->kick_sfx,
                                      &input);

    if (allow_advanced
        && ctx->bot_in_air_flag
        && *ctx->bot_current_attack_ref == AiBotAttackNone
        && *ctx->bot_attack_cooldown_ref == 0
        && player_airborne
        && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref,
                                   (int)*ctx->bot_world_y_ref,
                                   ctx->bot_ref->flip,
                                   ctx->player_world_x,
                                   ctx->player_world_y,
                                   BOT_HIT_RANGE_KICK2))
    {
        int air_noise = JO_ABS((ctx->player_world_x * 17)
            + (ctx->player_world_y * 9)
            + ((int)*ctx->bot_world_x_ref * 5)
            + ((int)*ctx->bot_world_y_ref * 3)
            + (*ctx->bot_attack_step_ref * 11)) % 100;

        if (air_noise < BOT_AIR_KICK_TRIGGER_CHANCE)
        {
            ctx->start_attack(AiBotAttackKick2, false);
        }
    }

    if (*ctx->bot_current_attack_ref != AiBotAttackNone)
    {
        int attack_damage = 0;
        int attack_trigger = 0;
        int attack_range = BOT_HIT_RANGE_PUNCH1;
        float attack_knockback = ctx->bot_ref->knockback_punch1;
        int attack_stun = STUN_LIGHT_FRAMES;

        if (*ctx->bot_current_attack_ref == AiBotAttackPunch1)
        {
            attack_damage = 1;
            attack_trigger = 9;
            attack_range = BOT_HIT_RANGE_PUNCH1;
            attack_knockback = ctx->bot_ref->knockback_punch1;
            attack_stun = STUN_LIGHT_FRAMES;
        }
        else if (*ctx->bot_current_attack_ref == AiBotAttackPunch2)
        {
            attack_damage = 3;
            attack_trigger = 11;
            attack_range = BOT_HIT_RANGE_PUNCH2;
            attack_knockback = ctx->bot_ref->knockback_punch2;
            attack_stun = STUN_HEAVY_FRAMES;
        }
        else if (*ctx->bot_current_attack_ref == AiBotAttackKick1)
        {
            attack_damage = 2;
            attack_trigger = 10;
            attack_range = BOT_HIT_RANGE_KICK1;
            attack_knockback = ctx->bot_ref->knockback_kick1;
            attack_stun = STUN_LIGHT_FRAMES;
        }
        else if (*ctx->bot_current_attack_ref == AiBotAttackKick2)
        {
            attack_damage = 5;
            attack_trigger = 12;
            attack_range = BOT_HIT_RANGE_KICK2;
            attack_knockback = ctx->bot_ref->knockback_kick2;
            attack_stun = STUN_HEAVY_FRAMES;
        }

        if (!*ctx->bot_attack_damage_done_ref && *ctx->bot_attack_timer_ref == attack_trigger && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref, ctx->bot_ref->flip, ctx->player_world_x, ctx->player_world_y, attack_range))
        {
            *ctx->bot_attack_damage_done_ref = true;

            if (ctx->player->spin)
            {
                int defender_direction = ctx->bot_ref->flip ? -1 : 1;
                int attacker_direction = -defender_direction;

                ctx->apply_hit_effect_to_player(ctx->player, ctx->bot_ref->flip, attack_knockback * 2.0f, 0);
                *ctx->bot_speed_x_ref = (float)attacker_direction * attack_knockback;
                ctx->bot_ref->stun_timer = COUNTER_STUN_FRAMES;
                *ctx->bot_attack_cooldown_ref = COUNTER_STUN_FRAMES;
                *ctx->bot_current_attack_ref = AiBotAttackNone;
                *ctx->bot_attack_timer_ref = 0;
                *ctx->bot_combo_requested_ref = false;
                ctx->bot_ref->punch = false;
                ctx->bot_ref->punch2 = false;
                ctx->bot_ref->kick = false;
                ctx->bot_ref->kick2 = false;
                damage_fx_trigger_world(ctx->player_world_x, ctx->player_world_y);
                game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
            }
            else
            {
                ctx->player->life -= attack_damage;
                ctx->apply_hit_effect_to_player(ctx->player, ctx->bot_ref->flip, attack_knockback, attack_stun);
                damage_fx_trigger_world(ctx->player_world_x, ctx->player_world_y);

                if (*ctx->bot_current_attack_ref == AiBotAttackPunch2 || *ctx->bot_current_attack_ref == AiBotAttackKick2)
                    game_audio_play_sfx_next_channel(game_audio_get_damage_hi_sfx());
                else
                    game_audio_play_sfx_next_channel(game_audio_get_damage_low_sfx());

                if (ctx->player->life <= 0)
                {
                    ctx->player->life = 0;
                    *ctx->player_defeated = true;
                    ctx->player->walk = false;
                    ctx->player->run = 0;
                    ctx->player->punch = false;
                    ctx->player->kick = false;
                    ctx->player->punch2 = false;
                    ctx->player->kick2 = false;
                    ctx->player->spin = false;
                }
            }
        }

        --(*ctx->bot_attack_timer_ref);
        if (*ctx->bot_attack_timer_ref <= 0)
        {
            if (*ctx->bot_current_attack_ref == AiBotAttackPunch1 && *ctx->bot_combo_requested_ref && !ctx->bot_in_air_flag && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref, ctx->bot_ref->flip, ctx->player_world_x, ctx->player_world_y, BOT_COMBO_PUNCH2_RANGE))
            {
                ctx->start_attack(AiBotAttackPunch2, false);
            }
            else if (*ctx->bot_current_attack_ref == AiBotAttackKick1 && *ctx->bot_combo_requested_ref && !ctx->bot_in_air_flag && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref, ctx->bot_ref->flip, ctx->player_world_x, ctx->player_world_y, BOT_COMBO_KICK2_RANGE))
            {
                ctx->start_attack(AiBotAttackKick2, false);
            }
            else
            {
                int post_attack_cooldown = ATTACK_COOLDOWN_FRAMES;
                if (*ctx->bot_current_attack_ref == AiBotAttackPunch2)
                    post_attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
                else if (*ctx->bot_current_attack_ref == AiBotAttackKick2)
                    post_attack_cooldown = ATTACK_COOLDOWN_KICK2_FRAMES;

                *ctx->bot_current_attack_ref = AiBotAttackNone;
                *ctx->bot_attack_timer_ref = 0;
                *ctx->bot_combo_requested_ref = false;
                ctx->bot_ref->punch = false;
                ctx->bot_ref->punch2 = false;
                ctx->bot_ref->kick = false;
                ctx->bot_ref->kick2 = false;
                *ctx->bot_attack_cooldown_ref = post_attack_cooldown;
            }
        }
    }

    if (*ctx->bot_attack_cooldown_ref > 0)
    {
        --(*ctx->bot_attack_cooldown_ref);
    }
    else if (ctx->bot_ref->stun_timer == 0 && *ctx->bot_current_attack_ref == AiBotAttackNone && !ctx->bot_in_air_flag && ctx->is_close_enough(ctx->player_world_x, ctx->player_world_y, (int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref, BOT_ENGAGE_RANGE))
    {
        bool request_combo = ((*ctx->bot_attack_step_ref / 2) % 2) == 0;
        if (distance_abs <= BOT_PUNCH_START_RANGE)
            ctx->start_attack(AiBotAttackPunch1, request_combo);
        else
            ctx->start_attack(AiBotAttackKick1, request_combo);

        ++(*ctx->bot_attack_step_ref);
    }
}
