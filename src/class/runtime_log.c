#include "runtime_log.h"
#include "vram_cache.h"
#include "rotating_sprite_pool.h"
#include <jo/jo.h>
#include <stdio.h>
#include <stdarg.h>

#define RUNTIME_LOG_LINES 10
#define RUNTIME_LOG_LINE_SIZE 40
#define RUNTIME_LOG_SPRITE_X 0
#define RUNTIME_LOG_SPRITE_Y 18
#define RUNTIME_LOG_SPRITE_POOLS_PER_PAGE 2

static const char g_runtime_log_blank_line[] = "                                       ";

/* Toggle these runtime checkpoints on/off for future tests. */
static runtime_log_mode_t g_runtime_log_mode = RuntimeLogModeOff;
static bool g_runtime_log_verbose_enabled = false;
static char g_runtime_log_lines[RUNTIME_LOG_LINES][RUNTIME_LOG_LINE_SIZE];
static int g_runtime_log_count = 0;
static int g_runtime_log_next = 0;
static int g_runtime_log_sprite_page = 0;
static char g_cart_ram_log_lines[RUNTIME_LOG_LINES][RUNTIME_LOG_LINE_SIZE];
static int g_cart_ram_log_count = 0;
static int g_cart_ram_log_next = 0;

static int runtime_log_compute_sprite_page_count(int pool_count)
{
    int page_count = (pool_count + RUNTIME_LOG_SPRITE_POOLS_PER_PAGE - 1) / RUNTIME_LOG_SPRITE_POOLS_PER_PAGE;

    return (page_count > 0) ? page_count : 1;
}

static void runtime_log_draw_sprite_stats(void)
{
    vram_cache_stats_t stats;
    rotating_sprite_pool_stats_t pool_stats;
    int page_count;
    int first_pool_index;
    int line = 8;
    int pool_offset;

    vram_cache_get_stats(&stats);
    rotating_sprite_pool_get_stats(&pool_stats);
    page_count = runtime_log_compute_sprite_page_count(pool_stats.pool_count);
    if (g_runtime_log_sprite_page >= page_count)
        g_runtime_log_sprite_page = page_count - 1;
    if (g_runtime_log_sprite_page < 0)
        g_runtime_log_sprite_page = 0;

    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 0, JO_COLOR_INDEX_White, "SPRITE LOGS %d/%d", g_runtime_log_sprite_page + 1, page_count);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 1, JO_COLOR_INDEX_White, "REQUESTS: %lu", (unsigned long)stats.tile_requests);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 2, JO_COLOR_INDEX_White, "MAPPED: %lu", (unsigned long)stats.tile_hits);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 3, JO_COLOR_INDEX_White, "MISSES: %lu", (unsigned long)stats.tile_misses);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 4, JO_COLOR_INDEX_White, "RESIDENTS: %lu", (unsigned long)stats.residents);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 5, JO_COLOR_INDEX_White, "UPLOADS: %lu", (unsigned long)stats.tile_uploads);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 6, JO_COLOR_INDEX_White, "POOL SLOTS: %d/%d", pool_stats.used_slots, pool_stats.total_slots);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 7, JO_COLOR_INDEX_White, "POOL VRAM: %u KB", (unsigned)(pool_stats.reserved_bytes / 1024ULL));

    // Show if the Sonic sheet is loaded into WRAM (used for on-demand VRAM sprite updates)
    {
        bool sonic_loading = vram_cache_is_pack_loading("sonic");
        bool sonic_sheet = vram_cache_is_pack_present("sonic");
        size_t sonic_size = vram_cache_get_pack_size("sonic");
        jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 8, JO_COLOR_INDEX_White,
                             "SNC_FUL: %s %s %uKB",
                             sonic_sheet ? "loaded" : "missing",
                             sonic_loading ? "(loading)" : "",
                             (unsigned)(sonic_size / 1024ULL));
    }

    // Ensure pool list starts below the SNC_FUL line
    line = 9;

    first_pool_index = g_runtime_log_sprite_page * RUNTIME_LOG_SPRITE_POOLS_PER_PAGE;
    for (pool_offset = 0; pool_offset < RUNTIME_LOG_SPRITE_POOLS_PER_PAGE; ++pool_offset)
    {
        int pool_index = first_pool_index + pool_offset;

        if (pool_index >= pool_stats.pool_count)
            break;

        jo_printf_with_color(RUNTIME_LOG_SPRITE_X,
                             RUNTIME_LOG_SPRITE_Y + line,
                             JO_COLOR_INDEX_White,
                             "P%d %ux%u %d/%d %uK",
                             pool_index,
                             pool_stats.pools[pool_index].width,
                             pool_stats.pools[pool_index].height,
                             pool_stats.pools[pool_index].used_slots,
                             pool_stats.pools[pool_index].total_slots,
                             (unsigned)(pool_stats.pools[pool_index].reserved_bytes / 1024ULL));
        ++line;
    }
}

void runtime_log_init(void)
{
    int i;

    g_runtime_log_count = 0;
    g_runtime_log_next = 0;
    g_cart_ram_log_count = 0;
    g_cart_ram_log_next = 0;
    for (i = 0; i < RUNTIME_LOG_LINES; ++i)
    {
        g_runtime_log_lines[i][0] = '\0';
        g_cart_ram_log_lines[i][0] = '\0';
    }

    if (g_runtime_log_mode == RuntimeLogModeSystem)
        runtime_log("runtime log ready");
}

void runtime_log_enable(bool enabled)
{
    runtime_log_set_mode(enabled ? RuntimeLogModeSystem : RuntimeLogModeOff);
}

void runtime_log_set_mode(runtime_log_mode_t mode)
{
    if (mode < RuntimeLogModeOff || mode >= RuntimeLogModeCount)
        mode = RuntimeLogModeOff;

    if (g_runtime_log_mode != mode)
    {
        if (mode == RuntimeLogModeOff)
        {
            runtime_log_clear();
            cart_ram_log_clear();
        }
    }

    g_runtime_log_mode = mode;
    if (g_runtime_log_mode != RuntimeLogModeSprite)
        g_runtime_log_sprite_page = 0;

    if (g_runtime_log_mode == RuntimeLogModeSystem)
    {
        g_runtime_log_verbose_enabled = false;
        runtime_log("runtime log enabled");
    }
    else if (g_runtime_log_mode == RuntimeLogModeSystemVerbose)
    {
        g_runtime_log_verbose_enabled = true;
        runtime_log("runtime log enabled (verbose)");
    }
    else if (g_runtime_log_mode == RuntimeLogModeCartRam)
    {
        g_runtime_log_verbose_enabled = false;
    }
}

runtime_log_mode_t runtime_log_get_mode(void)
{
    return g_runtime_log_mode;
}

bool runtime_log_is_enabled(void)
{
    return g_runtime_log_mode != RuntimeLogModeOff;
}

void runtime_log_set_sprite_page(int page)
{
    if (page < 0)
        page = 0;
    g_runtime_log_sprite_page = page;
}

int runtime_log_get_sprite_page(void)
{
    return g_runtime_log_sprite_page;
}

int runtime_log_get_sprite_page_count(void)
{
    rotating_sprite_pool_stats_t pool_stats;

    rotating_sprite_pool_get_stats(&pool_stats);
    return runtime_log_compute_sprite_page_count(pool_stats.pool_count);
}

void runtime_log_draw(int x, int y)
{
    int visible_count;
    int first_index;
    int i;

    if (y < 2)
        y = 2;

    if (g_runtime_log_mode == RuntimeLogModeOff)
        return;

    if (g_runtime_log_mode == RuntimeLogModeCartRam)
    {
        int cart_visible_count = g_cart_ram_log_count;
        int cart_first_index;

        if (cart_visible_count <= 0)
            return;

        for (i = 0; i < RUNTIME_LOG_LINES; ++i)
            jo_printf_with_color(x, y + i, JO_COLOR_INDEX_White, "%s", g_runtime_log_blank_line);

        cart_first_index = g_cart_ram_log_count < RUNTIME_LOG_LINES ? 0 : g_cart_ram_log_next;
        for (i = 0; i < cart_visible_count; ++i)
        {
            int line_index = (cart_first_index + i) % RUNTIME_LOG_LINES;
            jo_printf_with_color(x, y + i, JO_COLOR_INDEX_White, "%s", g_cart_ram_log_lines[line_index]);
        }
        return;
    }

    if (g_runtime_log_mode == RuntimeLogModeSprite)
    {
        for (i = 0; i < RUNTIME_LOG_LINES; ++i)
            jo_printf_with_color(x, y + i, JO_COLOR_INDEX_White, "%s", g_runtime_log_blank_line);
        runtime_log_draw_sprite_stats();
        return;
    }

    visible_count = g_runtime_log_count;
    if (visible_count <= 0)
        return;

    for (i = 0; i < RUNTIME_LOG_LINES; ++i)
        jo_printf_with_color(x, y + i, JO_COLOR_INDEX_White, "%s", g_runtime_log_blank_line);

    first_index = g_runtime_log_count < RUNTIME_LOG_LINES ? 0 : g_runtime_log_next;
    for (i = 0; i < visible_count; ++i) {
        int line_index = (first_index + i) % RUNTIME_LOG_LINES;
        /* Single pass drawing (no shadow) to keep text readable on emulator */
        jo_printf_with_color(x, y + i, JO_COLOR_INDEX_White, "%s", g_runtime_log_lines[line_index]);
    }
}

void runtime_log_clear(void)
{
    int i;
    g_runtime_log_count = 0;
    g_runtime_log_next = 0;
    for (i = 0; i < RUNTIME_LOG_LINES; ++i)
        g_runtime_log_lines[i][0] = '\0';
}

void cart_ram_log_clear(void)
{
    int i;

    g_cart_ram_log_count = 0;
    g_cart_ram_log_next = 0;
    for (i = 0; i < RUNTIME_LOG_LINES; ++i)
        g_cart_ram_log_lines[i][0] = '\0';
}

void runtime_log(const char *fmt, ...)
{
    int line_index;
    va_list ap;

    if (g_runtime_log_mode != RuntimeLogModeSystem && g_runtime_log_mode != RuntimeLogModeSystemVerbose)
        return;

    line_index = g_runtime_log_next;
    va_start(ap, fmt);
    vsnprintf(g_runtime_log_lines[line_index], RUNTIME_LOG_LINE_SIZE, fmt, ap);
    va_end(ap);

    g_runtime_log_next = (g_runtime_log_next + 1) % RUNTIME_LOG_LINES;
    if (g_runtime_log_count < RUNTIME_LOG_LINES)
        ++g_runtime_log_count;
}

void runtime_log_verbose(const char *fmt, ...)
{
    int line_index;
    va_list ap;

    if (g_runtime_log_mode != RuntimeLogModeSystemVerbose)
        return;

    line_index = g_runtime_log_next;
    va_start(ap, fmt);
    vsnprintf(g_runtime_log_lines[line_index], RUNTIME_LOG_LINE_SIZE, fmt, ap);
    va_end(ap);

    g_runtime_log_next = (g_runtime_log_next + 1) % RUNTIME_LOG_LINES;
    if (g_runtime_log_count < RUNTIME_LOG_LINES)
        ++g_runtime_log_count;
}

void cart_ram_log(const char *fmt, ...)
{
    int line_index;
    va_list ap;

    line_index = g_cart_ram_log_next;
    va_start(ap, fmt);
    vsnprintf(g_cart_ram_log_lines[line_index], RUNTIME_LOG_LINE_SIZE, fmt, ap);
    va_end(ap);

    g_cart_ram_log_next = (g_cart_ram_log_next + 1) % RUNTIME_LOG_LINES;
    if (g_cart_ram_log_count < RUNTIME_LOG_LINES)
        ++g_cart_ram_log_count;
}

void runtime_log_close(void)
{
    if (g_runtime_log_mode == RuntimeLogModeSystem)
        runtime_log("runtime log closed");
}
