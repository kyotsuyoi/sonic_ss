#include <jo/jo.h>
#include "player.h"
#include "game_audio.h"
#include "game_constants.h"

#define DEFAULT_ATTACK_FORWARD_IMPULSE_LIGHT 0.60f
#define DEFAULT_ATTACK_FORWARD_IMPULSE_HEAVY 1.10f
#define CONTROL_FRAME_MS (17)
#define VARIABLE_JUMP_MAX_HOLD_MS (170)
#define VARIABLE_JUMP_MIN_SPEED_FACTOR (0.40f)
#define CHARGED_KICK_HOLD_MS (1000)
#define GROUND_NO_WALK_BRAKE_FACTOR (0.65f)
#define GROUND_NO_WALK_STOP_EPSILON (0.20f)

character_t player;
character_t player2;

/* Toggle for showing debug logs for punch/kick state machines. */
bool g_show_attack_debug = false;

character_t *player_get_instance(int index)
{
	if (index == 1)
		return &player2;

	return &player;
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
	if (!controlled_player->punch && !controlled_player->punch2 && !controlled_player->kick && !controlled_player->kick2 && !physics->is_in_air && controlled_player->attack_cooldown == 0 && a_down)
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

	if (!controlled_player->kick && !controlled_player->kick2 && !controlled_player->punch && !controlled_player->punch2 && !physics->is_in_air && controlled_player->attack_cooldown == 0 && c_down)
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
	if (physics->is_in_air)
	{
		if (spin_pressed)
			controlled_player->spin = true;

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

	/* read current animation state early so stage‑2 logic always has valid values */
	anim_frame = jo_get_sprite_anim_frame(controlled_player->punch_anim_id);
	anim_stopped = jo_is_sprite_anim_stopped(controlled_player->punch_anim_id);

	/* DEBUG: punch state timers */
	if (g_show_attack_debug)
	{
		if (controlled_player == &player)
		{
			jo_printf(1, 9, "P1 PNCH f=%d t1=%d t2=%d", anim_frame,
				controlled_player->punch_stage1_ticks,
				controlled_player->punch_stage2_ticks);
		}
		else if (controlled_player == &player2)
		{
			jo_printf(1, 10, "P2 PNCH f=%d t1=%d t2=%d", anim_frame,
				controlled_player->punch_stage1_ticks,
				controlled_player->punch_stage2_ticks);
		}
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
				/* set total ticks for stage2: two frames each lasting DEFAULT */
				controlled_player->punch_stage2_ticks = DEFAULT_SPRITE_FRAME_DURATION * 2;
				/* explicitly show first frame of stage2; do not start anim */
				jo_set_sprite_anim_frame(controlled_player->punch_anim_id, punch_stage2_start_frame);
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
	if (controlled_player->punch_stage2_ticks > 0)
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

	// Sonic uses an 8-frame punch sheet (4 frames punch1 + 4 frames punch2)
	if (controlled_player->character_id == CHARACTER_ID_SONIC)
	{
		/* stage1 frames 0-3, stage2 frames 4-7 (combo trigger at frame 3) */
		player_update_punch_state(controlled_player, 3, 3, 4, 7, true, true, false);
		return;
	}

	/* Unified 4-frame profile matching the working Knuckles combo flow. */
	/* 2 + 2 frames: stage1 frames 0-1, stage2 frames 2-3. Allow combo trigger at frame 1. */
	player_update_punch_state(controlled_player, 1, 1, 2, 3, true, true, false);
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

	/* read current animation state early so stage-2 logic always has valid values */
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
				/* set total ticks for stage2: two frames each lasting DEFAULT */
				controlled_player->kick_stage2_ticks = DEFAULT_SPRITE_FRAME_DURATION * 2;
				/* explicitly show first frame of stage2; do not start anim */
				jo_set_sprite_anim_frame(controlled_player->kick_anim_id, kick_stage2_start_frame);
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
	if (controlled_player->kick_stage2_ticks > 0)
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

	// Sonic uses an 8-frame kick sheet (4 frames kick1 + 4 frames kick2)
	if (controlled_player->character_id == CHARACTER_ID_SONIC)
	{
		/* stage1 frames 0-3, stage2 frames 4-7 (combo trigger at frame 3) */
		player_update_kick_state(controlled_player, 3, 3, 4, 7, true, true, false);
		return;
	}

	/* Unified 4-frame profile matching the punch combo flow:
	   stage1 frames 0-1, stage2 frames 2-3 */
	player_update_kick_state(controlled_player, 1, 1, 2, 3, true, true, false);
}