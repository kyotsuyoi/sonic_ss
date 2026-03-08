#include "player.h"

character_t player;
character_t player2;

character_t *player_get_instance(int index)
{
	if (index == 1)
		return &player2;

	return &player;
}

int player_get_max_instances(void)
{
	return PLAYER_MAX_DEFAULT_COUNT;
}