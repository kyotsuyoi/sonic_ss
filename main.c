#include <jo/jo.h>
#include "src/class/game_constants.h"
#include "src/class/bot.h"
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
static bool show_ram_cart_screen = true;
static bool ram_cart_start_released = true;
static ram_cart_status_t ram_cart_status = RamCartStatusNotDetected;
static int sprite_anim_bootstrap_id = -1;

#define GAME_VERSION_TEXT "ver. 0.1.18a"
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
    jo_printf(0, 2, "startup: begin");
    runtime_log("startup: begin");
    ini_screen_img.data = JO_NULL;
    jo_printf(0, 3, "startup: loading ini TGA");
    if (jo_tga_loader(&ini_screen_img, "BG", "BGG_INI.TGA", JO_COLOR_Black) == JO_TGA_OK)
        ini_screen_loaded = true;

    jo_printf(0, 4, "startup: init ui_control");
    ui_control_init(&ui_state);
    jo_printf(0, 5, "startup: init game flow ctx");
    game_flow_ctx.physics = &physics;
    game_flow_ctx.map_pos_x = &map_pos_x;
    game_flow_ctx.map_pos_y = &map_pos_y;
    game_flow_ctx.player_defeated = &player_defeated;
    game_flow_ctx.ui_state = &ui_state;

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
    jo_printf(0, 7, "startup: game_loop_init done");

    jo_printf(0, 8, "startup: debug_init");
    debug_init();
    jo_printf(0, 9, "startup: audio setup");
    game_audio_setup();
    jo_printf(0, 10, "startup: damage_fx setup");
    damage_fx_setup();
    jo_printf(0, 11, "startup: world_map_load");
    runtime_log("startup: world_map_load");
    world_map_load();
    ram_cart_status = ram_cart_detect();
    jo_printf(0, 12, "startup: ram_cart detect=%d", ram_cart_status);

    show_boot_loading = false;
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
}

static void reset_to_splash_screen(void)
{
    game_flow_return_to_character_select(&game_flow_ctx);
    ui_control_init(&ui_state);
    player = (character_t){0};
    player2 = (character_t){0};
    physics = (jo_sidescroller_physics_params){0};
    map_pos_x = WORLD_DEFAULT_X;
    map_pos_y = WORLD_DEFAULT_Y;
    jump_cooldown = 0;
    total_pcm = 0;
    player_defeated = false;
    show_ram_cart_screen = false;
    show_initial_screen = true;
    splash_track_playing = false;
    splash_start_released = false;
    ui_control_clear_text_layer();
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
        ui_control_clear_text_layer();

        if (!splash_track_playing)
        {
            jo_audio_stop_cd();
            jo_audio_play_cd_track(3, 99, true);
            splash_track_playing = true;
        }

        if (ini_screen_loaded)
            jo_set_background_sprite(&ini_screen_img, 0, 0);

        jo_move_background(0, 0);
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
    if (show_boot_loading)
    {
        ui_control_draw_loading();
        runtime_log_draw(0, 20);
        return;
    }

    if (show_ram_cart_screen)
    {
        display_ram_cart_screen();
        runtime_log_draw(0, 20);
        return;
    }

    if (show_initial_screen)
    {
        display_initial_screen();
        jo_printf(GAME_VERSION_TEXT_X, GAME_VERSION_TEXT_Y, GAME_VERSION_TEXT);
        runtime_log_draw(0, 20);
        return;
    }

    game_loop_draw();
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
