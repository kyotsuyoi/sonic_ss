#ifndef DEBUG_H
#define DEBUG_H

#include <jo/jo.h>

typedef enum
{
	DebugDisplayHardware = 0,
	DebugDisplayPlayer
} debug_display_mode_t;

typedef struct
{
	int center_dx;
	int center_dy;
	int hreach_p1;
	int hreach_p2;
	int hreach_k1;
	int hreach_k2;
	int vreach;
	bool hit_p1;
	bool hit_p2;
	bool hit_k1;
	bool hit_k2;
} debug_hitbox_snapshot_t;

/* debug info set by game_loop_apply_hit_effect */
extern int g_dbg_knock_dir;
extern float g_dbg_knock_force;
extern float g_dbg_knock_speed;
extern int g_dbg_knock_dx;

/* debug info for damage tracking */
extern int g_debug_last_damage_dealt;
extern int g_debug_last_damage_dealt_target;
extern int g_debug_last_damage_received;
extern int g_debug_last_damage_received_from;

/* debug info for knockback tracking */
extern int g_debug_last_knockback_dealt;
extern int g_debug_last_knockback_received;

/* Inicializa debug (se precisar) */
void debug_init(void);

/* Track last damage dealt/received for the local player */
void debug_track_player_damage_dealt(int target_id, int damage);
void debug_track_player_damage_received(int attacker_id, int damage);

/* Track last knockback dealt/received for the local player */
void debug_track_player_knockback_dealt(int knockback);
void debug_track_player_knockback_received(int knockback);

void debug_frame(void);
void debug_set_display_mode(debug_display_mode_t mode);
debug_display_mode_t debug_get_display_mode(void);

/* Chamar no draw */
void debug_draw(void);

/* Chamar no update */
void debug_update(void);

void print_debug_valu(char* description, int value, char* suffix, int screen_line);
void print_debug_side(char* description, bool player_value, int screen_line);
void print_debug_bool(char* description, bool player_value, int screen_line);
void debug_stack_usage(int screen_line);
void debug_scsp_memory(int screen_line, int screen_line2);
void debug_scsp(int screen_line);
void debug_vdp1(int screen_line);

#endif