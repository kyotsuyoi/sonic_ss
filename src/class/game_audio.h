#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <jo/jo.h>

void game_audio_setup(void);
jo_sound *game_audio_get_jump_sfx(void);
jo_sound *game_audio_get_punch_sfx(void);
jo_sound *game_audio_get_kick_sfx(void);
jo_sound *game_audio_get_spin_sfx(void);
jo_sound *game_audio_get_damage_low_sfx(void);
jo_sound *game_audio_get_damage_hi_sfx(void);
int game_audio_play_sfx_next_channel(jo_sound *sound);
int game_audio_total_pcm(void);

#endif