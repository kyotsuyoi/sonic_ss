/* Header for audio extension used by the game to call adjusted audio checks */
#ifndef JO_AUDIO_EXT_H
#define JO_AUDIO_EXT_H

#include <stdbool.h>
#include "jo/jo.h"

// Extended version of jo_audio_is_channel_playing used by the game when
// the modified behavior is required.
bool jo_audio_is_channel_playing_ext(const unsigned char channel);

#endif // JO_AUDIO_EXT_H
