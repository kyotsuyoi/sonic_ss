#ifndef BOT_H
#define BOT_H

#include "character.h"
#include "debug.h"

typedef struct bot_instance bot_instance_t;

#define BOT_MAX_DEFAULT_COUNT (6)

bot_instance_t *bot_default_instance(void);
void bot_instance_init(bot_instance_t *instance,
                       int selected_player_character,
                       int selected_bot_character,
                       int selected_bot_group,
                       int player_world_x,
                       int player_world_y);
void bot_instance_unload(bot_instance_t *instance);
void bot_instance_update(bot_instance_t *instance,
                         character_t *player,
                         bool *player_defeated,
                         jo_sidescroller_physics_params *player_physics,
                         character_t *player2,
                         bool *player2_defeated,
                         jo_sidescroller_physics_params *player2_physics,
                         bool versus_mode,
                         int map_pos_x,
                         int map_pos_y);
void bot_instance_draw(bot_instance_t *instance, int map_pos_x, int map_pos_y);
bool bot_instance_is_defeated(bot_instance_t *instance);

void bot_set_active_count(int count);
int bot_get_active_count(void);

void bot_init(int selected_player_character, int selected_bot_character, int player_world_x, int player_world_y);
void bot_init_multi(int selected_player_character, const int selected_bot_characters[], const int selected_bot_groups[], int bot_count, int selected_player_group, int player_world_x, int player_world_y);
void bot_init_versus(int selected_player_character,
                     int selected_player2_character,
                     int clones_per_side,
                     int player_world_x,
                     int player_world_y);
void bot_unload(void);
void bot_invalidate_asset_cache(void);
void bot_update(character_t *player,
                bool *player_defeated,
                jo_sidescroller_physics_params *player_physics,
                character_t *player2,
                bool *player2_defeated,
                jo_sidescroller_physics_params *player2_physics,
                bool versus_mode,
                int map_pos_x,
                int map_pos_y);
void bot_draw(int map_pos_x, int map_pos_y);
bool bot_is_defeated(void);
bool bot_debug_get_hitbox_snapshot(debug_hitbox_snapshot_t *snapshot);

int bot_get_life(int index);
int bot_get_character_id(int index);
int bot_get_group(int index);

#endif
