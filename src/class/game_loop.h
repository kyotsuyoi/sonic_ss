#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include <jo/jo.h>
#include "ui_control.h"
#include "game_flow.h"
#include "debug.h"

typedef struct
{
    ui_control_state_t *ui_state;
    game_flow_context_t *game_flow_ctx;
    jo_sidescroller_physics_params *physics;
    int *map_pos_x;
    int *map_pos_y;
    int *jump_cooldown;
    bool *player_defeated;
    int *total_pcm;
} game_loop_context_t;

void game_loop_init(game_loop_context_t *ctx);
void game_loop_draw(void);
void game_loop_input(void);
void game_loop_update(void);
void game_loop_debug_frame(void);

/* Helpers used by debug display */
int game_loop_get_map_pos_x(void);
int game_loop_get_map_pos_y(void);

/* External helpers */
void game_loop_mark_ui_dirty(void);

#endif