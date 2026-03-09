#include "shadow.h"
#include "sonic.h"

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
