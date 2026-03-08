#include <jo/jo.h>
#include "world_physics.h"

void world_physics_init_character(jo_sidescroller_physics_params *physics)
{
    jo_physics_init_for_sonic(physics);
}
