#include <jo/jo.h>
#include "input_mapping.h"

static input_mapping_config_t player1_mapping =
{
    JO_KEY_LEFT,
    JO_KEY_RIGHT,
    JO_KEY_Y,
    JO_KEY_A,
    JO_KEY_X,
    JO_KEY_B
};

static input_mapping_config_t player2_mapping =
{
    JO_KEY_LEFT,
    JO_KEY_RIGHT,
    JO_KEY_Y,
    JO_KEY_A,
    JO_KEY_X,
    JO_KEY_B
};

int input_mapping_get_player2_port(void)
{
    int port;

    for (port = 1; port < JO_INPUT_MAX_DEVICE; ++port)
    {
        if (jo_is_input_available(port))
            return port;
    }

    return -1;
}

bool input_mapping_is_player2_available(void)
{
    return input_mapping_get_player2_port() >= 0;
}

bool input_mapping_is_player2_key_down(jo_gamepad_keys key)
{
    int port = input_mapping_get_player2_port();

    if (port < 0)
        return false;

    return jo_is_input_key_down(port, key);
}

bool input_mapping_is_player2_key_pressed(jo_gamepad_keys key)
{
    int port = input_mapping_get_player2_port();

    if (port < 0)
        return false;

    return jo_is_input_key_pressed(port, key);
}

void input_mapping_set_player1(const input_mapping_config_t *config)
{
    if (config == JO_NULL)
        return;

    player1_mapping = *config;
}

void input_mapping_get_player1(input_mapping_config_t *out_config)
{
    if (out_config == JO_NULL)
        return;

    *out_config = player1_mapping;
}

void input_mapping_read_player1(control_input_t *out_input)
{
    if (out_input == JO_NULL)
        return;

    out_input->left_pressed = jo_is_pad1_key_pressed(player1_mapping.move_left_key);
    out_input->right_pressed = jo_is_pad1_key_pressed(player1_mapping.move_right_key);
    out_input->spin_pressed = jo_is_pad1_key_pressed(player1_mapping.spin_key);
    out_input->a_down = jo_is_pad1_key_down(player1_mapping.punch_key);
    out_input->b_pressed = jo_is_pad1_key_down(player1_mapping.jump_key);
    out_input->b_down = jo_is_pad1_key_pressed(player1_mapping.jump_key);
    out_input->c_down = jo_is_pad1_key_down(player1_mapping.kick_key);
    out_input->c_hold = jo_is_pad1_key_pressed(player1_mapping.kick_key);
}

void input_mapping_set_player2(const input_mapping_config_t *config)
{
    if (config == JO_NULL)
        return;

    player2_mapping = *config;
}

void input_mapping_get_player2(input_mapping_config_t *out_config)
{
    if (out_config == JO_NULL)
        return;

    *out_config = player2_mapping;
}

void input_mapping_read_player2(control_input_t *out_input)
{
    int port;

    if (out_input == JO_NULL)
        return;

    port = input_mapping_get_player2_port();
    if (port < 0)
    {
        out_input->left_pressed = false;
        out_input->right_pressed = false;
        out_input->spin_pressed = false;
        out_input->a_down = false;
        out_input->b_pressed = false;
        out_input->b_down = false;
        out_input->c_down = false;
        out_input->c_hold = false;
        return;
    }

    out_input->left_pressed = jo_is_input_key_pressed(port, player2_mapping.move_left_key);
    out_input->right_pressed = jo_is_input_key_pressed(port, player2_mapping.move_right_key);
    out_input->spin_pressed = jo_is_input_key_pressed(port, player2_mapping.spin_key);
    out_input->a_down = jo_is_input_key_down(port, player2_mapping.punch_key);
    out_input->b_pressed = jo_is_input_key_down(port, player2_mapping.jump_key);
    out_input->b_down = jo_is_input_key_pressed(port, player2_mapping.jump_key);
    out_input->c_down = jo_is_input_key_down(port, player2_mapping.kick_key);
    out_input->c_hold = jo_is_input_key_pressed(port, player2_mapping.kick_key);
}
