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
}

void unload_shadow(void)
{
    unload_sonic();
}
