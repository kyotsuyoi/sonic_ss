#ifndef __AMY_H__
# define __AMY_H__

#include "character.h"
#include "game_constants.h"

#define AMY_WIDTH  CHARACTER_WIDTH
#define AMY_HEIGHT CHARACTER_HEIGHT

typedef character_t amy_t;

extern amy_t player;

void amy_running_animation_handling(void);
void amy_draw(character_t *chr);
void display_amy(void);
void amy_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_amy(void);
void unload_amy(void);

#endif /* !__AMY_H__ */
