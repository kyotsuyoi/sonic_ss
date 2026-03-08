#ifndef CHARACTER_H
#define CHARACTER_H

#include <jo/jo.h>

#define ATTACK_COOLDOWN_FRAMES (15)
#define ATTACK_COOLDOWN_PUNCH2_FRAMES (21)
#define ATTACK_COOLDOWN_KICK2_FRAMES (23)

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
    bool kick;
    bool kick2;
    bool kick2_requested;
    bool perform_kick2;
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
    int life;
} character_t;

#endif
