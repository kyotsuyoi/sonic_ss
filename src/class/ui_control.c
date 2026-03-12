#include <jo/jo.h>
#include "ui_control.h"
#include "input_mapping.h"
#include "game_audio.h"
#include "rotating_sprite_pool.h"
#include "runtime_log.h"

#define MENU_SPRITE_Y 88
#define MENU_NAME_Y 6
#define MENU_REMAP_ACTION_COUNT 6
#define MENU_REMAP_SWAP_MESSAGE_FRAMES 240
#define MENU_PAD2_WARNING_FRAMES 180
#define MENU_MAIN_OPTION_COUNT 2
#define MENU_MODE_OPTION_COUNT 2
#define MENU_BATTLE_OPTION_COUNT 2
#define MENU_MULTIPLAYER_OPTION_COUNT 2
#define MENU_OPTIONS_OPTION_COUNT 2
#define SONG_TEST_LINE_COUNT 2
#define SONG_TEST_SFX_COUNT 6
#define SONG_TEST_MIN_TRACK 2
#define SONG_TEST_MAX_TRACK 4
#define MENU_REMAP_PLAYER_COUNT 2

enum
{
    MenuMainStartGame = 0,
    MenuMainOptions
};

enum
{
    MenuModeAdventure = 0,
    MenuModeBattle
};

enum
{
    MenuBattleSinglePlayer = 0,
    MenuBattleMultiplayer
};

enum
{
    MenuMultiplayerCoop = 0,
    MenuMultiplayerVersus
};

enum
{
    MenuOptionsConfigControls = 0,
    MenuOptionsSongTest
};

enum
{
    MenuRemapMoveLeft = 0,
    MenuRemapMoveRight,
    MenuRemapSpin,
    MenuRemapJump,
    MenuRemapPunch,
    MenuRemapKick
};

enum
{
    SongTestLineBgm = 0,
    SongTestLineSfx
};

enum
{
    SongTestSfxJump = 0,
    SongTestSfxPunch,
    SongTestSfxKick,
    SongTestSfxSpin,
    SongTestSfxDamageLow,
    SongTestSfxDamageHigh
};

static bool menu_sprites_loaded = false;
static int menu_jump_sprite_pool_id = -1;
static int menu_sonic_jump_handle = -1;
static int menu_amy_jump_handle = -1;
static int menu_tails_jump_handle = -1;
static int menu_knuckles_jump_handle = -1;
static int menu_shadow_jump_handle = -1;
static int menu_sonic_jump_sprite_id = -1;
static int menu_amy_jump_sprite_id = -1;
static int menu_tails_jump_sprite_id = -1;
static int menu_knuckles_jump_sprite_id = -1;
static int menu_shadow_jump_sprite_id = -1;
static int menu_remap_swap_action_a = -1;
static int menu_remap_swap_action_b = -1;
static int menu_remap_swap_message_timer = 0;
static bool menu_main_bgm_playing = false;
static int menu_current_blink_timer = 0;

static const char *menu_remap_action_names[MENU_REMAP_ACTION_COUNT] =
{
    "MOVE LEFT",
    "MOVE RIGHT",
    "SPIN",
    "JUMP",
    "PUNCH",
    "KICK"
};

static const char *menu_remap_player_names[MENU_REMAP_PLAYER_COUNT] =
{
    "P1",
    "P2"
};

static const char *song_test_sfx_names[SONG_TEST_SFX_COUNT] =
{
    "JUMP",
    "PUNCH",
    "KICK",
    "SPIN",
    "DMG LOW",
    "DMG HIGH"
};

static const char *ui_control_key_name(int key)
{
    if (key == JO_KEY_LEFT) return "LEFT";
    if (key == JO_KEY_RIGHT) return "RIGHT";
    if (key == JO_KEY_UP) return "UP";
    if (key == JO_KEY_DOWN) return "DOWN";
    if (key == JO_KEY_A) return "A";
    if (key == JO_KEY_B) return "B";
    if (key == JO_KEY_C) return "C";
    if (key == JO_KEY_X) return "X";
    if (key == JO_KEY_Y) return "Y";
    if (key == JO_KEY_Z) return "Z";
    if (key == JO_KEY_L) return "L";
    if (key == JO_KEY_R) return "R";
    if (key == JO_KEY_START) return "START";
    return "UNK";
}

static bool ui_control_try_capture_menu_key_player1(int *out_key)
{
    if (jo_is_pad1_key_down(JO_KEY_LEFT)) { *out_key = JO_KEY_LEFT; return true; }
    if (jo_is_pad1_key_down(JO_KEY_RIGHT)) { *out_key = JO_KEY_RIGHT; return true; }
    if (jo_is_pad1_key_down(JO_KEY_UP)) { *out_key = JO_KEY_UP; return true; }
    if (jo_is_pad1_key_down(JO_KEY_DOWN)) { *out_key = JO_KEY_DOWN; return true; }
    if (jo_is_pad1_key_down(JO_KEY_A)) { *out_key = JO_KEY_A; return true; }
    if (jo_is_pad1_key_down(JO_KEY_B)) { *out_key = JO_KEY_B; return true; }
    if (jo_is_pad1_key_down(JO_KEY_C)) { *out_key = JO_KEY_C; return true; }
    if (jo_is_pad1_key_down(JO_KEY_X)) { *out_key = JO_KEY_X; return true; }
    if (jo_is_pad1_key_down(JO_KEY_Y)) { *out_key = JO_KEY_Y; return true; }
    if (jo_is_pad1_key_down(JO_KEY_Z)) { *out_key = JO_KEY_Z; return true; }
    if (jo_is_pad1_key_down(JO_KEY_L)) { *out_key = JO_KEY_L; return true; }
    if (jo_is_pad1_key_down(JO_KEY_R)) { *out_key = JO_KEY_R; return true; }
    if (jo_is_pad1_key_down(JO_KEY_START)) { *out_key = JO_KEY_START; return true; }
    return false;
}

static bool ui_control_try_capture_menu_key_player2(int *out_key)
{
    if (!input_mapping_is_player2_available())
        return false;

    if (input_mapping_is_player2_key_down(JO_KEY_LEFT)) { *out_key = JO_KEY_LEFT; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_RIGHT)) { *out_key = JO_KEY_RIGHT; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_UP)) { *out_key = JO_KEY_UP; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_DOWN)) { *out_key = JO_KEY_DOWN; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_A)) { *out_key = JO_KEY_A; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_B)) { *out_key = JO_KEY_B; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_C)) { *out_key = JO_KEY_C; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_X)) { *out_key = JO_KEY_X; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_Y)) { *out_key = JO_KEY_Y; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_Z)) { *out_key = JO_KEY_Z; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_L)) { *out_key = JO_KEY_L; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_R)) { *out_key = JO_KEY_R; return true; }
    if (input_mapping_is_player2_key_down(JO_KEY_START)) { *out_key = JO_KEY_START; return true; }
    return false;
}

static bool ui_control_try_capture_menu_key_for_player(int player_index, int *out_key)
{
    if (player_index == 1)
        return ui_control_try_capture_menu_key_player2(out_key);

    return ui_control_try_capture_menu_key_player1(out_key);
}

static bool ui_control_any_mappable_key_down_for_player(int player_index)
{
    int key;
    return ui_control_try_capture_menu_key_for_player(player_index, &key);
}

static void ui_control_get_mapping_for_player(int player_index, input_mapping_config_t *mapping)
{
    if (player_index == 1)
        input_mapping_get_player2(mapping);
    else
        input_mapping_get_player1(mapping);
}

static void ui_control_set_mapping_for_player(int player_index, const input_mapping_config_t *mapping)
{
    if (player_index == 1)
        input_mapping_set_player2(mapping);
    else
        input_mapping_set_player1(mapping);
}

static int ui_control_get_mapping_action(const input_mapping_config_t *mapping, int action)
{
    if (action == MenuRemapMoveLeft) return mapping->move_left_key;
    if (action == MenuRemapMoveRight) return mapping->move_right_key;
    if (action == MenuRemapSpin) return mapping->spin_key;
    if (action == MenuRemapJump) return mapping->jump_key;
    if (action == MenuRemapPunch) return mapping->punch_key;
    return mapping->kick_key;
}

static void ui_control_set_mapping_action_raw(input_mapping_config_t *mapping, int action, int key)
{
    if (action == MenuRemapMoveLeft) mapping->move_left_key = key;
    else if (action == MenuRemapMoveRight) mapping->move_right_key = key;
    else if (action == MenuRemapSpin) mapping->spin_key = key;
    else if (action == MenuRemapJump) mapping->jump_key = key;
    else if (action == MenuRemapPunch) mapping->punch_key = key;
    else if (action == MenuRemapKick) mapping->kick_key = key;
}

static int ui_control_set_mapping_action(input_mapping_config_t *mapping, int action, int key)
{
    int selected_old_key;
    int idx;
    int swapped_action = -1;

    selected_old_key = ui_control_get_mapping_action(mapping, action);
    for (idx = 0; idx < MENU_REMAP_ACTION_COUNT; ++idx)
    {
        if (idx != action && ui_control_get_mapping_action(mapping, idx) == key)
        {
            ui_control_set_mapping_action_raw(mapping, idx, selected_old_key);
            swapped_action = idx;
            break;
        }
    }

    ui_control_set_mapping_action_raw(mapping, action, key);
    return swapped_action;
}

static void ui_control_load_menu_sprites(void)
{
    if (menu_sprites_loaded)
    {
        menu_sonic_jump_sprite_id = rotating_sprite_pool_request(menu_sonic_jump_handle);
        menu_amy_jump_sprite_id = rotating_sprite_pool_request(menu_amy_jump_handle);
        menu_tails_jump_sprite_id = rotating_sprite_pool_request(menu_tails_jump_handle);
        menu_knuckles_jump_sprite_id = rotating_sprite_pool_request(menu_knuckles_jump_handle);
        menu_shadow_jump_sprite_id = rotating_sprite_pool_request(menu_shadow_jump_handle);
        return;
    }

    if (menu_jump_sprite_pool_id < 0)
    {
        menu_jump_sprite_pool_id = rotating_sprite_pool_create(32, 36, 5);
        if (menu_jump_sprite_pool_id < 0)
            return;
    }

    menu_sonic_jump_handle = rotating_sprite_pool_register_tga(menu_jump_sprite_pool_id, "SPT", "SNC_JMP.TGA", JO_COLOR_Green);
    menu_amy_jump_handle = rotating_sprite_pool_register_tga(menu_jump_sprite_pool_id, "SPT", "AMY_JMP.TGA", JO_COLOR_Green);
    menu_tails_jump_handle = rotating_sprite_pool_register_tga(menu_jump_sprite_pool_id, "SPT", "TLS_JMP.TGA", JO_COLOR_Green);
    menu_knuckles_jump_handle = rotating_sprite_pool_register_tga(menu_jump_sprite_pool_id, "SPT", "KNK_JMP.TGA", JO_COLOR_Green);
    menu_shadow_jump_handle = rotating_sprite_pool_register_tga(menu_jump_sprite_pool_id, "SPT", "SDW_JMP.TGA", JO_COLOR_Green);

    rotating_sprite_pool_prefetch(menu_sonic_jump_handle);
    rotating_sprite_pool_prefetch(menu_amy_jump_handle);
    rotating_sprite_pool_prefetch(menu_tails_jump_handle);
    rotating_sprite_pool_prefetch(menu_knuckles_jump_handle);
    rotating_sprite_pool_prefetch(menu_shadow_jump_handle);

    menu_sonic_jump_sprite_id = -1;
    menu_amy_jump_sprite_id = -1;
    menu_tails_jump_sprite_id = -1;
    menu_knuckles_jump_sprite_id = -1;
    menu_shadow_jump_sprite_id = -1;
    menu_sprites_loaded = true;
}

void ui_control_reset_menu_sprites(void)
{
    menu_sprites_loaded = false;
    menu_sonic_jump_handle = -1;
    menu_amy_jump_handle = -1;
    menu_tails_jump_handle = -1;
    menu_knuckles_jump_handle = -1;
    menu_shadow_jump_handle = -1;
    menu_sonic_jump_sprite_id = -1;
    menu_amy_jump_sprite_id = -1;
    menu_tails_jump_sprite_id = -1;
    menu_knuckles_jump_sprite_id = -1;
    menu_shadow_jump_sprite_id = -1;
}

static bool ui_control_character_is_locked(ui_character_choice_t choice)
{
    return choice == UiCharacterShadow;
}

static bool ui_control_is_multiplayer_versus(const ui_control_state_t *state)
{
    return state->menu_multiplayer_versus;
}

static bool ui_control_current_name_visible(void)
{
    ++menu_current_blink_timer;
    if (menu_current_blink_timer >= 30)
        menu_current_blink_timer = 0;
    return menu_current_blink_timer < 15;
}

static ui_character_choice_t ui_control_prev_character(ui_character_choice_t choice)
{
    if (choice == UiCharacterSonic)
        return (ui_character_choice_t)(UiCharacterCount - 1);
    return (ui_character_choice_t)(choice - 1);
}

static ui_character_choice_t ui_control_next_character(ui_character_choice_t choice)
{
    if (choice == (ui_character_choice_t)(UiCharacterCount - 1))
        return UiCharacterSonic;
    return (ui_character_choice_t)(choice + 1);
}

static bool ui_control_consume_pressed(bool down, bool *released)
{
    if (down)
    {
        if (*released)
        {
            *released = false;
            return true;
        }
        return false;
    }

    *released = true;
    return false;
}

static jo_sound *ui_control_song_test_get_sfx(int index)
{
    if (index == SongTestSfxJump) return game_audio_get_jump_sfx();
    if (index == SongTestSfxPunch) return game_audio_get_punch_sfx();
    if (index == SongTestSfxKick) return game_audio_get_kick_sfx();
    if (index == SongTestSfxSpin) return game_audio_get_spin_sfx();
    if (index == SongTestSfxDamageLow) return game_audio_get_damage_low_sfx();
    return game_audio_get_damage_hi_sfx();
}

static void ui_control_sync_menu_bgm(const ui_control_state_t *state)
{
    if (state->menu_screen == UiMenuScreenCharacterSelect)
    {
        if (!menu_main_bgm_playing)
        {
            jo_audio_stop_cd();
            jo_audio_play_cd_track(2, 2, true);
            menu_main_bgm_playing = true;
        }
    }
    else if (menu_main_bgm_playing)
    {
        jo_audio_stop_cd();
        menu_main_bgm_playing = false;
    }
}

void ui_control_reset_menu_bgm_state(void)
{
    menu_main_bgm_playing = false;
}

void ui_control_init(ui_control_state_t *state)
{
    *state = (ui_control_state_t){0};
    state->current_game_state = UiGameStateMenu;
    state->menu_screen = UiMenuScreenMain;
    state->menu_main_selected_option = MenuMainStartGame;
    state->menu_mode_selected_option = MenuModeBattle;
    state->menu_battle_selected_option = MenuBattleSinglePlayer;
    state->menu_options_selected_option = MenuOptionsConfigControls;
    state->menu_selected_character = UiCharacterSonic;
    state->menu_selected_bot_character = UiCharacterAmy;
    state->menu_selected_player2_character = UiCharacterAmy;
    state->menu_cursor_character = UiCharacterSonic;
    state->menu_cursor_bot_character = UiCharacterAmy;
    state->menu_cursor_player2_character = UiCharacterAmy;
    state->menu_selecting_bot_character = false;
    state->menu_selecting_player2_character = false;
    state->menu_player1_confirmed = false;
    state->menu_player2_confirmed = false;
    state->menu_multiplayer_versus = false;
    state->menu_multiplayer_selected_option = MenuMultiplayerVersus;
    state->menu_bot_count = 0;
    state->menu_up_released = true;
    state->menu_down_released = true;
    state->menu_left_released = true;
    state->menu_right_released = true;
    state->menu_a_released = true;
    state->menu_b_released = true;
    state->menu_c_released = true;
    state->menu_p2_left_released = true;
    state->menu_p2_right_released = true;
    state->menu_p2_up_released = true;
    state->menu_p2_down_released = true;
    state->menu_p2_a_released = true;
    state->menu_p2_b_released = true;
    state->menu_remap_mode = false;
    state->menu_remap_waiting_key = false;
    state->menu_remap_wait_release = false;
    state->menu_remap_selected_player = 0;
    state->menu_remap_selected_action = 0;
    state->menu_pad2_warning_timer = 0;
    state->song_test_selected_line = SongTestLineBgm;
    state->song_test_bgm_track = SONG_TEST_MIN_TRACK;
    state->song_test_sfx_index = SongTestSfxJump;
    state->game_paused = false;
    state->debug_enabled = false;
    state->debug_mode = UiDebugModeOff;
    state->log_mode = RuntimeLogModeOff;

    runtime_log_set_mode(state->log_mode);
    state->pause_selected_option = UiPauseOptionContinue;
    state->pause_up_released = true;
    state->pause_down_released = true;
    state->pause_a_released = true;
    state->pause_start_released = true;
    state->pause_lr_released = true;
}

void ui_control_clear_text_layer(void)
{
    int line;
    for (line = 0; line < 30; ++line)
        jo_printf(0, line, "                                        ");
}

void ui_control_draw_pause_menu(const ui_control_state_t *state)
{
    static const char *debug_mode_label[UiDebugModeCount] = {"OFF", "HARDWARE", "PLAYER"};
    static const char *log_mode_label[RuntimeLogModeCount] = {"OFF", "SYSTEM", "SPRITE"};
    int sprite_log_page = runtime_log_get_sprite_page() + 1;
    int sprite_log_page_count = runtime_log_get_sprite_page_count();

    jo_printf(17, 6, "PAUSED");
    jo_printf(8, 9, "%s CONTINUE", state->pause_selected_option == UiPauseOptionContinue ? ">" : " ");
    
    jo_printf(8, 11, "%s RESET FIGHT", state->pause_selected_option == UiPauseOptionResetFight ? ">" : " ");
    jo_printf(8, 12, "%s CHARACTER SELECT", state->pause_selected_option == UiPauseOptionCharacterSelect ? ">" : " ");
    jo_printf(8, 14, "%s DEBUG: %s", state->pause_selected_option == UiPauseOptionDebug ? ">" : " ", debug_mode_label[state->debug_mode]);
    jo_printf(8, 15, "%s LOGS: %s", state->pause_selected_option == UiPauseOptionLogs ? ">" : " ", log_mode_label[state->log_mode]);
    jo_printf(8, 16, "%s LOG PAGE: %d/%d", state->pause_selected_option == UiPauseOptionLogPage ? ">" : " ", sprite_log_page, sprite_log_page_count);
    
    jo_printf(4, 24, "UP/DOWN: SELECT");
    jo_printf(4, 25, "A: CONFIRM");
    jo_printf(4, 26, "L+R: CHANGE MODE");
    jo_printf(4, 27, "START: RESUME");
}

void ui_control_draw_character_menu(const ui_control_state_t *state)
{
    static const char *character_names[UiCharacterCount] = {"SONIC", "AMY", "TAILS", "KNUCKLES", "SHADOW"};
    static const int item_x[UiCharacterCount] = {52, 104, 156, 208, 266};
    input_mapping_config_t mapping;
    int idx;
    int sprite_id;

    ui_control_load_menu_sprites();

    if (state->menu_screen == UiMenuScreenMain)
    {
        jo_printf(15, 6, "MAIN MENU");
        jo_printf(8, 10, "%s START GAME", state->menu_main_selected_option == MenuMainStartGame ? ">" : " ");
        jo_printf(8, 11, "%s OPTIONS", state->menu_main_selected_option == MenuMainOptions ? ">" : " ");
        
        jo_printf(4, 25, "UP/DOWN: SELECT");
        jo_printf(4, 26, "A: CONFIRM");
        return;
    }

    if (state->menu_screen == UiMenuScreenModeSelect)
    {
        jo_printf(14, 6, "SELECT MODE");
        jo_printf(8, 10, "%s ADVENTURE MODE", state->menu_mode_selected_option == MenuModeAdventure ? "X" : " ");
        jo_printf(8, 11, "%s BATTLE MODE", state->menu_mode_selected_option == MenuModeBattle ? ">" : " ");
        
        jo_printf(4, 24, "UP/DOWN: SELECT");
        jo_printf(4, 25, "A: CONFIRM");
        jo_printf(4, 26, "B: BACK");
        return;
    }

    if (state->menu_screen == UiMenuScreenBattleModeSelect)
    {
        bool pad2_available = input_mapping_is_player2_available();
        int p2_port = input_mapping_get_player2_port();

        jo_printf(14, 6, "BATTLE MODE");
        jo_printf(8, 10, "%s SINGLE PLAYER", state->menu_battle_selected_option == MenuBattleSinglePlayer ? ">" : " ");
        jo_printf(8, 11, "%s MULTIPLAYER", state->menu_battle_selected_option == MenuBattleMultiplayer ? ">" : " ");

        if (state->menu_pad2_warning_timer > 0)
            jo_printf(4, 22, "CONNECT PAD2 FOR MULTIPLAYER");
        if (pad2_available)
            jo_printf(4, 23, "PAD2: CONNECTED (P%d)", p2_port + 1);
        else
            jo_printf(4, 23, "PAD2: DISCONNECTED");
        jo_printf(4, 24, "UP/DOWN: SELECT");
        jo_printf(4, 25, "A: CONFIRM");
        jo_printf(4, 26, "B: BACK");
        return;
    }

    if (state->menu_screen == UiMenuScreenMultiplayerModeSelect)
    {
        bool pad2_available = input_mapping_is_player2_available();
        int p2_port = input_mapping_get_player2_port();

        jo_printf(13, 6, "MULTIPLAYER MODE");
        jo_printf(8, 10, "%s COOP", state->menu_multiplayer_selected_option == MenuMultiplayerCoop ? "X" : " ");
        jo_printf(8, 11, "%s VERSUS", state->menu_multiplayer_selected_option == MenuMultiplayerVersus ? ">" : " ");

        if (state->menu_pad2_warning_timer > 0)
            jo_printf(4, 22, "CONNECT PAD2 TO START VERSUS");
        if (pad2_available)
            jo_printf(4, 23, "PAD2: CONNECTED (P%d)", p2_port + 1);
        else
            jo_printf(4, 23, "PAD2: DISCONNECTED");
        jo_printf(4, 24, "UP/DOWN: SELECT");
        jo_printf(4, 25, "A: CONFIRM");
        jo_printf(4, 26, "B: BACK");
        return;
    }

    if (state->menu_screen == UiMenuScreenOptions)
    {
        jo_printf(16, 6, "OPTIONS");
        jo_printf(6, 10, "%s CONFIG CONTROLS", state->menu_options_selected_option == MenuOptionsConfigControls ? ">" : " ");
        jo_printf(6, 11, "%s SONG TEST", state->menu_options_selected_option == MenuOptionsSongTest ? ">" : " ");
        
        jo_printf(4, 24, "UP/DOWN: SELECT");
        jo_printf(4, 25, "A: CONFIRM");
        jo_printf(4, 26, "B: BACK");
        return;
    }

    if (state->menu_screen == UiMenuScreenConfigControls)
    {
        bool pad2_available = input_mapping_is_player2_available();
        int p2_port = input_mapping_get_player2_port();

        ui_control_get_mapping_for_player(state->menu_remap_selected_player, &mapping);
        jo_printf(13, 6, "CONTROL MAPPING");
        jo_printf(4,
                  7,
                  "PLAYER: %s  LEFT/RIGHT: CHANGE",
                  menu_remap_player_names[state->menu_remap_selected_player]);
        if (pad2_available)
            jo_printf(4, 8, "PAD2: CONNECTED (P%d)", p2_port + 1);
        else
            jo_printf(4, 8, "PAD2: DISCONNECTED");

        for (idx = 0; idx < MENU_REMAP_ACTION_COUNT; ++idx)
        {
            int key = JO_KEY_A;
            if (idx == MenuRemapMoveLeft) key = mapping.move_left_key;
            else if (idx == MenuRemapMoveRight) key = mapping.move_right_key;
            else if (idx == MenuRemapSpin) key = mapping.spin_key;
            else if (idx == MenuRemapJump) key = mapping.jump_key;
            else if (idx == MenuRemapPunch) key = mapping.punch_key;
            else if (idx == MenuRemapKick) key = mapping.kick_key;

            jo_printf(4, 9 + idx, "%s %s: %s",
                      state->menu_remap_selected_action == idx ? ">" : " ",
                      menu_remap_action_names[idx],
                      ui_control_key_name(key));
        }

        if (state->menu_remap_waiting_key)
            jo_printf(4,
                      25,
                      "PRESS A KEY ON %s...",
                      menu_remap_player_names[state->menu_remap_selected_player]);
        else
            jo_printf(4, 25, "A: REMAP  B: BACK");

        jo_printf(4, 26, "UP/DOWN: SELECT");

        if (menu_remap_swap_message_timer > 0)
        {
            jo_printf(4, 24, "SWAPPED: %s <-> %s",
                      menu_remap_action_names[menu_remap_swap_action_a],
                      menu_remap_action_names[menu_remap_swap_action_b]);
            --menu_remap_swap_message_timer;
        }
        else if (state->menu_pad2_warning_timer > 0)
        {
            jo_printf(4, 24, "PAD2 DISCONNECTED");
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenSongTest)
    {
        jo_printf(15, 6, "SONG TEST");
        jo_printf(4, 9, "%s BGM TRACK: %d", state->song_test_selected_line == SongTestLineBgm ? ">" : " ", state->song_test_bgm_track);
        jo_printf(4, 10, "%s SFX: %s", state->song_test_selected_line == SongTestLineSfx ? ">" : " ", song_test_sfx_names[state->song_test_sfx_index]);
        
        jo_printf(4, 22, "UP/DOWN: SELECT LINE");
        jo_printf(4, 23, "LEFT/RIGHT: CHANGE");
        jo_printf(4, 24, "A: PLAY SELECTED");
        jo_printf(4, 25, "C: STOP BGM");
        jo_printf(4, 26, "B: BACK");
        return;
    }

    jo_printf(12, 7, "SELECT CHARACTER");

    for (idx = 0; idx < UiCharacterCount; ++idx)
    {
        bool is_locked = ui_control_character_is_locked((ui_character_choice_t)idx);
        bool is_hovered = state->menu_selecting_bot_character
            ? (state->menu_cursor_bot_character == (ui_character_choice_t)idx)
            : (state->menu_cursor_character == (ui_character_choice_t)idx);
        bool p1_hovered = (state->menu_cursor_character == (ui_character_choice_t)idx);
        bool p2_hovered = (state->menu_cursor_player2_character == (ui_character_choice_t)idx);
        int pointer_base_x = (item_x[idx] / 8) - 2;
        int pointer_base_y = (MENU_SPRITE_Y / 8) + 1;

        if (idx == UiCharacterSonic)
            sprite_id = menu_sonic_jump_sprite_id;
        else if (idx == UiCharacterAmy)
            sprite_id = menu_amy_jump_sprite_id;
        else if (idx == UiCharacterTails)
            sprite_id = menu_tails_jump_sprite_id;
        else if (idx == UiCharacterKnuckles)
            sprite_id = menu_knuckles_jump_sprite_id;
        else
            sprite_id = menu_shadow_jump_sprite_id;

        {
            int name_x = (item_x[idx] / 8) - 2;
            if (idx == UiCharacterKnuckles)
                --name_x;
            else if (idx == UiCharacterShadow)
                ++name_x;
            jo_printf(name_x, 9, "%s", character_names[idx]);
        }
        if (sprite_id >= 0)
        {
            if (is_locked)
            {
                jo_sprite_enable_half_transparency();
                jo_sprite_enable_shadow_filter();
            }
            jo_sprite_draw3D2(sprite_id, item_x[idx]-16, MENU_SPRITE_Y, 420);
            if (is_locked)
            {
                jo_sprite_disable_shadow_filter();
                jo_sprite_disable_half_transparency();
            }
        }

        if (ui_control_is_multiplayer_versus(state))
        {
            if (p1_hovered && p2_hovered)
            {
                jo_printf(pointer_base_x, pointer_base_y, "1%s", is_locked ? "X" : ">");
                jo_printf(pointer_base_x, pointer_base_y + 1, "2%s", is_locked ? "X" : ">");
            }
            else
            {
                if (p1_hovered)
                    jo_printf(pointer_base_x, pointer_base_y, "1%s", is_locked ? "X" : ">");
                if (p2_hovered)
                    jo_printf(pointer_base_x, pointer_base_y, "2%s", is_locked ? "X" : ">");
            }
        }
        else
        {
            jo_printf((item_x[idx] / 8) - 2,
                      (MENU_SPRITE_Y / 8) + 1,
                      "%s",
                      is_hovered ? (is_locked ? "X" : ">") : " ");
        }
    }

    if (ui_control_is_multiplayer_versus(state))
    {
        bool blink_visible = ui_control_current_name_visible();
        bool show_p1 = state->menu_player1_confirmed || blink_visible;
        bool show_p2 = state->menu_player2_confirmed || blink_visible;
        const char *p1_name = show_p1 ? character_names[state->menu_cursor_character] : "";
        const char *p2_name = show_p2 ? character_names[state->menu_cursor_player2_character] : "";

        jo_printf(4,
                  20,
                  "CURRENT: [%-8s] VS [%-8s]",
                  p1_name,
                  p2_name);
    }
    else if (state->menu_selecting_bot_character)
        jo_printf(4,
                  20,
                  "CURRENT: %s VS %s",
                  character_names[state->menu_selected_character],
                  character_names[state->menu_cursor_bot_character]);
    else
        jo_printf(4, 20, "CURRENT: %s", character_names[state->menu_cursor_character]);

    jo_printf(4, 21, "BOTS: %d", state->menu_bot_count);
    if (ui_control_is_multiplayer_versus(state))
    {
        int p2_port = input_mapping_get_player2_port();
        if (p2_port >= 0)
            jo_printf(4, 22, "PAD2: CONNECTED (P%d)", p2_port + 1);
        else
            jo_printf(4, 22, "PAD2: DISCONNECTED");
        if (state->menu_pad2_warning_timer > 0)
            jo_printf(4, 19, "P2 DISCONNECTED");
    }

    jo_printf(4, 23, "LEFT/RIGHT: CHAR");
    jo_printf(4, 24, "UP/DOWN: BOTS");
    jo_printf(4, 25, state->menu_selecting_bot_character ? "A: START GAME" : "A: CONFIRM CHAR");
    jo_printf(4, 26, "B: BACK");
}

void ui_control_draw_loading(void)
{
    jo_clear_background(JO_COLOR_Black);
    jo_printf(4, 26, "LOADING...");
}

void ui_control_handle_menu_input(ui_control_state_t *state, ui_control_start_game_fn on_start_game, void *user_data)
{
    ui_control_sync_menu_bgm(state);

    if (state->menu_pad2_warning_timer > 0)
        state->menu_pad2_warning_timer--;

    bool pad2_available = input_mapping_is_player2_available();
    bool pressed_up = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_UP), &state->menu_up_released);
    bool pressed_down = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_DOWN), &state->menu_down_released);
    bool pressed_left = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_LEFT), &state->menu_left_released);
    bool pressed_right = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_RIGHT), &state->menu_right_released);
    bool pressed_a = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_A), &state->menu_a_released);
    bool pressed_b = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_B), &state->menu_b_released);
    bool pressed_c = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_C), &state->menu_c_released);
    bool pressed_up_p2 = ui_control_consume_pressed(input_mapping_is_player2_key_down(JO_KEY_UP), &state->menu_p2_up_released);
    bool pressed_down_p2 = ui_control_consume_pressed(input_mapping_is_player2_key_down(JO_KEY_DOWN), &state->menu_p2_down_released);
    bool pressed_left_p2 = ui_control_consume_pressed(input_mapping_is_player2_key_down(JO_KEY_LEFT), &state->menu_p2_left_released);
    bool pressed_right_p2 = ui_control_consume_pressed(input_mapping_is_player2_key_down(JO_KEY_RIGHT), &state->menu_p2_right_released);
    bool pressed_a_p2 = ui_control_consume_pressed(input_mapping_is_player2_key_down(JO_KEY_A), &state->menu_p2_a_released);
    bool pressed_b_p2 = ui_control_consume_pressed(input_mapping_is_player2_key_down(JO_KEY_B), &state->menu_p2_b_released);

    if (state->menu_screen == UiMenuScreenConfigControls)
    {
        bool pressed_up_active = (state->menu_remap_selected_player == 1) ? pressed_up_p2 : pressed_up;
        bool pressed_down_active = (state->menu_remap_selected_player == 1) ? pressed_down_p2 : pressed_down;
        bool pressed_a_active = (state->menu_remap_selected_player == 1) ? pressed_a_p2 : pressed_a;

        if (state->menu_remap_waiting_key)
        {
            if (state->menu_remap_wait_release)
            {
                if (!ui_control_any_mappable_key_down_for_player(state->menu_remap_selected_player))
                    state->menu_remap_wait_release = false;
                return;
            }

            {
                int captured_key = 0;
                if (ui_control_try_capture_menu_key_for_player(state->menu_remap_selected_player, &captured_key))
                {
                    input_mapping_config_t mapping;
                    int swapped_action;

                    ui_control_get_mapping_for_player(state->menu_remap_selected_player, &mapping);
                    swapped_action = ui_control_set_mapping_action(&mapping, state->menu_remap_selected_action, captured_key);
                    ui_control_set_mapping_for_player(state->menu_remap_selected_player, &mapping);

                    if (swapped_action >= 0)
                    {
                        menu_remap_swap_action_a = state->menu_remap_selected_action;
                        menu_remap_swap_action_b = swapped_action;
                        menu_remap_swap_message_timer = MENU_REMAP_SWAP_MESSAGE_FRAMES;
                    }

                    state->menu_remap_waiting_key = false;
                    state->menu_remap_wait_release = true;
                    state->menu_a_released = false;
                }
            }
            return;
        }

        if (pressed_left)
        {
            if (state->menu_remap_selected_player == 0)
                state->menu_remap_selected_player = MENU_REMAP_PLAYER_COUNT - 1;
            else
                state->menu_remap_selected_player--;

            state->menu_remap_waiting_key = false;
            state->menu_remap_wait_release = false;
        }
        else if (pressed_right)
        {
            state->menu_remap_selected_player = (state->menu_remap_selected_player + 1) % MENU_REMAP_PLAYER_COUNT;
            state->menu_remap_waiting_key = false;
            state->menu_remap_wait_release = false;
        }
        else if (pressed_up_active)
        {
            if (state->menu_remap_selected_action == 0)
                state->menu_remap_selected_action = MENU_REMAP_ACTION_COUNT - 1;
            else
                state->menu_remap_selected_action--;
        }
        else if (pressed_down_active)
        {
            if (state->menu_remap_selected_action == MENU_REMAP_ACTION_COUNT - 1)
                state->menu_remap_selected_action = 0;
            else
                state->menu_remap_selected_action++;
        }
        else if (pressed_a_active)
        {
            if (state->menu_remap_selected_player == 1 && !pad2_available)
            {
                state->menu_pad2_warning_timer = MENU_PAD2_WARNING_FRAMES;
                return;
            }
            state->menu_remap_waiting_key = true;
        }
        else if (pressed_b || pressed_b_p2)
        {
            state->menu_screen = UiMenuScreenOptions;
            state->menu_remap_waiting_key = false;
            state->menu_remap_wait_release = false;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenMain)
    {
        if (pressed_up || pressed_down)
            state->menu_main_selected_option = (state->menu_main_selected_option + 1) % MENU_MAIN_OPTION_COUNT;
        else if (pressed_a)
        {
            if (state->menu_main_selected_option == MenuMainStartGame)
                state->menu_screen = UiMenuScreenModeSelect;
            else
                state->menu_screen = UiMenuScreenOptions;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenModeSelect)
    {
        if (pressed_up)
        {
            if (state->menu_mode_selected_option == 0)
                state->menu_mode_selected_option = MENU_MODE_OPTION_COUNT - 1;
            else
                state->menu_mode_selected_option--;
        }
        else if (pressed_down)
        {
            state->menu_mode_selected_option = (state->menu_mode_selected_option + 1) % MENU_MODE_OPTION_COUNT;
        }
        else if (pressed_a)
        {
            if (state->menu_mode_selected_option == MenuModeBattle)
                state->menu_screen = UiMenuScreenBattleModeSelect;
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenMain;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenBattleModeSelect)
    {
        if (pressed_up)
        {
            if (state->menu_battle_selected_option == 0)
                state->menu_battle_selected_option = MENU_BATTLE_OPTION_COUNT - 1;
            else
                state->menu_battle_selected_option--;
        }
        else if (pressed_down)
        {
            state->menu_battle_selected_option = (state->menu_battle_selected_option + 1) % MENU_BATTLE_OPTION_COUNT;
        }
        else if (pressed_a)
        {
            if (state->menu_battle_selected_option == MenuBattleSinglePlayer)
            {
                state->menu_multiplayer_versus = false;
                state->menu_screen = UiMenuScreenCharacterSelect;
                state->menu_selecting_bot_character = false;
                state->menu_selecting_player2_character = false;
                state->menu_cursor_character = state->menu_selected_character;
                state->menu_cursor_bot_character = state->menu_selected_bot_character;
            }
            else
            {
                if (!pad2_available)
                {
                    state->menu_pad2_warning_timer = MENU_PAD2_WARNING_FRAMES;
                    return;
                }
                state->menu_screen = UiMenuScreenMultiplayerModeSelect;
            }
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenModeSelect;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenMultiplayerModeSelect)
    {
        if (pressed_up)
        {
            if (state->menu_multiplayer_selected_option == 0)
                state->menu_multiplayer_selected_option = MENU_MULTIPLAYER_OPTION_COUNT - 1;
            else
                state->menu_multiplayer_selected_option--;
        }
        else if (pressed_down)
        {
            state->menu_multiplayer_selected_option = (state->menu_multiplayer_selected_option + 1) % MENU_MULTIPLAYER_OPTION_COUNT;
        }
        else if (pressed_a)
        {
            if (state->menu_multiplayer_selected_option == MenuMultiplayerVersus)
            {
                if (!pad2_available)
                {
                    state->menu_pad2_warning_timer = MENU_PAD2_WARNING_FRAMES;
                    return;
                }
                state->menu_multiplayer_versus = true;
                state->menu_screen = UiMenuScreenCharacterSelect;
                state->menu_selecting_player2_character = false;
                state->menu_player1_confirmed = false;
                state->menu_player2_confirmed = false;
                state->menu_cursor_character = state->menu_selected_character;
                state->menu_cursor_player2_character = state->menu_selected_player2_character;
                if (state->menu_bot_count < 0)
                    state->menu_bot_count = 0;
            }
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenBattleModeSelect;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenCharacterSelect)
    {
        if (ui_control_is_multiplayer_versus(state))
        {
            if (!pad2_available && state->menu_player2_confirmed)
                state->menu_player2_confirmed = false;

            if (pressed_up)
            {
                if (state->menu_bot_count < 2)
                    state->menu_bot_count++;
            }
            else if (pressed_down)
            {
                if (state->menu_bot_count > 0)
                    state->menu_bot_count--;
            }

            if (!state->menu_player1_confirmed && pressed_left)
            {
                ui_character_choice_t *target_cursor = &state->menu_cursor_character;
                *target_cursor = ui_control_prev_character(*target_cursor);
            }
            if (!state->menu_player1_confirmed && pressed_right)
            {
                ui_character_choice_t *target_cursor = &state->menu_cursor_character;
                *target_cursor = ui_control_next_character(*target_cursor);
            }

            if (pad2_available && !state->menu_player2_confirmed && pressed_left_p2)
            {
                state->menu_cursor_player2_character = ui_control_prev_character(state->menu_cursor_player2_character);
            }
            if (pad2_available && !state->menu_player2_confirmed && pressed_right_p2)
            {
                state->menu_cursor_player2_character = ui_control_next_character(state->menu_cursor_player2_character);
            }

            if (pressed_a)
            {
                if (!state->menu_player1_confirmed && !ui_control_character_is_locked(state->menu_cursor_character))
                {
                    state->menu_selected_character = state->menu_cursor_character;
                    state->menu_player1_confirmed = true;
                }
            }

            if (pad2_available && pressed_a_p2)
            {
                if (!state->menu_player2_confirmed && !ui_control_character_is_locked(state->menu_cursor_player2_character))
                {
                    state->menu_selected_player2_character = state->menu_cursor_player2_character;
                    state->menu_player2_confirmed = true;
                }
            }

            if (pressed_b)
            {
                if (state->menu_player1_confirmed)
                    state->menu_player1_confirmed = false;
                else
                    state->menu_screen = UiMenuScreenMultiplayerModeSelect;
            }

            if (pad2_available && pressed_b_p2)
            {
                if (state->menu_player2_confirmed)
                    state->menu_player2_confirmed = false;
            }

            if (!pad2_available)
                state->menu_pad2_warning_timer = MENU_PAD2_WARNING_FRAMES;

            if (pad2_available && state->menu_player1_confirmed && state->menu_player2_confirmed)
            {
                state->menu_selected_bot_character = state->menu_selected_player2_character;
                on_start_game(state->menu_selected_character,
                              state->menu_selected_player2_character,
                              user_data);
            }
            return;
        }

        if (pressed_up)
        {
            if (state->menu_bot_count < 2)
                state->menu_bot_count++;
        }
        else if (pressed_down)
        {
            if (state->menu_bot_count > 0)
                state->menu_bot_count--;
        }
        else if (pressed_left)
        {
            ui_character_choice_t *target_cursor = state->menu_selecting_bot_character
                                                       ? &state->menu_cursor_bot_character
                                                       : &state->menu_cursor_character;

            *target_cursor = ui_control_prev_character(*target_cursor);
        }
        else if (pressed_right)
        {
            ui_character_choice_t *target_cursor = state->menu_selecting_bot_character
                                                       ? &state->menu_cursor_bot_character
                                                       : &state->menu_cursor_character;

            *target_cursor = ui_control_next_character(*target_cursor);
        }
        else if (pressed_a)
        {
            ui_character_choice_t cursor_choice = state->menu_selecting_bot_character
                ? state->menu_cursor_bot_character
                : state->menu_cursor_character;

            if (ui_control_character_is_locked(cursor_choice))
                return;

            if (!state->menu_selecting_bot_character)
            {
                state->menu_selected_character = state->menu_cursor_character;
                state->menu_selected_bot_character = state->menu_selected_character;
                state->menu_cursor_bot_character = state->menu_selected_character;
                state->menu_selecting_bot_character = true;
            }
            else
            {
                state->menu_selected_bot_character = state->menu_cursor_bot_character;
                on_start_game(state->menu_selected_character,
                              state->menu_selected_bot_character,
                              user_data);
            }
        }
        else if (pressed_b)
        {
            if (state->menu_selecting_bot_character)
            {
                state->menu_selecting_bot_character = false;
                state->menu_cursor_character = state->menu_selected_character;
            }
            else
                state->menu_screen = UiMenuScreenBattleModeSelect;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenOptions)
    {
        if (pressed_up || pressed_down)
            state->menu_options_selected_option = (state->menu_options_selected_option + 1) % MENU_OPTIONS_OPTION_COUNT;
        else if (pressed_a)
        {
            if (state->menu_options_selected_option == MenuOptionsConfigControls)
                state->menu_screen = UiMenuScreenConfigControls;
            else
                state->menu_screen = UiMenuScreenSongTest;
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenMain;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenSongTest)
    {
        if (pressed_up)
        {
            if (state->song_test_selected_line == 0)
                state->song_test_selected_line = SONG_TEST_LINE_COUNT - 1;
            else
                state->song_test_selected_line--;
        }
        else if (pressed_down)
        {
            state->song_test_selected_line = (state->song_test_selected_line + 1) % SONG_TEST_LINE_COUNT;
        }
        else if (pressed_left)
        {
            if (state->song_test_selected_line == SongTestLineBgm)
            {
                if (state->song_test_bgm_track <= SONG_TEST_MIN_TRACK)
                    state->song_test_bgm_track = SONG_TEST_MAX_TRACK;
                else
                    state->song_test_bgm_track--;
            }
            else
            {
                if (state->song_test_sfx_index == 0)
                    state->song_test_sfx_index = SONG_TEST_SFX_COUNT - 1;
                else
                    state->song_test_sfx_index--;
            }
        }
        else if (pressed_right)
        {
            if (state->song_test_selected_line == SongTestLineBgm)
            {
                if (state->song_test_bgm_track >= SONG_TEST_MAX_TRACK)
                    state->song_test_bgm_track = SONG_TEST_MIN_TRACK;
                else
                    state->song_test_bgm_track++;
            }
            else
            {
                state->song_test_sfx_index = (state->song_test_sfx_index + 1) % SONG_TEST_SFX_COUNT;
            }
        }
        else if (pressed_a)
        {
            if (state->song_test_selected_line == SongTestLineBgm)
            {
                jo_audio_stop_cd();
                jo_audio_play_cd_track(state->song_test_bgm_track, state->song_test_bgm_track, true);
            }
            else
            {
                game_audio_play_sfx_next_channel(ui_control_song_test_get_sfx(state->song_test_sfx_index));
            }
        }
        else if (pressed_c)
        {
            jo_audio_stop_cd();
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenOptions;
        }
    }
}

void ui_control_handle_pause_input(ui_control_state_t *state,
                                   ui_control_action_fn on_reset_fight,
                                   ui_control_action_fn on_character_select,
                                   void *user_data)
{
    if (jo_is_pad1_key_down(JO_KEY_L) && jo_is_pad1_key_down(JO_KEY_R))
    {
        if (state->pause_lr_released)
        {
            if (state->pause_selected_option == UiPauseOptionDebug)
            {
                if (state->debug_mode == (ui_debug_mode_t)(UiDebugModeCount - 1))
                    state->debug_mode = UiDebugModeOff;
                else
                    state->debug_mode = (ui_debug_mode_t)(state->debug_mode + 1);

                state->debug_enabled = (state->debug_mode != UiDebugModeOff);
            }
            else if (state->pause_selected_option == UiPauseOptionLogs)
            {
                if (state->log_mode == (runtime_log_mode_t)(RuntimeLogModeCount - 1))
                    state->log_mode = RuntimeLogModeOff;
                else
                    state->log_mode = (runtime_log_mode_t)(state->log_mode + 1);

                runtime_log_set_mode(state->log_mode);
            }
            else if (state->pause_selected_option == UiPauseOptionLogPage)
            {
                int next_page = runtime_log_get_sprite_page() + 1;
                int page_count = runtime_log_get_sprite_page_count();

                if (next_page >= page_count)
                    next_page = 0;
                runtime_log_set_sprite_page(next_page);
            }
            else
            {
                state->debug_enabled = (state->debug_mode != UiDebugModeOff);
            }
        }
        state->pause_lr_released = false;
    }
    else
    {
        state->pause_lr_released = true;
    }

    if (jo_is_pad1_key_down(JO_KEY_UP))
    {
        if (state->pause_up_released)
        {
            if (state->pause_selected_option == UiPauseOptionContinue)
                state->pause_selected_option = (ui_pause_option_t)(UiPauseOptionCount - 1);
            else
                state->pause_selected_option = (ui_pause_option_t)(state->pause_selected_option - 1);
        }
        state->pause_up_released = false;
    }
    else
    {
        state->pause_up_released = true;
    }

    if (jo_is_pad1_key_down(JO_KEY_DOWN))
    {
        if (state->pause_down_released)
        {
            if (state->pause_selected_option == (ui_pause_option_t)(UiPauseOptionCount - 1))
                state->pause_selected_option = UiPauseOptionContinue;
            else
                state->pause_selected_option = (ui_pause_option_t)(state->pause_selected_option + 1);
        }
        state->pause_down_released = false;
    }
    else
    {
        state->pause_down_released = true;
    }

    if (jo_is_pad1_key_down(JO_KEY_A))
    {
        if (state->pause_a_released)
        {
            if (state->pause_selected_option == UiPauseOptionContinue)
                state->game_paused = false;
            else if (state->pause_selected_option == UiPauseOptionResetFight)
            {
                on_reset_fight(user_data);
                state->game_paused = false;
            }
            else if (state->pause_selected_option == UiPauseOptionCharacterSelect)
            {
                on_character_select(user_data);
                state->game_paused = false;
                state->current_game_state = UiGameStateMenu;
                state->menu_screen = UiMenuScreenCharacterSelect;
                state->menu_selecting_bot_character = false;
                state->menu_selecting_player2_character = false;
                state->menu_player1_confirmed = false;
                state->menu_player2_confirmed = false;
                state->menu_cursor_character = state->menu_selected_character;
                state->menu_cursor_bot_character = state->menu_selected_bot_character;
                state->menu_cursor_player2_character = state->menu_selected_player2_character;
                state->pause_selected_option = UiPauseOptionContinue;
                state->menu_a_released = false;
            }
        }

        state->pause_a_released = false;
    }
    else
    {
        state->pause_a_released = true;
    }

    if (jo_is_pad1_key_down(JO_KEY_START))
    {
        if (state->pause_start_released)
            state->game_paused = false;

        state->pause_start_released = false;
    }
    else
    {
        state->pause_start_released = true;
    }
}

void ui_control_handle_start_toggle(ui_control_state_t *state)
{
    if (jo_is_pad1_key_down(JO_KEY_START))
    {
        if (state->pause_start_released)
            state->game_paused = !state->game_paused;

        state->pause_start_released = false;
    }
    else
    {
        state->pause_start_released = true;
    }
}
