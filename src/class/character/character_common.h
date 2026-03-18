#ifndef CHARACTER_COMMON_H
#define CHARACTER_COMMON_H

#include <jo/jo.h>
#include "character.h"

/**
 * Shared logic for running/walking animation state.
 *
 * This implements the same behavior used by Sonic and Amy and keeps animation
 * timing consistent by driving a set of dummy sprite animations.
 */
void character_running_animation_handling(character_t *chr, jo_sidescroller_physics_params *physics);

/* Helpers shared by multiple characters (Sonic/Amy) */
int character_create_blank_sprite(void);
int character_create_blank_animation(int frame_count);
int character_ensure_wram_sprite(character_t *chr, int *global_sprite_id);
int character_ensure_defeated_wram_sprite(character_t *chr, int defeated_width, int defeated_height);
void character_copy_sheet_frame_to_sprite(int sprite_id, const jo_img *sheet, int frame_x, int frame_y);
void character_copy_defeated_sheet_frame_to_sprite(int sprite_id, const jo_img *sheet, int defeated_width, int defeated_height);

bool character_load_sheet(jo_img *sheet, const char *filename, const char *dir, jo_color transparent);
void character_unload_sheet(jo_img *sheet);

/**
 * Decide which sheet frame (row/col) corresponds to the current character state.
 *
 * Returns true if a frame was selected (and row/col are set), or false if the
 * character is defeated (the caller should handle drawing the defeated sprite).
 */
bool character_get_sheet_frame_coords(const character_t *chr, int *row, int *col);

/**
 * Draws the correct frame from the provided sheet based on character state.
 *
 * Returns true if a frame was drawn, false if the character is defeated.
 */
bool character_draw_sheet_frame(character_t *chr, int sprite_id, const jo_img *sheet);

/**
 * Draws the defeated sprite of `chr` using `sheet` and dimensions.
 *
 * Returns true if a defeated sprite was drawn (i.e., life <= 0).
 */
bool character_draw_defeated(character_t *chr, int sprite_id, const jo_img *sheet, int defeated_width, int defeated_height);

/**
 * Draw using the legacy per-frame animation path (no sheet required).
 *
 * This is used when the WRAM sprite sheet is not available (e.g. failed load),
 * but we still want to render based on the character's anim IDs.
 */
void character_draw_legacy_frame(const character_t *chr);

#endif // CHARACTER_COMMON_H
