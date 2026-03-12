#ifndef ROTATING_SPRITE_POOL_H
#define ROTATING_SPRITE_POOL_H

#include <jo/jo.h>
#include <stdint.h>

#define ROTATING_SPRITE_POOL_MAX_POOLS 8
#define ROTATING_SPRITE_POOL_MAX_ITEMS 64

typedef struct
{
	bool used;
	unsigned short width;
	unsigned short height;
	int used_slots;
	int total_slots;
	uint64_t reserved_bytes;
} rotating_sprite_pool_usage_t;

typedef struct
{
	int pool_count;
	int used_slots;
	int total_slots;
	uint64_t reserved_bytes;
	rotating_sprite_pool_usage_t pools[ROTATING_SPRITE_POOL_MAX_POOLS];
} rotating_sprite_pool_stats_t;

// Create a pool of fixed-size VRAM slots that can be reused with jo_sprite_replace().
int rotating_sprite_pool_create(unsigned short width, unsigned short height, int slot_count);

// Register a TGA file that will be loaded to WRAM on demand and mapped to a pool slot.
int rotating_sprite_pool_register_tga(int pool_id, const char *sub_dir, const char *filename, jo_color transparent_color);

// Start asynchronous WRAM loading for an item without binding it to a VRAM slot yet.
bool rotating_sprite_pool_prefetch(int item_id);

// Request a sprite id for drawing. Returns a stable sprite id when the item is ready.
// Returns -1 while the file is still loading or if the item cannot be loaded.
int rotating_sprite_pool_request(int item_id);

// Return the WRAM-side decoded image data for an item, or JO_NULL if unavailable.
void *rotating_sprite_pool_get_wram_ptr(int item_id);

// Return a stable placeholder sprite id from the item's pool.
int rotating_sprite_pool_get_fallback_sprite_id(int item_id);

bool rotating_sprite_pool_is_ready(int item_id);
bool rotating_sprite_pool_is_loading(int item_id);

// Release WRAM-side decoded images and reset slot ownership.
void rotating_sprite_pool_reset(void);
void rotating_sprite_pool_release_freed_sprites(int first_sprite_id);

// Dump runtime statistics (hits/misses/uploads/waste)
void rotating_sprite_pool_dump_stats(void);
void rotating_sprite_pool_get_stats(rotating_sprite_pool_stats_t *out_stats);
#endif