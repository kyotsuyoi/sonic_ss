#include "ram_cart.h"
#include "runtime_log.h"
#include "ui_control.h"

#include <string.h>
#include <stdint.h>

#define CART_RAM_BASE_ADDR ((volatile unsigned short *)0x22400000)
#define CART_RAM_1MB_LAST_WORD ((volatile unsigned short *)0x224FFFFE)
#define CART_RAM_4MB_LAST_WORD ((volatile unsigned short *)0x227FFFFE)

#define CART_RAM_DIR_SIZE     (16 * 1024)
#define CART_RAM_DIR_OFFSET   (0)
#define CART_RAM_DATA_OFFSET  (CART_RAM_DIR_SIZE)
#define CART_RAM_MAX_ENTRIES  64
#define CART_RAM_NAME_LENGTH  16

typedef struct
{
    char name[CART_RAM_NAME_LENGTH];
    uint32_t offset;
    uint32_t size;
    bool used;
} ram_cart_entry_t;

static ram_cart_status_t g_ram_cart_status = RamCartStatusNotDetected;
static uint32_t g_ram_cart_total_size = 0;
static uint32_t g_ram_cart_next_free = CART_RAM_DATA_OFFSET;

static volatile uint8_t *ram_cart_bytes(void)
{
    return (volatile uint8_t *)CART_RAM_BASE_ADDR;
}

static volatile uint16_t *ram_cart_words(void)
{
    return (volatile uint16_t *)CART_RAM_BASE_ADDR;
}

static bool ram_cart_probe_word(volatile unsigned short *addr, unsigned short pattern)
{
    unsigned short old_value;
    unsigned short read_back;

    if (addr == JO_NULL)
        return false;

    old_value = *addr;
    *addr = pattern;
    read_back = *addr;
    *addr = old_value;

    return read_back == pattern;
}

static bool ram_cart_probe_independent_words(volatile unsigned short *addr_a,
                                             volatile unsigned short *addr_b,
                                             unsigned short pattern_a,
                                             unsigned short pattern_b)
{
    unsigned short old_a;
    unsigned short old_b;
    unsigned short read_a;
    unsigned short read_b;

    if (addr_a == JO_NULL || addr_b == JO_NULL)
        return false;

    old_a = *addr_a;
    old_b = *addr_b;

    *addr_a = pattern_a;
    *addr_b = pattern_b;

    read_a = *addr_a;
    read_b = *addr_b;

    *addr_a = old_a;
    *addr_b = old_b;

    return read_a == pattern_a && read_b == pattern_b;
}

static bool ram_cart_copy_to_cart(uint32_t offset, const void *src, size_t size)
{
    if (src == JO_NULL || size == 0)
        return false;

    if ((offset & 1) != 0 || (size & 1) != 0)
        return false;

    volatile uint16_t *dst = ram_cart_words() + (offset / 2);
    const uint16_t *src16 = (const uint16_t *)src;
    size_t words = size / 2;

    for (size_t i = 0; i < words; ++i)
        dst[i] = src16[i];

    return true;
}

static bool ram_cart_copy_from_cart(uint32_t offset, void *dst, size_t size)
{
    if (dst == JO_NULL || size == 0)
        return false;

    if ((offset & 1) != 0 || (size & 1) != 0)
        return false;

    volatile uint16_t *src = ram_cart_words() + (offset / 2);
    uint16_t *dst16 = (uint16_t *)dst;
    size_t words = size / 2;

    for (size_t i = 0; i < words; ++i)
        dst16[i] = src[i];

    return true;
}

static ram_cart_entry_t *ram_cart_entry_table(void)
{
    return (ram_cart_entry_t *)(ram_cart_bytes() + CART_RAM_DIR_OFFSET);
}

static ram_cart_entry_t *ram_cart_find_entry(const char *name)
{
    ram_cart_entry_t *table = ram_cart_entry_table();

    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
    {
        if (!table[i].used)
            continue;

        if (strncmp(table[i].name, name, CART_RAM_NAME_LENGTH) == 0)
            return &table[i];
    }

    return JO_NULL;
}

static ram_cart_entry_t *ram_cart_alloc_entry(const char *name)
{
    ram_cart_entry_t *table = ram_cart_entry_table();

    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
    {
        if (!table[i].used)
        {
            table[i].used = true;
            memset(table[i].name, 0, CART_RAM_NAME_LENGTH);
            strncpy(table[i].name, name, CART_RAM_NAME_LENGTH - 1);
            table[i].offset = 0;
            table[i].size = 0;
            return &table[i];
        }
    }

    return JO_NULL;
}

ram_cart_status_t ram_cart_detect(void)
{
    if (!ram_cart_probe_word(CART_RAM_BASE_ADDR, 0x55AA))
    {
        g_ram_cart_status = RamCartStatusNotDetected;
        return g_ram_cart_status;
    }

    if (ram_cart_probe_independent_words(CART_RAM_BASE_ADDR,
                                         CART_RAM_4MB_LAST_WORD,
                                         0x55AA,
                                         0xAA55))
    {
        g_ram_cart_status = RamCartStatus4MB;
        g_ram_cart_total_size = 4 * 1024 * 1024;
    }
    else if (ram_cart_probe_word(CART_RAM_1MB_LAST_WORD, 0x5AA5))
    {
        g_ram_cart_status = RamCartStatus1MB;
        g_ram_cart_total_size = 1 * 1024 * 1024;
    }
    else
    {
        g_ram_cart_status = RamCartStatusNotDetected;
        g_ram_cart_total_size = 0;
    }

    /* no behavior change on detect called multiple times */
    return g_ram_cart_status;
}

ram_cart_status_t ram_cart_get_status(void)
{
    return g_ram_cart_status;
}

bool ram_cart_store_sprite(const char *name, const void *data, size_t size)
{
    if (name == JO_NULL || data == JO_NULL || size == 0)
        return false;

    if (g_ram_cart_status == RamCartStatusNotDetected && ram_cart_detect() == RamCartStatusNotDetected)
    {
        runtime_log("ram_cart_store: not detected");
        return false;
    }

    if (size + g_ram_cart_next_free > g_ram_cart_total_size)
    {
        cart_ram_log("ram_cart_store: %s fails, no space (%u/%u)", name, (unsigned)g_ram_cart_next_free, (unsigned)g_ram_cart_total_size);
        return false;
    }

    ram_cart_entry_t *existing = ram_cart_find_entry(name);
    if (existing != JO_NULL)
    {
        if (existing->size >= size)
        {
            existing->size = (uint32_t)size;
            return ram_cart_copy_to_cart(existing->offset, data, size);
        }

        // overwrite: allocate new slot
        existing->used = false;
    }

    ram_cart_entry_t *entry = ram_cart_alloc_entry(name);
    if (entry == JO_NULL)
        return false;

    entry->offset = g_ram_cart_next_free;
    entry->size = (uint32_t)size;

    if (!ram_cart_copy_to_cart(entry->offset, data, size))
    {
        entry->used = false;
        cart_ram_log("ram_cart_store: %s at %u failed copy", name, (unsigned)entry->offset);
        return false;
    }

    g_ram_cart_next_free += (uint32_t)size;
    cart_ram_log("ram_cart_store: %s at %u size %u", name, (unsigned)entry->offset, (unsigned)size);
    return true;
}

bool ram_cart_load_sprite(const char *name, void *out_buffer, size_t size)
{
    if (name == JO_NULL || out_buffer == JO_NULL || size == 0)
        return false;

    if (g_ram_cart_status == RamCartStatusNotDetected && ram_cart_detect() == RamCartStatusNotDetected)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
    {
        cart_ram_log("ram_cart_load: %s missing", name);
        return false;
    }

    if (size < entry->size)
    {
        cart_ram_log("ram_cart_load: %s buffer too small (%u<%u)", name, (unsigned)size, (unsigned)entry->size);
        return false;
    }

    cart_ram_log("ram_cart_load: %s at %u size %u", name, (unsigned)entry->offset, (unsigned)entry->size);

    return ram_cart_copy_from_cart(entry->offset, out_buffer, entry->size);
}

bool ram_cart_draw_frame(const char *name, int sheet_width, int sheet_height, int frame_x, int frame_y, int width, int height, int sprite_id)
{
    if (name == JO_NULL || width <= 0 || height <= 0 || sprite_id < 0)
        return false;

    cart_ram_log("ram_cart_draw_frame: %s sheet %dx%d frame %dx%d+%d,%d size %dx%d sprite %d", name, sheet_width, sheet_height, frame_x, frame_y, width, height, sprite_id);

    if (g_ram_cart_status == RamCartStatusNotDetected && ram_cart_detect() == RamCartStatusNotDetected)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
    {
        cart_ram_log("ram_cart_draw_frame: %s missing", name);
        return false;
    }

    if (frame_x < 0 || frame_y < 0 || frame_x + width > sheet_width || frame_y + height > sheet_height)
    {
        cart_ram_log("ram_cart_draw_frame: %s frame out of bounds", name);
        return false;
    }

    size_t frame_size = (size_t)width * (size_t)height * sizeof(unsigned short);
    jo_img tmp = {0};
    tmp.width = width;
    tmp.height = height;
    tmp.data = jo_malloc(frame_size);
    if (tmp.data == JO_NULL)
        return false;

    unsigned short *dst = (unsigned short *)tmp.data;

    for (int y = 0; y < height; ++y)
    {
        uint32_t row_offset = (uint32_t)(frame_y + y) * (uint32_t)sheet_width;
        uint32_t src_offset = entry->offset + (row_offset + (uint32_t)frame_x) * sizeof(unsigned short);
        if (!ram_cart_copy_from_cart(src_offset, dst + (size_t)y * (size_t)width, (size_t)width * sizeof(unsigned short)))
        {
            cart_ram_log("ram_cart_draw_frame: %s copy failed at row %d offset %u", name, y, (unsigned)src_offset);
            jo_free_img(&tmp);
            return false;
        }
    }

    jo_sprite_replace(&tmp, sprite_id);
    jo_free_img(&tmp);
    return true;
}

bool ram_cart_delete_sprite(const char *name)
{
    if (name == JO_NULL)
        return false;

    ram_cart_entry_t *entry = ram_cart_find_entry(name);
    if (entry == JO_NULL)
        return false;

    entry->used = false;
    return true;
}

void ram_cart_clear(void)
{
    ram_cart_entry_t *table = ram_cart_entry_table();

    for (size_t i = 0; i < CART_RAM_MAX_ENTRIES; ++i)
        table[i].used = false;

    g_ram_cart_next_free = CART_RAM_DATA_OFFSET;
}
