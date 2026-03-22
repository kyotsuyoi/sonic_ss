#ifndef __SONIC_H__
# define __SONIC_H__

#include "character.h"
#include "game_constants.h"

#define SONIC_WIDTH  CHARACTER_WIDTH
#define SONIC_HEIGHT CHARACTER_HEIGHT

typedef character_t sonic_t;

extern sonic_t player;

void sonic_running_animation_handling(void);
void sonic_draw(character_t *chr);
void display_sonic(void);
void sonic_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_sonic(void);
void unload_sonic(void);

#endif /* !__SONIC_H__ */
