#ifndef PLAYER_H
#define PLAYER_H

#include "character.h"

#define PLAYER_MAX_DEFAULT_COUNT (2)

extern character_t player;
extern character_t player2;
extern bool g_show_attack_debug;

character_t *player_get_instance(int index);
int player_get_max_instances(void);

void player_update_punch_state(character_t *controlled_player,
							   int punch_combo_trigger_frame,
							   int punch_stage1_last_frame,
							   int punch_stage2_start_frame,
							   int punch_stage2_last_frame,
							   bool has_punch2_stage,
							   bool finish_stage1_on_last_frame,
							   bool finish_stage2_on_last_frame);

void player_update_punch_state_for_character(character_t *controlled_player);
void player_update_kick_state_for_character(character_t *controlled_player);

void player_handle_command_inputs(jo_sidescroller_physics_params *physics,
								  character_t *controlled_player,
								  int *jump_cooldown,
								  int *jump_hold_ms,
								  bool *jump_cut_applied,
								  jo_sound *jump_sfx,
								  int jump_sfx_channel,
								  jo_sound *spin_sfx,
								  jo_sound *punch_sfx,
								  jo_sound *kick_sfx,
								  bool left_pressed,
								  bool right_pressed,
								  bool spin_pressed,
								  bool b_pressed,
								  bool b_down,
								  bool a_down,
								  bool c_down,
								  bool c_hold);

#endif
