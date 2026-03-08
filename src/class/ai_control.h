#ifndef AI_CONTROL_H
#define AI_CONTROL_H

#include <jo/jo.h>
#include "character.h"

typedef enum
{
    AiBotAttackNone = 0,
    AiBotAttackPunch1,
    AiBotAttackPunch2,
    AiBotAttackKick1,
    AiBotAttackKick2
} ai_bot_attack_t;

typedef bool (*ai_is_close_enough_fn)(int x1, int y1, int x2, int y2, int range);
typedef bool (*ai_is_attack_in_range_fn)(int attacker_x, int attacker_y, bool attacker_flip, int target_x, int target_y, int range);
typedef void (*ai_bot_start_attack_fn)(ai_bot_attack_t attack, bool request_combo);
typedef void (*ai_apply_hit_effect_to_player_fn)(character_t *player, bool attacker_flip, float knockback_force, int stun_frames);

typedef struct
{
    character_t *player;
    bool *player_defeated;
    character_t *bot_ref;
    bool bot_defeated_flag;
    bool bot_in_air_flag;
    jo_sidescroller_physics_params *bot_physics_ref;
    float *bot_world_x_ref;
    float *bot_world_y_ref;
    float *bot_speed_x_ref;
    float *bot_speed_y_ref;
    int player_world_x;
    int player_world_y;
    int *bot_attack_cooldown_ref;
    int *bot_attack_step_ref;
    int *bot_jump_cooldown_ref;
    int *bot_jump_hold_ms_ref;
    int *bot_jump_hold_target_ms_ref;
    bool *bot_jump_cut_applied_ref;
    ai_bot_attack_t *bot_current_attack_ref;
    int *bot_attack_timer_ref;
    bool *bot_attack_damage_done_ref;
    bool *bot_combo_requested_ref;
    int warmup_frames_left;
    jo_sound *jump_sfx;
    jo_sound *spin_sfx;
    jo_sound *punch_sfx;
    jo_sound *kick_sfx;
    int jump_sfx_channel;
    ai_is_close_enough_fn is_close_enough;
    ai_is_attack_in_range_fn is_attack_in_range;
    ai_bot_start_attack_fn start_attack;
    ai_apply_hit_effect_to_player_fn apply_hit_effect_to_player;
} ai_bot_context_t;

void ai_control_handle_bot_commands(ai_bot_context_t *ctx);

#endif
