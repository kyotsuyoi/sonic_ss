#include "shadow.h"
#include "sonic.h"

/*
Speed -> Animation mapping:
- `speed_step = (int)JO_ABS(physics.speed)` converts the physics speed to a discrete level.
- The code maps speed ranges to an animation variant and a sprite frame rate:
    * `speed_step >= 6`: use `running2` with `frame_rate = 3` (fast)
    * `speed_step >= 5`: use `running1` with `frame_rate = 4`
    * `speed_step >= 4`: use `running1` with `frame_rate = 5`
    * `speed_step >= 3`: use `running1` with `frame_rate = DEFAULT_SPRITE_FRAME_DURATION` (medium)
    * `speed_step >= 2`: use `running1` with `frame_rate = 7`
    * `speed_step >= 1`: use `walking` with `frame_rate = 8`
    * `else`: use `walking` with `frame_rate = 9`
- Lower `frame_rate` values advance animation frames faster (so `3` animates quicker than `9`).
- `run` selects which animation variant is drawn (0 = walking, 1 = running1, 2 = running2).
This mapping keeps the visual animation speed consistent with the character's physical speed.
*/
void shadow_running_animation_handling(void)
{
    sonic_running_animation_handling();
}

void display_shadow(void)
{
    display_sonic();
}

void load_shadow(void)
{
    load_sonic();
    player.character_id = CHARACTER_ID_SHADOW;
}

void unload_shadow(void)
{
    unload_sonic();
}
