#include <jo/jo.h>
#include "control.h"
#include "game_audio.h"
#include "input_mapping.h"

#define DEFAULT_ATTACK_FORWARD_IMPULSE_LIGHT 0.60f
#define DEFAULT_ATTACK_FORWARD_IMPULSE_HEAVY 1.10f
#define CONTROL_FRAME_MS (17)
#define VARIABLE_JUMP_MAX_HOLD_MS (170)
#define VARIABLE_JUMP_MIN_SPEED_FACTOR (0.40f)

static inline float control_get_attack_forward_impulse_light(const character_t *controlled_player)
{
    return (controlled_player->attack_forward_impulse_light > 0.0f)
        ? controlled_player->attack_forward_impulse_light
        : DEFAULT_ATTACK_FORWARD_IMPULSE_LIGHT;
}

static inline float control_get_attack_forward_impulse_heavy(const character_t *controlled_player)
{
    return (controlled_player->attack_forward_impulse_heavy > 0.0f)
        ? controlled_player->attack_forward_impulse_heavy
        : DEFAULT_ATTACK_FORWARD_IMPULSE_HEAVY;
}

static inline void control_apply_attack_forward_impulse(jo_sidescroller_physics_params *physics,
                                                        const character_t *controlled_player,
                                                        float impulse)
{
    if (controlled_player->flip)
        physics->speed -= impulse;
    else
        physics->speed += impulse;
}

static inline void control_make_player_jump(jo_sidescroller_physics_params *physics, character_t *controlled_player, jo_sound *jump_sfx, int jump_sfx_channel)
{
    (void)jump_sfx_channel;
    controlled_player->can_jump = false;
    controlled_player->jump = true;
    controlled_player->punch = false;
    controlled_player->kick = false;

    if (jump_sfx != JO_NULL)
        game_audio_play_sfx_next_channel(jump_sfx);
    jo_physics_jump(physics);
}

static inline void control_update_variable_jump(jo_sidescroller_physics_params *physics,
                                                const control_input_t *input,
                                                int *jump_hold_ms,
                                                bool *jump_cut_applied)
{
    float hold_ratio;
    float desired_factor;
    float desired_speed_y;

    if (input == JO_NULL || jump_hold_ms == JO_NULL || jump_cut_applied == JO_NULL)
        return;

    if (!physics->is_in_air)
    {
        *jump_hold_ms = 0;
        *jump_cut_applied = false;
        return;
    }

    if (input->b_down && *jump_hold_ms < VARIABLE_JUMP_MAX_HOLD_MS)
    {
        *jump_hold_ms += CONTROL_FRAME_MS;
        if (*jump_hold_ms > VARIABLE_JUMP_MAX_HOLD_MS)
            *jump_hold_ms = VARIABLE_JUMP_MAX_HOLD_MS;
    }

    if (input->b_down || *jump_cut_applied)
        return;

    if (physics->speed_y >= 0.0f)
    {
        *jump_cut_applied = true;
        return;
    }

    hold_ratio = (float)(*jump_hold_ms) / (float)VARIABLE_JUMP_MAX_HOLD_MS;
    if (hold_ratio < 0.0f)
        hold_ratio = 0.0f;
    if (hold_ratio > 1.0f)
        hold_ratio = 1.0f;

    desired_factor = VARIABLE_JUMP_MIN_SPEED_FACTOR + ((1.0f - VARIABLE_JUMP_MIN_SPEED_FACTOR) * hold_ratio);
    desired_speed_y = physics->jump_speed_y * desired_factor;

    if (physics->speed_y < desired_speed_y)
        physics->speed_y = desired_speed_y;

    *jump_cut_applied = true;
}

static void control_input_punch(character_t *controlled_player,
                                jo_sidescroller_physics_params *physics,
                                jo_sound *punch_sfx,
                                jo_sound *kick_sfx,
                                bool a_down)
{
    if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2 && !physics->is_in_air && controlled_player->attack_cooldown == 0 && a_down)
    {
        controlled_player->punch = true;
        control_apply_attack_forward_impulse(physics, controlled_player, control_get_attack_forward_impulse_light(controlled_player));
        if (punch_sfx != JO_NULL)
            game_audio_play_sfx_next_channel(punch_sfx);
    }
    else if (controlled_player->punch && !controlled_player->punch2 && a_down)
    {
        controlled_player->punch2_requested = true;
    }

    if (controlled_player->punch2 && controlled_player->perform_punch2)
    {
        controlled_player->perform_punch2 = false;
        control_apply_attack_forward_impulse(physics, controlled_player, control_get_attack_forward_impulse_heavy(controlled_player));
        if (kick_sfx != JO_NULL)
            game_audio_play_sfx_next_channel(kick_sfx);
    }
}

static void control_input_kick(character_t *controlled_player,
                               jo_sidescroller_physics_params *physics,
                               jo_sound *punch_sfx,
                               jo_sound *kick_sfx,
                               bool c_down)
{
    if (physics->is_in_air && c_down && !controlled_player->air_kick_used && !controlled_player->kick && !controlled_player->kick2 && !controlled_player->punch && !controlled_player->punch2)
    {
        controlled_player->kick = false;
        controlled_player->kick2 = true;
        controlled_player->kick2_requested = false;
        controlled_player->perform_kick2 = true;
        controlled_player->air_kick_used = true;
        controlled_player->spin = false;
    }

    if (!controlled_player->kick && !controlled_player->kick2 && !controlled_player->punch && !controlled_player->punch2 && !physics->is_in_air && controlled_player->attack_cooldown == 0 && c_down)
    {
        controlled_player->kick = true;
        control_apply_attack_forward_impulse(physics, controlled_player, control_get_attack_forward_impulse_light(controlled_player));
        if (punch_sfx != JO_NULL)
            game_audio_play_sfx_next_channel(punch_sfx);
    }
    else if (controlled_player->kick && !controlled_player->kick2 && c_down)
    {
        controlled_player->kick2_requested = true;
    }

    if (controlled_player->kick2 && controlled_player->perform_kick2)
    {
        controlled_player->perform_kick2 = false;
        control_apply_attack_forward_impulse(physics, controlled_player, control_get_attack_forward_impulse_heavy(controlled_player));
        if (kick_sfx != JO_NULL)
            game_audio_play_sfx_next_channel(kick_sfx);
    }
}

void control_handle_character_commands(jo_sidescroller_physics_params *physics,
                                       character_t *controlled_player,
                                       int *jump_cooldown,
                                       int *jump_hold_ms,
                                       bool *jump_cut_applied,
                                       jo_sound *jump_sfx,
                                       int jump_sfx_channel,
                                       jo_sound *spin_sfx,
                                       jo_sound *punch_sfx,
                                       jo_sound *kick_sfx,
                                       const control_input_t *input)
{
    if (input == JO_NULL)
        return;

    if (physics->is_in_air)
    {
        if (input->spin_pressed)
            controlled_player->spin = true;

        if (input->left_pressed)
        {
            controlled_player->flip = true;
            jo_physics_accelerate_left(physics);
        }
        else if (input->right_pressed)
        {
            controlled_player->flip = false;
            jo_physics_accelerate_right(physics);
        }
    }
    else
    {
        controlled_player->air_kick_used = false;
        if (jo_physics_is_standing(physics))
        {
            controlled_player->walk = false;
            controlled_player->run = false;
        }

        if (input->left_pressed)
        {
            controlled_player->flip = true;
            controlled_player->walk = true;
            if (jo_physics_is_going_on_the_right(physics) || jo_physics_should_brake(physics))
                jo_physics_decelerate_left(physics);
            else if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2)
                jo_physics_accelerate_left(physics);
            else
                jo_physics_apply_friction(physics);
        }
        else if (input->right_pressed)
        {
            controlled_player->flip = false;
            controlled_player->walk = true;
            if (jo_physics_is_going_on_the_left(physics) || jo_physics_should_brake(physics))
                jo_physics_decelerate_right(physics);
            else if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2)
                jo_physics_accelerate_right(physics);
            else
                jo_physics_apply_friction(physics);
        }
        else
            jo_physics_apply_friction(physics);
    }

    if (controlled_player->can_jump && input->b_pressed && *jump_cooldown == 0)
    {
        *jump_cooldown = 20;
        if (jump_hold_ms != JO_NULL)
            *jump_hold_ms = 0;
        if (jump_cut_applied != JO_NULL)
            *jump_cut_applied = false;
        control_make_player_jump(physics, controlled_player, jump_sfx, jump_sfx_channel);
    }

    control_update_variable_jump(physics, input, jump_hold_ms, jump_cut_applied);

    control_input_punch(controlled_player, physics, punch_sfx, kick_sfx, input->a_down);
    control_input_kick(controlled_player, physics, punch_sfx, kick_sfx, input->c_down);

    if (controlled_player->spin && spin_sfx != JO_NULL)
    {
        if (spin_sfx->current_playing_channel >= JO_SOUND_MAX_CHANNEL || !jo_audio_is_channel_playing(spin_sfx->current_playing_channel))
            game_audio_play_sfx_next_channel(spin_sfx);
    }
}

void control_handle_player_commands(jo_sidescroller_physics_params *physics,
                                    character_t *controlled_player,
                                    int *jump_cooldown,
                                    int *jump_hold_ms,
                                    bool *jump_cut_applied,
                                    jo_sound *jump_sfx,
                                    jo_sound *spin_sfx,
                                    jo_sound *punch_sfx,
                                    jo_sound *kick_sfx)
{
    control_input_t input;

    input_mapping_read_player1(&input);

    control_handle_character_commands(physics,
                                      controlled_player,
                                      jump_cooldown,
                                      jump_hold_ms,
                                      jump_cut_applied,
                                      jump_sfx,
                                      1,
                                      spin_sfx,
                                      punch_sfx,
                                      kick_sfx,
                                      &input);
}