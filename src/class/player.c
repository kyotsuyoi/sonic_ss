#include <jo/jo.h>
#include <string.h>
#include "player.h"
#include "character/tails.h"
#include "game_audio.h"
#include "game_constants.h"
#include "game_flow.h"

character_action_status_t character_update_cooldowns(character_t *character, int *jump_cooldown)
{
    if (character == JO_NULL)
        return CharacterActionBlockedDefeated;

    if (jump_cooldown != JO_NULL && *jump_cooldown > 0)
        (*jump_cooldown)--;

    if (character->attack_cooldown > 0)
        character->attack_cooldown--;

    if (character->stun_timer > 0)
        character->stun_timer--;

    if (character->life <= 0)
        return CharacterActionBlockedDefeated;
    if (character->stun_timer > 0)
        return CharacterActionBlockedStun;
    if (jump_cooldown != JO_NULL && *jump_cooldown > 0)
        return CharacterActionBlockedJumpCooldown;
    if (character->attack_cooldown > 0)
        return CharacterActionBlockedAttackCooldown;

    return CharacterActionAllowed;
}
#include "world_map.h"
#include "world_physics.h"
#include "world_collision.h"
#include "control.h"
#include "debug.h"
#include "character/sonic.h"
#include "character/amy.h"
#include "character/knuckles.h"
#include "character/shadow.h"

#define DEFAULT_ATTACK_FORWARD_IMPULSE_LIGHT 0.60f
#define DEFAULT_ATTACK_FORWARD_IMPULSE_HEAVY 1.10f
#define CONTROL_FRAME_MS (17)
#define VARIABLE_JUMP_MAX_HOLD_MS (170)
#define VARIABLE_JUMP_MIN_SPEED_FACTOR (0.40f)
#define CHARGED_KICK_HOLD_MS (1000)
#define GROUND_NO_WALK_BRAKE_FACTOR (0.65f)
#define GROUND_NO_WALK_STOP_EPSILON (0.20f)

/* Tails specific animation state constants */
#define TAILS_KICK1_ROTATION_TIME 16
#define TAILS_KICK2_ROTATION_TIME 32
#define TAILS_TAIL_FRAME_COUNT 8
#define TAILS_TAIL_FRAME_DURATION 5

character_t player;
character_t player2;

/* Toggle for showing debug logs for punch/kick state machines. */
bool g_show_attack_debug = false;

/* Runtime state for each player instance (P1 / P2 / bots). */
static player_instance_t g_player_instances[PLAYER_MAX_DEFAULT_COUNT];
static bool g_players_initialized = false;

static void player_init_instances(void)
{
    if (g_players_initialized)
        return;

    for (int i = 0; i < PLAYER_MAX_DEFAULT_COUNT; ++i)
    {
        g_player_instances[i].id = i;
        g_player_instances[i].active = (i == 0); /* P1 is always active by default */
        g_player_instances[i].character = (i == 0) ? &player : &player2;
        memset(&g_player_instances[i].physics, 0, sizeof(g_player_instances[i].physics));
        g_player_instances[i].world_x = (float)g_player_instances[i].character->x;
        g_player_instances[i].world_y = (float)g_player_instances[i].character->y;
        g_player_instances[i].initialized = false;
        g_player_instances[i].defeated = false;
        g_player_instances[i].jump_cooldown = 0;
        g_player_instances[i].jump_hold_ms = 0;
        g_player_instances[i].jump_cut_applied = false;
        g_player_instances[i].airborne_time_ms = 0;
        g_player_instances[i].prev_punch = false;
        g_player_instances[i].prev_punch2 = false;
        g_player_instances[i].prev_kick = false;
        g_player_instances[i].prev_kick2 = false;
    }

    g_players_initialized = true;
}

/* Debug snapshot for player-vs-player hitbox checks */
static int g_dbg_pvp_center_dx = 0;
static int g_dbg_pvp_center_dy = 0;
static int g_dbg_pvp_hreach_p1 = 0;
static int g_dbg_pvp_hreach_p2 = 0;
static int g_dbg_pvp_hreach_k1 = 0;
static int g_dbg_pvp_hreach_k2 = 0;
static int g_dbg_pvp_vreach = 0;
static bool g_dbg_pvp_hit_p1 = false;
static bool g_dbg_pvp_hit_p2 = false;
static bool g_dbg_pvp_hit_k1 = false;
static bool g_dbg_pvp_hit_k2 = false;
static bool g_dbg_pvp_available = false;

player_instance_t *player_get_instance(int index)
{
    player_init_instances();

    if (index < 0 || index >= PLAYER_MAX_DEFAULT_COUNT)
        return JO_NULL;

    return &g_player_instances[index];
}

player_instance_t *player_get_default_player(void)
{
    return player_get_instance(0);
}

player_instance_t *player_get_default_player2(void)
{
    return player_get_instance(1);
}

bool player2_is_active(void)
{
    player_instance_t *inst = player_get_default_player2();
    return inst != JO_NULL && inst->active;
}

static void player_instance_reset_runtime(player_instance_t *inst, int *map_pos_x, int *map_pos_y)
{
    if (inst == JO_NULL)
        return;

    inst->initialized = false;
    inst->defeated = false;
    inst->jump_cooldown = 0;
    inst->jump_hold_ms = 0;
    inst->jump_cut_applied = false;
    inst->airborne_time_ms = 0;
    inst->prev_punch = false;
    inst->prev_punch2 = false;
    inst->prev_kick = false;
    inst->prev_kick2 = false;
    inst->character->landed = false;
    inst->character->was_in_air = false;
    /* leave generic player animation state out of this layer; use character-specific module state */
    inst->character->sonic_anim_mode = 0;
    inst->character->sonic_anim_frame = 0;
    inst->character->sonic_anim_ticks = 0;
    inst->character->knuckles_anim_mode = 0;
    inst->character->knuckles_anim_frame = 0;
    inst->character->knuckles_anim_ticks = 0;
    player_reset_tail_state(inst->character);

    /* Use the current character screen position (set by reset_fight), not an offset from others. */
    inst->world_x = (float)(*map_pos_x + inst->character->x);
    inst->world_y = (float)(*map_pos_y + inst->character->y);
}

void player_instance_update_runtime(player_instance_t *inst, int *map_pos_x, int *map_pos_y)
{
    if (inst == JO_NULL || !inst->active)
        return;

    player_runtime_update(inst->character,
                          &inst->physics,
                          &inst->world_x,
                          &inst->world_y,
                          &inst->initialized,
                          &inst->defeated,
                          &inst->jump_cooldown,
                          &inst->jump_hold_ms,
                          &inst->jump_cut_applied,
                          &inst->airborne_time_ms,
                          inst->id,
                          map_pos_x,
                          map_pos_y);
}

void player_apply_defeated_state(character_t *controlled_player,
                                jo_sidescroller_physics_params *physics,
                                bool *defeated_flag)
{
    if (controlled_player == JO_NULL || physics == JO_NULL)
        return;

    if (defeated_flag != JO_NULL)
        *defeated_flag = true;

    controlled_player->walk = false;
    controlled_player->run = 0;
    controlled_player->spin = false;
    controlled_player->punch = false;
    controlled_player->punch2 = false;
    controlled_player->kick = false;
    controlled_player->kick2 = false;

    jo_physics_apply_friction(physics);
}

void player_instance_handle_input(player_instance_t *inst,
                                     const control_input_t *input,
                                     jo_sound *jump_sfx,
                                     int jump_sfx_channel,
                                     jo_sound *spin_sfx,
                                     jo_sound *punch_sfx,
                                     jo_sound *kick_sfx)
{
    if (inst == JO_NULL || !inst->active)
        return;

    if (inst->character->life <= 0)
    {
        inst->active = true; // keep alive to continue draw/update paths for defeated state
        player_apply_defeated_state(inst->character, &inst->physics, &inst->defeated);
        return;
    }

    character_action_status_t status = character_update_cooldowns(inst->character, &inst->jump_cooldown);

    if (status != CharacterActionAllowed)
    {
        if (status == CharacterActionBlockedDefeated)
        {
            inst->active = true; // keep active for defeated logic and draw
            player_apply_defeated_state(inst->character, &inst->physics, &inst->defeated);
            return;
        }

        if (status == CharacterActionBlockedStun)
        {
            player_apply_defeated_state(inst->character, &inst->physics, JO_NULL);
            return;
        }

        if (status == CharacterActionBlockedJumpCooldown || status == CharacterActionBlockedAttackCooldown)
        {
            control_handle_character_commands(&inst->physics,
                                              inst->character,
                                              &inst->jump_cooldown,
                                              &inst->jump_hold_ms,
                                              &inst->jump_cut_applied,
                                              jump_sfx,
                                              jump_sfx_channel,
                                              spin_sfx,
                                              punch_sfx,
                                              kick_sfx,
                                              input);
            return;
        }
    }

    control_handle_character_commands(&inst->physics,
                                      inst->character,
                                      &inst->jump_cooldown,
                                      &inst->jump_hold_ms,
                                      &inst->jump_cut_applied,
                                      jump_sfx,
                                      jump_sfx_channel,
                                      spin_sfx,
                                      punch_sfx,
                                      kick_sfx,
                                      input);
}

void player2_sync_mode(bool multiplayer_versus)
{
    player_instance_t *p2 = player_get_default_player2();
    if (p2 == JO_NULL)
        return;

    if (!multiplayer_versus)
    {
        p2->active = false;
        return;
    }

    p2->active = true;
}

void player2_reset_runtime(int *map_pos_x, int *map_pos_y)
{
    player_instance_t *p2 = player_get_default_player2();
    if (p2 == JO_NULL)
        return;

    player_instance_reset_runtime(p2, map_pos_x, map_pos_y);
}

void player2_update_runtime(int *map_pos_x, int *map_pos_y)
{
    player_instance_update_runtime(player_get_default_player2(), map_pos_x, map_pos_y);
}

bool player2_is_defeated(void)
{
    player_instance_t *p2 = player_get_default_player2();
    return (p2 != JO_NULL) ? (p2->character->life <= 0) : true;
}

jo_sidescroller_physics_params *player2_get_physics(void)
{
    player_instance_t *p2 = player_get_default_player2();
    return (p2 != JO_NULL) ? &p2->physics : JO_NULL;
}

static bool player_debug_is_attack_in_range(int attacker_x,
                                              int attacker_y,
                                              bool attacker_flip,
                                              int target_x,
                                              int target_y,
                                              int range)
{
    int forward_range = range;
    int attacker_left = attacker_x;
    int attacker_right = attacker_x + CHARACTER_WIDTH;
    int attacker_top = attacker_y;
    int attacker_bottom = attacker_y + CHARACTER_HEIGHT;
    int attack_left;
    int attack_right;
    int target_left = target_x;
    int target_right = target_x + CHARACTER_WIDTH;
    int target_top = target_y;
    int target_bottom = target_y + CHARACTER_HEIGHT;

    if (forward_range > 1)
        forward_range = (forward_range + 1) / 2;

    if (attacker_flip)
    {
        attack_left = attacker_left - forward_range;
        attack_right = attacker_left;
    }
    else
    {
        attack_left = attacker_right;
        attack_right = attacker_right + forward_range;
    }

    return target_right >= attack_left
        && target_left <= attack_right
        && target_bottom >= attacker_top
        && target_top <= attacker_bottom;
}

void player2_update_pvp_hitbox_snapshot(int p1_world_x, int p1_world_y, int p2_world_x, int p2_world_y)
{
	player_instance_t *p2 = player_get_default_player2();
	if (p2 == JO_NULL || !p2->active)
	{
		g_dbg_pvp_available = false;
		return;
	}

	int center_x_p1 = p1_world_x + (CHARACTER_WIDTH / 2);
	int center_x_p2 = p2_world_x + (CHARACTER_WIDTH / 2);
	int center_y_p1 = p1_world_y + (CHARACTER_HEIGHT / 2);
	int center_y_p2 = p2_world_y + (CHARACTER_HEIGHT / 2);

	g_dbg_pvp_center_dx = JO_ABS(center_x_p1 - center_x_p2);
	g_dbg_pvp_center_dy = JO_ABS(center_y_p1 - center_y_p2);
	g_dbg_pvp_hreach_p1 = (CHARACTER_WIDTH / 2) + player.hit_range_punch1;
	g_dbg_pvp_hreach_p2 = (CHARACTER_WIDTH / 2) + player.hit_range_punch2;
	g_dbg_pvp_hreach_k1 = (CHARACTER_WIDTH / 2) + player.hit_range_kick1;
	g_dbg_pvp_hreach_k2 = (CHARACTER_WIDTH / 2) + player.hit_range_kick2;
	g_dbg_pvp_vreach = (CHARACTER_HEIGHT / 2) + 12;

	g_dbg_pvp_hit_p1 = player_debug_is_attack_in_range(p1_world_x,
	                                                 p1_world_y,
	                                                 player.flip,
	                                                 p2_world_x,
	                                                 p2_world_y,
	                                                 player.hit_range_punch1);
	g_dbg_pvp_hit_p2 = player_debug_is_attack_in_range(p1_world_x,
	                                                 p1_world_y,
	                                                 player.flip,
	                                                 p2_world_x,
	                                                 p2_world_y,
	                                                 player.hit_range_punch2);
	g_dbg_pvp_hit_k1 = player_debug_is_attack_in_range(p1_world_x,
	                                                 p1_world_y,
	                                                 player.flip,
	                                                 p2_world_x,
	                                                 p2_world_y,
	                                                 player.hit_range_kick1);
	g_dbg_pvp_hit_k2 = player_debug_is_attack_in_range(p1_world_x,
	                                                 p1_world_y,
	                                                 player.flip,
	                                                 p2_world_x,
	                                                 p2_world_y,
	                                                 player.hit_range_kick2);

	g_dbg_pvp_available = true;
}

bool player2_debug_get_hitbox_snapshot(debug_hitbox_snapshot_t *snapshot)
{
	if (snapshot == JO_NULL || !g_dbg_pvp_available)
		return false;

	snapshot->center_dx = g_dbg_pvp_center_dx;
	snapshot->center_dy = g_dbg_pvp_center_dy;
	snapshot->hreach_p1 = g_dbg_pvp_hreach_p1;
	snapshot->hreach_p2 = g_dbg_pvp_hreach_p2;
	snapshot->hreach_k1 = g_dbg_pvp_hreach_k1;
	snapshot->hreach_k2 = g_dbg_pvp_hreach_k2;
	snapshot->vreach = g_dbg_pvp_vreach;
	snapshot->hit_p1 = g_dbg_pvp_hit_p1;
	snapshot->hit_p2 = g_dbg_pvp_hit_p2;
	snapshot->hit_k1 = g_dbg_pvp_hit_k1;
	snapshot->hit_k2 = g_dbg_pvp_hit_k2;
	return true;
}

void player2_handle_input(const control_input_t *input)
{
    player_instance_t *p2 = player_get_default_player2();
    player_instance_handle_input(p2,
                                 input,
                                 game_audio_get_jump_sfx(),
                                 0,
                                 game_audio_get_spin_sfx(),
                                 game_audio_get_punch_sfx(),
                                 game_audio_get_kick_sfx());
}

int player_get_max_instances(void)
{
	return PLAYER_MAX_DEFAULT_COUNT;
}

static inline float player_get_attack_forward_impulse_light(const character_t *controlled_player)
{
	return (controlled_player->attack_forward_impulse_light > 0.0f)
		? controlled_player->attack_forward_impulse_light
		: DEFAULT_ATTACK_FORWARD_IMPULSE_LIGHT;
}

static inline float player_get_attack_forward_impulse_heavy(const character_t *controlled_player)
{
	return (controlled_player->attack_forward_impulse_heavy > 0.0f)
		? controlled_player->attack_forward_impulse_heavy
		: DEFAULT_ATTACK_FORWARD_IMPULSE_HEAVY;
}

static inline void player_apply_attack_forward_impulse(jo_sidescroller_physics_params *physics,
													   const character_t *controlled_player,
													   float impulse)
{
	if (controlled_player->flip)
		physics->speed -= impulse;
	else
		physics->speed += impulse;
}

static inline void player_make_jump(jo_sidescroller_physics_params *physics,
									character_t *controlled_player,
									jo_sound *jump_sfx,
									int jump_sfx_channel)
{
	(void)jump_sfx_channel;
	controlled_player->can_jump = false;
	controlled_player->jump = true;
	controlled_player->punch = false;
	controlled_player->kick = false;

	if (jump_sfx != JO_NULL)
		game_audio_play_sfx_next_channel(jump_sfx);
	jo_physics_jump(physics);
}

static void player_update_variable_jump(jo_sidescroller_physics_params *physics,
										bool b_down,
										int *jump_hold_ms,
										bool *jump_cut_applied)
{
	float hold_ratio;
	float desired_factor;
	float desired_speed_y;

	if (jump_hold_ms == JO_NULL || jump_cut_applied == JO_NULL)
		return;

	if (!physics->is_in_air)
	{
		*jump_hold_ms = 0;
		*jump_cut_applied = false;
		return;
	}

	if (b_down && *jump_hold_ms < VARIABLE_JUMP_MAX_HOLD_MS)
	{
		*jump_hold_ms += CONTROL_FRAME_MS;
		if (*jump_hold_ms > VARIABLE_JUMP_MAX_HOLD_MS)
			*jump_hold_ms = VARIABLE_JUMP_MAX_HOLD_MS;
	}

	if (b_down || *jump_cut_applied)
		return;

	if (physics->speed_y >= 0.0f)
	{
		*jump_cut_applied = true;
		return;
	}

	hold_ratio = (float)(*jump_hold_ms) / (float)VARIABLE_JUMP_MAX_HOLD_MS;
	if (hold_ratio < 0.0f)
		hold_ratio = 0.0f;
	if (hold_ratio > 1.0f)
		hold_ratio = 1.0f;

	desired_factor = VARIABLE_JUMP_MIN_SPEED_FACTOR + ((1.0f - VARIABLE_JUMP_MIN_SPEED_FACTOR) * hold_ratio);
	desired_speed_y = physics->jump_speed_y * desired_factor;

	if (physics->speed_y < desired_speed_y)
		physics->speed_y = desired_speed_y;

	*jump_cut_applied = true;
}

static void player_input_punch(character_t *controlled_player,
							   jo_sidescroller_physics_params *physics,
							   jo_sound *punch_sfx,
							   jo_sound *kick_sfx,
							   bool a_down)
{
	if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2 && !physics->is_in_air && !controlled_player->jump && controlled_player->attack_cooldown == 0 && a_down)
	{
		controlled_player->punch = true;
		controlled_player->hit_done_punch1 = false;
		/* start stage1 timer, reset any leftover stage2 timer */
		controlled_player->punch_stage1_ticks = DEFAULT_SPRITE_FRAME_DURATION * 2;
		controlled_player->punch_stage2_ticks = 0;
		player_apply_attack_forward_impulse(physics, controlled_player, player_get_attack_forward_impulse_light(controlled_player));
		if (punch_sfx != JO_NULL)
			game_audio_play_sfx_next_channel(punch_sfx);
	}
	else if (controlled_player->punch && !controlled_player->punch2 && a_down)
	{
		controlled_player->punch2_requested = true;
		controlled_player->hit_done_punch2 = false;
	}

	if (controlled_player->punch2 && controlled_player->perform_punch2)
	{
		controlled_player->perform_punch2 = false;
		player_apply_attack_forward_impulse(physics, controlled_player, player_get_attack_forward_impulse_heavy(controlled_player));
		if (kick_sfx != JO_NULL)
			game_audio_play_sfx_next_channel(kick_sfx);
	}
}

static void player_input_kick(character_t *controlled_player,
							  jo_sidescroller_physics_params *physics,
							  jo_sound *punch_sfx,
							  jo_sound *kick_sfx,
							  bool c_down,
							  bool c_hold)
{
	/* Keep tracking whether the player is holding kick, even if in air. */
	if (controlled_player->charged_kick_enabled)
		controlled_player->charged_kick_button_held = c_hold;

	if (controlled_player->charged_kick_enabled && !physics->is_in_air)
	{
		if (c_hold
			&& !controlled_player->kick2
			&& !controlled_player->punch
			&& !controlled_player->punch2
			&& controlled_player->attack_cooldown == 0)
		{
			if (!controlled_player->kick)
			{
				controlled_player->kick = true;
				controlled_player->hit_done_kick1 = false;
				controlled_player->kick2 = false;
				controlled_player->kick2_requested = false;
				controlled_player->perform_kick2 = false;
				controlled_player->charged_kick_hold_ms = 0;
				controlled_player->charged_kick_ready = false;
				controlled_player->charged_kick_active = false;
				controlled_player->charged_kick_phase = 0;
				controlled_player->charged_kick_phase_timer = 0;
				if (punch_sfx != JO_NULL)
					game_audio_play_sfx_next_channel(punch_sfx);
			}

			controlled_player->kick = true;

			if (controlled_player->charged_kick_hold_ms < CHARGED_KICK_HOLD_MS)
			{
				controlled_player->charged_kick_hold_ms += CONTROL_FRAME_MS;
				if (controlled_player->charged_kick_hold_ms > CHARGED_KICK_HOLD_MS)
					controlled_player->charged_kick_hold_ms = CHARGED_KICK_HOLD_MS;
			}
			controlled_player->charged_kick_ready = (controlled_player->charged_kick_hold_ms >= CHARGED_KICK_HOLD_MS);
			return;
		}

		if (controlled_player->kick && !controlled_player->kick2)
		{
			controlled_player->kick2_requested = false;

			if (controlled_player->charged_kick_ready)
			{
				controlled_player->kick = false;
				controlled_player->kick2 = true;
				controlled_player->hit_done_kick2 = false;
				controlled_player->kick2_requested = false;
				controlled_player->perform_kick2 = true;
				controlled_player->charged_kick_active = true;
				controlled_player->charged_kick_phase = 1;
				controlled_player->charged_kick_phase_timer = 0;
			}
			else
			{
				controlled_player->kick = false;
				controlled_player->kick2 = false;
				controlled_player->kick2_requested = false;
				controlled_player->perform_kick2 = false;
				controlled_player->charged_kick_hold_ms = 0;
				controlled_player->charged_kick_ready = false;
				controlled_player->charged_kick_active = false;
				controlled_player->charged_kick_phase = 0;
				controlled_player->charged_kick_phase_timer = 0;
			}
			return;
		}

		if (controlled_player->kick2 && controlled_player->perform_kick2)
		{
			controlled_player->perform_kick2 = false;
			player_apply_attack_forward_impulse(physics, controlled_player, player_get_attack_forward_impulse_heavy(controlled_player));
			if (kick_sfx != JO_NULL)
				game_audio_play_sfx_next_channel(kick_sfx);
			return;
		}
	}

	if (physics->is_in_air && c_down && !controlled_player->air_kick_used && !controlled_player->kick && !controlled_player->kick2 && !controlled_player->punch && !controlled_player->punch2)
	{
		controlled_player->kick = false;
		controlled_player->kick2 = true;
		controlled_player->hit_done_kick2 = false;
		controlled_player->kick2_requested = false;
		controlled_player->perform_kick2 = true;
		controlled_player->air_kick_used = true;
		controlled_player->spin = false;
	}

	if (!controlled_player->kick && !controlled_player->kick2 && !controlled_player->punch && !controlled_player->punch2 && !physics->is_in_air && !controlled_player->jump && controlled_player->attack_cooldown == 0 && c_down)
	{
		controlled_player->kick = true;
		controlled_player->hit_done_kick1 = false;
		/* start stage1 timer, clear any leftover stage2 timer */
		controlled_player->kick_stage1_ticks = DEFAULT_SPRITE_FRAME_DURATION * 2;
		controlled_player->kick_stage2_ticks = 0;
		player_apply_attack_forward_impulse(physics, controlled_player, player_get_attack_forward_impulse_light(controlled_player));
		if (punch_sfx != JO_NULL)
			game_audio_play_sfx_next_channel(punch_sfx);
	}
	else if (controlled_player->kick && !controlled_player->kick2 && c_down && !controlled_player->charged_kick_enabled)
	{
		controlled_player->kick2_requested = true;
		controlled_player->hit_done_kick2 = false;
	}

	if (controlled_player->kick2 && controlled_player->perform_kick2)
	{
		controlled_player->perform_kick2 = false;
		player_apply_attack_forward_impulse(physics, controlled_player, player_get_attack_forward_impulse_heavy(controlled_player));
		if (kick_sfx != JO_NULL)
			game_audio_play_sfx_next_channel(kick_sfx);
	}
}

void player_handle_command_inputs(jo_sidescroller_physics_params *physics,
								  character_t *controlled_player,
								  int *jump_cooldown,
								  int *jump_hold_ms,
								  bool *jump_cut_applied,
								  jo_sound *jump_sfx,
								  int jump_sfx_channel,
								  jo_sound *spin_sfx,
								  jo_sound *punch_sfx,
								  jo_sound *kick_sfx,
								  bool left_pressed,
								  bool right_pressed,
								  bool spin_pressed,
								  bool b_pressed,
								  bool b_down,
								  bool a_down,
								  bool c_down,
								  bool c_hold)
{
	// static int debug_input_frames = 60;
	// if (debug_input_frames > 0 && controlled_player == &player)
	// {
	// 	debug_input_frames--;
	// 	jo_printf(1, 19, "INP L=%d R=%d B=%d A=%d C=%d", left_pressed ? 1 : 0, right_pressed ? 1 : 0, b_down ? 1 : 0, a_down ? 1 : 0, c_down ? 1 : 0);
	// }

	if (left_pressed || right_pressed || spin_pressed || b_pressed || b_down || a_down || c_down || c_hold)
	{
		controlled_player->landed = false;
	}

	if (physics->is_in_air)
	{
		if (left_pressed)
		{
			controlled_player->flip = true;
			jo_physics_accelerate_left(physics);
		}
		else if (right_pressed)
		{
			controlled_player->flip = false;
			jo_physics_accelerate_right(physics);
		}
	}
	else
	{
		// Ensure spin is cleared when touching the ground (both P1 and P2).
		controlled_player->spin = false;
		controlled_player->angle = 0;

		controlled_player->air_kick_used = false;
		if (jo_physics_is_standing(physics))
		{
			controlled_player->walk = false;
			controlled_player->run = false;
		}

		if (left_pressed)
		{
			controlled_player->flip = true;
			controlled_player->walk = true;
			if (jo_physics_is_going_on_the_right(physics) || jo_physics_should_brake(physics))
				jo_physics_decelerate_left(physics);
			else if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2)
				jo_physics_accelerate_left(physics);
			else
				jo_physics_apply_friction(physics);
		}
		else if (right_pressed)
		{
			controlled_player->flip = false;
			controlled_player->walk = true;
			if (jo_physics_is_going_on_the_left(physics) || jo_physics_should_brake(physics))
				jo_physics_decelerate_right(physics);
			else if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2)
				jo_physics_accelerate_right(physics);
			else
				jo_physics_apply_friction(physics);
		}
		else
			jo_physics_apply_friction(physics);
	}

	if (controlled_player->can_jump && b_pressed && *jump_cooldown == 0)
	{
		*jump_cooldown = 20;
		if (jump_hold_ms != JO_NULL)
			*jump_hold_ms = 0;
		if (jump_cut_applied != JO_NULL)
			*jump_cut_applied = false;
		player_make_jump(physics, controlled_player, jump_sfx, jump_sfx_channel);
	}

	player_update_variable_jump(physics, b_down, jump_hold_ms, jump_cut_applied);

	// Allow spin to start if player is airborne (or just started a jump).
	if (spin_pressed && (physics->is_in_air || controlled_player->jump))
		controlled_player->spin = true;

	player_input_punch(controlled_player, physics, punch_sfx, kick_sfx, a_down);
	player_input_kick(controlled_player, physics, punch_sfx, kick_sfx, c_down, c_hold);

	/* Global anti-slide: when no walk input on ground, bleed horizontal speed fast. */
	if (!physics->is_in_air && !controlled_player->walk)
	{
		physics->speed *= GROUND_NO_WALK_BRAKE_FACTOR;
		if (JO_ABS(physics->speed) < GROUND_NO_WALK_STOP_EPSILON)
			physics->speed = 0.0f;
	}

	if (controlled_player->spin && spin_sfx != JO_NULL)
	{
		if (spin_sfx->current_playing_channel >= JO_SOUND_MAX_CHANNEL || !jo_audio_is_channel_playing(spin_sfx->current_playing_channel))
			game_audio_play_sfx_next_channel(spin_sfx);
	}
}

void player_update_punch_state(character_t *controlled_player,
							   int punch_combo_trigger_frame,
							   int punch_stage1_last_frame,
							   int punch_stage2_start_frame,
							   int punch_stage2_last_frame,
							   bool has_punch2_stage,
							   bool finish_stage1_on_last_frame,
							   bool finish_stage2_on_last_frame)
{
	int anim_frame;
	bool anim_stopped;
	int combo_trigger_frame;

	if (controlled_player == JO_NULL || controlled_player->punch_anim_id < 0)
		return;

	/* Ensure animation is reset if not in punch action, to avoid frame desync after hit */
	if (!controlled_player->punch && !controlled_player->punch2)
	{
		jo_set_sprite_anim_frame(controlled_player->punch_anim_id, 0);
		jo_reset_sprite_anim(controlled_player->punch_anim_id);
		return;
	}

	anim_frame = jo_get_sprite_anim_frame(controlled_player->punch_anim_id);
	anim_stopped = jo_is_sprite_anim_stopped(controlled_player->punch_anim_id);

	/* DEBUG: punch state timers (Sonic/Knuckles only) */
	if (g_show_attack_debug &&
		(controlled_player->character_id == CHARACTER_ID_SONIC || controlled_player->character_id == CHARACTER_ID_KNUCKLES))
	{
		const char *label = (controlled_player == &player) ? "P1" : "P2";
		int y = (controlled_player == &player) ? 9 : 10;
		int attack_mode = 0;
		if (controlled_player->character_id == CHARACTER_ID_KNUCKLES)
			attack_mode = controlled_player->knuckles_anim_mode;
		else if (controlled_player->character_id == CHARACTER_ID_SONIC)
			attack_mode = controlled_player->sonic_anim_mode;

		jo_printf(1, y, "%s ATK mode=%d frame=%d p=%d p2=%d", label,
			attack_mode,
			controlled_player->knuckles_anim_frame,
			controlled_player->punch ? 1 : 0,
			controlled_player->punch2 ? 1 : 0);
	}

	/* Punch flow owns the attack slot; clear any stale kick state to avoid
	   sprite bleed (notably visible on P2 Amy at combo end). */
	if (controlled_player->punch || controlled_player->punch2)
	{
		controlled_player->kick = false;
		controlled_player->kick2 = false;
		controlled_player->kick2_requested = false;
		controlled_player->perform_kick2 = false;
	}

	combo_trigger_frame = punch_combo_trigger_frame;
	if (combo_trigger_frame > punch_stage1_last_frame)
		combo_trigger_frame = punch_stage1_last_frame;

	/* countdown stage timer (used for both stages) */
	if (controlled_player->punch_stage1_ticks > 0)
		controlled_player->punch_stage1_ticks--;

	if (controlled_player->punch)
	{
		/* stage1-specific handling */

		/* debug log for P1 + P2 */
		// if (controlled_player == &player)
		// {
		// 	jo_printf(1, 8, "PUNCH f=%d rq=%d t=%d", anim_frame,
		// 		controlled_player->punch2_requested ? 1 : 0,
		// 		controlled_player->punch_stage1_ticks);
		// }

		/* keep anim running until stage1 finishes */
		if (anim_stopped && anim_frame < punch_stage1_last_frame)
		{
			jo_start_sprite_anim(controlled_player->punch_anim_id);
			anim_stopped = false;
		}

		/* while timer active, clamp to last frame and skip transitions */
		if (controlled_player->punch_stage1_ticks > 0)
		{
			if (anim_frame > punch_stage1_last_frame)
			{
				jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage1_last_frame);
				jo_start_sprite_anim(controlled_player->punch_anim_id);
			}
			return;
		}

		/* timer expired: resume normal flow */
		if (anim_frame > punch_stage1_last_frame)
		{
			if (!controlled_player->punch2_requested)
			{
				jo_set_sprite_anim_frame(controlled_player->punch_anim_id, 0);
				jo_start_sprite_anim(controlled_player->punch_anim_id);
				anim_frame = jo_get_sprite_anim_frame(controlled_player->punch_anim_id);
				anim_stopped = jo_is_sprite_anim_stopped(controlled_player->punch_anim_id);
			}
		}

		if (anim_frame >= combo_trigger_frame)
		{
			if (controlled_player->punch2_requested && has_punch2_stage)
			{
				controlled_player->punch = false;
				controlled_player->punch2 = true;
				controlled_player->punch2_requested = false;
				controlled_player->perform_punch2 = true;

				/* For Sonic/Amy/Tails, stage2 is a full 4-frame animation (frames 4-7). */
				if (controlled_player->character_id == CHARACTER_ID_SONIC || controlled_player->character_id == CHARACTER_ID_AMY || controlled_player->character_id == CHARACTER_ID_TAILS)
				{
					controlled_player->punch_stage2_ticks = 0;
					jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage2_start_frame);
					jo_start_sprite_anim(controlled_player->punch_anim_id);
				}
				else
				{
					/* set total ticks for stage2: two frames each lasting DEFAULT */
					controlled_player->punch_stage2_ticks = DEFAULT_SPRITE_FRAME_DURATION * 2;
					/* explicitly show first frame of stage2; do not start anim */
					jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage2_start_frame);
				}
			}
			else if (anim_frame >= punch_stage1_last_frame
				 && (anim_stopped || finish_stage1_on_last_frame))
			{
				controlled_player->punch = false;
				controlled_player->punch2_requested = false;
				controlled_player->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
				jo_reset_sprite_anim(controlled_player->punch_anim_id);
			}
		}
		return;
	}

	if (!controlled_player->punch2)
		return;

	/* manual control of stage2 frames: ensure frame2 then frame3 each for DEFAULT ticks */
	if (controlled_player->punch_stage2_ticks > 0
		&& !(controlled_player->character_id == CHARACTER_ID_SONIC || controlled_player->character_id == CHARACTER_ID_AMY || controlled_player->character_id == CHARACTER_ID_TAILS))
	{
		int half = DEFAULT_SPRITE_FRAME_DURATION;
		if (controlled_player->punch_stage2_ticks > half)
		{
			jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage2_start_frame);
		}
		else
		{
			jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage2_last_frame);
		}
		/* don't start_anim, just lock frame */
		controlled_player->punch_stage2_ticks--;
		return;
	}

	if (!has_punch2_stage)
	{
		controlled_player->punch2 = false;
		controlled_player->punch2_requested = false;
		controlled_player->attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
		jo_reset_sprite_anim(controlled_player->punch_anim_id);
		return;
	}

	if (anim_frame < punch_stage2_start_frame)
	{
		jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage2_start_frame);
		jo_start_sprite_anim(controlled_player->punch_anim_id);
		anim_frame = jo_get_sprite_anim_frame(controlled_player->punch_anim_id);
	}
	else if (jo_is_sprite_anim_stopped(controlled_player->punch_anim_id) && anim_frame < punch_stage2_last_frame)
	{
		/* Keep stage-2 running until it reaches its end frame. */
		jo_start_sprite_anim(controlled_player->punch_anim_id);
	}

	if (anim_frame >= punch_stage2_last_frame
		&& (jo_is_sprite_anim_stopped(controlled_player->punch_anim_id) || finish_stage2_on_last_frame))
	{
		controlled_player->punch2 = false;
		controlled_player->attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
		jo_reset_sprite_anim(controlled_player->punch_anim_id);
	}
}

void player_update_punch_state_for_character(character_t *controlled_player)
{
	if (controlled_player == JO_NULL)
		return;

	// Sonic/Knuckles use 6-frame punch path (punch row 6 on sheet)
	if (controlled_player->character_id == CHARACTER_ID_SONIC ||
		controlled_player->character_id == CHARACTER_ID_KNUCKLES)
	{
		/* punch frames 0-4 (hit on frame 4), last frame 5 = return to idle */
		/* keep punch true through frame 5, so hit logic for frame 4 executes */
		player_update_punch_state(controlled_player, 4, 5, 5, 5, false, true, true);
		return;
	}

	// Sonic/Amy/Tails legacy 8-frame punch (4+4)
	if (controlled_player->character_id == CHARACTER_ID_AMY ||
		controlled_player->character_id == CHARACTER_ID_TAILS)
	{
		player_update_punch_state(controlled_player, 3, 3, 4, 7, true, true, false);
		return;
	}

	/* Unified 4-frame profile for remaining characters: stage1=0-1, stage2=2-3 */
	player_update_punch_state(controlled_player, 1, 1, 2, 3, true, true, false);
}
void player_update_animation_state(character_t *controlled_player, jo_sidescroller_physics_params *physics)
{
    if (controlled_player == JO_NULL || physics == JO_NULL)
        return;

    if (controlled_player->life <= 0)
    {
        controlled_player->walk = false;
        controlled_player->run = 0;
        controlled_player->spin = false;
        controlled_player->punch = false;
        controlled_player->punch2 = false;
        controlled_player->kick = false;
        controlled_player->kick2 = false;
        jo_physics_apply_friction(physics);
        return;
    }

    bool is_tails = (controlled_player->character_id == CHARACTER_ID_TAILS);
    bool is_sonic = (controlled_player->character_id == CHARACTER_ID_SONIC);
    bool is_amy = (controlled_player->character_id == CHARACTER_ID_AMY);
	bool is_knuckles = (controlled_player->character_id == CHARACTER_ID_KNUCKLES);

    /* Tails-specific tail animation loop state */
    if (is_tails)
    {
        controlled_player->tail_timer++;
        if (controlled_player->tail_timer >= TAILS_TAIL_FRAME_DURATION)
        {
            controlled_player->tail_timer = 0;
            controlled_player->tail_frame = (controlled_player->tail_frame + 1) % TAILS_TAIL_FRAME_COUNT;
        }
    }
    else
    {
        controlled_player->tail_timer = 0;
        controlled_player->tail_frame = 0;

        controlled_player->tails_kick_timer = 0;
        controlled_player->tails_kick_duration = 0;
        controlled_player->tails_kick_total_degrees = 0;
        controlled_player->tails_kick_rotation_active = false;
    }

    /* Sonic/Amy/Tails use WRAM-sheet animation handlers which rely on a global current character pointer. */
    if (is_sonic)
    {
        sonic_set_current(controlled_player, physics);
        sonic_running_animation_handling();
        sonic_set_current(&player, NULL);
    }

    if (is_amy)
    {
        amy_set_current(controlled_player, physics);
        amy_running_animation_handling();
        amy_set_current(&player, NULL);
    }

    if (is_tails)
    {
        tails_set_current(controlled_player, physics);
        tails_running_animation_handling();
        tails_set_current(&player, NULL);
    }

	if (is_knuckles)
	{
		knuckles_set_current(controlled_player, physics);
		knuckles_running_animation_handling();
		knuckles_set_current(&player, NULL);
	}

    /* Generic walk/run/stand animation handling for remaining characters. */
	if (is_sonic || is_amy || is_tails || is_knuckles)
		return;

	if (jo_physics_is_standing(physics))
    {
        jo_reset_sprite_anim(controlled_player->running1_anim_id);
        jo_reset_sprite_anim(controlled_player->running2_anim_id);

        if (!jo_is_sprite_anim_stopped(controlled_player->walking_anim_id))
        {
            jo_reset_sprite_anim(controlled_player->walking_anim_id);
            jo_reset_sprite_anim(controlled_player->stand_sprite_id);
        }
        else
        {
            if (jo_is_sprite_anim_stopped(controlled_player->stand_sprite_id))
                jo_start_sprite_anim_loop(controlled_player->stand_sprite_id);

            if (jo_get_sprite_anim_frame(controlled_player->stand_sprite_id) == 0)
                jo_set_sprite_anim_frame_rate(controlled_player->stand_sprite_id, 70);
            else
                jo_set_sprite_anim_frame_rate(controlled_player->stand_sprite_id, 2);
        }
    }
    else
    {
        int speed_step = (int)JO_ABS(physics->speed);

        if (controlled_player->run == 0)
        {
            jo_reset_sprite_anim(controlled_player->running1_anim_id);
            jo_reset_sprite_anim(controlled_player->running2_anim_id);

            if (jo_is_sprite_anim_stopped(controlled_player->walking_anim_id))
                jo_start_sprite_anim_loop(controlled_player->walking_anim_id);
        }
        else if (controlled_player->run == 1)
        {
            jo_reset_sprite_anim(controlled_player->walking_anim_id);
            jo_reset_sprite_anim(controlled_player->running2_anim_id);

            if (jo_is_sprite_anim_stopped(controlled_player->running1_anim_id))
                jo_start_sprite_anim_loop(controlled_player->running1_anim_id);
        }
        else if (controlled_player->run == 2)
        {
            jo_reset_sprite_anim(controlled_player->walking_anim_id);
            jo_reset_sprite_anim(controlled_player->running1_anim_id);

            if (jo_is_sprite_anim_stopped(controlled_player->running2_anim_id))
                jo_start_sprite_anim_loop(controlled_player->running2_anim_id);
        }

        if (speed_step >= 6)
        {
            jo_set_sprite_anim_frame_rate(controlled_player->running2_anim_id, 3);
            controlled_player->run = 2;
        }
        else if (speed_step >= 5)
        {
            jo_set_sprite_anim_frame_rate(controlled_player->running1_anim_id, 4);
            controlled_player->run = 1;
        }
        else if (speed_step >= 4)
        {
            jo_set_sprite_anim_frame_rate(controlled_player->running1_anim_id, 5);
            controlled_player->run = 1;
        }
        else if (speed_step >= 3)
        {
            jo_set_sprite_anim_frame_rate(controlled_player->running1_anim_id, 6);
            controlled_player->run = 1;
        }
        else if (speed_step >= 2)
        {
            jo_set_sprite_anim_frame_rate(controlled_player->running1_anim_id, 7);
            controlled_player->run = 1;
        }
        else if (speed_step >= 1)
        {
            jo_set_sprite_anim_frame_rate(controlled_player->walking_anim_id, 8);
            controlled_player->run = 0;
        }
        else
        {
            jo_set_sprite_anim_frame_rate(controlled_player->walking_anim_id, 9);
            controlled_player->run = 0;
        }
    }

    /* Knuckles-specific flow removed from gameplay; fallback to common hit state. */
    player_update_punch_state_for_character(controlled_player);
    player_update_kick_state_for_character(controlled_player);
}

bool player_should_draw_tail(const character_t *controlled_player)
{
    if (controlled_player == JO_NULL)
        return false;

    if (controlled_player->character_id != CHARACTER_ID_TAILS)
        return false;

    if (controlled_player->spin)
        return false;
    if (controlled_player->life <= 0)
        return false;
    if (controlled_player->stun_timer > 0)
        return false;
    if (controlled_player->kick2)
        return false;
    if (controlled_player->tails_kick_rotation_active)
        return false;

    return true;
}

void player_reset_tail_state(character_t *controlled_player)
{
    if (controlled_player == JO_NULL)
        return;

    controlled_player->tail_timer = 0;
    controlled_player->tail_frame = 0;

    controlled_player->tails_kick_timer = 0;
    controlled_player->tails_kick_duration = 0;
    controlled_player->tails_kick_total_degrees = 0;
    controlled_player->tails_kick_rotation_active = false;
}

int player_get_tail_offset_x(const character_t *controlled_player)
{
    (void)controlled_player;
    /* Tails uses a fixed tail offset; other chars do not draw a tail. */
    return 14;
}

int player_get_defeated_sprite_height(const character_t *controlled_player)
{
    (void)controlled_player;
    /* Height used for the “defeated” sprite vertical offset. */
    return 32;
}

void player_draw(character_t *controlled_player, jo_sidescroller_physics_params *physics)
{
    int anim_sprite_id;

    if (controlled_player == JO_NULL || physics == JO_NULL)
        return;

    bool apply_flip = controlled_player->flip;
    if (apply_flip)
        jo_sprite_enable_horizontal_flip();

    /* Character-specific drawing (Sonic/Amy use WRAM). */
    if (controlled_player->character_id == CHARACTER_ID_AMY)
    {
        amy_set_current(controlled_player, physics);
        amy_draw(controlled_player);
        amy_set_current(&player, NULL);
    }
    else if (controlled_player->character_id == CHARACTER_ID_SONIC)
    {
        sonic_set_current(controlled_player, physics);
        sonic_draw(controlled_player);
        sonic_set_current(&player, NULL);
    }
    else if (controlled_player->character_id == CHARACTER_ID_TAILS)
    {
        tails_set_current(controlled_player, physics);
        tails_draw(controlled_player);
        tails_set_current(&player, NULL);
    }
    else if (controlled_player->character_id == CHARACTER_ID_KNUCKLES)
    {
        knuckles_set_current(controlled_player, physics);
        knuckles_draw(controlled_player);
        knuckles_set_current(&player, NULL);
    }
    else if (controlled_player->character_id == CHARACTER_ID_SHADOW)
    {
        shadow_set_current(controlled_player, physics);
        shadow_draw(controlled_player);
        shadow_set_current(&player, NULL);
    }
    else
    {
        /* Common draw path for non-WRAM characters */
        if (controlled_player->spin)
        {
            jo_sprite_draw3D_and_rotate2(controlled_player->spin_sprite_id,
                                         controlled_player-> x,
                                         controlled_player->y,
                                         CHARACTER_SPRITE_Z,
                                         controlled_player->angle);
            if (controlled_player->flip)
                controlled_player->angle -= CHARACTER_SPIN_SPEED;
            else
                controlled_player->angle += CHARACTER_SPIN_SPEED;
        }
        else if (controlled_player->life <= 0 && controlled_player->defeated_sprite_id >= 0)
        {
            jo_sprite_draw3D2(controlled_player->defeated_sprite_id,
                              controlled_player->x,
                              controlled_player->y + (CHARACTER_HEIGHT - player_get_defeated_sprite_height(controlled_player)),
                              CHARACTER_SPRITE_Z);
        }
        else if (controlled_player->stun_timer > 0 && controlled_player->damage_sprite_id >= 0)
        {
            jo_sprite_draw3D2(controlled_player->damage_sprite_id,
                              controlled_player->x,
                              controlled_player->y,
                              CHARACTER_SPRITE_Z);
        }
        else if (controlled_player->punch || controlled_player->punch2)
        {
            anim_sprite_id = (controlled_player->punch_anim_id >= 0) ? jo_get_anim_sprite(controlled_player->punch_anim_id) : -1;
            if (anim_sprite_id >= 0)
                jo_sprite_draw3D2(anim_sprite_id, controlled_player->x, controlled_player->y, CHARACTER_SPRITE_Z);
        }
        else if (controlled_player->kick || controlled_player->kick2)
        {
            /* Tails has custom kick‑rotation logic stored in the character state. */
            if (controlled_player->character_id == CHARACTER_ID_TAILS)
            {
                int kick_sprite_id = game_flow_get_player2_kick_sprite_id();
                int kick_angle = 0;

                if (controlled_player->tails_kick_duration > 0)
                    kick_angle = (controlled_player->tails_kick_total_degrees * controlled_player->tails_kick_timer) / controlled_player->tails_kick_duration;
                if (controlled_player->flip)
                    kick_angle = -kick_angle;

                if (kick_sprite_id >= 0)
                    jo_sprite_draw3D_and_rotate2(kick_sprite_id, controlled_player->x, controlled_player->y, CHARACTER_SPRITE_Z, kick_angle);
            }
            else
            {
                anim_sprite_id = (controlled_player->kick_anim_id >= 0) ? jo_get_anim_sprite(controlled_player->kick_anim_id) : -1;
                if (anim_sprite_id >= 0)
                    jo_sprite_draw3D2(anim_sprite_id, controlled_player->x, controlled_player->y, CHARACTER_SPRITE_Z);
            }
        }
        else if (controlled_player->jump)
        {
            jo_sprite_draw3D2(controlled_player->jump_sprite_id, controlled_player->x, controlled_player->y, CHARACTER_SPRITE_Z);
        }
        else
        {
            if (controlled_player->walk && controlled_player->run == 0)
                anim_sprite_id = (controlled_player->walking_anim_id >= 0) ? jo_get_anim_sprite(controlled_player->walking_anim_id) : -1;
            else if (controlled_player->walk && controlled_player->run == 1)
                anim_sprite_id = (controlled_player->running1_anim_id >= 0) ? jo_get_anim_sprite(controlled_player->running1_anim_id) : -1;
            else if (controlled_player->walk && controlled_player->run == 2)
                anim_sprite_id = (controlled_player->running2_anim_id >= 0) ? jo_get_anim_sprite(controlled_player->running2_anim_id) : -1;
            else
                anim_sprite_id = (controlled_player->stand_sprite_id >= 0) ? jo_get_anim_sprite(controlled_player->stand_sprite_id) : -1;

            if (anim_sprite_id >= 0)
                jo_sprite_draw3D2(anim_sprite_id, controlled_player->x, controlled_player->y, CHARACTER_SPRITE_Z);
        }
    }

    if (apply_flip)
        jo_sprite_disable_horizontal_flip();
}

void player_runtime_update(character_t *controlled_player,
                            jo_sidescroller_physics_params *physics,
                            float *world_x,
                            float *world_y,
                            bool *initialized,
                            bool *defeated,
                            int *jump_cooldown,
                            int *jump_hold_ms,
                            bool *jump_cut_applied,
                            int *airborne_time_ms,
                            int player_index,
                            int *map_pos_x,
                            int *map_pos_y)
{
    if (controlled_player == JO_NULL || physics == JO_NULL || world_x == JO_NULL || world_y == JO_NULL
        || initialized == JO_NULL || defeated == JO_NULL || jump_cooldown == JO_NULL || jump_hold_ms == JO_NULL
        || jump_cut_applied == JO_NULL || airborne_time_ms == JO_NULL)
        return;

    if (!*initialized)
    {
        world_physics_init_character(physics);
        physics->speed = 0.0f;
        physics->speed_y = 0.0f;
        physics->is_in_air = false;

        *jump_cooldown = 0;
        *jump_hold_ms = 0;
        *jump_cut_applied = false;
        *airborne_time_ms = 0;

        *world_x = world_map_get_player_start_x(player_index, controlled_player->group);
        *world_y = world_map_get_player_start_y(player_index, controlled_player->group);

        controlled_player->x = (int)*world_x - *map_pos_x;
        controlled_player->y = (int)*world_y - *map_pos_y;
        controlled_player->flip = false;
        controlled_player->can_jump = true;
        controlled_player->landed = false;
        controlled_player->was_in_air = false;
        if (controlled_player->life <= 0)
            controlled_player->life = 50;

        player_reset_tail_state(controlled_player);

        *initialized = true;
    }

    bool was_air = controlled_player->was_in_air;
    bool now_air = physics->is_in_air;

    if (was_air && !now_air)
        controlled_player->landed = true;

    if (now_air)
        controlled_player->landed = false;

    controlled_player->was_in_air = now_air;

    controlled_player->x = (int)*world_x - *map_pos_x;
    controlled_player->y = (int)*world_y - *map_pos_y;

        int world_pos_x = *map_pos_x;
        world_handle_character_collision(physics, controlled_player, &world_pos_x, *map_pos_y);
        *map_pos_x = world_pos_x;

        *world_x = (float)(*map_pos_x + controlled_player->x);
        *world_y = (float)(*map_pos_y + controlled_player->y);

    controlled_player->speed = (int)JO_ABS(physics->speed);

    if (physics->is_in_air)
    {
        bool show_jump_sprite = false;

        *airborne_time_ms += GAME_FRAME_MS;
        if (physics->speed_y < 0.0f)
            show_jump_sprite = true;
        else if (physics->speed_y >= AIRBORNE_FALL_SPEED_THRESHOLD)
            show_jump_sprite = true;
        else if (*airborne_time_ms >= AIRBORNE_SPRITE_DELAY_MS)
            show_jump_sprite = true;

        controlled_player->jump = show_jump_sprite;
        controlled_player->falling = (show_jump_sprite && physics->speed_y > 0.0f);
    }
    else
    {
        *airborne_time_ms = 0;
        controlled_player->jump = false;
        controlled_player->falling = false;
    }

    player_update_animation_state(controlled_player, physics);
}

void player_update_kick_state(character_t *controlled_player,
							  int kick_combo_trigger_frame,
							  int kick_stage1_last_frame,
							  int kick_stage2_start_frame,
							  int kick_stage2_last_frame,
							  bool has_kick2_stage,
							  bool finish_stage1_on_last_frame,
							  bool finish_stage2_on_last_frame)
{
	int anim_frame;
	bool anim_stopped;
	int combo_trigger_frame;

	if (controlled_player == JO_NULL || controlled_player->kick_anim_id < 0)
		return;

	/* Ensure animation is reset if not in kick action, to avoid frame desync after hit */
	if (!controlled_player->kick && !controlled_player->kick2)
	{
		jo_set_sprite_anim_frame(controlled_player->kick_anim_id, 0);
		jo_reset_sprite_anim(controlled_player->kick_anim_id);
		return;
	}

	anim_frame = jo_get_sprite_anim_frame(controlled_player->kick_anim_id);
	anim_stopped = jo_is_sprite_anim_stopped(controlled_player->kick_anim_id);

	/* DEBUG: kick state timers */
	if (g_show_attack_debug)
	{
		if (controlled_player == &player)
		{
			jo_printf(1, 11, "P1 KICK f=%d t1=%d t2=%d", anim_frame,
				controlled_player->kick_stage1_ticks,
				controlled_player->kick_stage2_ticks);
			jo_printf(1, 12, "P1 C-ck hold=%d ready=%d ms=%d", controlled_player->charged_kick_button_held,
				controlled_player->charged_kick_ready,
				controlled_player->charged_kick_hold_ms);
		}
		else if (controlled_player == &player2)
		{
			jo_printf(1, 13, "P2 KICK f=%d t1=%d t2=%d", anim_frame,
				controlled_player->kick_stage1_ticks,
				controlled_player->kick_stage2_ticks);
			jo_printf(1, 14, "P2 C-ck hold=%d ready=%d ms=%d", controlled_player->charged_kick_button_held,
				controlled_player->charged_kick_ready,
				controlled_player->charged_kick_hold_ms);
		}
	}

	/* Kick flow owns the attack slot; clear any stale punch state to avoid
	   sprite bleed. */
	if (controlled_player->kick || controlled_player->kick2)
	{
		controlled_player->punch = false;
		controlled_player->punch2 = false;
		controlled_player->punch2_requested = false;
		controlled_player->perform_punch2 = false;
	}

	combo_trigger_frame = kick_combo_trigger_frame;
	if (combo_trigger_frame > kick_stage1_last_frame)
		combo_trigger_frame = kick_stage1_last_frame;

	/* countdown stage timer (used for both stages) */
	if (controlled_player->kick_stage1_ticks > 0)
		controlled_player->kick_stage1_ticks--;

	bool is_tails = (controlled_player->character_id == CHARACTER_ID_TAILS);

	if (is_tails && controlled_player->kick)
	{
		if (!controlled_player->tails_kick_rotation_active || controlled_player->tails_kick_total_degrees != 180)
		{
			controlled_player->tails_kick_timer = 0;
			controlled_player->tails_kick_duration = TAILS_KICK1_ROTATION_TIME;
			controlled_player->tails_kick_total_degrees = 180;
			controlled_player->tails_kick_rotation_active = true;
		}

		if (controlled_player->tails_kick_timer < controlled_player->tails_kick_duration)
		{
			++controlled_player->tails_kick_timer;
			return;
		}

		if (controlled_player->kick2_requested)
		{
			controlled_player->kick = false;
			controlled_player->kick2 = true;
			controlled_player->kick2_requested = false;
			controlled_player->perform_kick2 = true;

			controlled_player->tails_kick_timer = 0;
			controlled_player->tails_kick_duration = TAILS_KICK2_ROTATION_TIME;
			controlled_player->tails_kick_total_degrees = 360;
			controlled_player->tails_kick_rotation_active = true;
		}
		else
		{
			controlled_player->kick = false;
			controlled_player->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
			controlled_player->tails_kick_rotation_active = false;
		}
		return;
	}

	if (is_tails && controlled_player->kick2)
	{
		if (!controlled_player->tails_kick_rotation_active || controlled_player->tails_kick_total_degrees != 360)
		{
			controlled_player->tails_kick_timer = 0;
			controlled_player->tails_kick_duration = TAILS_KICK2_ROTATION_TIME;
			controlled_player->tails_kick_total_degrees = 360;
			controlled_player->tails_kick_rotation_active = true;
		}

		if (controlled_player->tails_kick_timer < controlled_player->tails_kick_duration)
		{
			++controlled_player->tails_kick_timer;
			return;
		}

		controlled_player->kick2 = false;
		controlled_player->attack_cooldown = ATTACK_COOLDOWN_KICK2_FRAMES;
		controlled_player->tails_kick_rotation_active = false;
		return;
	}

	if (is_tails && controlled_player->tails_kick_rotation_active && !controlled_player->kick && !controlled_player->kick2)
	{
		controlled_player->tails_kick_timer = 0;
		controlled_player->tails_kick_duration = 0;
		controlled_player->tails_kick_total_degrees = 0;
		controlled_player->tails_kick_rotation_active = false;
	}

	if (controlled_player->kick)
	{
		/* stage1-specific handling */

		/* While charging, lock the kick sprite to frame 0 until release. */
		if (controlled_player->charged_kick_enabled
			&& controlled_player->charged_kick_button_held
			&& !controlled_player->kick2
			&& !controlled_player->charged_kick_active)
		{
			if (controlled_player->kick_anim_id >= 0)
			{
				jo_set_sprite_anim_frame(controlled_player->kick_anim_id, 0);
				jo_reset_sprite_anim(controlled_player->kick_anim_id);
			}
			return;
		}

		/* keep anim running until stage1 finishes */
		if (anim_stopped && anim_frame < kick_stage1_last_frame)
		{
			jo_start_sprite_anim(controlled_player->kick_anim_id);
			anim_stopped = false;
		}

		/* while timer active, clamp to last frame and skip transitions */
		if (controlled_player->kick_stage1_ticks > 0)
		{
			if (anim_frame > kick_stage1_last_frame)
			{
				jo_set_sprite_anim_frame(controlled_player->kick_anim_id, kick_stage1_last_frame);
				jo_start_sprite_anim(controlled_player->kick_anim_id);
			}
			return;
		}

		/* timer expired: resume normal flow */
		if (anim_frame > kick_stage1_last_frame)
		{
			if (!controlled_player->kick2_requested)
			{
				jo_set_sprite_anim_frame(controlled_player->kick_anim_id, 0);
				jo_start_sprite_anim(controlled_player->kick_anim_id);
				anim_frame = jo_get_sprite_anim_frame(controlled_player->kick_anim_id);
				anim_stopped = jo_is_sprite_anim_stopped(controlled_player->kick_anim_id);
			}
		}

		if (anim_frame >= combo_trigger_frame)
		{
			if (controlled_player->kick2_requested && has_kick2_stage)
			{
				controlled_player->kick = false;
				controlled_player->kick2 = true;
				controlled_player->kick2_requested = false;
				controlled_player->perform_kick2 = true;

				controlled_player->kick_stage2_ticks = 0;
				jo_set_sprite_anim_frame(controlled_player->kick_anim_id, kick_stage2_start_frame);
				jo_start_sprite_anim(controlled_player->kick_anim_id);
			}
			else if (anim_frame >= kick_stage1_last_frame
				&& (anim_stopped || finish_stage1_on_last_frame))
			{
				controlled_player->kick = false;
				controlled_player->kick2_requested = false;
				controlled_player->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
				jo_reset_sprite_anim(controlled_player->kick_anim_id);
			}
		}
		return;
	}

	if (!controlled_player->kick2)
		return;

	/* manual control of stage2 frames: ensure frame2 then frame3 each for DEFAULT ticks */
	if (controlled_player->kick_stage2_ticks > 0
		&& !(controlled_player->character_id == CHARACTER_ID_SONIC || controlled_player->character_id == CHARACTER_ID_AMY || controlled_player->character_id == CHARACTER_ID_TAILS))
	{
		int half = DEFAULT_SPRITE_FRAME_DURATION;
		if (controlled_player->kick_stage2_ticks > half)
		{
			jo_set_sprite_anim_frame(controlled_player->kick_anim_id, kick_stage2_start_frame);
		}
		else
		{
			jo_set_sprite_anim_frame(controlled_player->kick_anim_id, kick_stage2_last_frame);
		}
		/* don't start_anim, just lock frame */
		controlled_player->kick_stage2_ticks--;
		return;
	}

	if (!has_kick2_stage)
	{
		controlled_player->kick2 = false;
		controlled_player->kick2_requested = false;
		controlled_player->attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
		jo_reset_sprite_anim(controlled_player->kick_anim_id);
		return;
	}

	if (is_tails)
	{
		/* Tails kick2 is handled via tails-specific tail rotation state in player_update_animation_state */
		return;
	}

	if (anim_frame < kick_stage2_start_frame)
	{
		jo_set_sprite_anim_frame(controlled_player->kick_anim_id, kick_stage2_start_frame);
		jo_start_sprite_anim(controlled_player->kick_anim_id);
		anim_frame = jo_get_sprite_anim_frame(controlled_player->kick_anim_id);
	}
	else if (jo_is_sprite_anim_stopped(controlled_player->kick_anim_id) && anim_frame < kick_stage2_last_frame)
	{
		/* Keep stage-2 running until it reaches its end frame. */
		jo_start_sprite_anim(controlled_player->kick_anim_id);
	}

	if (anim_frame >= kick_stage2_last_frame
		&& (jo_is_sprite_anim_stopped(controlled_player->kick_anim_id) || finish_stage2_on_last_frame))
	{
		controlled_player->kick2 = false;
		controlled_player->attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
		jo_reset_sprite_anim(controlled_player->kick_anim_id);
	}
}

void player_update_kick_state_for_character(character_t *controlled_player)
{
	if (controlled_player == JO_NULL)
		return;

	// Sonic/Amy use an 8-frame kick sheet (4 frames kick1 + 4 frames kick2)
	if (controlled_player->character_id == CHARACTER_ID_SONIC ||
		controlled_player->character_id == CHARACTER_ID_AMY)
	{
		/* stage1 frames 0-3, stage2 frames 4-7 (combo trigger at frame 3) */
		player_update_kick_state(controlled_player, 3, 3, 4, 7, true, true, false);
		return;
	}

	// Tails uses 5 frames for kick sequence (0-4), for both kick1 and kick2/air-kick
	if (controlled_player->character_id == CHARACTER_ID_TAILS)
	{
		/* stage1 frames 0-4, stage2 frames 0-4 (combo trigger at frame 4) */
		player_update_kick_state(controlled_player, 4, 4, 0, 4, true, true, false);
		return;
	}

	/* Unified 4-frame profile for remaining characters:
	   stage1 frames 0-1, stage2 frames 2-3 */
	player_update_kick_state(controlled_player, 1, 1, 2, 3, true, true, false);
}
