#ifndef TAILS_H
#define TAILS_H

#include "character.h"

typedef character_t tails_t;

void load_tails(void);
void unload_tails(void);
void tails_running_animation_handling(void);
void display_tails(void);
int tails_get_tail_base_id(void);
int tails_get_kick_sprite_id(void);

#endif
