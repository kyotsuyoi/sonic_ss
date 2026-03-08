#ifndef PLAYER_H
#define PLAYER_H

#include "character.h"

#define PLAYER_MAX_DEFAULT_COUNT (2)

extern character_t player;
extern character_t player2;

character_t *player_get_instance(int index);
int player_get_max_instances(void);

#endif
