#ifndef WORLD_MAP_H
#define WORLD_MAP_H

void world_map_load(void);
// Avança a carga do mapa em passos; chamar por frame até retornar true.
bool world_map_do_load_step(void);
void world_map_prepare_visible_tiles(int screen_x, int screen_y);
bool world_map_is_ready(void);

int world_map_get_player_start_x(int player_index, int group);
int world_map_get_player_start_y(int player_index, int group);

int world_map_get_max_tile_bottom(void);
int world_map_get_max_tile_top(void);

#endif