#include "jo_audio_ext.h"
#include "jo/sgl_prototypes.h"
#include "jo/conf.h"
#include "jo/types.h"

bool jo_audio_is_channel_playing_ext(const unsigned char channel)
{
#ifdef JO_DEBUG
    if (channel >= JO_SOUND_MAX_CHANNEL)
    {
        jo_core_error("channel (%d) is too high (max=%d)", channel, JO_SOUND_MAX_CHANNEL - 1);
        return false;
    }
#endif

    if (channel >= JO_SOUND_MAX_CHANNEL)
        return false;

    /* Reuse slPCMStat behavior; interpret result similarly to engine original. */
    const unsigned char status = slPCMStat(channel);
    return (status & 0x01u) != 0;
}
