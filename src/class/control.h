#ifndef CONTROL_H
#define CONTROL_H

#include <jo/jo.h>
#include "character.h"

typedef struct
{
    bool left_pressed;
    bool right_pressed;
    bool spin_pressed;
    bool a_down;
    bool b_pressed;
    bool b_down;
    bool c_down;
} control_input_t;

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
                                       const control_input_t *input);

void control_handle_player_commands(jo_sidescroller_physics_params *physics,
                                    character_t *player,
                                    int *jump_cooldown,
                                    int *jump_hold_ms,
                                    bool *jump_cut_applied,
                                    jo_sound *jump_sfx,
                                    jo_sound *spin_sfx,
                                    jo_sound *punch_sfx,
                                    jo_sound *kick_sfx);

#endif