#ifndef GAME_FLOW_H
#define GAME_FLOW_H

#include <jo/jo.h>
#include "ui_control.h"

typedef struct
{
    jo_sidescroller_physics_params *physics;
    int *map_pos_x;
    int *map_pos_y;
    bool *player_defeated;
    ui_control_state_t *ui_state;
} game_flow_context_t;

void game_flow_start_from_menu(ui_character_choice_t selected_character,
                               ui_character_choice_t selected_bot_character,
                               void *user_data);
void game_flow_process_loading(void *user_data);
void game_flow_reset_fight(void *user_data);
void game_flow_return_to_character_select(void *user_data);
void game_flow_runtime_tick(void);
void game_flow_update_animation(void);
void game_flow_display_character(void);
int game_flow_get_player2_tail_base_id(void);
int game_flow_get_player2_kick_sprite_id(void);
int game_flow_cdda_debug_state(void);
int game_flow_cdda_debug_delay_frames(void);
int game_flow_cdda_debug_attempts_left(void);
void game_flow_debug_force_cdda_play(void);
void game_flow_debug_force_cdda_stop(void);

void game_flow_get_last_player1_start(int *group, int *start_x, int *start_y, int *map_x, int *map_y, int *screen_x, int *screen_y);
void game_flow_get_last_player1_reset_info(int *effective_player, int *effective_group, int *menu_player, int *menu_group);

#endif