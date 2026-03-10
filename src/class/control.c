#include <jo/jo.h>
#include "control.h"
#include "input_mapping.h"
#include "player.h"

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

    player_handle_command_inputs(physics,
                                 controlled_player,
                                 jump_cooldown,
                                 jump_hold_ms,
                                 jump_cut_applied,
                                 jump_sfx,
                                 jump_sfx_channel,
                                 spin_sfx,
                                 punch_sfx,
                                 kick_sfx,
                                 input->left_pressed,
                                 input->right_pressed,
                                 input->spin_pressed,
                                 input->b_pressed,
                                 input->b_down,
                                 input->a_down,
                                 input->c_down,
                                 input->c_hold);
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