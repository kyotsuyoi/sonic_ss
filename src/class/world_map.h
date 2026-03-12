#ifndef WORLD_MAP_H
#define WORLD_MAP_H

void world_map_load(void);
// Avança a carga do mapa em passos; chamar por frame até retornar true.
bool world_map_do_load_step(void);
void world_map_prepare_visible_tiles(int screen_x, int screen_y);
bool world_map_is_ready(void);

#endif