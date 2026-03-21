#ifndef __SHADOW_H__
# define __SHADOW_H__

#include "character.h"

typedef character_t shadow_t;

void shadow_running_animation_handling(void);
void display_shadow(void);
void shadow_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_shadow(void);
void unload_shadow(void);

#endif
