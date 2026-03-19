#include <jo/jo.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include "bot.h"

/* Ensure rand() is declared regardless of toolchain header behavior. */
extern int rand(void);
#include "player.h"
#include "game_constants.h"
#include "world_collision.h"
#include "character/sonic.h"
#include "character/amy.h"
#include "world_map.h"
#include "world_physics.h"
#include "character_registry.h"
#include "jo_ext/jo_map_ext.h"
#include "debug.h"
#include "game_audio.h"
#include "damage_fx.h"
#include "game_loop.h"

/* This new bot system replaces legacy bot.c/ai_control.c for Sonic/Amy only.
   Other characters are intentionally disabled in this module. */

#define BOT_DEFAULT_STEP_X 64
#define BOT_RUN_SPEED 6.0f
#define BOT_WALK_SPEED 2.0f
#define BOT_ATTACK_DISTANCE 18
#define BOT_ATTACK_COOLDOWN_FRAMES 16

#define BOT_ENGAGE_RANGE 9
#define BOT_APPROACH_DISTANCE 16
#define BOT_JUMP_MIN_VERTICAL_GAP 18
#define BOT_JUMP_MAX_VERTICAL_GAP 180
#define BOT_JUMP_MAX_HORIZONTAL_GAP 120
#define BOT_JUMP_VERTICAL_BONUS_DIV 2
#define BOT_JUMP_DISTANCE_BONUS_DIV 3
#define BOT_JUMP_BASE_CHANCE_PROFILE 24
#define BOT_JUMP_CHANCE_MAX_PROFILE 85
#define BOT_JUMP_MIN_HOLD_MS_PROFILE 34
#define BOT_JUMP_MAX_HOLD_MS_PROFILE 170
#define BOT_JUMP_HOLD_VERTICAL_BONUS_MAX_PROFILE 96
#define BOT_JUMP_HOLD_DISTANCE_BONUS_MAX_PROFILE 40
#define BOT_JUMP_RETRY_COOLDOWN_MIN 8
#define BOT_JUMP_RETRY_COOLDOWN_MAX 18
#define BOT_AIR_KICK_TRIGGER_CHANCE 48
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
#define BOT_JUMP_REACTION_DELAY_FRAMES 8

struct bot_instance
{
    character_t character;
    jo_sidescroller_physics_params physics;
    bool initialized;
    bool active;
    bool defeated;
    int jump_cooldown;
    int jump_hold_ms;
    int jump_hold_target_ms;
    int jump_reaction_timer;
    bool prev_target_airborne;
    bool jump_cut_applied;
    int airborne_time_ms;
    float world_x;
    float world_y;
    bool prev_punch;
    bool prev_punch2;
    bool prev_kick;
    bool prev_kick2;
    ui_character_choice_t choice;
    int group;
};

static bot_instance_t g_bot_instances[BOT_MAX_DEFAULT_COUNT];
static int g_bot_active_count = 0;

static ui_character_choice_t bot_normalize_choice(ui_character_choice_t c)
{
    if (c == UiCharacterSonic || c == UiCharacterAmy)
        return c;
    return UiCharacterSonic;
}

static bool bot_is_supported_choice(ui_character_choice_t c)
{
    return (c == UiCharacterSonic || c == UiCharacterAmy);
}

static float bot_calc_speed_for_distance(float distance)
{
    if (distance > BOT_ATTACK_DISTANCE * 1.4f)
        return BOT_RUN_SPEED;
    return BOT_WALK_SPEED;
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
    int dist;

    if (candidate_x < 0 || bot_world_y < 0)
        return false;

    dist = jo_map_per_pixel_vertical_collision_ext(
        WORLD_MAP_ID,
        candidate_x + CHARACTER_WIDTH_2,
        bot_world_y + CHARACTER_HEIGHT,
        JO_NULL);

    if (dist == JO_MAP_NO_COLLISION)
        return false;

    attr = jo_map_hitbox_detection_custom_boundaries_ext(
        WORLD_MAP_ID,
        candidate_x + 1,
        bot_world_y + CHARACTER_HEIGHT - 1,
        CHARACTER_WIDTH - 2,
        2);

    if (attr != MAP_TILE_BLOCK_ATTR && attr != MAP_TILE_PLATFORM_ATTR)
        return false;

    return dist >= -BOT_NAV_MAX_GROUND_DELTA && dist <= BOT_NAV_MAX_GROUND_DELTA;
}

static int bot_nav_find_jump_target_x(int bot_world_x, int bot_world_y, int player_world_x)
{
    bool found = false;
    int best_x = bot_world_x;
    int best_score = -2147483647;
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
        return bot_world_x + ((player_world_x >= bot_world_x) ? BOT_NAV_SCAN_MAX_DISTANCE / 2 : -BOT_NAV_SCAN_MAX_DISTANCE / 2);

    return best_x;
}

static void bot_initialize_instance(bot_instance_t *inst,
                                    ui_character_choice_t selected_bot_character,
                                    int group,
                                    int start_world_x,
                                    int start_world_y)
{
    if (inst == NULL)
        return;

    memset(&inst->character, 0, sizeof(inst->character));

    /* Clear sprite handles to invalid state; 0 is a valid sprite id in JO engine. */
    inst->character.wram_sprite_id = -1;
    inst->character.defeated_sprite_id = -1;
    inst->character.stand_sprite_id = -1;
    inst->character.walking_anim_id = -1;
    inst->character.running1_anim_id = -1;
    inst->character.running2_anim_id = -1;
    inst->character.punch_anim_id = -1;
    inst->character.kick_anim_id = -1;
    inst->character.spin_sprite_id = -1;
    inst->character.jump_sprite_id = -1;
    inst->character.damage_sprite_id = -1;

    world_physics_init_character(&inst->physics);

    inst->physics.speed = 0.0f;
    inst->physics.speed_y = 0.0f;
    inst->physics.is_in_air = false;

    inst->initialized = true;
    inst->active = true;
    inst->defeated = false;
    inst->jump_cooldown = 0;
    inst->jump_hold_ms = 0;
    inst->jump_hold_target_ms = 0;
    inst->jump_reaction_timer = 0;
    inst->prev_target_airborne = false;
    inst->jump_cut_applied = false;
    inst->airborne_time_ms = 0;
    inst->world_x = (float)start_world_x;
    inst->world_y = (float)start_world_y;
    inst->prev_punch = false;
    inst->prev_punch2 = false;
    inst->prev_kick = false;
    inst->prev_kick2 = false;
    inst->choice = bot_normalize_choice(selected_bot_character);
    inst->group = group;

    character_registry_apply_combat_profile(&inst->character, inst->choice);
    inst->character.group = group;

    character_handlers_t handlers = character_registry_get(inst->choice);
    if (inst->choice == UiCharacterSonic)
        sonic_set_current(&inst->character, &inst->physics);
    else if (inst->choice == UiCharacterAmy)
        amy_set_current(&inst->character, &inst->physics);

    if (handlers.load != JO_NULL)
        handlers.load();

    if (inst->choice == UiCharacterSonic)
        sonic_set_current(&player, NULL);
    else if (inst->choice == UiCharacterAmy)
        amy_set_current(&player, NULL);

    // Ensure invalid initial WRAM sprite for instance so per-instance sprite is created.
    inst->character.wram_sprite_id = -1;
    inst->character.defeated_sprite_id = -1;
}


static void bot_apply_simple_ai(bot_instance_t *inst,
                                character_t *target_player,
                                int target_world_x,
                                int target_world_y,
                                bool target_defeated)
{
    if (inst == NULL)
        return;

    bool left_pressed = false;
    bool right_pressed = false;
    bool spin_pressed = false;
    bool b_pressed = false;
    bool b_down = false;
    bool a_down = false;
    bool c_down = false;
    bool c_hold = false;

    character_action_status_t status = character_update_cooldowns(&inst->character, &inst->jump_cooldown);

    if (status != CharacterActionAllowed)
    {
        if (status == CharacterActionBlockedDefeated)
        {
            inst->active = true; // keep alive to draw defeated state
            player_apply_defeated_state(&inst->character, &inst->physics, &inst->defeated);
            // do not set active=false to allow draw of defeated sprite
            return;
        }

        if (status == CharacterActionBlockedStun)
        {
            player_apply_defeated_state(&inst->character, &inst->physics, JO_NULL);
            return;
        }

        // If blocked only by jump/attack cooldown, keep AI decision path (it may choose strategy instead of direct attack)
    }

    if (inst->defeated || inst->character.life <= 0)
    {
        inst->active = true; // manter ativo para desenho do defeated
        player_apply_defeated_state(&inst->character, &inst->physics, &inst->defeated);
        return;
    }
    else if (inst->character.stun_timer > 0)
    {
        inst->character.walk = false;
        inst->character.run = 0;
        inst->character.spin = false;
        jo_physics_apply_friction(&inst->physics);
        return;
    }
    else if (target_player != JO_NULL && !target_defeated)
    {
        int distance_x = target_world_x - (int)inst->world_x;
        int distance_abs = JO_ABS(distance_x);
        int vertical_gap = (int)inst->world_y - target_world_y;
        bool on_ground = !inst->physics.is_in_air;

        character_combat_profile_t combat_profile = character_registry_get_combat_profile_by_character_id(inst->character.character_id);
        int punch1_range = combat_profile.hit_range_punch1;
        int punch2_range = combat_profile.hit_range_punch2;
        int kick_range = combat_profile.hit_range_kick2;
        if (combat_profile.charged_kick_enabled)
            kick_range += combat_profile.charged_kick_range_bonus;

        bool prefer_long_attack = ((rand() % 100) < BOT_ATTACK_STYLE_LONG_CHANCE);

        int chosen_attack_range;
        if (distance_abs <= punch2_range)
        {
            chosen_attack_range = punch2_range;
            prefer_long_attack = false;
        }
        else if (distance_abs <= kick_range)
        {
            chosen_attack_range = kick_range;
            prefer_long_attack = true;
        }
        else
        {
            chosen_attack_range = prefer_long_attack ? kick_range : punch1_range;
        }

        int desired_min_distance = chosen_attack_range - (prefer_long_attack ? BOT_ENGAGE_TOLERANCE_LONG : BOT_ENGAGE_TOLERANCE_SHORT);
        int desired_max_distance = chosen_attack_range + (prefer_long_attack ? BOT_ENGAGE_TOLERANCE_LONG : BOT_ENGAGE_TOLERANCE_SHORT);
        if (desired_min_distance < 1)
            desired_min_distance = 1;

        // Guarantee safe attack distance even after tactics.
        int fallback_safe_distance = chosen_attack_range;
        (void)fallback_safe_distance; // keep compiler happy when unused

        int desired_dir = (distance_x >= 0) ? 1 : -1;
        inst->character.flip = (desired_dir < 0);

        if (distance_abs > desired_max_distance)
        {
            if (desired_dir < 0)
                left_pressed = true;
            else
                right_pressed = true;
        }
        else if (distance_abs < desired_min_distance)
        {
            // Back off to safe distance
            if (desired_dir < 0)
                right_pressed = true;
            else
                left_pressed = true;
        }
        else
        {
            left_pressed = false;
            right_pressed = false;
        }

        bool target_airborne = (target_player->jump || target_player->falling);

        if (target_airborne && !inst->prev_target_airborne)
            inst->jump_reaction_timer = BOT_JUMP_REACTION_DELAY_FRAMES;

        if (inst->jump_reaction_timer > 0)
            inst->jump_reaction_timer--;

        inst->prev_target_airborne = target_airborne;

        // Jump logic: only jump para aerial opportunities, not ground repos.
        if (inst->character.can_jump && inst->jump_cooldown == 0 && on_ground && distance_abs <= BOT_JUMP_MAX_HORIZONTAL_GAP)
        {
            bool vertical_target = (vertical_gap >= BOT_JUMP_MIN_VERTICAL_GAP && vertical_gap <= BOT_JUMP_MAX_VERTICAL_GAP);
            bool should_jump = false;

            if (target_airborne && inst->jump_reaction_timer <= 0 && distance_abs <= BOT_JUMP_MAX_HORIZONTAL_GAP)
                should_jump = true;

            if (!should_jump && vertical_target && distance_abs <= BOT_JUMP_MAX_HORIZONTAL_GAP)
                should_jump = true;

            if (should_jump)
            {
                b_pressed = true;
                b_down = true;

                if (target_airborne)
                {
                    float height_factor = (float)vertical_gap / (float)BOT_JUMP_MAX_VERTICAL_GAP;
                    if (height_factor < 0.0f)
                        height_factor = 0.0f;
                    if (height_factor > 1.0f)
                        height_factor = 1.0f;

                    int hold_target_ms = BOT_JUMP_MIN_HOLD_MS_PROFILE + (int)((BOT_JUMP_MAX_HOLD_MS_PROFILE - BOT_JUMP_MIN_HOLD_MS_PROFILE) * height_factor);
                    if (hold_target_ms < BOT_JUMP_MIN_HOLD_MS_PROFILE)
                        hold_target_ms = BOT_JUMP_MIN_HOLD_MS_PROFILE;
                    if (hold_target_ms > BOT_JUMP_MAX_HOLD_MS_PROFILE)
                        hold_target_ms = BOT_JUMP_MAX_HOLD_MS_PROFILE;

                    inst->jump_hold_target_ms = hold_target_ms;
                }
                else
                {
                    int vertical_hold_bonus = JO_ABS(vertical_gap) > 0 ? (JO_ABS(vertical_gap) * BOT_JUMP_HOLD_VERTICAL_BONUS_MAX_PROFILE) / BOT_JUMP_MAX_VERTICAL_GAP : 0;
                    int distance_hold_bonus = ((BOT_JUMP_MAX_HORIZONTAL_GAP - distance_abs) * BOT_JUMP_HOLD_DISTANCE_BONUS_MAX_PROFILE) / BOT_JUMP_MAX_HORIZONTAL_GAP;
                    int hold_target_ms = BOT_JUMP_MIN_HOLD_MS_PROFILE + vertical_hold_bonus + distance_hold_bonus;
                    if (hold_target_ms > BOT_JUMP_MAX_HOLD_MS_PROFILE)
                        hold_target_ms = BOT_JUMP_MAX_HOLD_MS_PROFILE;
                    if (hold_target_ms < BOT_JUMP_MIN_HOLD_MS_PROFILE)
                        hold_target_ms = BOT_JUMP_MIN_HOLD_MS_PROFILE;
                    inst->jump_hold_target_ms = hold_target_ms;
                }
            }
        }

        // During airtime, hold jump until target or max is reached
        if (!on_ground)
        {
            if (inst->jump_hold_ms < inst->jump_hold_target_ms)
                b_down = true;
        }

        if (!on_ground && inst->jump_hold_ms > 0 && inst->jump_hold_ms < inst->jump_hold_target_ms)
            b_down = true;

        bool in_punch1_range = (distance_abs <= punch1_range);
        bool in_punch2_range = (distance_abs <= punch2_range);
        bool in_kick_range = (distance_abs <= kick_range);
        bool in_engage_range = (distance_abs >= desired_min_distance && distance_abs <= desired_max_distance);
        bool can_ground_attack = on_ground;

        if (inst->character.attack_cooldown == 0 && !inst->character.punch2 && !inst->character.kick2)
        {
            if (can_ground_attack && !inst->character.punch && !inst->character.kick)
            {
                if (in_punch2_range)
                    a_down = true; // use punch when possible
                else if (in_kick_range)
                    c_down = true; // use range kick first if far
                else if (in_engage_range)
                {
                    if (prefer_long_attack)
                        c_down = true;
                    else
                        a_down = true;
                }
            }

            if (can_ground_attack && inst->character.punch && !inst->character.punch2)
                a_down = true; // combo continuation
            if (can_ground_attack && inst->character.kick && !inst->character.kick2)
                c_down = true; // combo continuation
        }

        // Air kick attempt similar ao controle antigo
        if (!on_ground && inst->character.attack_cooldown == 0 && distance_abs <= (kick_range + BOT_AIR_KICK_ANTICIPATION_RANGE_BONUS))
        {
            int air_noise = JO_ABS((target_world_x * 17)
                + (target_world_y * 9)
                + ((int)inst->world_x * 5)
                + ((int)inst->world_y * 3)
                + (inst->jump_cooldown * 11)) % 100;
            if (air_noise < BOT_AIR_KICK_TRIGGER_CHANCE)
                c_down = true;
        }
    }

    player_handle_command_inputs(&inst->physics,
                                  &inst->character,
                                  &inst->jump_cooldown,
                                  &inst->jump_hold_ms,
                                  &inst->jump_cut_applied,
                                  JO_NULL,
                                  0,
                                  JO_NULL,
                                  JO_NULL,
                                  JO_NULL,
                                  left_pressed,
                                  right_pressed,
                                  spin_pressed,
                                  b_pressed,
                                  b_down,
                                  a_down,
                                  c_down,
                                  c_hold);
}

void bot_instance_init(bot_instance_t *instance,
                       int selected_player_character,
                       int selected_bot_character,
                       int selected_bot_group,
                       int player_world_x,
                       int player_world_y)
{
    if (instance == NULL)
        return;

    ui_character_choice_t normalized = bot_normalize_choice((ui_character_choice_t)selected_bot_character);
    bot_initialize_instance(instance,
                            normalized,
                            selected_bot_group,
                            player_world_x + BOT_DEFAULT_STEP_X,
                            player_world_y);
}

void bot_instance_unload(bot_instance_t *instance)
{
    if (instance == NULL)
        return;

    instance->active = false;
    instance->defeated = true;
    instance->character.life = 0;
    instance->character.walk = false;
    instance->character.run = 0;

    // We intentionally do not unload global character assets here to preserve shared view states.
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
    if (instance == NULL || !instance->active)
        return;

    if (instance->defeated || instance->character.life <= 0)
    {
        instance->active = true;
        player_apply_defeated_state(&instance->character, &instance->physics, &instance->defeated);
        // keep going to player_runtime_update() so collision + ground handling applies exactly like player path
    }

    int target_world_x = 0;
    int target_world_y = 0;
    character_t *target_ref = JO_NULL;
    bool target_dead = true;
    jo_sidescroller_physics_params *target_physics = JO_NULL;

    if (player_ref != JO_NULL && player_physics != JO_NULL && !(player_defeated != JO_NULL && *player_defeated))
    {
        target_ref = player_ref;
        target_physics = player_physics;
        target_world_x = map_pos_x + player_ref->x;
        target_world_y = map_pos_y + player_ref->y;
        target_dead = false;
    }

    if (versus_mode && player2_ref != JO_NULL && player2_physics != JO_NULL && !(player2_defeated != JO_NULL && *player2_defeated))
    {
        int player2_world_x = map_pos_x + player2_ref->x;
        int player2_world_y = map_pos_y + player2_ref->y;
        float dist1 = (target_ref == JO_NULL) ? FLT_MAX : (float)JO_ABS((int)instance->world_x - target_world_x);
        float dist2 = (float)JO_ABS((int)instance->world_x - player2_world_x);
        if (dist2 < dist1 || target_ref == JO_NULL)
        {
            target_ref = player2_ref;
            target_physics = player2_physics;
            target_world_x = player2_world_x;
            target_world_y = player2_world_y;
            target_dead = false;
        }
    }

    bot_apply_simple_ai(instance, target_ref, target_world_x, target_world_y, target_dead);

    instance->character.x = (int)instance->world_x - map_pos_x;
    instance->character.y = (int)instance->world_y - map_pos_y;

    int local_map_x = map_pos_x;
    int local_map_y = map_pos_y;
    player_runtime_update(&instance->character,
                          &instance->physics,
                          &instance->world_x,
                          &instance->world_y,
                          &instance->initialized,
                          &instance->defeated,
                          &instance->jump_cooldown,
                          &instance->jump_hold_ms,
                          &instance->jump_cut_applied,
                          &instance->airborne_time_ms,
                          0,
                          &local_map_x,
                          &local_map_y);

    int bot_world_x = map_pos_x + instance->character.x;
    int bot_world_y = map_pos_y + instance->character.y;

    if (!instance->defeated && instance->character.life > 0)
    {
        if (player_ref != JO_NULL && player_physics != JO_NULL)
        {
            game_loop_process_attack(&instance->character,
                                     &instance->physics,
                                     player_ref,
                                     player_physics,
                                     player_defeated,
                                     bot_world_x,
                                     bot_world_y,
                                     map_pos_x + player_ref->x,
                                     map_pos_y + player_ref->y,
                                     &instance->prev_punch,
                                     &instance->prev_punch2,
                                     &instance->prev_kick,
                                     &instance->prev_kick2);

            game_loop_process_attack(player_ref,
                                     player_physics,
                                     &instance->character,
                                     &instance->physics,
                                     &instance->defeated,
                                     map_pos_x + player_ref->x,
                                     map_pos_y + player_ref->y,
                                     bot_world_x,
                                     bot_world_y,
                                     JO_NULL,
                                     JO_NULL,
                                     JO_NULL,
                                     JO_NULL);
        }

        if (versus_mode && player2_ref != JO_NULL && player2_physics != JO_NULL)
        {
            game_loop_process_attack(&instance->character,
                                     &instance->physics,
                                     player2_ref,
                                     player2_physics,
                                     player2_defeated,
                                     bot_world_x,
                                     bot_world_y,
                                     map_pos_x + player2_ref->x,
                                     map_pos_y + player2_ref->y,
                                     &instance->prev_punch,
                                     &instance->prev_punch2,
                                     &instance->prev_kick,
                                     &instance->prev_kick2);

            game_loop_process_attack(player2_ref,
                                     player2_physics,
                                     &instance->character,
                                     &instance->physics,
                                     &instance->defeated,
                                     map_pos_x + player2_ref->x,
                                     map_pos_y + player2_ref->y,
                                     bot_world_x,
                                     bot_world_y,
                                     JO_NULL,
                                     JO_NULL,
                                     JO_NULL,
                                     JO_NULL);
        }
    }

    // Re-apply player-style animation decisions after AI/physics/attack resolving.
    player_update_animation_state(&instance->character, &instance->physics);
}

void bot_instance_draw(bot_instance_t *instance, int map_pos_x, int map_pos_y)
{
    if (instance == NULL || !instance->active)
        return;

    instance->character.x = (int)instance->world_x - map_pos_x;
    instance->character.y = (int)instance->world_y - map_pos_y;

    if (instance->character.life <= 0)
        instance->defeated = true;

    player_draw(&instance->character, &instance->physics);
}

bool bot_instance_is_defeated(bot_instance_t *instance)
{
    if (instance == NULL)
        return true;

    return (instance->defeated || instance->character.life <= 0);
}

bot_instance_t *bot_default_instance(void)
{
    return &g_bot_instances[0];
}

void bot_set_active_count(int count)
{
    if (count < 0)
        count = 0;
    if (count > BOT_MAX_DEFAULT_COUNT)
        count = BOT_MAX_DEFAULT_COUNT;

    g_bot_active_count = count;
}

int bot_get_active_count(void)
{
    return g_bot_active_count;
}

void bot_init(int selected_player_character, int selected_bot_character, int player_world_x, int player_world_y)
{
    int selected_bot_characters[1];
    int selected_bot_groups[1];
    selected_bot_characters[0] = selected_bot_character;
    selected_bot_groups[0] = 0;
    bot_init_multi(selected_player_character,
                   selected_bot_characters,
                   selected_bot_groups,
                   1,
                   0,
                   player_world_x,
                   player_world_y);
}

void bot_init_multi(int selected_player_character,
                    const int selected_bot_characters[],
                    const int selected_bot_groups[],
                    int bot_count,
                    int selected_player_group,
                    int player_world_x,
                    int player_world_y)
{
    if (bot_count < 0)
        bot_count = 0;
    if (bot_count > BOT_MAX_DEFAULT_COUNT)
        bot_count = BOT_MAX_DEFAULT_COUNT;

    g_bot_active_count = bot_count;

    for (int i = 0; i < BOT_MAX_DEFAULT_COUNT; ++i)
    {
        if (i < bot_count)
        {
            ui_character_choice_t c = bot_normalize_choice((ui_character_choice_t)selected_bot_characters[i % bot_count]);
            int group = selected_bot_groups[i % bot_count];
            int offset = BOT_DEFAULT_STEP_X * (i + 1);
            bot_initialize_instance(&g_bot_instances[i], c, group, player_world_x + offset, player_world_y);
        }
        else
        {
            bot_instance_unload(&g_bot_instances[i]);
        }
    }
}

void bot_init_versus(int selected_player_character,
                     int selected_player2_character,
                     int clones_per_side,
                     int player_world_x,
                     int player_world_y)
{
    int total_count = clones_per_side * 2;
    if (total_count > BOT_MAX_DEFAULT_COUNT)
        total_count = BOT_MAX_DEFAULT_COUNT;

    bot_set_active_count(total_count);
    for (int i = 0; i < BOT_MAX_DEFAULT_COUNT; ++i)
        bot_instance_unload(&g_bot_instances[i]);

    int ally_index = 0;
    int enemy_index = 1;

    for (int i = 0; i < clones_per_side; ++i)
    {
        if (ally_index < total_count)
        {
            bot_initialize_instance(&g_bot_instances[ally_index],
                                    bot_normalize_choice((ui_character_choice_t)selected_player_character),
                                    1,
                                    player_world_x - (96 * (i + 1)),
                                    player_world_y);
            g_bot_instances[ally_index].character.group = 1;
            ally_index += 2;
        }

        if (enemy_index < total_count)
        {
            bot_initialize_instance(&g_bot_instances[enemy_index],
                                    bot_normalize_choice((ui_character_choice_t)selected_player2_character),
                                    2,
                                    player_world_x + (96 * (i + 1)),
                                    player_world_y);
            g_bot_instances[enemy_index].character.group = 2;
            enemy_index += 2;
        }
    }
}

void bot_unload(void)
{
    for (int i = 0; i < BOT_MAX_DEFAULT_COUNT; ++i)
        bot_instance_unload(&g_bot_instances[i]);

    g_bot_active_count = 0;
}

void bot_invalidate_asset_cache(void)
{
    // Purely for compatibility; we may leave assets loaded because Sonic/Amy are shared.
    character_registry_get(UiCharacterSonic).unload();
    character_registry_get(UiCharacterAmy).unload();
}

void bot_reposition_from_map_start(void)
{
    for (int i = 0; i < g_bot_active_count; ++i)
    {
        bot_instance_t *inst = &g_bot_instances[i];
        if (!inst->active)
            continue;

        inst->world_x = (float)world_map_get_bot_start_x(i, inst->group);
        inst->world_y = (float)world_map_get_bot_start_y(i, inst->group);

        if (inst->world_x == WORLD_CAMERA_TARGET_X && inst->world_y == WORLD_CAMERA_TARGET_Y)
        {
            int px = world_map_get_player_start_x(0, inst->group);
            int py = world_map_get_player_start_y(0, inst->group);
            inst->world_x = (float)(px + BOT_DEFAULT_STEP_X * i);
            inst->world_y = (float)py;
        }
    }
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
    for (int i = 0; i < g_bot_active_count; ++i)
    {
        bot_instance_update(&g_bot_instances[i],
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
}

void bot_draw(int map_pos_x, int map_pos_y)
{
    for (int i = 0; i < g_bot_active_count; ++i)
        bot_instance_draw(&g_bot_instances[i], map_pos_x, map_pos_y);
}

bool bot_is_defeated(void)
{
    if (g_bot_active_count <= 0)
        return false;

    bool has_enemy = false;
    for (int i = 0; i < g_bot_active_count; ++i)
    {
        bot_instance_t *inst = &g_bot_instances[i];
        if (inst->group == 1)
            continue;

        has_enemy = true;
        if (!bot_instance_is_defeated(inst))
            return false;
    }

    return has_enemy;
}

bool bot_debug_get_hitbox_snapshot(debug_hitbox_snapshot_t *snapshot)
{
    if (snapshot == JO_NULL)
        return false;

    memset(snapshot, 0, sizeof(*snapshot));
    return false;
}

int bot_get_life(int index)
{
    if (index < 0 || index >= BOT_MAX_DEFAULT_COUNT)
        return 0;
    return g_bot_instances[index].character.life;
}

int bot_get_character_id(int index)
{
    if (index < 0 || index >= BOT_MAX_DEFAULT_COUNT)
        return CHARACTER_ID_SONIC;
    return g_bot_instances[index].character.character_id;
}

int bot_get_group(int index)
{
    if (index < 0 || index >= BOT_MAX_DEFAULT_COUNT)
        return 0;
    return g_bot_instances[index].group;
}
