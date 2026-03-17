#ifndef __SONIC_H__
# define __SONIC_H__

#include "character.h"
#include "game_constants.h"

# define SONIC_WIDTH                    CHARACTER_WIDTH
# define SONIC_WIDTH_2                  CHARACTER_WIDTH_2
# define SONIC_HEIGHT                   CHARACTER_HEIGHT
# define SONIC_SPIN_SPEED               CHARACTER_SPIN_SPEED

/* If sonic almost touch the ground we allow the user to jump */
# define SONIC_JUMP_PER_PIXEL_TOLERANCE CHARACTER_JUMP_PER_PIXEL_TOLERANCE

typedef character_t sonic_t;

extern sonic_t player;

void sonic_running_animation_handling();
void display_sonic(void);
void sonic_draw(character_t *chr);
void sonic_set_current(character_t *chr, jo_sidescroller_physics_params *phy);
void load_sonic(void);
void unload_sonic(void);

#endif /* !__SONIC_H__ */

/*
** END OF FILE
*/
