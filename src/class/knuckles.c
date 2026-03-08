#include "knuckles.h"
#include "sonic.h"

void knuckles_running_animation_handling(void)
{
    sonic_running_animation_handling();
}

void display_knuckles(void)
{
    display_sonic();
}

void load_knuckles(void)
{
    load_sonic();
}

void unload_knuckles(void)
{
    unload_sonic();
}
