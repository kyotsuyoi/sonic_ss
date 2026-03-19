#ifndef PLAYER_H
#define PLAYER_H

#include "character.h"
#include "control.h"
#include "debug.h"

#define PLAYER_MAX_DEFAULT_COUNT (2)

typedef struct player_instance
{
    int id;
    bool active;
    character_t *character;
    jo_sidescroller_physics_params physics;

    /* Runtime state (jump/cooldowns, etc.) */
    float world_x;
    float world_y;
    bool initialized;
    bool defeated;
    int jump_cooldown;
    int jump_hold_ms;
    bool jump_cut_applied;
    int airborne_time_ms;

    /* Previous attack input state (for hit detection timing) */
    bool prev_punch;
    bool prev_punch2;
    bool prev_kick;
    bool prev_kick2;
} player_instance_t;

extern character_t player;
extern character_t player2;
extern bool g_show_attack_debug;

player_instance_t *player_get_instance(int index);
int player_get_max_instances(void);

player_instance_t *player_get_default_player(void);
player_instance_t *player_get_default_player2(void);

/* Generic instance operations (for player/bot etc). */
void player_instance_update_runtime(player_instance_t *inst, int *map_pos_x, int *map_pos_y);
void player_instance_handle_input(player_instance_t *inst,
                                  const control_input_t *input,
                                  jo_sound *jump_sfx,
                                  int jump_sfx_channel,
                                  jo_sound *spin_sfx,
                                  jo_sound *punch_sfx,
                                  jo_sound *kick_sfx);

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

void player_update_animation_state(character_t *controlled_player, jo_sidescroller_physics_params *physics);

bool player_should_draw_tail(const character_t *controlled_player);
void player_reset_tail_state(character_t *controlled_player);
int player_get_tail_offset_x(const character_t *controlled_player);
int player_get_defeated_sprite_height(const character_t *controlled_player);

void player_draw(character_t *controlled_player, jo_sidescroller_physics_params *physics);

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

// Runtime management for multiple players (player1/player2, bots, etc.)
// This keeps per-instance runtime state in a single generic flow.
void player_runtime_update(character_t *controlled_player,
                            jo_sidescroller_physics_params *physics,
                            float *world_x,
                            float *world_y,
                            bool *initialized,
                            bool *defeated,
                            int *jump_cooldown,
                            int *jump_hold_ms,
                            bool *jump_cut_applied,
                            int *airborne_time_ms,
                            int player_index,
                            int *map_pos_x,
                            int *map_pos_y);

// Player2 runtime helpers (used by game_loop to keep player2 behavior in the player module)
void player2_sync_mode(bool multiplayer_versus);
bool player2_is_active(void);
void player2_update_runtime(int *map_pos_x, int *map_pos_y);
void player2_reset_runtime(int *map_pos_x, int *map_pos_y);

// Runtime queries
bool player2_is_defeated(void);
jo_sidescroller_physics_params *player2_get_physics(void);

void player2_handle_input(const control_input_t *input);

void player2_update_pvp_hitbox_snapshot(int p1_world_x, int p1_world_y, int p2_world_x, int p2_world_y);
bool player2_debug_get_hitbox_snapshot(debug_hitbox_snapshot_t *snapshot);

#endif
