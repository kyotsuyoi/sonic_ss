#include "debug.h"
#include "player.h"
#include "bot.h"
#include "game_loop.h"
#include "character_registry.h"
#include "game_constants.h"
#include <math.h>

/* globals exposed via debug.h */
int g_dbg_knock_dir = 0;
float g_dbg_knock_force = 0.0f;
float g_dbg_knock_speed = 0.0f;
int g_dbg_knock_dx = 0;

/* globals for debug damage tracking */
int g_debug_last_damage_dealt = 0;
int g_debug_last_damage_dealt_target = -1;
int g_debug_last_damage_received = 0;
int g_debug_last_damage_received_from = -1;

/* cumulative per-character damage dealt during the current fight */
int g_debug_battle_damage_dealt[CHARACTER_ID_SHADOW + 1] = {0};

/* globals for debug knockback tracking */
int g_debug_last_knockback_dealt = 0;
int g_debug_last_knockback_received = 0;

/* globals for debug stun tracking */
int g_debug_last_stun_dealt = 0;
int g_debug_last_stun_dealt_target = -1;
int g_debug_last_stun_received = 0;
int g_debug_last_stun_received_from = -1;

void debug_track_player_damage_dealt(int target_id, int damage)
{
    g_debug_last_damage_dealt = damage;
    g_debug_last_damage_dealt_target = target_id;
}

void debug_battle_add_damage(int attacker_id, int damage)
{
    //From first to last character, to avoid out-of-bounds if character_id is invalid
    if (attacker_id < CHARACTER_ID_SONIC || attacker_id > CHARACTER_ID_SHADOW)
        return;
    if (damage <= 0)
        return;

    g_debug_battle_damage_dealt[attacker_id] += damage;
}

void debug_track_player_stun_dealt(int target_id, int stun_frames)
{
    g_debug_last_stun_dealt = stun_frames;
    g_debug_last_stun_dealt_target = target_id;
}

void debug_track_player_stun_received(int attacker_id, int stun_frames)
{
    g_debug_last_stun_received = stun_frames;
    g_debug_last_stun_received_from = attacker_id;
}

void debug_track_player_damage_received(int attacker_id, int damage)
{
    g_debug_last_damage_received = damage;
    g_debug_last_damage_received_from = attacker_id;

    if (attacker_id >= CHARACTER_ID_SONIC && attacker_id <= CHARACTER_ID_SHADOW)
        g_debug_battle_damage_dealt[attacker_id] += damage;
}

void debug_track_player_knockback_dealt(float knockback)
{
    /* Store in centi-units to match balance display (e.g., 1.70 -> 170) */
    g_debug_last_knockback_dealt = (int)(knockback * 100.0f + 0.5f);
}

void debug_track_player_knockback_received(float knockback)
{
    g_debug_last_knockback_received = (int)(knockback * 100.0f + 0.5f);
}

/* Balance tuning */
static debug_balance_profile_t g_debug_balance_profiles[CHARACTER_ID_SHADOW + 1];

static void fill_balance_profile(debug_balance_profile_t *out, const character_combat_profile_t *src)
{
    if (out == JO_NULL || src == JO_NULL)
        return;

    /* damage */
    out->damage[DebugBalanceAttackPunch1] = src->damage_punch1;
    out->damage[DebugBalanceAttackPunch2] = src->damage_punch2;
    out->damage[DebugBalanceAttackKick1] = src->damage_kick1;
    out->damage[DebugBalanceAttackKick2] = src->damage_kick2;
    out->damage[DebugBalanceAttackAir] = src->damage_kick2; /* air uses same base as K2 */
    out->damage[DebugBalanceAttackCharged] = src->charged_kick_damage;

    /* knockback (scaled by 100 for display) */
    out->knockback[DebugBalanceAttackPunch1] = (int)(src->knockback_punch1 * 100.0f + 0.5f);
    out->knockback[DebugBalanceAttackPunch2] = (int)(src->knockback_punch2 * 100.0f + 0.5f);
    out->knockback[DebugBalanceAttackKick1] = (int)(src->knockback_kick1 * 100.0f + 0.5f);
    out->knockback[DebugBalanceAttackKick2] = (int)(src->knockback_kick2 * 100.0f + 0.5f);
    out->knockback[DebugBalanceAttackAir] = out->knockback[DebugBalanceAttackKick2]; /* air uses same base as K2 */
    out->knockback[DebugBalanceAttackCharged] = (int)(src->charged_kick_knockback_mult * 100.0f + 0.5f);

    /* stun */
    out->stun[DebugBalanceAttackPunch1] = STUN_LIGHT_FRAMES;
    out->stun[DebugBalanceAttackPunch2] = STUN_HEAVY_FRAMES;
    out->stun[DebugBalanceAttackKick1] = STUN_LIGHT_FRAMES;
    out->stun[DebugBalanceAttackKick2] = STUN_HEAVY_FRAMES;
    out->stun[DebugBalanceAttackAir] = STUN_LIGHT_FRAMES;
    out->stun[DebugBalanceAttackCharged] = STUN_HEAVY_FRAMES + src->charged_kick_stun_bonus;

    /* impulse (scaled by 100 for display) */
    out->impulse[DebugBalanceAttackPunch1] = (int)(src->attack_forward_impulse_light * 100.0f + 0.5f);
    out->impulse[DebugBalanceAttackPunch2] = (int)(src->attack_forward_impulse_heavy * 100.0f + 0.5f);
    out->impulse[DebugBalanceAttackKick1] = out->impulse[DebugBalanceAttackPunch1];
    out->impulse[DebugBalanceAttackKick2] = out->impulse[DebugBalanceAttackPunch2];
    out->impulse[DebugBalanceAttackAir] = out->impulse[DebugBalanceAttackPunch2];
    out->impulse[DebugBalanceAttackCharged] = out->impulse[DebugBalanceAttackPunch2];

    /* SPIN defaults (counter hit behavior) */
    out->damage[DebugBalanceAttackSpin] = 0;
    out->knockback[DebugBalanceAttackSpin] = 200;
    out->stun[DebugBalanceAttackSpin] = COUNTER_STUN_FRAMES;
    out->impulse[DebugBalanceAttackSpin] = 0;
}

void debug_balance_init(void)
{
    for (int cid = CHARACTER_ID_SONIC; cid <= CHARACTER_ID_SHADOW; ++cid)
    {
        character_combat_profile_t profile = character_registry_get_combat_profile_by_character_id(cid);
        fill_balance_profile(&g_debug_balance_profiles[cid], &profile);
    }
}

void debug_balance_reset_profile(int character_id)
{
    if (character_id < CHARACTER_ID_SONIC || character_id > CHARACTER_ID_SHADOW)
        return;

    character_combat_profile_t profile = character_registry_get_combat_profile_by_character_id(character_id);
    fill_balance_profile(&g_debug_balance_profiles[character_id], &profile);
}

debug_balance_profile_t *debug_balance_get_profile(int character_id)
{
    if (character_id < CHARACTER_ID_SONIC || character_id > CHARACTER_ID_SHADOW)
        return JO_NULL;
    return &g_debug_balance_profiles[character_id];
}

void debug_balance_apply_to_character(character_t *character)
{
    if (character == JO_NULL)
        return;

    debug_balance_profile_t *profile = debug_balance_get_profile(character->character_id);
    if (profile == JO_NULL)
        return;

    /* Apply impulse/knockback defaults so the player feels the updated balance in-game.
       Note: damage values are applied where attacks are resolved (game_loop / bot). */
    character->attack_forward_impulse_light = debug_balance_get_impulse(character->character_id, DebugBalanceAttackPunch1);
    character->attack_forward_impulse_heavy = debug_balance_get_impulse(character->character_id, DebugBalanceAttackPunch2);
    character->knockback_punch1 = debug_balance_get_knockback(character->character_id, DebugBalanceAttackPunch1);
    character->knockback_punch2 = debug_balance_get_knockback(character->character_id, DebugBalanceAttackPunch2);
    character->knockback_kick1 = debug_balance_get_knockback(character->character_id, DebugBalanceAttackKick1);
    character->knockback_kick2 = debug_balance_get_knockback(character->character_id, DebugBalanceAttackKick2);
}

int debug_balance_get_damage(int character_id, debug_balance_attack_t attack)
{
    debug_balance_profile_t *profile = debug_balance_get_profile(character_id);
    if (profile == JO_NULL || attack < 0 || attack >= DebugBalanceAttackCount)
        return 0;
    return profile->damage[attack];
}

float debug_balance_get_knockback(int character_id, debug_balance_attack_t attack)
{
    debug_balance_profile_t *profile = debug_balance_get_profile(character_id);
    if (profile == JO_NULL || attack < 0 || attack >= DebugBalanceAttackCount)
        return 0.0f;
    return (float)profile->knockback[attack] / 100.0f;
}

int debug_balance_get_stun(int character_id, debug_balance_attack_t attack)
{
    debug_balance_profile_t *profile = debug_balance_get_profile(character_id);
    if (profile == JO_NULL || attack < 0 || attack >= DebugBalanceAttackCount)
        return 0;
    return profile->stun[attack];
}

float debug_balance_get_impulse(int character_id, debug_balance_attack_t attack)
{
    debug_balance_profile_t *profile = debug_balance_get_profile(character_id);
    if (profile == JO_NULL || attack < 0 || attack >= DebugBalanceAttackCount)
        return 0.0f;
    return (float)profile->impulse[attack] / 100.0f;
}

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

    /* Initialize the in-game balance tuning profiles from defaults */
    debug_balance_init();
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

void debug_battle_stats_reset(void)
{
    for (int i = CHARACTER_ID_SONIC; i <= CHARACTER_ID_SHADOW; ++i)
        g_debug_battle_damage_dealt[i] = 0;
}

int debug_battle_damage_dealt(int character_id)
{
    if (character_id < CHARACTER_ID_SONIC || character_id > CHARACTER_ID_SHADOW)
        return 0;
    return g_debug_battle_damage_dealt[character_id];
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