#ifndef __SHADOW_H__
# define __SHADOW_H__

#include "character.h"

typedef character_t shadow_t;

void shadow_running_animation_handling(void);
void display_shadow(void);
void load_shadow(void);
void unload_shadow(void);

#endif
