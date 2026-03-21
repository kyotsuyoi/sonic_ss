#ifndef CHARACTER_H
#define CHARACTER_H

#include <jo/jo.h>

#define ATTACK_COOLDOWN_FRAMES (15)
#define ATTACK_COOLDOWN_PUNCH2_FRAMES (21)
#define ATTACK_COOLDOWN_KICK2_FRAMES (23)

#define CHARACTER_ID_SONIC 0
#define CHARACTER_ID_AMY 1
#define CHARACTER_ID_TAILS 2
#define CHARACTER_ID_KNUCKLES 3
#define CHARACTER_ID_SHADOW 4

typedef struct
{
    int stand_sprite_id;
    int walking_anim_id;
    int running1_anim_id;
    int running2_anim_id;
    int spin_sprite_id;
    int jump_sprite_id;
    int damage_sprite_id;
    int defeated_sprite_id;
    int defeated_wram_sprite_id;
    int wram_sprite_id; /* sprite used by WRAM-sheet rendering (e.g. Amy) */

    /* Per-character WRAM-sheet animation state (used by Sonic/Amy) */
    int wram_anim_state;
    int wram_anim_frame;
    int wram_anim_frame_ticks;
    int wram_anim_frame_duration;
    int wram_anim_frame_count;

    int punch_anim_id;
    int kick_anim_id;
    int x;
    int y;
    bool walk;
    int run;
    bool flip;
    bool spin;
    bool jump;
    bool falling;
    bool punch;
    bool punch2;
    bool punch2_requested;
    bool perform_punch2;
    int punch_stage1_ticks; /* remaining ticks for stage1 (0/1) */
    int punch_stage2_ticks; /* hold timer for final stage2 frame */
    bool kick;
    bool kick2;
    bool kick2_requested;
    bool perform_kick2;
    int kick_stage1_ticks; /* remaining ticks for stage1 (0/1) */
    int kick_stage2_ticks; /* hold timer for final stage2 frame */
    bool charged_kick_enabled;
    int charged_kick_hold_ms;
    bool charged_kick_button_held;
    bool charged_kick_ready;
    bool charged_kick_active;
    int charged_kick_phase;
    int charged_kick_phase_timer;
    int character_id;
    bool hit_done_punch1;
    bool hit_done_punch2;
    bool hit_done_kick1;
    bool hit_done_kick2;
    bool air_kick_used;
    int hit_range_punch1;
    int hit_range_punch2;
    int hit_range_kick1;
    int hit_range_kick2;
    float attack_forward_impulse_light;
    float attack_forward_impulse_heavy;
    float knockback_punch1;
    float knockback_punch2;
    float knockback_kick1;
    float knockback_kick2;
    int attack_cooldown;
    int stun_timer;
    bool can_jump;
    int angle;
    int speed;

    /* Tails-specific animation state (spin kick & tail loop). */
    int tails_kick_timer;
    int tails_kick_duration;
    int tails_kick_total_degrees;
    bool tails_kick_rotation_active;
    int tail_frame;
    int tail_timer;

    int life;
    int group; /* 0 = no group, 1 = group1, 2 = group2 */
    int battle_damage_dealt; /* accumulated damage dealt in the current fight */
} character_t;

#endif
