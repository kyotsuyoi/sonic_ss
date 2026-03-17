#ifndef AMY_H
#define AMY_H

#include "character.h"

typedef character_t amy_t;

void load_amy(void);
void unload_amy(void);

void amy_draw(character_t *chr);
void amy_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void amy_running_animation_handling(void);
void display_amy(void);

#endif
