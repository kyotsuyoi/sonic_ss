#include <jo/jo.h>
#include <string.h>
#include "ui_control.h"
#include "player.h"
#include "input_mapping.h"
#include "game_audio.h"
#include "rotating_sprite_pool.h"
#include "runtime_log.h"
#include "debug.h"
#include "world_map.h"
#include "vram_cache.h"
#include "game_loop.h"
#include "game_flow.h"

/* Must match CAMERA_TARGET_SCREEN_X/Y in game_loop.c for debug output. */
#define CAMERA_TARGET_SCREEN_X 130
#define CAMERA_TARGET_SCREEN_Y 75

#define MENU_SPRITE_Y 88
#define MENU_NAME_Y 6
#define MENU_REMAP_ACTION_COUNT 6
#define MENU_REMAP_SWAP_MESSAGE_FRAMES 240
#define MENU_PAD2_WARNING_FRAMES 180
#define MENU_MAIN_OPTION_COUNT 2
#define MENU_MODE_OPTION_COUNT 2
#define MENU_BATTLE_OPTION_COUNT 2
#define MENU_MULTIPLAYER_OPTION_COUNT 2
#define MENU_OPTIONS_OPTION_COUNT 3
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
    MenuOptionsSongTest,
    MenuOptionsDiagnostics
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

static bool ui_control_character_is_locked(const ui_control_state_t *state, ui_character_choice_t choice)
{
    /* "Out" means the character is not participating in the battle. */
    return state->menu_character_controller[choice] == UiControllerNone;
}

static bool ui_control_is_multiplayer_versus(const ui_control_state_t *state)
{
    return state->menu_multiplayer_versus;
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

static void ui_control_load_map_list(ui_control_state_t *state)
{
    char *contents = jo_fs_read_file_in_dir("MAPS.TXT", "MAP", JO_NULL);
    if (contents == JO_NULL)
    {
        strncpy(state->menu_map_names[0], "GHS.MAP", UI_MENU_MAX_MAP_NAME_LEN - 1);
        state->menu_map_names[0][UI_MENU_MAX_MAP_NAME_LEN - 1] = '\0';
        state->menu_map_count = 1;
        return;
    }

    int count = 0;
    char *line = contents;
    while (*line && count < UI_MENU_MAX_MAPS)
    {
        char *end = line;
        while (*end && *end != '\n' && *end != '\r')
            end++;

        int len = (int)(end - line);
        while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
            len--;

        if (len > 0)
        {
            int copy_len = len;
            if (copy_len >= UI_MENU_MAX_MAP_NAME_LEN)
                copy_len = UI_MENU_MAX_MAP_NAME_LEN - 1;
            memcpy(state->menu_map_names[count], line, copy_len);
            state->menu_map_names[count][copy_len] = '\0';
            count++;
        }

        while (*end == '\r' || *end == '\n')
            end++;
        line = end;
    }

    if (count == 0)
    {
        strncpy(state->menu_map_names[0], "GHS.MAP", UI_MENU_MAX_MAP_NAME_LEN - 1);
        state->menu_map_names[0][UI_MENU_MAX_MAP_NAME_LEN - 1] = '\0';
        count = 1;
    }

    state->menu_map_count = count;
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
    state->diag_runtime_log = false;
    state->diag_vram_uploads = true;
    state->diag_bot_updates = true;
    state->diag_show_fps = true;
    state->diag_draw_backgrounds = true;
    state->diag_disable_draw = false;
    state->diag_disable_cd_audio = false;
    state->diag_menu_only = false;
    state->diag_selected_option = 0;
    state->menu_selected_character = UiCharacterSonic;
    state->menu_selected_bot_character = UiCharacterAmy;
    state->menu_selected_player2_character = UiCharacterAmy;
    state->menu_cursor_character = UiCharacterSonic;
    state->menu_cursor_bot_character = UiCharacterAmy;
    state->menu_cursor_player2_character = UiCharacterAmy;
    for (int i = 0; i < UiCharacterCount; ++i)
    {
        state->menu_character_controller[i] = UiControllerNone;
        state->menu_character_group[i] = 0;
    }
    state->menu_character_controller[UiCharacterSonic] = UiControllerPlayer1;
    state->menu_group_count = 0;
    state->menu_group_cursor = 0;
    state->menu_selecting_bot_character = false;
    state->menu_selecting_player2_character = false;
    state->menu_player1_confirmed = false;
    state->menu_player2_confirmed = false;
    state->menu_multiplayer_versus = false;
    state->menu_multiplayer_selected_option = MenuMultiplayerVersus;
    state->menu_bot_count = 0;
    state->menu_start_released = true;
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
    state->debug_balance_selected_row = 0;
    state->debug_balance_selected_col = 0;
    state->debug_balance_hold_left_frames = 0;
    state->debug_balance_hold_right_frames = 0;

    state->menu_map_count = 0;
    state->menu_map_cursor = 0;
    state->menu_map_names[0][0] = '\0';
    state->menu_selected_map_name[0] = '\0';

    ui_control_load_map_list(state);

    runtime_log_set_mode(state->log_mode);
    state->pause_selected_option = UiPauseOptionContinue;
    state->pause_up_released = true;
    state->pause_down_released = true;
    state->pause_left_released = true;
    state->pause_right_released = true;
    state->pause_a_released = true;
    state->pause_b_released = true;
    state->pause_start_released = true;
    state->pause_c_released = true;
    state->pause_lr_released = true;
    state->pause_text_dirty = true;
}

void ui_control_clear_text_layer(void)
{
    int line;
    for (line = 0; line < 30; ++line)
        jo_printf(0, line, "                                        ");
}

void ui_control_draw_pause_menu(const ui_control_state_t *state)
{
    ui_control_clear_text_layer();

    static const char *debug_mode_label[UiDebugModeCount] = {"OFF", "HARDWARE", "PLAYER", "CAMERA", "ATK SPRITE", "MEM", "SPAWN"};
    static const char *log_mode_label[RuntimeLogModeCount] = {"OFF", "SYSTEM", "VERBOSE", "CART RAM", "SPRITE"};
    int sprite_log_page = runtime_log_get_sprite_page() + 1;
    int sprite_log_page_count = runtime_log_get_sprite_page_count();

    jo_printf(17, 6, "PAUSED");

    if (state->menu_screen == UiMenuScreenDebugBalance)
    {
        static const char *character_names[] = {"SONIC", "AMY", "TAILS", "KNUCKLES", "SHADOW"};
        static const char *attack_names[DebugBalanceAttackCount] = {"P01", "P02", "K01", "K02", "AIR", "CHG", "SPN"};
        debug_balance_profile_t *profile = debug_balance_get_profile(player.character_id);

        int row = state->debug_balance_selected_row;
        (void)state->debug_balance_selected_col; // keep compiler happy

        jo_printf(6, 8, "BALANCE: %s", character_names[player.character_id]);
        jo_printf(4, 10, "ROW  DMG   KB    STN   IMP");

        for (int i = 0; i < DebugBalanceAttackCount; ++i)
        {
            int y = 12 + i;
            const char *marker = (i == row) ? ">" : " ";
            int dmg = profile ? profile->damage[i] : 0;
            int kb = profile ? profile->knockback[i] : 0;
            int stn = profile ? profile->stun[i] : 0;
            int imp = profile ? profile->impulse[i] : 0;
            jo_printf(2, y, "%s %s   %2d   %3d   %3d   %3d", marker, attack_names[i], dmg, kb, stn, imp);
        } 

        const char *target_name = "?";
        const char *attacker_name = "?";

        switch (g_debug_last_damage_dealt_target)
        {
            case CHARACTER_ID_SONIC: target_name = "SNC"; break;
            case CHARACTER_ID_AMY: target_name = "AMY"; break;
            case CHARACTER_ID_TAILS: target_name = "TLS"; break;
            case CHARACTER_ID_KNUCKLES: target_name = "KNK"; break;
            case CHARACTER_ID_SHADOW: target_name = "SDW"; break;
            default: target_name = "?"; break;
        }      

        switch (g_debug_last_damage_received_from)
        {
            case CHARACTER_ID_SONIC: attacker_name = "SNC"; break;
            case CHARACTER_ID_AMY: attacker_name = "AMY"; break;
            case CHARACTER_ID_TAILS: attacker_name = "TLS"; break;
            case CHARACTER_ID_KNUCKLES: attacker_name = "KNK"; break;
            case CHARACTER_ID_SHADOW: attacker_name = "SDW"; break;
            default: attacker_name = "?"; break;
        }

        //Last Damage, Knockback and Stun received and dealt
        jo_printf(2, 20, "DMG/KB/STN DEALT: %2d/%2d/%2d (-> %s)", g_debug_last_damage_dealt, g_debug_last_knockback_dealt, g_debug_last_stun_dealt, target_name);
        jo_printf(2, 21, "DMG/KB/STN RECEI: %2d/%2d/%2d (<- %s)", g_debug_last_damage_received, g_debug_last_knockback_received, g_debug_last_stun_received, attacker_name);
        
        jo_printf(2, 23, "LEFT/RIGHT: +/-  UP/DOWN: ROW");
        jo_printf(2, 24, "A: COL  B: BACK  C: RESET");    
        
        return;
    }

    jo_printf(8, 9, "%s CONTINUE", state->pause_selected_option == UiPauseOptionContinue ? ">" : " ");   
    jo_printf(8, 10, "%s RESET FIGHT", state->pause_selected_option == UiPauseOptionResetFight ? ">" : " ");
    jo_printf(8, 11, "%s CHARACTER SELECT", state->pause_selected_option == UiPauseOptionCharacterSelect ? ">" : " ");
    
    jo_printf(8, 13, "%s BALANCE", state->pause_selected_option == UiPauseOptionBalance ? ">" : " ");
    jo_printf(8, 14, "%s FREEZE BOTS: %s", state->pause_selected_option == UiPauseOptionPauseBots ? ">" : " ", state->pause_bots ? "ON" : "OFF");
    jo_printf(8, 15, "%s DEBUG: %s", state->pause_selected_option == UiPauseOptionDebug ? ">" : " ", debug_mode_label[state->debug_mode]);
    jo_printf(8, 16, "%s LOGS: %s", state->pause_selected_option == UiPauseOptionLogs ? ">" : " ", log_mode_label[state->log_mode]);
    if (state->log_mode == RuntimeLogModeSprite)
        jo_printf(8, 17, "%s LOG PAGE: %d/%d", state->pause_selected_option == UiPauseOptionLogPage ? ">" : " ", sprite_log_page, sprite_log_page_count);

    // if (state->debug_mode == UiDebugModeDamage)
    // {
    //     const char *target_name = "?";
    //     const char *attacker_name = "?";

    //     switch (g_debug_last_damage_dealt_target)
    //     {
    //         case CHARACTER_ID_SONIC: target_name = "SNC"; break;
    //         case CHARACTER_ID_AMY: target_name = "AMY"; break;
    //         case CHARACTER_ID_TAILS: target_name = "TLS"; break;
    //         case CHARACTER_ID_KNUCKLES: target_name = "KNK"; break;
    //         case CHARACTER_ID_SHADOW: target_name = "SDW"; break;
    //         default: target_name = "?"; break;
    //     }

    //     jo_printf(2, 18, "LAST DMG/KG DEALT: %2d/%2d (-> %s)", g_debug_last_damage_dealt, g_debug_last_knockback_dealt, target_name);

    //     switch (g_debug_last_damage_received_from)
    //     {
    //         case CHARACTER_ID_SONIC: attacker_name = "SNC"; break;
    //         case CHARACTER_ID_AMY: attacker_name = "AMY"; break;
    //         case CHARACTER_ID_TAILS: attacker_name = "TLS"; break;
    //         case CHARACTER_ID_KNUCKLES: attacker_name = "KNK"; break;
    //         case CHARACTER_ID_SHADOW: attacker_name = "SDW"; break;
    //         default: attacker_name = "?"; break;
    //     }

    //     //Last Damage and Knockback received and dealt
    //     jo_printf(2, 19, "LAST DMG/KG RECEI: %2d/%2d (<- %s)", g_debug_last_damage_received, g_debug_last_knockback_received, attacker_name);
    // }
    // else 
    if (state->debug_mode == UiDebugModeMemory)
    {
        //VDP1 VRAM = sprite/object memory (512 KB)
        jo_printf(2, 18, "SPR_MEM_U: %d%%", jo_sprite_usage_percent());
        //heap/dynamique (malloc/etc) memory LWRAM (1 MB)
        jo_printf(2, 19, "DIN_MEM_U: %d%%", jo_memory_usage_percent());
    }
    else if (state->debug_mode == UiDebugModeSpawn)
    {
        int p1x = world_map_get_player_start_x(0, player.group);
        int p1y = world_map_get_player_start_y(0, player.group);
        int p2x = world_map_get_player_start_x(1, player2.group);
        int p2y = world_map_get_player_start_y(1, player2.group);
        int mapx = game_loop_get_map_pos_x();
        int mapy = game_loop_get_map_pos_y();
        int world_p1_x = mapx + player.x;
        int world_p1_y = mapy + player.y;
        int world_p2_x = mapx + player2.x;
        int world_p2_y = mapy + player2.y;

        jo_printf(2, 16, "MAP POS: (%4d,%4d)", mapx, mapy);
        jo_printf(2, 17, "SPAWN P1: (%4d,%4d) G%d", p1x, p1y, player.group);
        jo_printf(2, 18, "SPAWN P2: (%4d,%4d) G%d", p2x, p2y, player2.group);
        jo_printf(2, 19, "WORLD P1: (%4d,%4d)", world_p1_x, world_p1_y);
        jo_printf(2, 20, "WORLD P2: (%4d,%4d)", world_p2_x, world_p2_y);
        jo_printf(2, 21, "CURR  P1: (%4d,%4d)", player.x, player.y);
        jo_printf(2, 22, "CURR  P2: (%4d,%4d)", player2.x, player2.y);

        /* Provide a stable view of what reset_fight() used for P1 spawn calculation. */
        if (state->debug_mode == UiDebugModeSpawn)
        {
            int grp, sx, sy, mx, my, px, py;
            game_flow_get_last_player1_start(&grp, &sx, &sy, &mx, &my, &px, &py);
            jo_printf(2, 22, "RESET: G%d start=(%4d,%4d) map=(%4d,%4d)", grp, sx, sy, mx, my);

            int eff_player, eff_group, menu_player, menu_group;
            game_flow_get_last_player1_reset_info(&eff_player, &eff_group, &menu_player, &menu_group);
            jo_printf(2, 23, "RESET: eff=%d g=%d  menu=%d g=%d", eff_player, eff_group, menu_player, menu_group);
        }
    }

    jo_printf(4, 24, "UP/DOWN: SELECT");
    jo_printf(4, 25, "A: CONFIRM");
    jo_printf(4, 26, "LEFT/RIGHT: CHANGE MODE");
    jo_printf(4, 27, "START: RESUME");
}

static void ui_control_balance_adjust(ui_control_state_t *state, int delta)
{
    debug_balance_profile_t *profile = debug_balance_get_profile(player.character_id);
    if (profile == JO_NULL)
        return;

    int row = state->debug_balance_selected_row;
    int col = state->debug_balance_selected_col;

    if (col == 0)
    {
        int value = profile->damage[row];
        value += delta;
        if (value < 0)
            value = 0;
        profile->damage[row] = value;
    }
    else if (col == 1)
    {
        int value = profile->knockback[row];
        value += delta;
        if (value < 0)
            value = 0;
        profile->knockback[row] = value;
    }
    else if (col == 2)
    {
        int value = profile->stun[row];
        value += delta;
        if (value < 0)
            value = 0;
        profile->stun[row] = value;
    }
    else if (col == 3)
    {
        int value = profile->impulse[row];
        value += delta;
        if (value < 0)
            value = 0;
        profile->impulse[row] = value;
    }

    debug_balance_apply_to_character(&player);
}

// static int ui_control_count_cpu(const ui_control_state_t *state)
// {
//     int count = 0;
//     for (int i = 0; i < UiCharacterCount; ++i)
//     {
//         if (state->menu_character_controller[i] == UiControllerCpu)
//             count++;
//     }
//     return count;
// }

static void ui_control_build_available_controllers(const ui_control_state_t *state, int cursor_index, ui_controller_type_t out_list[4], int *out_count)
{
    bool allow_p2 = ui_control_is_multiplayer_versus(state);
    bool has_p1 = false;
    bool has_p2 = false;

    for (int i = 0; i < UiCharacterCount; ++i)
    {
        if (state->menu_character_controller[i] == UiControllerPlayer1)
            has_p1 = true;
        if (state->menu_character_controller[i] == UiControllerPlayer2)
            has_p2 = true;
    }

    ui_controller_type_t current = state->menu_character_controller[cursor_index];
    int count = 0;

    /* Order is fixed: P1, P2, CPU, NONE (X). Only include a controller if it is not already used somewhere else,
       except the current value is always kept so the user can keep it.
       P2 is only included when multiplayer is active. */
    if (!has_p1 || current == UiControllerPlayer1)
        out_list[count++] = UiControllerPlayer1;

    if (allow_p2 && (!has_p2 || current == UiControllerPlayer2))
        out_list[count++] = UiControllerPlayer2;

    out_list[count++] = UiControllerCpu;
    out_list[count++] = UiControllerNone;

    *out_count = count;
}

static ui_controller_type_t ui_control_next_controller(const ui_control_state_t *state, int cursor_index)
{
    ui_controller_type_t list[4];
    int count;
    ui_control_build_available_controllers(state, cursor_index, list, &count);

    ui_controller_type_t current = state->menu_character_controller[cursor_index];
    int idx = 0;
    while (idx < count && list[idx] != current)
        idx++;

    if (idx >= count)
        idx = 0;
    else
        idx = (idx + 1) % count;

    return list[idx];
}

static ui_controller_type_t ui_control_prev_controller(const ui_control_state_t *state, int cursor_index)
{
    ui_controller_type_t list[4];
    int count;
    ui_control_build_available_controllers(state, cursor_index, list, &count);

    ui_controller_type_t current = state->menu_character_controller[cursor_index];
    int idx = 0;
    while (idx < count && list[idx] != current)
        idx++;

    if (idx >= count)
        idx = 0;
    else
        idx = (idx + count - 1) % count;

    return list[idx];
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

    if (state->menu_screen == UiMenuScreenGroupAssign)
    {
        jo_printf(12, 4, "GROUP ASSIGN");
        jo_printf(4, 6, "GROUP 1");
        jo_printf(18, 6, "NO GROUP");
        jo_printf(32, 6, "GROUP 2");

        for (int i = 0; i < state->menu_group_count; ++i)
        {
            int y = 9 + (i * 3);
            ui_character_choice_t ch = state->menu_group_order[i];
            const char *name = character_names[ch];
            bool selected = (i == state->menu_group_cursor);
            int group = state->menu_character_group[ch];
            const char *group_label = group == 1 ? "G1" : (group == 2 ? "G2" : "--");

            jo_printf(6, y, "%s %s", selected ? ">" : " ", name);
            jo_printf(22, y, "%s", group_label);
        }

        jo_printf(4, 24, "UP/DOWN: SELECT");
        jo_printf(4, 25, "LEFT/RIGHT: GROUP");
        jo_printf(4, 26, "A: MAP");
        jo_printf(4, 27, "B: BACK");
        return;
    }

    if (state->menu_screen == UiMenuScreenMapSelect)
    {
        jo_printf(14, 6, "SELECT MAP");

        int max_visible = 8;
        int start = state->menu_map_cursor - (max_visible / 2);
        if (start < 0)
            start = 0;
        if (start + max_visible > state->menu_map_count)
            start = state->menu_map_count - max_visible;
        if (start < 0)
            start = 0;

        for (int i = 0; i < max_visible && start + i < state->menu_map_count; ++i)
        {
            int y = 10 + i;
            bool selected = ((start + i) == state->menu_map_cursor);
            jo_printf(8, y, "%s %s", selected ? ">" : " ", state->menu_map_names[start + i]);
        }

        jo_printf(4, 22, "UP/DOWN: SELECT");
        jo_printf(4, 23, "A: CONFIRM");
        jo_printf(4, 24, "B: BACK");
        jo_printf(8, 19, "SELECTED: %s", state->menu_map_names[state->menu_map_cursor]);
        return;
    }


    if (state->menu_screen == UiMenuScreenOptions)
    {
        jo_printf(16, 6, "OPTIONS");
        jo_printf(6, 10, "%s CONFIG CONTROLS", state->menu_options_selected_option == MenuOptionsConfigControls ? ">" : " ");
        jo_printf(6, 11, "%s SONG TEST", state->menu_options_selected_option == MenuOptionsSongTest ? ">" : " ");
        jo_printf(6, 12, "%s DIAGNOSTICS", state->menu_options_selected_option == MenuOptionsDiagnostics ? ">" : " ");
        
        jo_printf(4, 25, "UP/DOWN: SELECT");
        jo_printf(4, 26, "A: CONFIRM");
        jo_printf(4, 27, "B: BACK");
        return;
    }

    if (state->menu_screen == UiMenuScreenDiagnostics)
    {
        jo_printf(12, 6, "DIAGNOSTICS");
        jo_printf(8, 10, "%s RUNTIME LOG", state->diag_selected_option == 0 ? ">" : " ");
        jo_printf(25, 10, "%s", state->diag_runtime_log ? "ON" : "OFF");
        jo_printf(8, 11, "%s VRAM UPLOADS", state->diag_selected_option == 1 ? ">" : " ");
        jo_printf(25, 11, "%s", state->diag_vram_uploads ? "ON" : "OFF");
        jo_printf(8, 12, "%s BOT UPDATES", state->diag_selected_option == 2 ? ">" : " ");
        jo_printf(25, 12, "%s", state->diag_bot_updates ? "ON" : "OFF");
        jo_printf(8, 13, "%s SHOW FPS", state->diag_selected_option == 3 ? ">" : " ");
        jo_printf(25, 13, "%s", state->diag_show_fps ? "ON" : "OFF");
        jo_printf(8, 14, "%s DRAW BACKGROUNDS", state->diag_selected_option == 4 ? ">" : " ");
        jo_printf(25, 14, "%s", state->diag_draw_backgrounds ? "ON" : "OFF");
        jo_printf(8, 15, "%s DRAW OFF", state->diag_selected_option == 5 ? ">" : " ");
        jo_printf(25, 15, "%s", state->diag_disable_draw ? "ON" : "OFF");
        jo_printf(8, 16, "%s CD AUDIO", state->diag_selected_option == 6 ? ">" : " ");
        jo_printf(25, 16, "%s", state->diag_disable_cd_audio ? "OFF" : "ON ");
        jo_printf(8, 17, "%s MENU ONLY", state->diag_selected_option == 7 ? ">" : " ");
        jo_printf(25, 17, "%s", state->diag_menu_only ? "ON" : "OFF");
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
        bool is_locked = ui_control_character_is_locked(state, (ui_character_choice_t)idx);
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

        /* Draw selection cursors + controller type label for each character */
        jo_printf((item_x[idx] / 8) - 2,
                  (MENU_SPRITE_Y / 8) + 1,
                  "%s",
                  is_hovered ? (is_locked ? ">" : ">") : " ");

        const char *controller_label;
        switch (state->menu_character_controller[idx])
        {
            case UiControllerPlayer1:
                controller_label = "P1 ";
                break;
            case UiControllerPlayer2:
                controller_label = "P2 ";
                break;
            case UiControllerCpu:
                controller_label = "CPU";
                break;
            default:
                controller_label = "OUT";
                break;
        }
        jo_printf((item_x[idx] / 8) - 2,
                  (MENU_SPRITE_Y / 8) + 4,
                  "%s",
                  controller_label);
    }

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

    //jo_printf(4, 22, "UP/DOWN: CLONE BOTS");
    jo_printf(4, 23, "LEFT/RIGHT: CHAR");
    jo_printf(4, 24, "UP/DOWN: SWITCH TYPE  B: BACK");
    jo_printf(4, 25, "A: NEXT");
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
    bool pressed_start = ui_control_consume_pressed(jo_is_pad1_key_down(JO_KEY_START), &state->menu_start_released);
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
        if (pressed_up)
        {
            if (state->menu_main_selected_option == 0)
                state->menu_main_selected_option = MENU_MAIN_OPTION_COUNT - 1;
            else
                state->menu_main_selected_option--;
        }
        else if (pressed_down)
        {
            state->menu_main_selected_option = (state->menu_main_selected_option + 1) % MENU_MAIN_OPTION_COUNT;
        }
        else if (pressed_a)
        {
            if (state->menu_main_selected_option == MenuMainStartGame)
            {
                if (!state->diag_menu_only)
                    state->menu_screen = UiMenuScreenModeSelect;
            }
            else if (state->menu_main_selected_option == MenuMainOptions)
                state->menu_screen = UiMenuScreenOptions;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenDiagnostics)
    {
        if (pressed_up)
        {
            if (state->diag_selected_option == 0)
                state->diag_selected_option = 7;
            else
                state->diag_selected_option--;
        }
        else if (pressed_down)
        {
            state->diag_selected_option = (state->diag_selected_option + 1) % 8;
        }
        else if (pressed_a)
        {
            switch (state->diag_selected_option)
            {
                case 0:
                    state->diag_runtime_log = !state->diag_runtime_log;
                    if (!state->diag_runtime_log)
                        runtime_log_set_mode(RuntimeLogModeOff);
                    else
                        runtime_log_set_mode(RuntimeLogModeSystem);
                    break;
                case 1:
                    state->diag_vram_uploads = !state->diag_vram_uploads;
                    vram_cache_enable(state->diag_vram_uploads);
                    if (!state->diag_vram_uploads)
                        vram_cache_clear();
                    break;
                case 2:
                    state->diag_bot_updates = !state->diag_bot_updates;
                    break;
                case 3:
                    state->diag_show_fps = !state->diag_show_fps;
                    break;
                case 4:
                    state->diag_draw_backgrounds = !state->diag_draw_backgrounds;
                    break;
                case 5:
                    state->diag_disable_draw = !state->diag_disable_draw;
                    break;
                case 6:
                    state->diag_disable_cd_audio = !state->diag_disable_cd_audio;
                    break;
                case 7:
                    state->diag_menu_only = !state->diag_menu_only;
                    if (state->diag_menu_only)
                        state->current_game_state = UiGameStateMenu;
                    break;
            }
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenMain;
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

                /* Ensure no P2 controller is left assigned from a previous multiplayer session. */
                for (int i = 0; i < UiCharacterCount; ++i)
                    if (state->menu_character_controller[i] == UiControllerPlayer2)
                        state->menu_character_controller[i] = UiControllerNone;

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
        if (!ui_control_is_multiplayer_versus(state))
        {
            /* Make sure P2 is never shown in single player mode. */
            for (int i = 0; i < UiCharacterCount; ++i)
            {
                if (state->menu_character_controller[i] == UiControllerPlayer2)
                    state->menu_character_controller[i] = UiControllerNone;
            }
        }

        if (ui_control_is_multiplayer_versus(state))
        {
            if (!pad2_available)
                state->menu_pad2_warning_timer = MENU_PAD2_WARNING_FRAMES;

            /* Navigation: move cursor among characters */
            /* Left/Right: move cursor among characters */
            if (pressed_left || (pad2_available && pressed_left_p2))
            {
                state->menu_cursor_character = ui_control_prev_character(state->menu_cursor_character);
            }
            else if (pressed_right || (pad2_available && pressed_right_p2))
            {
                state->menu_cursor_character = ui_control_next_character(state->menu_cursor_character);
            }
            /* Up/Down: cycle controller type for the highlighted character */
            else if (pressed_up || (pad2_available && pressed_up_p2))
            {
                ui_character_choice_t cursor = state->menu_cursor_character;
                state->menu_character_controller[cursor] = ui_control_next_controller(state, cursor);
            }
            else if (pressed_down || (pad2_available && pressed_down_p2))
            {
                ui_character_choice_t cursor = state->menu_cursor_character;
                state->menu_character_controller[cursor] = ui_control_prev_controller(state, cursor);
            }

            /* Go to group assignment before starting the fight */
            if (pressed_a)
            {
                /* Prevent entering group assignment when all characters are out-of-combat (X). */
                bool any_selected = false;
                for (int i = 0; i < UiCharacterCount; ++i)
                {
                    if (state->menu_character_controller[i] != UiControllerNone
                        && !ui_control_character_is_locked(state, (ui_character_choice_t)i))
                    {
                        any_selected = true;
                        break;
                    }
                }

                if (!any_selected)
                {
                    return;
                }

                int p1_char = -1;
                int p2_char = -1;
                int first_cpu_char = -1;
                int cpu_count = 0;
                for (int i = 0; i < UiCharacterCount; ++i)
                {
                    if (state->menu_character_controller[i] == UiControllerPlayer1)
                        p1_char = i;
                    else if (state->menu_character_controller[i] == UiControllerPlayer2)
                        p2_char = i;
                    else if (state->menu_character_controller[i] == UiControllerCpu)
                    {
                        if (cpu_count == 0)
                            first_cpu_char = i;
                        cpu_count++;
                    }
                }

                if (p1_char < 0)
                    p1_char = (first_cpu_char >= 0) ? first_cpu_char : UiCharacterSonic;
                if (p2_char < 0)
                    p2_char = (p1_char == UiCharacterSonic) ? UiCharacterAmy : UiCharacterSonic;

                state->menu_selected_character = (ui_character_choice_t)p1_char;
                state->menu_selected_player2_character = (ui_character_choice_t)p2_char;
                state->menu_selected_bot_character = (ui_character_choice_t)(first_cpu_char >= 0 ? first_cpu_char : UiCharacterAmy);

                /* Build group assignment list (for player 1, player 2 and CPU slots) */
                int count = 0;
                for (int i = 0; i < UiCharacterCount; ++i)
                {
                    if (state->menu_character_controller[i] == UiControllerPlayer1
                        || state->menu_character_controller[i] == UiControllerPlayer2
                        || state->menu_character_controller[i] == UiControllerCpu)
                    {
                        if (ui_control_character_is_locked(state, (ui_character_choice_t)i))
                            continue;

                        state->menu_group_order[count++] = (ui_character_choice_t)i;
                        if (state->menu_character_controller[i] == UiControllerPlayer1)
                            state->menu_character_group[i] = 1;
                        else if (state->menu_character_controller[i] == UiControllerPlayer2)
                            state->menu_character_group[i] = 2;
                        else
                            state->menu_character_group[i] = 0;
                    }
                }

                if (count == 0)
                {
                    /* No characters are actually participating in the battle. */
                    return;
                }

                state->menu_group_count = count;
                state->menu_group_cursor = 0;
                state->menu_screen = UiMenuScreenGroupAssign;
            }

            if (pressed_b || (pad2_available && pressed_b_p2))
            {
                state->menu_screen = UiMenuScreenBattleModeSelect;
            }

            return;
        }

        if (pressed_left)
        {
            state->menu_cursor_character = ui_control_prev_character(state->menu_cursor_character);
        }
        else if (pressed_right)
        {
            state->menu_cursor_character = ui_control_next_character(state->menu_cursor_character);
        }
        else if (pressed_up)
        {
            ui_character_choice_t cursor = state->menu_cursor_character;
            state->menu_character_controller[cursor] = ui_control_next_controller(state, cursor);
        }
        else if (pressed_down)
        {
            ui_character_choice_t cursor = state->menu_cursor_character;
            state->menu_character_controller[cursor] = ui_control_prev_controller(state, cursor);
        }
        else if (pressed_a)
        {
            /* Prevent entering group assignment when all characters are out-of-combat (OUT). */
            bool any_selected = false;
            for (int i = 0; i < UiCharacterCount; ++i)
            {
                if (state->menu_character_controller[i] != UiControllerNone)
                {
                    any_selected = true;
                    break;
                }
            }

            if (!any_selected)
            {
                return;
            }

            /* Determine selected player and CPUs before going to group assignment */
            int player_char = -1;
            ui_character_choice_t first_cpu_char = UiCharacterSonic;
            int cpu_found = 0;

            for (int i = 0; i < UiCharacterCount; ++i)
            {
                if (state->menu_character_controller[i] == UiControllerPlayer1)
                    player_char = i;
                if (state->menu_character_controller[i] == UiControllerCpu)
                {
                    if (cpu_found == 0)
                        first_cpu_char = (ui_character_choice_t)i;
                    cpu_found++;
                }
            }

            if (player_char < 0 && cpu_found > 0)
            {
                /* No explicit Player selected: promote first CPU to Player to avoid duplicates. */
                player_char = first_cpu_char;
                state->menu_character_controller[player_char] = UiControllerPlayer1;
                cpu_found--;
            }

            if (player_char < 0)
                player_char = UiCharacterSonic;

            state->menu_selected_character = (ui_character_choice_t)player_char;
            state->menu_selected_bot_character = first_cpu_char;

            /* Move to group assignment before starting battle */
            int count = 0;
            for (int i = 0; i < UiCharacterCount; ++i)
            {
                if (state->menu_character_controller[i] == UiControllerPlayer1
                    || state->menu_character_controller[i] == UiControllerCpu)
                {
                    state->menu_group_order[count++] = (ui_character_choice_t)i;
                    state->menu_character_group[i] = 0;
                }
            }
            state->menu_group_count = count;
            state->menu_group_cursor = 0;
            state->menu_screen = UiMenuScreenGroupAssign;
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenBattleModeSelect;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenGroupAssign)
    {
        if (state->menu_group_count > 0)
        {
            if (pressed_up)
            {
                if (state->menu_group_cursor == 0)
                    state->menu_group_cursor = state->menu_group_count - 1;
                else
                    state->menu_group_cursor--;
            }
            else if (pressed_down)
            {
                state->menu_group_cursor = (state->menu_group_cursor + 1) % state->menu_group_count;
            }
            else if (pressed_left)
            {
                int char_id = state->menu_group_order[state->menu_group_cursor];
                int current = state->menu_character_group[char_id];
                if (current == 2)
                    state->menu_character_group[char_id] = 0;
                else if (current == 0)
                    state->menu_character_group[char_id] = 1;
            }
            else if (pressed_right)
            {
                int char_id = state->menu_group_order[state->menu_group_cursor];
                int current = state->menu_character_group[char_id];
                if (current == 1)
                    state->menu_character_group[char_id] = 0;
                else if (current == 0)
                    state->menu_character_group[char_id] = 2;
            }
            else if (pressed_a)
            {
                /* Move to map selection before starting the battle */
                state->menu_map_cursor = 0;
                if (state->menu_map_count > 0)
                    strncpy(state->menu_selected_map_name, state->menu_map_names[0], UI_MENU_MAX_MAP_NAME_LEN - 1);
                state->menu_screen = UiMenuScreenMapSelect;
                state->menu_a_released = false;
            }
            else if (pressed_b)
            {
                state->menu_screen = UiMenuScreenCharacterSelect;
            }
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenMapSelect)
    {
        if (state->menu_map_count <= 0)
            return;

        if (pressed_up)
        {
            if (state->menu_map_cursor == 0)
                state->menu_map_cursor = state->menu_map_count - 1;
            else
                state->menu_map_cursor--;
        }
        else if (pressed_down)
        {
            state->menu_map_cursor = (state->menu_map_cursor + 1) % state->menu_map_count;
        }
        else if (pressed_a)
        {
            strncpy(state->menu_selected_map_name,
                    state->menu_map_names[state->menu_map_cursor],
                    UI_MENU_MAX_MAP_NAME_LEN - 1);
            state->menu_selected_map_name[UI_MENU_MAX_MAP_NAME_LEN - 1] = '\0';
            on_start_game(state->menu_selected_character,
                          state->menu_selected_bot_character,
                          user_data);
            state->menu_a_released = false;
        }
        else if (pressed_b)
        {
            state->menu_screen = UiMenuScreenGroupAssign;
        }
        return;
    }

    if (state->menu_screen == UiMenuScreenOptions)
    {
        if (pressed_up)
        {
            if (state->menu_options_selected_option == 0)
                state->menu_options_selected_option = MENU_OPTIONS_OPTION_COUNT - 1;
            else
                state->menu_options_selected_option--;
        }
        else if (pressed_down)
        {
            state->menu_options_selected_option = (state->menu_options_selected_option + 1) % MENU_OPTIONS_OPTION_COUNT;
        }
        else if (pressed_a)
        {
            if (state->menu_options_selected_option == MenuOptionsConfigControls)
                state->menu_screen = UiMenuScreenConfigControls;
            else if (state->menu_options_selected_option == MenuOptionsSongTest)
                state->menu_screen = UiMenuScreenSongTest;
            else
                state->menu_screen = UiMenuScreenDiagnostics;
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
    bool pause_interacted = false;

    if (state->menu_screen == UiMenuScreenDebugBalance)
    {
        bool handled = false;

        if (jo_is_pad1_key_down(JO_KEY_UP))
        {
            if (state->pause_up_released)
            {
                state->debug_balance_selected_row = (state->debug_balance_selected_row + DebugBalanceAttackCount - 1) % DebugBalanceAttackCount;
                handled = true;
                pause_interacted = true;
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
                state->debug_balance_selected_row = (state->debug_balance_selected_row + 1) % DebugBalanceAttackCount;
                handled = true;
                pause_interacted = true;
            }
            state->pause_down_released = false;
        }
        else
        {
            state->pause_down_released = true;
        }

        if (jo_is_pad1_key_down(JO_KEY_LEFT))
        {
            if (state->pause_left_released)
            {
                ui_control_balance_adjust(state, -1);
                handled = true;
                pause_interacted = true;
                state->debug_balance_hold_left_frames = 0;
            }
            else
            {
                state->debug_balance_hold_left_frames++;
                if (state->debug_balance_hold_left_frames >= 60 && (state->debug_balance_hold_left_frames % 4) == 0)
                {
                    ui_control_balance_adjust(state, -1);
                    handled = true;
                }
            }
            state->pause_left_released = false;
        }
        else
        {
            state->pause_left_released = true;
            state->debug_balance_hold_left_frames = 0;
        }

        if (jo_is_pad1_key_down(JO_KEY_RIGHT))
        {
            if (state->pause_right_released)
            {
                ui_control_balance_adjust(state, 1);
                handled = true;
                pause_interacted = true;
                state->debug_balance_hold_right_frames = 0;
            }
            else
            {
                state->debug_balance_hold_right_frames++;
                if (state->debug_balance_hold_right_frames >= 60 && (state->debug_balance_hold_right_frames % 4) == 0)
                {
                    ui_control_balance_adjust(state, 1);
                    handled = true;
                }
            }
            state->pause_right_released = false;
        }
        else
        {
            state->pause_right_released = true;
            state->debug_balance_hold_right_frames = 0;
        }

        if (jo_is_pad1_key_down(JO_KEY_A))
        {
            if (state->pause_a_released)
            {
                state->debug_balance_selected_col = (state->debug_balance_selected_col + 1) % 4;
                handled = true;
            }
            state->pause_a_released = false;
        }
        else
        {
            state->pause_a_released = true;
        }

        if (jo_is_pad1_key_down(JO_KEY_B))
        {
            if (state->pause_b_released)
            {
                /* Back to pause menu */
                state->menu_screen = UiMenuScreenMain;
                handled = true;
            }
            state->pause_b_released = false;
        }
        else
        {
            state->pause_b_released = true;
        }

        if (jo_is_pad1_key_down(JO_KEY_C))
        {
            if (state->pause_c_released)
            {
                debug_balance_reset_profile(player.character_id);
                debug_balance_apply_to_character(&player);
                handled = true;
            }
            state->pause_c_released = false;
        }
        else
        {
            state->pause_c_released = true;
        }

        if (handled)
        {
            debug_balance_apply_to_character(&player);
            pause_interacted = true;
        }

        if (pause_interacted)
            state->pause_text_dirty = true;

        return;
    }

    if (jo_is_pad1_key_down(JO_KEY_RIGHT))
    {
        if (state->pause_right_released)
        {
            if (state->pause_selected_option == UiPauseOptionDebug)
            {
                if (state->debug_mode == (ui_debug_mode_t)(UiDebugModeCount - 1))
                    state->debug_mode = UiDebugModeOff;
                else
                    state->debug_mode = (ui_debug_mode_t)(state->debug_mode + 1);

                /* Show debug overlay during gameplay for these modes. */
                state->debug_enabled = (state->debug_mode == UiDebugModeHardware
                                       || state->debug_mode == UiDebugModePlayer
                                       || state->debug_mode == UiDebugModeCamera);
                g_show_attack_debug = (state->debug_mode == UiDebugModeAttack);
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionLogs)
            {
                if (state->log_mode == (runtime_log_mode_t)(RuntimeLogModeCount - 1))
                    state->log_mode = RuntimeLogModeOff;
                else
                    state->log_mode = (runtime_log_mode_t)(state->log_mode + 1);

                runtime_log_set_mode(state->log_mode);
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionLogPage)
            {
                int next_page = runtime_log_get_sprite_page() + 1;
                int page_count = runtime_log_get_sprite_page_count();

                if (next_page >= page_count)
                    next_page = 0;
                runtime_log_set_sprite_page(next_page);
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionBalance)
            {
                /* balance submenu does not change debug toggles */
                pause_interacted = true;
            }
            else
            {
                state->debug_enabled = (state->debug_mode != UiDebugModeOff);
                pause_interacted = true;
            }
        }
        state->pause_right_released = false;
    }
    else
    {
        state->pause_right_released = true;
    }

    if (jo_is_pad1_key_down(JO_KEY_LEFT))
    {
        if (state->pause_left_released)
        {
            if (state->pause_selected_option == UiPauseOptionDebug)
            {
                if (state->debug_mode == UiDebugModeOff)
                    state->debug_mode = (ui_debug_mode_t)(UiDebugModeCount - 1);
                else
                    state->debug_mode = (ui_debug_mode_t)(state->debug_mode - 1);

                /* Show debug overlay during gameplay for these modes. */
                state->debug_enabled = (state->debug_mode == UiDebugModeHardware
                                       || state->debug_mode == UiDebugModePlayer
                                       || state->debug_mode == UiDebugModeCamera);
                g_show_attack_debug = (state->debug_mode == UiDebugModeAttack);
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionLogs)
            {
                if (state->log_mode == RuntimeLogModeOff)
                    state->log_mode = (runtime_log_mode_t)(RuntimeLogModeCount - 1);
                else
                    state->log_mode = (runtime_log_mode_t)(state->log_mode - 1);

                runtime_log_set_mode(state->log_mode);
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionLogPage)
            {
                int page_count = runtime_log_get_sprite_page_count();
                int prev_page = runtime_log_get_sprite_page() - 1;
                if (prev_page < 0)
                    prev_page = page_count - 1;
                runtime_log_set_sprite_page(prev_page);
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionBalance)
            {
                /* balance submenu does not change debug toggles */
                pause_interacted = true;
            }
            else
            {
                state->debug_enabled = (state->debug_mode != UiDebugModeOff);
                pause_interacted = true;
            }
        }
        state->pause_left_released = false;
    }
    else
    {
        state->pause_left_released = true;
    }

    if (jo_is_pad1_key_down(JO_KEY_UP))
    {
        if (state->pause_up_released)
        {
            if (state->pause_selected_option == UiPauseOptionContinue)
                state->pause_selected_option = (ui_pause_option_t)(UiPauseOptionCount - 1);
            else
                state->pause_selected_option = (ui_pause_option_t)(state->pause_selected_option - 1);

            pause_interacted = true;

            /* Skip LOG PAGE when sprite logs are not active */
            while (state->pause_selected_option == UiPauseOptionLogPage && state->log_mode != RuntimeLogModeSprite)
            {
                if (state->pause_selected_option == UiPauseOptionContinue)
                    state->pause_selected_option = (ui_pause_option_t)(UiPauseOptionCount - 1);
                else
                    state->pause_selected_option = (ui_pause_option_t)(state->pause_selected_option - 1);
            }
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

            pause_interacted = true;

            /* Skip LOG PAGE when sprite logs are not active */
            while (state->pause_selected_option == UiPauseOptionLogPage && state->log_mode != RuntimeLogModeSprite)
            {
                if (state->pause_selected_option == (ui_pause_option_t)(UiPauseOptionCount - 1))
                    state->pause_selected_option = UiPauseOptionContinue;
                else
                    state->pause_selected_option = (ui_pause_option_t)(state->pause_selected_option + 1);
            }
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
            {
                state->game_paused = false;
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionPauseBots)
            {
                state->pause_bots = !state->pause_bots;
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionResetFight)
            {
                on_reset_fight(user_data);
                state->game_paused = false;
                pause_interacted = true;
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
                pause_interacted = true;
            }
            else if (state->pause_selected_option == UiPauseOptionBalance)
            {
                state->menu_screen = UiMenuScreenDebugBalance;
                state->debug_balance_selected_row = 0;
                state->debug_balance_selected_col = 0;
                state->pause_a_released = false;
                pause_interacted = true;
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
        {
            state->game_paused = false;
            pause_interacted = true;
        }

        state->pause_start_released = false;
    }
    else
    {
        state->pause_start_released = true;
    }

    if (pause_interacted)
        state->pause_text_dirty = true;
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

