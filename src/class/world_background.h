#ifndef WORLD_BACKGROUND_H
#define WORLD_BACKGROUND_H

void world_background_load(void);
void world_background_set_gameplay(void);
void world_background_set_menu(void);
void world_background_invalidate(void);
void world_background_draw_parallax(int map_pos_x, int map_pos_y);

#endif