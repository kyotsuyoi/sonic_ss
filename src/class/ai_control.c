#include <jo/jo.h>
#include "ai_control.h"
#include "control.h"
#include "game_audio.h"
#include "damage_fx.h"
#include "game_constants.h"
#include "character_registry.h"
#include "jo_audio_ext/jo_map_ext.h"
#include <stdlib.h>

#define BOT_ENGAGE_RANGE 9
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
#define BOT_AIR_KICK_ANTICIPATION_FRAMES 10
#define BOT_AIR_KICK_ANTICIPATION_RANGE_BONUS 6
#define BOT_ATTACK_STYLE_LONG_CHANCE 50
#define BOT_ENGAGE_TOLERANCE_SHORT 2
#define BOT_ENGAGE_TOLERANCE_LONG 4
#define BOT_KNUCKLES_CHARGED_MAX_BUFFER 3
#define BOT_NAV_MIN_VERTICAL_GAP 24
#define BOT_NAV_MAX_VERTICAL_GAP 220
#define BOT_NAV_SCAN_STEP 12
#define BOT_NAV_SCAN_MAX_DISTANCE 168
#define BOT_NAV_PROBE_WIDTH 6
#define BOT_NAV_PROBE_HEIGHT 72
#define BOT_NAV_LANE_TOLERANCE 8
#define BOT_NAV_MAX_GROUND_DELTA 20

static ui_character_choice_t ai_choice_from_character_id(int character_id)
{
    switch (character_id)
    {
    case CHARACTER_ID_AMY:
        return UiCharacterAmy;
    case CHARACTER_ID_TAILS:
        return UiCharacterTails;
    case CHARACTER_ID_KNUCKLES:
        return UiCharacterKnuckles;
    case CHARACTER_ID_SHADOW:
        return UiCharacterShadow;
    case CHARACTER_ID_SONIC:
    default:
        return UiCharacterSonic;
    }
}

static int ai_bot_last_frame_for_attack(const character_t *attacker, int attack_kind)
{
    int cid = attacker->character_id;
    character_animation_profile_t profile = character_registry_get_animation_profile(ai_choice_from_character_id(cid));
    int punch_last = profile.punch_count - 1;
    int kick_last = profile.kick_count - 1;

    if (attack_kind == 0)
    {
        return JO_MAX(0, JO_MIN(punch_last, 2));
    }
    if (attack_kind == 1)
    {
        return JO_MAX(0, JO_MIN(punch_last, 3));
    }
    if (attack_kind == 2)
    {
        if (cid == CHARACTER_ID_KNUCKLES)
            return -1;
        return JO_MAX(0, JO_MIN(kick_last, 1));
    }

    if (cid == CHARACTER_ID_KNUCKLES && (attacker->charged_kick_active || attacker->charged_kick_phase > 0))
        return JO_MAX(0, JO_MIN(kick_last, 2));
    return JO_MAX(0, JO_MIN(kick_last, 2));
}

static bool ai_bot_attack_reached_hit_frame(const character_t *attacker, int attack_kind)
{
    int anim_id;
    int last_frame;

    if (attack_kind == 0 || attack_kind == 1)
        anim_id = attacker->punch_anim_id;
    else
        anim_id = attacker->kick_anim_id;

    if (anim_id < 0)
        return false;

    last_frame = ai_bot_last_frame_for_attack(attacker, attack_kind);
    if (last_frame < 0)
        return false;

    return jo_get_sprite_anim_frame(anim_id) >= last_frame;
}

static bool bot_nav_has_headroom_at_x(int world_x, int world_y)
{
    int attr;
    int probe_x = world_x + CHARACTER_WIDTH_2 - (BOT_NAV_PROBE_WIDTH / 2);
    int probe_y = world_y - BOT_NAV_PROBE_HEIGHT;

    if (probe_x < 0 || probe_y < 0)
        return false;

    attr = jo_map_hitbox_detection_custom_boundaries_ext(
        WORLD_MAP_ID,
        probe_x,
        probe_y,
        BOT_NAV_PROBE_WIDTH,
        BOT_NAV_PROBE_HEIGHT);

    return attr != MAP_TILE_BLOCK_ATTR;
}

static bool bot_nav_is_ground_reachable_at_x(int candidate_x, int bot_world_y)
{
    int attr;
    int foot_probe_x = candidate_x + 1;
    int foot_probe_y = bot_world_y + CHARACTER_HEIGHT - 1;
    int dist = jo_map_per_pixel_vertical_collision_ext(
        WORLD_MAP_ID,
        candidate_x + CHARACTER_WIDTH_2,
        bot_world_y + CHARACTER_HEIGHT,
        JO_NULL);

    if (candidate_x < 0 || bot_world_y < 0)
        return false;

    attr = jo_map_hitbox_detection_custom_boundaries_ext(
        WORLD_MAP_ID,
        foot_probe_x,
        foot_probe_y,
        CHARACTER_WIDTH - 2,
        2);

    if (dist == JO_MAP_NO_COLLISION)
        return false;
    if (attr != MAP_TILE_BLOCK_ATTR && attr != MAP_TILE_PLATFORM_ATTR)
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

static bool ai_attack_likely_in_range_soon(ai_bot_context_t *ctx, int range, int frames_ahead)
{
    int predicted_bot_x;
    int predicted_player_x;

    if (ctx == JO_NULL)
        return false;

    predicted_bot_x = (int)*ctx->bot_world_x_ref + (int)(*ctx->bot_speed_x_ref * (float)frames_ahead);
    predicted_player_x = ctx->player_world_x;
    if (ctx->player_physics != JO_NULL)
        predicted_player_x += (int)(ctx->player_physics->speed * (float)frames_ahead);

    return ctx->is_attack_in_range(predicted_bot_x,
                                   (int)*ctx->bot_world_y_ref,
                                   ctx->bot_ref->flip,
                                   predicted_player_x,
                                   ctx->player_world_y,
                                   range);
}

void ai_control_handle_bot_commands(ai_bot_context_t *ctx)
{
    character_combat_profile_t combat_profile;
    int distance_x;
    int distance_abs;
    int vertical_gap;
    bool allow_advanced;
    bool player_airborne;
    bool has_headroom_current;
    int nav_target_x;
    bool nav_mode;
    bool should_hold_jump;
    int short_attack_range;
    int long_attack_range;
    int desired_min_distance;
    int desired_max_distance;
    bool prefer_long_attack;
    int style_noise;
    control_input_t input = {0};

    combat_profile = character_registry_get_combat_profile_by_character_id(ctx->bot_ref->character_id);

    if (*ctx->player_defeated || ctx->bot_defeated_flag)
        return;

    if (*ctx->bot_jump_cooldown_ref > 0)
        --(*ctx->bot_jump_cooldown_ref);

    if (ctx->bot_ref->stun_timer > 0)
    {
        --ctx->bot_ref->stun_timer;
        /* While stunned, skip AI input so hit knockback is not immediately
           canceled by movement/friction logic in control handling. */
        return;
    }

    distance_x = ctx->player_world_x - (int)*ctx->bot_world_x_ref;
    distance_abs = JO_ABS(distance_x);
    short_attack_range = combat_profile.hit_range_punch1;
    long_attack_range = combat_profile.hit_range_kick2;
    if (ctx->bot_ref->charged_kick_enabled)
        long_attack_range += combat_profile.charged_kick_range_bonus;

    /* Prefer punches (combo). If Knuckles can charged-kick, only try long attack ~20% of the time. */
    int long_chance = BOT_ATTACK_STYLE_LONG_CHANCE;
    if (ctx->bot_ref->charged_kick_enabled)
        long_chance = 20; /* attempt long attack ~20% for charged-kick characters */

    if (long_chance <= 0)
        prefer_long_attack = false;
    else if (long_chance >= 100)
        prefer_long_attack = true;
    else
        prefer_long_attack = (rand() % 100) < long_chance;

    if (prefer_long_attack)
    {
        desired_min_distance = long_attack_range - BOT_ENGAGE_TOLERANCE_LONG;
        desired_max_distance = long_attack_range + BOT_ENGAGE_TOLERANCE_LONG;
        if (ctx->bot_ref->charged_kick_enabled)
        {
            desired_min_distance = long_attack_range;
            desired_max_distance = long_attack_range + BOT_KNUCKLES_CHARGED_MAX_BUFFER;
        }
    }
    else
    {
        desired_min_distance = short_attack_range - BOT_ENGAGE_TOLERANCE_SHORT;
        desired_max_distance = short_attack_range + BOT_ENGAGE_TOLERANCE_SHORT;
    }
    if (desired_min_distance < 1)
        desired_min_distance = 1;
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
            if (distance_abs > desired_max_distance)
            {
                if ((int)*ctx->bot_world_x_ref < nav_target_x - BOT_NAV_LANE_TOLERANCE)
                    input.right_pressed = true;
                else if ((int)*ctx->bot_world_x_ref > nav_target_x + BOT_NAV_LANE_TOLERANCE)
                    input.left_pressed = true;
                else if (distance_x > 0)
                    input.right_pressed = true;
                else if (distance_x < 0)
                    input.left_pressed = true;
            }
            else if (distance_abs < desired_min_distance)
            {
                if (distance_x > 0)
                    input.left_pressed = true;
                else if (distance_x < 0)
                    input.right_pressed = true;
            }
        }
        else
        {
            if (distance_abs > desired_max_distance)
            {
                if (distance_x > 0)
                    input.right_pressed = true;
                else if (distance_x < 0)
                    input.left_pressed = true;
            }
            else if (distance_abs < desired_min_distance)
            {
                if (distance_x > 0)
                    input.left_pressed = true;
                else if (distance_x < 0)
                    input.right_pressed = true;
            }
        }

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
                else if (!has_headroom_current)
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
        && ai_attack_likely_in_range_soon(ctx,
                                   combat_profile.hit_range_kick2 + BOT_AIR_KICK_ANTICIPATION_RANGE_BONUS,
                                   BOT_AIR_KICK_ANTICIPATION_FRAMES))
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
        if (ctx->bot_ref->charged_kick_enabled && *ctx->bot_current_attack_ref == AiBotAttackKick1)
        {
            bool desired_flip = (ctx->player_world_x < (int)*ctx->bot_world_x_ref);
            ctx->bot_ref->flip = desired_flip;

            /* Freeze drift while charging so release doesn't slide past target. */
            *ctx->bot_speed_x_ref *= 0.55f;
            if (JO_ABS(*ctx->bot_speed_x_ref) < 0.10f)
                *ctx->bot_speed_x_ref = 0.0f;
        }

        bool knuckles_charge_attack = (ctx->bot_ref->charged_kick_enabled
            && *ctx->bot_current_attack_ref == AiBotAttackKick2);
        int attack_damage = 0;
        int attack_kind = -1;
        int attack_range = combat_profile.hit_range_punch1;
        float attack_knockback = ctx->bot_ref->knockback_punch1;
        int attack_stun = STUN_LIGHT_FRAMES;

        if (*ctx->bot_current_attack_ref == AiBotAttackPunch1)
        {
            attack_damage = combat_profile.damage_punch1;
            attack_kind = 0;
            attack_range = combat_profile.hit_range_punch1;
            attack_knockback = ctx->bot_ref->knockback_punch1;
            attack_stun = STUN_LIGHT_FRAMES;
        }
        else if (*ctx->bot_current_attack_ref == AiBotAttackPunch2)
        {
            attack_damage = combat_profile.damage_punch2;
            attack_kind = 1;
            attack_range = combat_profile.hit_range_punch2;
            attack_knockback = ctx->bot_ref->knockback_punch2;
            attack_stun = STUN_HEAVY_FRAMES;
        }
        else if (*ctx->bot_current_attack_ref == AiBotAttackKick1)
        {
            attack_damage = ctx->bot_ref->charged_kick_enabled ? 0 : combat_profile.damage_kick1;
            attack_kind = ctx->bot_ref->charged_kick_enabled ? -1 : 2;
            attack_range = combat_profile.hit_range_kick1;
            attack_knockback = ctx->bot_ref->knockback_kick1;
            attack_stun = STUN_LIGHT_FRAMES;
        }
        else if (*ctx->bot_current_attack_ref == AiBotAttackKick2)
        {
            attack_damage = knuckles_charge_attack ? combat_profile.charged_kick_damage : combat_profile.damage_kick2;
            attack_kind = 3;
            attack_range = knuckles_charge_attack
                ? (combat_profile.hit_range_kick2 + combat_profile.charged_kick_range_bonus)
                : combat_profile.hit_range_kick2;
            attack_knockback = knuckles_charge_attack
                ? (ctx->bot_ref->knockback_kick2 * combat_profile.charged_kick_knockback_mult)
                : ctx->bot_ref->knockback_kick2;
            attack_stun = knuckles_charge_attack
                ? (STUN_HEAVY_FRAMES + combat_profile.charged_kick_stun_bonus)
                : STUN_HEAVY_FRAMES;
        }

        if (!*ctx->bot_attack_damage_done_ref
            && attack_kind >= 0
            && ai_bot_attack_reached_hit_frame(ctx->bot_ref, attack_kind)
            && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref,
                                       (int)*ctx->bot_world_y_ref,
                                       ctx->bot_ref->flip,
                                       ctx->player_world_x,
                                       ctx->player_world_y,
                                       attack_range))
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
            if (*ctx->bot_current_attack_ref == AiBotAttackKick1 && ctx->bot_ref->charged_kick_enabled)
            {
                bool desired_flip = (ctx->player_world_x < (int)*ctx->bot_world_x_ref);
                int charged_range = combat_profile.hit_range_kick2 + combat_profile.charged_kick_range_bonus;
                bool can_hit_current;
                bool can_hit_flipped;

                can_hit_current = ctx->is_attack_in_range((int)*ctx->bot_world_x_ref,
                                                          (int)*ctx->bot_world_y_ref,
                                                          ctx->bot_ref->flip,
                                                          ctx->player_world_x,
                                                          ctx->player_world_y,
                                                          charged_range);

                can_hit_flipped = ctx->is_attack_in_range((int)*ctx->bot_world_x_ref,
                                                          (int)*ctx->bot_world_y_ref,
                                                          desired_flip,
                                                          ctx->player_world_x,
                                                          ctx->player_world_y,
                                                          charged_range);

                if (can_hit_flipped)
                    ctx->bot_ref->flip = desired_flip;

                if (can_hit_current || can_hit_flipped)
                {
                    ctx->start_attack(AiBotAttackKick2, false);
                }
                else
                {
                    /* Out of range after charge: cancel by releasing attack. */
                    *ctx->bot_current_attack_ref = AiBotAttackNone;
                    *ctx->bot_attack_timer_ref = 0;
                    *ctx->bot_combo_requested_ref = false;
                    ctx->bot_ref->kick = false;
                    ctx->bot_ref->kick2 = false;
                    *ctx->bot_attack_cooldown_ref = ATTACK_COOLDOWN_FRAMES;
                }
            }
            else
            {
            if (*ctx->bot_current_attack_ref == AiBotAttackPunch1 && *ctx->bot_combo_requested_ref && !ctx->bot_in_air_flag && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref, ctx->bot_ref->flip, ctx->player_world_x, ctx->player_world_y, combat_profile.hit_range_punch2))
            {
                ctx->start_attack(AiBotAttackPunch2, false);
            }
            else if (*ctx->bot_current_attack_ref == AiBotAttackKick1
                     && *ctx->bot_combo_requested_ref
                     && !ctx->bot_ref->charged_kick_enabled
                     && !ctx->bot_in_air_flag
                     && ctx->is_attack_in_range((int)*ctx->bot_world_x_ref, (int)*ctx->bot_world_y_ref, ctx->bot_ref->flip, ctx->player_world_x, ctx->player_world_y, combat_profile.hit_range_kick2))
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
    }

    if (*ctx->bot_attack_cooldown_ref > 0)
    {
        --(*ctx->bot_attack_cooldown_ref);
    }
    else if (ctx->bot_ref->stun_timer == 0 && *ctx->bot_current_attack_ref == AiBotAttackNone && !ctx->bot_in_air_flag)
    {
        bool short_in_range = ctx->is_attack_in_range((int)*ctx->bot_world_x_ref,
                                                      (int)*ctx->bot_world_y_ref,
                                                      ctx->bot_ref->flip,
                                                      ctx->player_world_x,
                                                      ctx->player_world_y,
                                                      short_attack_range);
        bool long_in_range = ctx->is_attack_in_range((int)*ctx->bot_world_x_ref,
                                                     (int)*ctx->bot_world_y_ref,
                                                     ctx->bot_ref->flip,
                                                     ctx->player_world_x,
                                                     ctx->player_world_y,
                                                     long_attack_range);
        bool request_combo = ((*ctx->bot_attack_step_ref / 2) % 2) == 0;

        if (short_in_range && long_in_range)
        {
            /* Prefer long attack but allow randomness to occasionally choose punch. */
            if (prefer_long_attack && (rand() % 100) < 70)
                ctx->start_attack(AiBotAttackKick1, request_combo);
            else
                ctx->start_attack(AiBotAttackPunch1, request_combo);
            ++(*ctx->bot_attack_step_ref);
            return;
        }
        if (long_in_range)
        {
            /* If punch also in range that case handled earlier. Here long only.
               Prefer to approach and try to get punch range; only kick ~20% of the time. */
            if ((rand() % 100) < 1)
            {
                ctx->start_attack(AiBotAttackKick1, request_combo);
                ++(*ctx->bot_attack_step_ref);
                return;
            }
            else
            {
                /* Move toward player to prefer punch/combo next frame. */
                if (distance_x > 0)
                    input.right_pressed = true;
                else if (distance_x < 0)
                    input.left_pressed = true;
            }
        }
        if (short_in_range)
        {
            ctx->start_attack(AiBotAttackPunch1, request_combo);
            ++(*ctx->bot_attack_step_ref);
            return;
        }

        if (ctx->is_close_enough(ctx->player_world_x,
                                 ctx->player_world_y,
                                 (int)*ctx->bot_world_x_ref,
                                 (int)*ctx->bot_world_y_ref,
                                 BOT_ENGAGE_RANGE))
            ++(*ctx->bot_attack_step_ref);
    }
}
