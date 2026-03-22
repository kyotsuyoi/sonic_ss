#ifndef RAM_CART_H
#define RAM_CART_H

#include <jo/jo.h>
#include <stddef.h>

typedef enum
{
    RamCartStatusNotDetected = 0,
    RamCartStatus1MB,
    RamCartStatus4MB
} ram_cart_status_t;

ram_cart_status_t ram_cart_detect(void);
ram_cart_status_t ram_cart_get_status(void);

bool ram_cart_store_sprite(const char *name, const void *data, size_t size);
bool ram_cart_load_sprite(const char *name, void *out_buffer, size_t size);
bool ram_cart_delete_sprite(const char *name);
void ram_cart_clear(void);

bool ram_cart_draw_frame(const char *name, int sheet_width, int sheet_height, int frame_x, int frame_y, int width, int height, int sprite_id);

#endif
