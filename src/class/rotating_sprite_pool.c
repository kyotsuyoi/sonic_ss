#include "rotating_sprite_pool.h"
#include <string.h>
#include <stdint.h>
#include "runtime_log.h"

#define ROTATING_SPRITE_POOL_MAX_SLOTS 64
#define ROTATING_SPRITE_NAME_MAX 32
#define ROTATING_SPRITE_DIR_MAX 16

typedef struct
{
    bool used;
    unsigned short width;
    unsigned short height;
    int slot_count;
    int sprite_ids[ROTATING_SPRITE_POOL_MAX_SLOTS];
    int item_ids[ROTATING_SPRITE_POOL_MAX_SLOTS];
    unsigned int last_used[ROTATING_SPRITE_POOL_MAX_SLOTS];
} rotating_sprite_pool_t;

typedef struct
{
    bool used;
    bool pending;
    bool loading;
    bool ready;
    bool failed;
    int pool_id;
    int slot_index;
    jo_color transparent_color;
    char sub_dir[ROTATING_SPRITE_DIR_MAX];
    char filename[ROTATING_SPRITE_NAME_MAX];
    jo_img image;
} rotating_sprite_item_t;

static rotating_sprite_pool_t g_rotating_pools[ROTATING_SPRITE_POOL_MAX_POOLS];
static rotating_sprite_item_t g_rotating_items[ROTATING_SPRITE_POOL_MAX_ITEMS];
static unsigned int g_rotating_tick = 0;
static int g_rotating_active_item_id = -1;

/* Statistics */
static uint32_t rsp_total_requests = 0;
static uint32_t rsp_hits = 0;
static uint32_t rsp_misses = 0;
static uint32_t rsp_replacements = 0;
static uint64_t rsp_total_upload_bytes = 0ULL;
static uint64_t rsp_total_wasted_bytes = 0ULL;

static bool rotating_sprite_pool_decode_item(rotating_sprite_item_t *item, char *contents)
{
    rotating_sprite_pool_t *pool;

    item->image.data = JO_NULL;
    if (contents == JO_NULL)
    {
        item->failed = true;
        return false;
    }

    if (jo_tga_loader_from_stream(&item->image, contents, item->transparent_color) != JO_TGA_OK)
    {
        jo_free(contents);
        item->failed = true;
        return false;
    }
    jo_free(contents);

    pool = &g_rotating_pools[item->pool_id];
    if (item->image.width != pool->width || item->image.height != pool->height)
    {
        jo_free_img(&item->image);
        item->image.data = JO_NULL;
        item->failed = true;
        return false;
    }

    item->ready = true;
    item->failed = false;
    item->loading = false;
    item->pending = false;
    return true;
}

static int rotating_sprite_pool_create_blank_sprite(unsigned short width, unsigned short height)
{
    jo_img blank;
    unsigned int pixel_count;
    unsigned int i;
    int sprite_id;

    blank.width = width;
    blank.height = height;
    pixel_count = (unsigned int)width * (unsigned int)height;
    blank.data = (unsigned short *)jo_malloc_with_behaviour(pixel_count * sizeof(*blank.data), JO_MALLOC_TRY_REUSE_BLOCK);
    if (blank.data == JO_NULL)
        return -1;

    for (i = 0; i < pixel_count; ++i)
        blank.data[i] = JO_COLOR_Transparent;

    sprite_id = jo_sprite_add(&blank);
    jo_free_img(&blank);
    return sprite_id;
}

static void rotating_sprite_pool_on_file_loaded(char *contents, int length, int optional_token)
{
    rotating_sprite_item_t *item;

    JO_UNUSED_ARG(length);
    if (optional_token < 0 || optional_token >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return;

    if (g_rotating_active_item_id == optional_token)
        g_rotating_active_item_id = -1;

    item = &g_rotating_items[optional_token];
    if (!item->used)
    {
        if (contents != JO_NULL)
            jo_free(contents);
        return;
    }

    item->loading = false;
    item->pending = false;
    rotating_sprite_pool_decode_item(item, contents);
}

static void rotating_sprite_pool_service_async_loads(void)
{
    JO_UNUSED_ARG(g_rotating_active_item_id);
}

static int rotating_sprite_pool_find_free_item(void)
{
    int i;

    for (i = 0; i < ROTATING_SPRITE_POOL_MAX_ITEMS; ++i)
        if (!g_rotating_items[i].used)
            return i;
    return -1;
}

static int rotating_sprite_pool_find_free_pool(void)
{
    int i;

    for (i = 0; i < ROTATING_SPRITE_POOL_MAX_POOLS; ++i)
        if (!g_rotating_pools[i].used)
            return i;
    return -1;
}

static int rotating_sprite_pool_select_slot(rotating_sprite_pool_t *pool)
{
    int i;
    int lru_index;

    for (i = 0; i < pool->slot_count; ++i)
        if (pool->item_ids[i] < 0)
            return i;

    lru_index = 0;
    for (i = 1; i < pool->slot_count; ++i)
        if (pool->last_used[i] < pool->last_used[lru_index])
            lru_index = i;
    return lru_index;
}

void rotating_sprite_pool_dump_stats(void)
{
    runtime_log("rotating_sprite_pool: total_requests=%u hits=%u misses=%u replacements=%u uploads=%llu wasted=%llu",
                rsp_total_requests, rsp_hits, rsp_misses, rsp_replacements,
                (unsigned long long)rsp_total_upload_bytes, (unsigned long long)rsp_total_wasted_bytes);
    for (int p = 0; p < ROTATING_SPRITE_POOL_MAX_POOLS; ++p) {
        rotating_sprite_pool_t *pool = &g_rotating_pools[p];
        if (!pool->used) continue;
        runtime_log("rotating_sprite_pool: pool %d %ux%u slots=%d", p, pool->width, pool->height, pool->slot_count);
    }
}

void rotating_sprite_pool_get_stats(rotating_sprite_pool_stats_t *out_stats)
{
    int p;
    int out_index = 0;

    if (out_stats == JO_NULL)
        return;

    jo_memset(out_stats, 0, sizeof(*out_stats));
    for (p = 0; p < ROTATING_SPRITE_POOL_MAX_POOLS; ++p)
    {
        int slot_index;
        int used_slots = 0;
        uint64_t reserved_bytes;
        rotating_sprite_pool_t *pool = &g_rotating_pools[p];

        if (!pool->used)
            continue;

        for (slot_index = 0; slot_index < pool->slot_count; ++slot_index)
            if (pool->item_ids[slot_index] >= 0)
                ++used_slots;

        reserved_bytes = (uint64_t)pool->width * (uint64_t)pool->height * 2ULL * (uint64_t)pool->slot_count;
        out_stats->pools[out_index].used = true;
        out_stats->pools[out_index].width = pool->width;
        out_stats->pools[out_index].height = pool->height;
        out_stats->pools[out_index].used_slots = used_slots;
        out_stats->pools[out_index].total_slots = pool->slot_count;
        out_stats->pools[out_index].reserved_bytes = reserved_bytes;
        ++out_stats->pool_count;
        out_stats->used_slots += used_slots;
        out_stats->total_slots += pool->slot_count;
        out_stats->reserved_bytes += reserved_bytes;
        ++out_index;
    }
}

int rotating_sprite_pool_create(unsigned short width, unsigned short height, int slot_count)
{
    rotating_sprite_pool_t *pool;
    int pool_id;
    int i;

    if (slot_count <= 0 || slot_count > ROTATING_SPRITE_POOL_MAX_SLOTS)
        return -1;

    pool_id = rotating_sprite_pool_find_free_pool();
    if (pool_id < 0)
        return -1;

    pool = &g_rotating_pools[pool_id];
    pool->used = true;
    pool->width = width;
    pool->height = height;
    pool->slot_count = slot_count;

    for (i = 0; i < slot_count; ++i)
    {
        pool->sprite_ids[i] = rotating_sprite_pool_create_blank_sprite(width, height);
        if (pool->sprite_ids[i] < 0)
            return -1;
        pool->item_ids[i] = -1;
        pool->last_used[i] = 0;
    }

    /* account for blank sprite allocation as upload bytes (approx) */
    rsp_total_upload_bytes += (uint64_t)width * (uint64_t)height * 3ULL * (uint64_t)slot_count;

    return pool_id;
}

int rotating_sprite_pool_register_tga(int pool_id, const char *sub_dir, const char *filename, jo_color transparent_color)
{
    rotating_sprite_item_t *item;
    int item_id;

    if (pool_id < 0 || pool_id >= ROTATING_SPRITE_POOL_MAX_POOLS)
        return -1;
    if (!g_rotating_pools[pool_id].used || filename == JO_NULL)
        return -1;

    item_id = rotating_sprite_pool_find_free_item();
    if (item_id < 0)
        return -1;

    item = &g_rotating_items[item_id];
    jo_memset(item, 0, sizeof(*item));
    item->used = true;
    item->pool_id = pool_id;
    item->slot_index = -1;
    item->transparent_color = transparent_color;
    if (sub_dir != JO_NULL)
        strncpy(item->sub_dir, sub_dir, ROTATING_SPRITE_DIR_MAX - 1);
    strncpy(item->filename, filename, ROTATING_SPRITE_NAME_MAX - 1);
    return item_id;
}

bool rotating_sprite_pool_prefetch(int item_id)
{
    rotating_sprite_item_t *item;
    char *contents;

    if (item_id < 0 || item_id >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return false;

    item = &g_rotating_items[item_id];
    if (!item->used)
        return false;

    if (item->ready)
        return true;

    contents = jo_fs_read_file_in_dir(item->filename, item->sub_dir[0] != '\0' ? item->sub_dir : JO_NULL, JO_NULL);
    return rotating_sprite_pool_decode_item(item, contents);
}

int rotating_sprite_pool_request(int item_id)
{
    rotating_sprite_item_t *item;
    rotating_sprite_pool_t *pool;
    int slot_index;
    int previous_item_id;

    if (item_id < 0 || item_id >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return -1;

    item = &g_rotating_items[item_id];
    if (!item->used)
        return -1;

    ++rsp_total_requests;

    if (!item->ready)
    {
        rotating_sprite_pool_prefetch(item_id);
        if (!item->ready)
            return -1;
    }

    pool = &g_rotating_pools[item->pool_id];
    ++g_rotating_tick;

    if (item->slot_index >= 0)
    {
        ++rsp_hits;
        pool->last_used[item->slot_index] = g_rotating_tick;
        return pool->sprite_ids[item->slot_index];
    }

    ++rsp_misses;
    slot_index = rotating_sprite_pool_select_slot(pool);
    previous_item_id = pool->item_ids[slot_index];
    if (previous_item_id >= 0 && previous_item_id < ROTATING_SPRITE_POOL_MAX_ITEMS)
    {
        ++rsp_replacements;
        g_rotating_items[previous_item_id].slot_index = -1;
    }

    /* track upload/waste bytes (reporting uses 3 bytes/pixel as approximation) */
    uint64_t slot_bytes = (uint64_t)pool->width * (uint64_t)pool->height * 3ULL;
    uint64_t image_bytes = (uint64_t)item->image.width * (uint64_t)item->image.height * 3ULL;
    rsp_total_upload_bytes += slot_bytes;
    if (slot_bytes > image_bytes) rsp_total_wasted_bytes += (slot_bytes - image_bytes);

    jo_sprite_replace(&item->image, pool->sprite_ids[slot_index]);
    pool->item_ids[slot_index] = item_id;
    pool->last_used[slot_index] = g_rotating_tick;
    item->slot_index = slot_index;
    return pool->sprite_ids[slot_index];
}

void *rotating_sprite_pool_get_wram_ptr(int item_id)
{
    if (item_id < 0 || item_id >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return JO_NULL;
    if (!g_rotating_items[item_id].used)
        return JO_NULL;
    if (!g_rotating_items[item_id].ready)
        return JO_NULL;

    return g_rotating_items[item_id].image.data;
}

int rotating_sprite_pool_get_fallback_sprite_id(int item_id)
{
    rotating_sprite_item_t *item;
    rotating_sprite_pool_t *pool;

    if (item_id < 0 || item_id >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return -1;

    item = &g_rotating_items[item_id];
    if (!item->used)
        return -1;

    pool = &g_rotating_pools[item->pool_id];
    if (!pool->used || pool->slot_count <= 0)
        return -1;

    return pool->sprite_ids[0];
}

bool rotating_sprite_pool_is_ready(int item_id)
{
    if (item_id < 0 || item_id >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return false;
    return g_rotating_items[item_id].used && g_rotating_items[item_id].ready;
}

bool rotating_sprite_pool_is_loading(int item_id)
{
    if (item_id < 0 || item_id >= ROTATING_SPRITE_POOL_MAX_ITEMS)
        return false;
    return g_rotating_items[item_id].used && g_rotating_items[item_id].loading;
}

void rotating_sprite_pool_reset(void)
{
    int i;
    int j;

    for (i = 0; i < ROTATING_SPRITE_POOL_MAX_ITEMS; ++i)
    {
        if (!g_rotating_items[i].used)
            continue;
        if (g_rotating_items[i].image.data != JO_NULL)
            jo_free_img(&g_rotating_items[i].image);
        jo_memset(&g_rotating_items[i], 0, sizeof(g_rotating_items[i]));
        g_rotating_items[i].slot_index = -1;
    }

    for (i = 0; i < ROTATING_SPRITE_POOL_MAX_POOLS; ++i)
    {
        if (!g_rotating_pools[i].used)
            continue;
        for (j = 0; j < g_rotating_pools[i].slot_count; ++j)
        {
            g_rotating_pools[i].item_ids[j] = -1;
            g_rotating_pools[i].last_used[j] = 0;
        }
    }
    g_rotating_active_item_id = -1;
    g_rotating_tick = 0;
}

void rotating_sprite_pool_release_freed_sprites(int first_sprite_id)
{
    int i;
    int j;

    if (first_sprite_id < 0)
        return;

    for (i = 0; i < ROTATING_SPRITE_POOL_MAX_POOLS; ++i)
    {
        bool pool_was_freed = false;

        if (!g_rotating_pools[i].used)
            continue;

        for (j = 0; j < g_rotating_pools[i].slot_count; ++j)
        {
            if (g_rotating_pools[i].sprite_ids[j] >= first_sprite_id)
            {
                pool_was_freed = true;
                break;
            }
        }

        if (!pool_was_freed)
            continue;

        for (j = 0; j < ROTATING_SPRITE_POOL_MAX_ITEMS; ++j)
        {
            if (!g_rotating_items[j].used || g_rotating_items[j].pool_id != i)
                continue;

            if (g_rotating_items[j].image.data != JO_NULL)
                jo_free_img(&g_rotating_items[j].image);
            jo_memset(&g_rotating_items[j], 0, sizeof(g_rotating_items[j]));
            g_rotating_items[j].slot_index = -1;
        }

        for (j = 0; j < ROTATING_SPRITE_POOL_MAX_SLOTS; ++j)
        {
            g_rotating_pools[i].sprite_ids[j] = -1;
            g_rotating_pools[i].item_ids[j] = -1;
            g_rotating_pools[i].last_used[j] = 0;
        }

        g_rotating_pools[i].used = false;
        g_rotating_pools[i].width = 0;
        g_rotating_pools[i].height = 0;
        g_rotating_pools[i].slot_count = 0;
    }
}