#include <jo/jo.h>
#include "game_audio.h"
#include "jo_audio_ext/jo_audio_ext.h"

#define GAME_SFX_SAMPLE_RATE_DEFAULT 36000
#define GAME_SFX_SAMPLE_RATE_SPIN 36000
#define GAME_SFX_VOLUME 100

static jo_sound jump_sfx;
static char *jump_pcm;
static int jump_len;

static jo_sound punch_sfx;
static char *punch_pcm;
static int punch_len;

static jo_sound kick_sfx;
static char *kick_pcm;
static int kick_len;

static jo_sound spin_sfx;
static char *spin_pcm;
static int spin_len;

static jo_sound damage_low_sfx;
static char *damage_low_pcm;
static int damage_low_len;

static jo_sound damage_hi_sfx;
static char *damage_hi_pcm;
static int damage_hi_len;

#define GAME_SFX_RETRIGGER_GUARD_TICKS 0

enum
{
    GameSfxJump = 0,
    GameSfxPunch,
    GameSfxKick,
    GameSfxSpin,
    GameSfxDamageLow,
    GameSfxDamageHigh,
    GameSfxCount
};

static unsigned int last_sfx_tick[GameSfxCount] = {0};

static int get_sfx_index(const jo_sound *sound)
{
    if (sound == &jump_sfx) return GameSfxJump;
    if (sound == &punch_sfx) return GameSfxPunch;
    if (sound == &kick_sfx) return GameSfxKick;
    if (sound == &spin_sfx) return GameSfxSpin;
    if (sound == &damage_low_sfx) return GameSfxDamageLow;
    if (sound == &damage_hi_sfx) return GameSfxDamageHigh;
    return -1;
}

static bool can_play_sfx_now(const jo_sound *sound)
{
    int index = get_sfx_index(sound);
    unsigned int now;

    if (index < 0)
        return true;

    now = jo_get_ticks();
    if (now - last_sfx_tick[index] <= GAME_SFX_RETRIGGER_GUARD_TICKS)
        return false;

    return true;
}

static void mark_sfx_played_now(const jo_sound *sound)
{
    int index = get_sfx_index(sound);

    if (index >= 0)
        last_sfx_tick[index] = jo_get_ticks();
}

void jo_audio_init(void);

static void load_sfx(const char *filename,
                     const char *label,
                     jo_sound *sound,
                     char **pcm_data,
                     int *pcm_len,
                     unsigned short sample_rate)
{
    *pcm_data = jo_fs_read_file_in_dir(filename, "SFX", pcm_len);
    if (*pcm_data == JO_NULL)
    {
        jo_printf(0, 12, "failed to load %s sfx", label);
        sound->volume = JO_MIN_AUDIO_VOLUME;
        return;
    }

    sound->mode = JoSoundMono8Bit;
    sound->data_length = *pcm_len;
    sound->data = *pcm_data;
    sound->sample_rate = sample_rate;
    sound->pan = 0;
    sound->volume = GAME_SFX_VOLUME;
}

void game_audio_setup(void)
{
    jo_audio_init();
    jo_audio_set_volume(JO_MAX_AUDIO_VOLUME);

    load_sfx("JMP.PCM", "jump", &jump_sfx, &jump_pcm, &jump_len, GAME_SFX_SAMPLE_RATE_DEFAULT);
    load_sfx("PNC.PCM", "punch", &punch_sfx, &punch_pcm, &punch_len, GAME_SFX_SAMPLE_RATE_DEFAULT);
    load_sfx("KCK.PCM", "kick", &kick_sfx, &kick_pcm, &kick_len, GAME_SFX_SAMPLE_RATE_DEFAULT);
    load_sfx("SPN.PCM", "spin", &spin_sfx, &spin_pcm, &spin_len, GAME_SFX_SAMPLE_RATE_SPIN);
    load_sfx("DML.PCM", "damage low", &damage_low_sfx, &damage_low_pcm, &damage_low_len, GAME_SFX_SAMPLE_RATE_DEFAULT);
    load_sfx("DMH.PCM", "damage hi", &damage_hi_sfx, &damage_hi_pcm, &damage_hi_len, GAME_SFX_SAMPLE_RATE_DEFAULT);
}

jo_sound *game_audio_get_jump_sfx(void)
{
    return &jump_sfx;
}

jo_sound *game_audio_get_punch_sfx(void)
{
    return &punch_sfx;
}

jo_sound *game_audio_get_kick_sfx(void)
{
    return &kick_sfx;
}

jo_sound *game_audio_get_spin_sfx(void)
{
    return &spin_sfx;
}

jo_sound *game_audio_get_damage_low_sfx(void)
{
    return &damage_low_sfx;
}

jo_sound *game_audio_get_damage_hi_sfx(void)
{
    return &damage_hi_sfx;
}

int game_audio_play_sfx_next_channel(jo_sound *sound)
{
    int channel;

    if (sound == JO_NULL)
        return -1;

    if (!can_play_sfx_now(sound))
        return -1;

    jo_audio_play_sound(sound);
    channel = sound->current_playing_channel;
    if (channel >= 0 && channel < JO_SOUND_MAX_CHANNEL && jo_audio_is_channel_playing_ext((unsigned char)channel))
    {
        mark_sfx_played_now(sound);
        return channel;
    }

    return -1;
}

int game_audio_total_pcm(void)
{
    return jump_len + punch_len + kick_len + spin_len + damage_low_len + damage_hi_len;
}