#ifndef INPUT_MAPPING_H
#define INPUT_MAPPING_H

#include <jo/jo.h>
#include "control.h"

typedef struct
{
    int move_left_key;
    int move_right_key;
    int spin_key;
    int jump_key;
    int punch_key;
    int kick_key;
} input_mapping_config_t;

void input_mapping_set_player1(const input_mapping_config_t *config);
void input_mapping_get_player1(input_mapping_config_t *out_config);
void input_mapping_read_player1(control_input_t *out_input);
void input_mapping_set_player2(const input_mapping_config_t *config);
void input_mapping_get_player2(input_mapping_config_t *out_config);
void input_mapping_read_player2(control_input_t *out_input);
int input_mapping_get_player2_port(void);
bool input_mapping_is_player2_available(void);
bool input_mapping_is_player2_key_down(jo_gamepad_keys key);
bool input_mapping_is_player2_key_pressed(jo_gamepad_keys key);

#endif
