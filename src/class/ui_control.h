#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include <jo/jo.h>
#include "runtime_log.h"

typedef enum
{
    UiGameStateMenu,
    UiGameStateLoading,
    UiGameStatePlaying
} ui_game_state_t;

typedef enum
{
    UiCharacterSonic = 0,
    UiCharacterAmy,
    UiCharacterTails,
    UiCharacterKnuckles,
    UiCharacterShadow,
    UiCharacterCount
} ui_character_choice_t;

typedef enum
{
    UiPauseOptionContinue = 0,
    UiPauseOptionResetFight,
    UiPauseOptionCharacterSelect,
    UiPauseOptionBalance,
    UiPauseOptionPauseBots,
    UiPauseOptionDebug,
    UiPauseOptionLogs,
    UiPauseOptionLogPage,
    UiPauseOptionCount
} ui_pause_option_t;

typedef enum
{
    UiControllerNone = 0,
    UiControllerPlayer1,
    UiControllerPlayer2,
    UiControllerCpu,
    UiControllerCount
} ui_controller_type_t;

typedef enum
{
    UiDebugModeOff = 0,
    UiDebugModeHardware,
    UiDebugModePlayer,
    UiDebugModeAttack,
    //UiDebugModeDamage,
    UiDebugModeSpawn,
    UiDebugModeCount
} ui_debug_mode_t;

typedef enum
{
    UiMenuScreenMain = 0,
    UiMenuScreenModeSelect,
    UiMenuScreenBattleModeSelect,
    UiMenuScreenMultiplayerModeSelect,
    UiMenuScreenCharacterSelect,
    UiMenuScreenGroupAssign,
    UiMenuScreenMapSelect,
    UiMenuScreenOptions,
    UiMenuScreenConfigControls,
    UiMenuScreenSongTest,
    UiMenuScreenDebugBalance
} ui_menu_screen_t;

#define UI_MENU_MAX_MAPS 16
#define UI_MENU_MAX_MAP_NAME_LEN 24

typedef struct
{
    ui_game_state_t current_game_state;
    ui_character_choice_t menu_selected_character;
    ui_character_choice_t menu_selected_bot_character;
    ui_character_choice_t menu_selected_player2_character;
    ui_character_choice_t menu_cursor_character;
    ui_character_choice_t menu_cursor_bot_character;
    ui_character_choice_t menu_cursor_player2_character;
    ui_controller_type_t menu_character_controller[UiCharacterCount];
    int menu_character_group[UiCharacterCount];
    ui_character_choice_t menu_group_order[UiCharacterCount];
    int menu_group_count;
    int menu_group_cursor;
    bool menu_selecting_bot_character;
    bool menu_selecting_player2_character;
    bool menu_player1_confirmed;
    bool menu_player2_confirmed;
    bool menu_multiplayer_versus;
    int menu_multiplayer_selected_option;
    int menu_bot_count;
    bool menu_start_released;
    bool menu_up_released;
    bool menu_down_released;
    bool menu_left_released;
    bool menu_right_released;
    bool menu_a_released;
    bool menu_b_released;
    bool menu_c_released;
    bool menu_p2_left_released;
    bool menu_p2_right_released;
    bool menu_p2_up_released;
    bool menu_p2_down_released;
    bool menu_p2_a_released;
    bool menu_p2_b_released;
    ui_menu_screen_t menu_screen;
    int menu_main_selected_option;
    int menu_mode_selected_option;
    int menu_battle_selected_option;
    int menu_options_selected_option;
    bool menu_remap_mode;
    bool menu_remap_waiting_key;
    bool menu_remap_wait_release;
    int menu_remap_selected_player;
    int menu_remap_selected_action;
    int menu_pad2_warning_timer;
    int song_test_selected_line;
    int song_test_bgm_track;
    int song_test_sfx_index;

    /* Map selection (loaded from /MAP/MAPS.TXT) */
    int menu_map_count;
    int menu_map_cursor;
    char menu_map_names[UI_MENU_MAX_MAPS][UI_MENU_MAX_MAP_NAME_LEN];
    char menu_selected_map_name[UI_MENU_MAX_MAP_NAME_LEN];

    bool game_paused;
    bool pause_bots;
    bool debug_enabled;
    ui_debug_mode_t debug_mode;
    runtime_log_mode_t log_mode;
    ui_pause_option_t pause_selected_option;
    bool pause_up_released;
    bool pause_down_released;
    bool pause_left_released;
    bool pause_right_released;
    bool pause_a_released;
    bool pause_b_released;
    bool pause_start_released;
    bool pause_lr_released;
    bool pause_c_released;
    bool pause_text_dirty;

    /* Debug balance submenu state */
    int debug_balance_selected_row;
    int debug_balance_selected_col;

    /* Balance submenu hold-repeat counters (for faster adjustments) */
    int debug_balance_hold_left_frames;
    int debug_balance_hold_right_frames;
} ui_control_state_t;

typedef void (*ui_control_start_game_fn)(ui_character_choice_t selected_character,
                                         ui_character_choice_t selected_bot_character,
                                         void *user_data);
typedef void (*ui_control_action_fn)(void *user_data);

void ui_control_init(ui_control_state_t *state);
void ui_control_clear_text_layer(void);
void ui_control_draw_character_menu(const ui_control_state_t *state);
void ui_control_draw_loading(void);
void ui_control_draw_pause_menu(const ui_control_state_t *state);
void ui_control_handle_menu_input(ui_control_state_t *state, ui_control_start_game_fn on_start_game, void *user_data);
void ui_control_handle_pause_input(ui_control_state_t *state,
                                   ui_control_action_fn on_reset_fight,
                                   ui_control_action_fn on_character_select,
                                   void *user_data);
void ui_control_handle_start_toggle(ui_control_state_t *state);
void ui_control_reset_menu_bgm_state(void);
void ui_control_reset_menu_sprites(void);

#endif