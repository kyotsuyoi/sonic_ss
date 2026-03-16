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
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 1, JO_COLOR_INDEX_White, "REQUESTS: %u", stats.tile_requests);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 2, JO_COLOR_INDEX_White, "MAPPED: %u", stats.tile_hits);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 3, JO_COLOR_INDEX_White, "MISSES: %u", stats.tile_misses);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 4, JO_COLOR_INDEX_White, "RESIDENTS: %u", stats.residents);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 5, JO_COLOR_INDEX_White, "UPLOADS: %u", stats.tile_uploads);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 6, JO_COLOR_INDEX_White, "POOL SLOTS: %d/%d", pool_stats.used_slots, pool_stats.total_slots);
    jo_printf_with_color(RUNTIME_LOG_SPRITE_X, RUNTIME_LOG_SPRITE_Y + 7, JO_COLOR_INDEX_White, "POOL VRAM: %u KB", (unsigned)(pool_stats.reserved_bytes / 1024ULL));

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
    for (i = 0; i < RUNTIME_LOG_LINES; ++i)
        g_runtime_log_lines[i][0] = '\0';

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
        runtime_log_clear();

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
}

runtime_log_mode_t runtime_log_get_mode(void)
{
    return g_runtime_log_mode;
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

    if (g_runtime_log_mode == RuntimeLogModeOff)
        return;

    if (y < 2)
        y = 2;

    for (i = 0; i < RUNTIME_LOG_LINES; ++i)
        jo_printf_with_color(x, y + i, JO_COLOR_INDEX_White, "%s", g_runtime_log_blank_line);

    if (g_runtime_log_mode == RuntimeLogModeSprite)
    {
        runtime_log_draw_sprite_stats();
        return;
    }

    visible_count = g_runtime_log_count;
    if (visible_count <= 0)
        return;

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

void runtime_log_close(void)
{
    if (g_runtime_log_mode == RuntimeLogModeSystem)
        runtime_log("runtime log closed");
}
