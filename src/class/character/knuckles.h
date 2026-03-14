#ifndef __KNUCKLES_H__
# define __KNUCKLES_H__

#include "character.h"
#include "game_constants.h"

# define KNUCKLES_WIDTH                    CHARACTER_WIDTH
# define KNUCKLES_WIDTH_2                  CHARACTER_WIDTH_2
# define KNUCKLES_HEIGHT                   CHARACTER_HEIGHT
# define KNUCKLES_SPIN_SPEED               CHARACTER_SPIN_SPEED

/* If knuckles almost touch the ground we allow the user to jump */
# define KNUCKLES_JUMP_PER_PIXEL_TOLERANCE CHARACTER_JUMP_PER_PIXEL_TOLERANCE

typedef character_t knuckles_t;

void knuckles_running_animation_handling(void);
void display_knuckles(void);

/* Parameterized variants for Player 2 / multi-character use. */
void knuckles_update_animation_for(character_t *character, jo_sidescroller_physics_params *physics);
void knuckles_display_for(character_t *character, jo_sidescroller_physics_params *physics);

void load_knuckles(void);
void unload_knuckles(void);

#endif /* !__KNUCKLES_H__ */
