#ifndef __KNUCKLES_H__
# define __KNUCKLES_H__

#include "character.h"
#include "game_constants.h"

#define KNUCKLES_WIDTH  CHARACTER_WIDTH
#define KNUCKLES_HEIGHT CHARACTER_HEIGHT

typedef character_t knuckles_t;

extern knuckles_t player;

void knuckles_running_animation_handling(void);
void knuckles_draw(character_t *chr);
void display_knuckles(void);
void knuckles_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_knuckles(void);
void unload_knuckles(void);

#endif /* !__KNUCKLES_H__ */
