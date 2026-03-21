#ifndef __TAILS_H__
# define __TAILS_H__

#include "character.h"
#include "game_constants.h"

# define TAILS_WIDTH                    CHARACTER_WIDTH
# define TAILS_WIDTH_2                  CHARACTER_WIDTH_2
# define TAILS_HEIGHT                   CHARACTER_HEIGHT
# define TAILS_SPIN_SPEED               CHARACTER_SPIN_SPEED

/* If Tails almost touches the ground we allow the user to jump */
# define TAILS_JUMP_PER_PIXEL_TOLERANCE CHARACTER_JUMP_PER_PIXEL_TOLERANCE

typedef character_t tails_t;

extern tails_t player;

void tails_running_animation_handling();
void display_tails(void);
void tails_draw(character_t *chr);
void tails_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_tails(void);
void unload_tails(void);

#endif /* !__TAILS_H__ */

/*
** END OF FILE
*/
