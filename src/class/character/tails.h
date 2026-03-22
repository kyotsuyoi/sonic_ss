#ifndef __TAILS_H__
# define __TAILS_H__

#include "character.h"
#include "game_constants.h"

#define TAILS_WIDTH  CHARACTER_WIDTH
#define TAILS_HEIGHT CHARACTER_HEIGHT

#define TAILS_TAIL_FRAME_COUNT 8
#define TAILS_TAIL_FRAME_DURATION 5

typedef character_t tails_t;

extern tails_t player;

void tails_running_animation_handling(void);
void tails_draw(character_t *chr);
void display_tails(void);
void tails_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_tails(void);
void unload_tails(void);

#endif /* !__TAILS_H__ */
