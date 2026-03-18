#ifndef __AMY_H__
# define __AMY_H__

#include "character.h"
#include "game_constants.h"

# define AMY_WIDTH                    CHARACTER_WIDTH
# define AMY_WIDTH_2                  CHARACTER_WIDTH_2
# define AMY_HEIGHT                   CHARACTER_HEIGHT
# define AMY_SPIN_SPEED               CHARACTER_SPIN_SPEED

/* If Amy almost touches the ground we allow the user to jump */
# define AMY_JUMP_PER_PIXEL_TOLERANCE CHARACTER_JUMP_PER_PIXEL_TOLERANCE

typedef character_t amy_t;

extern amy_t player;

void amy_running_animation_handling();
void display_amy(void);
void amy_draw(character_t *chr);
void amy_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_amy(void);
void unload_amy(void);

#endif /* !__AMY_H__ */

/*
** END OF FILE
*/
