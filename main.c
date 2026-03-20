#include <jo/jo.h>
#include "src/class/game_constants.h"
#include "src/class/player.h"
#include "src/class/control.h"
#include "src/class/ui_control.h"
#include "src/class/game_flow.h"
#include "src/class/world_collision.h"
#include "src/class/world_camera.h"
#include "src/class/world_background.h"
#include "src/class/world_map.h"
#include "src/class/game_audio.h"
#include "src/class/game_loop.h"
#include "src/class/debug.h"
#include "src/class/damage_fx.h"
#include "src/class/ram_cart.h"
#include "src/class/runtime_log.h"

static ui_control_state_t ui_state;
static game_flow_context_t game_flow_ctx;
static game_loop_context_t game_loop_ctx;
static bool show_boot_loading = true;
static bool player_defeated = false;
static bool show_initial_screen = true;
static jo_img ini_screen_img;
static bool ini_screen_loaded = false;
static bool splash_track_playing = false;
static bool splash_start_released = true;
static bool splash_text_dirty = true;
static bool show_ram_cart_screen = true;
static bool ram_cart_start_released = true;
static bool ram_cart_text_dirty = true;
static bool splash_background_drawn = false;
static int splash_frame_counter = 0;

/* FPS counter for diagnostics */
static unsigned int g_fps_frame_count = 0;
static unsigned int g_fps_last_tick = 0;
static unsigned int g_fps_value = 0;

static ram_cart_status_t ram_cart_status = RamCartStatusNotDetected;
static int sprite_anim_bootstrap_id = -1;

#define GAME_VERSION_TEXT "ver. 0.1.32a"
#define GAME_VERSION_TEXT_X 28
#define GAME_VERSION_TEXT_Y 28
#define RAM_CART_TEXT_X 7
#define RAM_CART_TEXT_Y 12

/*
** SPECIAL NOTE: It's not the Sonic that I'm working on, but you can now write your own :)
*/

jo_sidescroller_physics_params physics;
static int map_pos_x = WORLD_DEFAULT_X;
static int map_pos_y = WORLD_DEFAULT_Y;
static int jump_cooldown = 0;

int total_pcm = 0;

static void perform_startup_initialization(void)
{
    runtime_log_init();
    if (runtime_log_is_enabled())
    {
        jo_printf(0, 2, "startup: begin");
        runtime_log("startup: begin");
    }
    ini_screen_img.data = JO_NULL;
    if (runtime_log_is_enabled())
        jo_printf(0, 3, "startup: loading ini TGA");
    if (jo_tga_loader(&ini_screen_img, "BG", "BGG_INI.TGA", JO_COLOR_Black) == JO_TGA_OK)
        ini_screen_loaded = true;

    if (runtime_log_is_enabled())
        jo_printf(0, 4, "startup: init ui_control");
    ui_control_init(&ui_state);
    if (runtime_log_is_enabled())
        jo_printf(0, 5, "startup: init game flow ctx");
    game_flow_ctx.physics = &physics;
    game_flow_ctx.map_pos_x = &map_pos_x;
    game_flow_ctx.map_pos_y = &map_pos_y;
    game_flow_ctx.player_defeated = &player_defeated;
    game_flow_ctx.ui_state = &ui_state;

    if (runtime_log_is_enabled())
        jo_printf(0, 6, "startup: init game loop ctx");
    game_loop_ctx.ui_state = &ui_state;
    game_loop_ctx.game_flow_ctx = &game_flow_ctx;
    game_loop_ctx.physics = &physics;
    game_loop_ctx.map_pos_x = &map_pos_x;
    game_loop_ctx.map_pos_y = &map_pos_y;
    game_loop_ctx.jump_cooldown = &jump_cooldown;
    game_loop_ctx.player_defeated = &player_defeated;
    game_loop_ctx.total_pcm = &total_pcm;
    game_loop_init(&game_loop_ctx);
    if (runtime_log_is_enabled())
        jo_printf(0, 7, "startup: game_loop_init done");

    if (runtime_log_is_enabled())
        jo_printf(0, 8, "startup: debug_init");
    debug_init();
    if (runtime_log_is_enabled())
        jo_printf(0, 9, "startup: audio setup");
    game_audio_setup();
    if (runtime_log_is_enabled())
        jo_printf(0, 10, "startup: damage_fx setup");
    damage_fx_setup();
    if (runtime_log_is_enabled())
        jo_printf(0, 11, "startup: world_map_load");
    runtime_log("startup: world_map_load");
    world_map_load();
    ram_cart_status = ram_cart_detect();
    if (runtime_log_is_enabled())
        jo_printf(0, 12, "startup: ram_cart detect=%d", ram_cart_status);

    show_boot_loading = false;
    if (runtime_log_is_enabled())
        jo_printf(0, 13, "startup: finished");
}


static void display_ram_cart_screen(void)
{
    ui_control_clear_text_layer();

    if (ram_cart_status == RamCartStatus4MB)
    {
        jo_printf(RAM_CART_TEXT_X, RAM_CART_TEXT_Y, "4MB RAM Cartridge - OK");
    }
    else if (ram_cart_status == RamCartStatus1MB)
    {
        jo_printf(RAM_CART_TEXT_X, RAM_CART_TEXT_Y, "1MB RAM Cartridge - Not OK");
        jo_printf(RAM_CART_TEXT_X, RAM_CART_TEXT_Y + 1, "Required 4MB RAM Cartridge");
    }
    else
    {
        jo_printf(RAM_CART_TEXT_X, RAM_CART_TEXT_Y, "RAM Cartridge Not Detected");
        jo_printf(RAM_CART_TEXT_X, RAM_CART_TEXT_Y + 1, "Required 4MB RAM Cartridge");
    }

    jo_printf(RAM_CART_TEXT_X, RAM_CART_TEXT_Y + 3, "Press Start to Continue");

    /* FPS + input monitor */
    /* Put these near the bottom so they aren't hidden by other screen layouts. */
    jo_printf(0, 1, "FPS: %3d", g_fps_value);
    // if (jo_is_pad1_key_down(JO_KEY_LEFT))
    //     jo_printf(0, 27, "IN: <");
    // else if (jo_is_pad1_key_down(JO_KEY_RIGHT))
    //     jo_printf(0, 27, "IN: >");
    // else if (jo_is_pad1_key_down(JO_KEY_A))
    //     jo_printf(0, 27, "IN: A");
}

static void reset_to_splash_screen(void)
{
    /* Preserve diagnostics settings when returning to the splash screen. */
    bool diag_runtime_log = ui_state.diag_runtime_log;
    bool diag_vram_uploads = ui_state.diag_vram_uploads;
    bool diag_bot_updates = ui_state.diag_bot_updates;
    bool diag_show_fps = ui_state.diag_show_fps;
    bool diag_draw_backgrounds = ui_state.diag_draw_backgrounds;
    bool diag_disable_draw = ui_state.diag_disable_draw;
    bool diag_disable_cd_audio = ui_state.diag_disable_cd_audio;
    bool diag_menu_only = ui_state.diag_menu_only;
    int diag_selected_option = ui_state.diag_selected_option;

    game_flow_return_to_character_select(&game_flow_ctx);
    ui_control_init(&ui_state);

    ui_state.diag_runtime_log = diag_runtime_log;
    ui_state.diag_vram_uploads = diag_vram_uploads;
    ui_state.diag_bot_updates = diag_bot_updates;
    ui_state.diag_show_fps = diag_show_fps;
    ui_state.diag_draw_backgrounds = diag_draw_backgrounds;
    ui_state.diag_disable_draw = diag_disable_draw;
    ui_state.diag_disable_cd_audio = diag_disable_cd_audio;
    ui_state.diag_menu_only = diag_menu_only;
    ui_state.diag_selected_option = diag_selected_option;

    player = (character_t){0};
    player2 = (character_t){0};
    player.wram_sprite_id = -1;
    player2.wram_sprite_id = -1;
    physics = (jo_sidescroller_physics_params){0};
    map_pos_x = WORLD_DEFAULT_X;
    map_pos_y = WORLD_DEFAULT_Y;
    jump_cooldown = 0;
    total_pcm = 0;
    player_defeated = false;
    show_ram_cart_screen = false;
    show_initial_screen = true;
    splash_track_playing = false;
    splash_background_drawn = false;
    splash_start_released = false;
    ui_control_clear_text_layer();
    jo_clear_background(JO_COLOR_Black);
    world_background_invalidate();
    game_loop_mark_ui_dirty();
}

static bool handle_global_reset_combo(void)
{
    bool combo_down = jo_is_pad1_key_pressed(JO_KEY_A)
        && jo_is_pad1_key_pressed(JO_KEY_B)
        && jo_is_pad1_key_pressed(JO_KEY_C)
        && jo_is_pad1_key_down(JO_KEY_START);

    if (combo_down)
    {
        reset_to_splash_screen();
        return true;
    }

    return false;
}

void display_initial_screen(void)
{
    if (show_initial_screen)
    {
        if (splash_text_dirty)
        {
            ui_control_clear_text_layer();
            splash_text_dirty = false;
            /* Avoid stuck-start (button held from previous screen) */
            splash_start_released = false;
        }

        if (ui_state.diag_disable_cd_audio)
        {
            if (splash_track_playing)
            {
                jo_audio_stop_cd();
                splash_track_playing = false;
            }
        }
        else if (!splash_track_playing)
        {
            jo_audio_stop_cd();
            jo_audio_play_cd_track(3, 99, true);
            splash_track_playing = true;
        }

        if (!splash_background_drawn)
        {
            if (ui_state.diag_draw_backgrounds)
            {
                if (ini_screen_loaded)
                    jo_set_background_sprite(&ini_screen_img, 0, 0);

                jo_move_background(0, 0);
            }
            else
            {
                jo_clear_background(JO_COLOR_Black);
            }

            splash_background_drawn = true;
        }

        /* Frame counter to help detect slowdown on the real hardware */
        splash_frame_counter++;

        /* The global FPS counter is already updated in main_draw_callback.
         * Avoid double-counting by only printing it here. */
        if (ui_state.diag_show_fps)
        {
            jo_printf(0, 1, "FPS: %3d", g_fps_value);
        }
        else
        {
            jo_printf(0, 1, "FRAMES: %d", splash_frame_counter);
        }

        /* Keep input indicator from sticking once the button is released. */
        // jo_printf(0, 2, "     ");
        // if (jo_is_pad1_key_down(JO_KEY_LEFT))
        //     jo_printf(0, 2, "IN: <");
        // else if (jo_is_pad1_key_down(JO_KEY_RIGHT))
        //     jo_printf(0, 2, "IN: >");
        // else if (jo_is_pad1_key_down(JO_KEY_A))
        //     jo_printf(0, 2, "IN: A");

        /* Show whether background redraw is enabled (can affect performance). */
        //jo_printf(0, 3, "BG: %s", ui_state.diag_draw_backgrounds ? "ON " : "OFF");

        if (jo_is_pad1_key_down(JO_KEY_START))
        {
            if (splash_start_released)
            {
                show_initial_screen = false;
                if (splash_track_playing)
                {
                    jo_audio_stop_cd();
                    splash_track_playing = false;
                }
                ui_state.current_game_state = UiGameStateMenu;
                ui_state.menu_screen = UiMenuScreenMain;
                splash_text_dirty = true;
                game_loop_mark_ui_dirty();
            }
        }
        else
        {
            splash_start_released = true;
        }
    }
}

void main_draw_callback(void)
{
    /* Always keep FPS counting, even in MENU ONLY, so we can display last value. */
    {
        unsigned int now = jo_get_ticks();
        g_fps_frame_count++;
        if (now - g_fps_last_tick >= 1000)
        {
            g_fps_value = g_fps_frame_count;
            g_fps_frame_count = 0;
            g_fps_last_tick = now;
        }
    }

    if (show_boot_loading)
    {
        /* Ensure no runtime log is shown while loading */
        runtime_log_set_mode(RuntimeLogModeOff);
        runtime_log_clear();

        ui_control_draw_loading();
        if (ui_state.diag_show_fps)
            jo_printf(0, 1, "FPS: %3d", g_fps_value);
        return;
    }

    if (show_ram_cart_screen)
    {
        if (ram_cart_text_dirty)
        {
            ui_control_clear_text_layer();
            ram_cart_text_dirty = false;
        }

        display_ram_cart_screen();
        runtime_log_draw(0, 20);
        return;
    }

    if (show_initial_screen)
    {
        display_initial_screen();
        jo_printf(GAME_VERSION_TEXT_X, GAME_VERSION_TEXT_Y, GAME_VERSION_TEXT);
        jo_printf(0, 1, "FPS: %3d", g_fps_value);
        // if (jo_is_pad1_key_down(JO_KEY_LEFT))
        //     jo_printf(0, 2, "IN: <");
        // else if (jo_is_pad1_key_down(JO_KEY_RIGHT))
        //     jo_printf(0, 2, "IN: >");
        // else if (jo_is_pad1_key_down(JO_KEY_A))
        //     jo_printf(0, 2, "IN: A");
        runtime_log_draw(0, 20);
        return;
    }

    game_loop_draw();
    if (ui_state.diag_show_fps)
        jo_printf(0, 1, "FPS: %3d", g_fps_value);
    runtime_log_draw(0, 20);
}

void main_input_callback(void)
{
    if (show_boot_loading)
        return;

    if (handle_global_reset_combo())
        return;

    if (show_ram_cart_screen)
    {
        if (jo_is_pad1_key_down(JO_KEY_START))
        {
            if (ram_cart_start_released)
            {
                show_ram_cart_screen = false;
                ram_cart_start_released = false;
                ui_control_clear_text_layer();
            }
        }
        else
        {
            ram_cart_start_released = true;
        }
        return;
    }

    if (show_initial_screen)
        return;

    game_loop_input();
}

void main_update_callback(void)
{
    if (show_boot_loading)
    {
        perform_startup_initialization();
        return;
    }

    if (show_ram_cart_screen)
        return;

    if (show_initial_screen)
        return;

    game_loop_update();
}

static void bootstrap_sprite_animator_callback(void)
{
    if (sprite_anim_bootstrap_id >= 0)
        return;

    sprite_anim_bootstrap_id = jo_create_sprite_anim(0, 1, 1);
    if (sprite_anim_bootstrap_id >= 0)
        jo_start_sprite_anim_loop(sprite_anim_bootstrap_id);
}

void jo_main(void)
{     
    jo_core_init(JO_COLOR_Black);

    bootstrap_sprite_animator_callback();

	jo_core_add_callback(main_draw_callback);
	jo_core_add_callback(main_input_callback);
	jo_core_add_callback(main_update_callback);
	jo_core_run();
}

/*
** END OF FILE
*/
