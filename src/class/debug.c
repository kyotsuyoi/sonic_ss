#include "debug.h"
#include "player.h"
#include "bot.h"
#include "game_loop.h"
#include <math.h>

/* globals exposed via debug.h */
int g_dbg_knock_dir = 0;
float g_dbg_knock_force = 0.0f;
float g_dbg_knock_speed = 0.0f;
int g_dbg_knock_dx = 0;

static volatile int frame_counter = 0;
static int fps = 0;
static int last_frame_count = 0;
static int frame_accumulator = 0;

#define DEBUG_SAFE_TOP_LINES 10

#define DEBUG_DISPLAY_HARDWARE 0
#define DEBUG_DISPLAY_PLAYER   1

static int debug_display_mode = DEBUG_DISPLAY_PLAYER;

static void debug_draw_hardware(void);
static void debug_draw_player(void);
static void debug_draw_hitbox_snapshot(const char *title, const debug_hitbox_snapshot_t *snapshot);

static unsigned int initial_stack;

/* ======== VBlank ======== */
static void vblank_callback(void)
{
    frame_counter++;
}

/* ======== Init ======== */
void debug_init(void)
{
    jo_core_add_vblank_callback(vblank_callback);
    asm volatile ("mov r15, %0" : "=r" (initial_stack));
}

void debug_frame(void)
{
    debug_update();
    debug_draw();
}

void debug_set_display_mode(debug_display_mode_t mode)
{
    if (mode == DebugDisplayHardware)
        debug_display_mode = DEBUG_DISPLAY_HARDWARE;
    else
        debug_display_mode = DEBUG_DISPLAY_PLAYER;
}

debug_display_mode_t debug_get_display_mode(void)
{
    if (debug_display_mode == DEBUG_DISPLAY_HARDWARE)
        return DebugDisplayHardware;

    return DebugDisplayPlayer;
}

/* ======== FPS ======== */
void debug_update(void)
{
    frame_accumulator++;

    if (frame_accumulator >= 60)
    {
        fps = frame_counter - last_frame_count;
        last_frame_count = frame_counter;
        frame_accumulator = 0;
    }
}

/* ======== Draw ======== */
void debug_draw(void)
{
    if (debug_display_mode == DEBUG_DISPLAY_HARDWARE)
    {
        debug_draw_hardware();
        return;
    }

    debug_draw_player();

    // int total = jo_audio_get_cd_track_count();
    // jo_printf("Tracks", total, 9);
}

static void debug_draw_hardware(void)
{
    print_debug_valu("FPS______", fps,".", 0 + DEBUG_SAFE_TOP_LINES); // frames per second
    jo_printf(0, 1 + DEBUG_SAFE_TOP_LINES, "SPR_MEM_U: %d%% ", jo_sprite_usage_percent()); // percentage of sprite memory currently in use
    print_debug_valu("SPRT_CONT", jo_sprite_count(), "sprt",  3 + DEBUG_SAFE_TOP_LINES); // number of sprites currently in use    
    debug_scsp_memory(4 + DEBUG_SAFE_TOP_LINES, 5 + DEBUG_SAFE_TOP_LINES); // SCSP PCM memory usage in kB and percentage
    debug_scsp(6 + DEBUG_SAFE_TOP_LINES); // number of active SCSP voices
    debug_vdp1(7 + DEBUG_SAFE_TOP_LINES); // VDP1 status register
    debug_stack_usage(8 + DEBUG_SAFE_TOP_LINES); // stack usage in bytes    
    jo_printf(0, 2 + DEBUG_SAFE_TOP_LINES, "DIN_MEM_U: %d%%", jo_memory_usage_percent()); // percentage of dynamic memory currently in use
    
}

static void debug_draw_player(void)
{
    debug_hitbox_snapshot_t snapshot;

    print_debug_side("side_", player.flip, 0 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("walk_", player.walk, 1 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("run__", player.run, 2 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("jump_", player.jump, 3 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("spin_", player.spin, 4 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("pnch_", player.punch, 5 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("pnc2r", player.punch2_requested, 6 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("pnch2", player.punch2, 7 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("kick_", player.kick, 8 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("kik2r", player.kick2_requested, 9 + DEBUG_SAFE_TOP_LINES);
    print_debug_bool("kick2", player.kick2, 10 + DEBUG_SAFE_TOP_LINES);
    print_debug_valu("speed", (int)JO_ABS(player.speed), "px", 11 + DEBUG_SAFE_TOP_LINES);

    jo_printf(0, 12 + DEBUG_SAFE_TOP_LINES, "SPR_WALK: %d", jo_get_sprite_anim_frame(player.walking_anim_id));
    jo_printf(0, 13 + DEBUG_SAFE_TOP_LINES, "SPR_RUN01: %d", jo_get_sprite_anim_frame(player.running1_anim_id));
    jo_printf(0, 14 + DEBUG_SAFE_TOP_LINES, "SPR_RUN02: %d", jo_get_sprite_anim_frame(player.running2_anim_id));

    if (game_loop_debug_get_player2_hitbox_snapshot(&snapshot))
        debug_draw_hitbox_snapshot("PVP HITBOX", &snapshot);
    else if (bot_debug_get_hitbox_snapshot(&snapshot))
        debug_draw_hitbox_snapshot("HITBOX DBG", &snapshot);

    jo_printf(0, 15 + DEBUG_SAFE_TOP_LINES, "P:%3d,%3d M:%4d,%4d", player.x, player.y, game_loop_get_map_pos_x(), game_loop_get_map_pos_y());
}

static void debug_draw_hitbox_snapshot(const char *title, const debug_hitbox_snapshot_t *snapshot)
{
    jo_printf(26, 14, "%s", title);
    jo_printf(26, 15, "DX:%3d DY:%3d", snapshot->center_dx, snapshot->center_dy);
    jo_printf(26, 16, "P1:%3d/%3d %s", snapshot->center_dx, snapshot->hreach_p1, snapshot->hit_p1 ? "HIT" : "---");
    jo_printf(26, 17, "P2:%3d/%3d %s", snapshot->center_dx, snapshot->hreach_p2, snapshot->hit_p2 ? "HIT" : "---");
    jo_printf(26, 18, "K1:%3d/%3d %s", snapshot->center_dx, snapshot->hreach_k1, snapshot->hit_k1 ? "HIT" : "---");
    jo_printf(26, 19, "K2:%3d/%3d %s", snapshot->center_dx, snapshot->hreach_k2, snapshot->hit_k2 ? "HIT" : "---");
    jo_printf(26, 20, "V :%3d/%3d", snapshot->center_dy, snapshot->vreach);
    /* show recent knockback info */
    char dir = '-';
    if (g_dbg_knock_dir > 0)
        dir = 'R';
    else if (g_dbg_knock_dir < 0)
        dir = 'L';

    float abs_force = g_dbg_knock_force;
    if (abs_force < 0.0f)
        abs_force = -abs_force;
    int force_int = (int)abs_force;
    int force_frac = (int)(abs_force * 100.0f) % 100;

    float abs_speed = g_dbg_knock_speed;
    if (abs_speed < 0.0f)
        abs_speed = -abs_speed;
    int speed_int = (int)abs_speed;
    int speed_frac = (int)(abs_speed * 100.0f) % 100;

    jo_printf(26, 21, "KB dir=%c f=%d.%02d", dir, force_int, force_frac);
    jo_printf(26, 22, "SPD=%d.%02d DX=%d", speed_int, speed_frac, g_dbg_knock_dx);
}

void print_debug_valu(char* description, int value, char* suffix, int screen_line){
    jo_printf(0, screen_line, "           ");
    jo_printf(0, screen_line, "%s: %d%s", description, value, suffix);
}

void print_debug_side(char* description, bool player_value, int screen_line){
    char* string = ">";
    if (player_value){
        string = "<";
    }
    jo_printf(0, screen_line, "           ");
    jo_printf(0, screen_line, "%s: %s", description, string);
}

void print_debug_bool(char* description, bool player_value, int screen_line){
    char* string = "0";
    if (player_value){
        string = "1";
    }
    jo_printf(0, screen_line, "           ");
    jo_printf(0, screen_line, "%s: %s", description, string);
}

void debug_stack_usage(int screen_line)
{
    /*
        Work RAM (onde está sua stack)
        2 types of RAM:
            High Work RAM (HWRAM) → 1 MB
            Low Work RAM (LWRAM) → 1 MB
            Total 2 MB
        Used for: Stack, Heap, Code, Variables and Structures of game
    */

    unsigned int current_sp;
    asm volatile ("mov r15, %0" : "=r" (current_sp));

    unsigned int used = initial_stack - current_sp;

    jo_printf(0, screen_line, "STACK_USE: %dB", used); //bytes
}

#define SCSP_RAM_TOTAL (512 * 1024)
extern int total_pcm;
void debug_scsp_memory(int screen_line, int screen_line2)
{
    int used_kb = total_pcm / 1024;
    int percent = (total_pcm * 100) / SCSP_RAM_TOTAL;

    jo_printf(0, screen_line, "SCSP_USED: %dkB", used_kb); //Used PCM in kB
    jo_printf(0, screen_line2, "SCSP_PERC: %d%%", percent); //Used PCM in percentage
}

#define SCSP_BASE 0x25B00000
void debug_scsp(int screen_line)
{
    /*
        0	No key on
        1	Key on
    */

    int active = 0;

    for (int i = 0; i < 32; i++)
    {
        volatile unsigned short *slot =
            (unsigned short *)(SCSP_BASE + (i * 0x20));

        if (slot[0] & 0x0800) // key-on bit
            active++;
    }

    jo_printf(0, screen_line, "SCSP_VOIC: %d actives", active); //Active voices 
}

#define VDP1_EDSR   (*(volatile unsigned short *)0x25D00010)
void debug_vdp1(int screen_line)
{    
    /*
        0	Drawing end flag
        1	Drawing in progress
        2	Command end
        3	OK
    */

    unsigned short status = VDP1_EDSR;
    jo_printf(0, screen_line, "VDP1_STAT: %04X", status);
}