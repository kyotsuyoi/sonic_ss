#include <jo/jo.h>
#include <string.h>
#include "game_constants.h"
#include "world_map.h"
#include "runtime_log.h"
#include "rotating_sprite_pool.h"
#include "vram_cache.h"
#include "jo_ext/jo_map_ext.h"

#define WORLD_MAP_FIELD_BUFFER_SIZE (16)
#define WORLD_MAP_MAX_GROUPS (ROTATING_SPRITE_POOL_MAX_POOLS)

typedef struct
{
    char filename[JO_MAX_FILENAME_LENGTH];
    unsigned short width;
    unsigned short height;
    int group_index;
    int item_id;
    int fallback_sprite_id;
    int prepared_sprite_id;
    unsigned int prepared_tick;
} world_map_asset_t;

typedef struct
{
    bool used;
    unsigned short width;
    unsigned short height;
    int pool_id;
    int asset_count;
    int max_visible_assets;
} world_map_group_t;

typedef struct
{
    short x;
    short y;
    unsigned char attribute;
    unsigned char layer;
    bool is_animated;
    unsigned char anim_id;
    short width;
    short height;
    short asset_index;
    unsigned short tile_index;
    int applied_sprite_id;
} world_map_tile_t;

static bool deferred_load_started = false;
static bool deferred_load_completed = false;
static char *world_map_stream = JO_NULL;
static char *world_tiles_stream = JO_NULL;
static char world_map_filename[JO_MAX_FILENAME_LENGTH] = "GHS.MAP";

/* Player start position marker (from `STP10.TGA`/`STP11.TGA`/`STP12.TGA` for P1 or `STP20.TGA`/`STP21.TGA`/`STP22.TGA` for P2, or attribute 255 entries). */
static int world_map_player_start_x[2][3] =
{
    {WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X},
    {WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X}
};
static int world_map_player_start_y[2][3] =
{
    {WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y},
    {WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y}
};

/* Bot start position marker (from `STB10.TGA`/`STB11.TGA`/`STB12.TGA` for Bot1, `STB20.TGA`/`STB21.TGA`/`STB22.TGA` for Bot2, etc.) */
static int world_map_bot_start_x[4][3] =
{
    {WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X},
    {WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X},
    {WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X},
    {WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X, WORLD_CAMERA_TARGET_X}
};
static int world_map_bot_start_y[4][3] =
{
    {WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y},
    {WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y},
    {WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y},
    {WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y, WORLD_CAMERA_TARGET_Y}
};

/* Track the vertical extent of the map so characters falling off the bottom can be teleported back onto the last tile. */
static int world_map_max_tile_bottom = 0;
static int world_map_max_tile_top = 0;

static world_map_asset_t world_assets[JO_MAX_FILE_IN_IMAGE_PACK];

void world_map_set_filename(const char *filename)
{
    if (filename == JO_NULL)
        return;
    strncpy(world_map_filename, filename, JO_MAX_FILENAME_LENGTH - 1);
    world_map_filename[JO_MAX_FILENAME_LENGTH - 1] = '\0';
}
static world_map_group_t world_groups[WORLD_MAP_MAX_GROUPS];
static world_map_tile_t *world_tiles = JO_NULL;
static int world_asset_count = 0;
static int world_probe_index = 0;
static int world_prefetch_index = 0;
static int world_map_tile_count = 0;
static unsigned int world_prepare_tick = 0;
static int deferred_load_step = 0;

static void world_map_reset_streams(void)
{
    if (world_map_stream != JO_NULL)
    {
        jo_free(world_map_stream);
        world_map_stream = JO_NULL;
    }
    if (world_tiles_stream != JO_NULL)
    {
        jo_free(world_tiles_stream);
        world_tiles_stream = JO_NULL;
    }
}

static void world_map_reset_groups(void)
{
    int index;

    for (index = 0; index < WORLD_MAP_MAX_GROUPS; ++index)
    {
        world_groups[index].used = false;
        world_groups[index].width = 0;
        world_groups[index].height = 0;
        world_groups[index].pool_id = -1;
        world_groups[index].asset_count = 0;
        world_groups[index].max_visible_assets = 0;
    }
}

static void world_map_reset_assets(void)
{
    int index;

    for (index = 0; index < JO_MAX_FILE_IN_IMAGE_PACK; ++index)
    {
        world_assets[index].filename[0] = '\0';
        world_assets[index].width = 0;
        world_assets[index].height = 0;
        world_assets[index].group_index = -1;
        world_assets[index].item_id = -1;
        world_assets[index].fallback_sprite_id = -1;
        world_assets[index].prepared_sprite_id = -1;
        world_assets[index].prepared_tick = 0;
    }
    world_asset_count = 0;
    world_probe_index = 0;
    world_prefetch_index = 0;
}

static void world_map_reset_tiles(void)
{
    if (world_tiles != JO_NULL)
    {
        jo_free(world_tiles);
        world_tiles = JO_NULL;
    }
    world_map_tile_count = 0;
    for (int player = 0; player < 2; ++player)
    {
        for (int group = 0; group < 3; ++group)
        {
            world_map_player_start_x[player][group] = WORLD_CAMERA_TARGET_X;
            world_map_player_start_y[player][group] = WORLD_CAMERA_TARGET_Y;
        }
    }

    for (int bot = 0; bot < 4; ++bot)
    {
        for (int group = 0; group < 3; ++group)
        {
            world_map_bot_start_x[bot][group] = WORLD_CAMERA_TARGET_X;
            world_map_bot_start_y[bot][group] = WORLD_CAMERA_TARGET_Y;
        }
    }
    world_map_max_tile_bottom = 0;
    world_map_max_tile_top = 0;
}

static int world_map_count_tiles(const char *stream)
{
    int count;
    bool has_token;

    count = 0;
    has_token = false;
    while (*stream)
    {
        if (*stream == '\n')
        {
            if (has_token)
                ++count;
            has_token = false;
        }
        else if (!jo_tools_is_whitespace(*stream))
        {
            has_token = true;
        }
        ++stream;
    }
    if (has_token)
        ++count;
    return count;
}

static bool world_map_parse_tiles_manifest(const char *stream)
{
    int char_index;

    world_map_reset_assets();
    while (*stream)
    {
        while (*stream && jo_tools_is_whitespace(*stream))
            ++stream;
        if (!*stream)
            break;

        if (world_asset_count >= JO_MAX_FILE_IN_IMAGE_PACK)
            return false;

        for (char_index = 0; *stream && !jo_tools_is_whitespace(*stream); ++char_index)
        {
            if (char_index >= JO_MAX_FILENAME_LENGTH - 1)
                return false;
            world_assets[world_asset_count].filename[char_index] = *stream++;
        }
        world_assets[world_asset_count].filename[char_index] = '\0';
        ++world_asset_count;
    }

    return world_asset_count > 0;
}

static int world_map_find_asset_index(const char *filename)
{
    int index;
    static char warned_missing[32][JO_MAX_FILENAME_LENGTH];
    static int warned_count = 0;

    for (index = 0; index < world_asset_count; ++index)
        if (strcmp(world_assets[index].filename, filename) == 0)
            return index;

    for (index = 0; index < warned_count; ++index)
        if (strcmp(warned_missing[index], filename) == 0)
            return -1;

    if (warned_count < (int)(sizeof(warned_missing) / sizeof(warned_missing[0])))
    {
        strcpy(warned_missing[warned_count], filename);
        ++warned_count;
    }

    runtime_log("world_map: missing tile asset '%s' (check BLK.TEX)", filename);
    return -1;
}

static int world_map_find_or_add_group(unsigned short width, unsigned short height)
{
    int index;

    for (index = 0; index < WORLD_MAP_MAX_GROUPS; ++index)
    {
        if (!world_groups[index].used)
            continue;
        if (world_groups[index].width == width && world_groups[index].height == height)
            return index;
    }

    for (index = 0; index < WORLD_MAP_MAX_GROUPS; ++index)
    {
        if (world_groups[index].used)
            continue;
        world_groups[index].used = true;
        world_groups[index].width = width;
        world_groups[index].height = height;
        world_groups[index].pool_id = -1;
        world_groups[index].asset_count = 0;
        world_groups[index].max_visible_assets = 0;
        return index;
    }

    return -1;
}

static bool world_map_probe_asset_dimensions(world_map_asset_t *asset)
{
    char *contents;
    jo_img image;
    int group_index;

    runtime_log("world_map: probing asset '%s'", asset->filename);

    contents = jo_fs_read_file_in_dir(asset->filename, "BLK", JO_NULL);
    if (contents == JO_NULL)
    {
        runtime_log("world_map: missing asset '%s' (skipping)", asset->filename);
        // Avoid stalling the load if a tile asset is missing.
        asset->width = CHARACTER_WIDTH;
        asset->height = CHARACTER_HEIGHT;
        group_index = world_map_find_or_add_group(asset->width, asset->height);
        if (group_index < 0)
        {
            runtime_log("world_map: too many unique tile sizes, falling back to group 0");
            group_index = 0;
        }
        asset->group_index = group_index;
        ++world_groups[group_index].asset_count;
        return true;
    }

    image.data = JO_NULL;
    if (jo_tga_loader_from_stream(&image, contents, JO_COLOR_Red) != JO_TGA_OK)
    {
        runtime_log("world_map: failed to decode '%s' (skipping)", asset->filename);
        jo_free(contents);

        asset->width = CHARACTER_WIDTH;
        asset->height = CHARACTER_HEIGHT;
        group_index = world_map_find_or_add_group(asset->width, asset->height);
        if (group_index < 0)
        {
            runtime_log("world_map: too many unique tile sizes, falling back to group 0");
            group_index = 0;
        }
        asset->group_index = group_index;
        ++world_groups[group_index].asset_count;
        return true;
    }

    asset->width = image.width;
    asset->height = image.height;
    jo_free_img(&image);
    jo_free(contents);

    group_index = world_map_find_or_add_group(asset->width, asset->height);
    if (group_index < 0)
    {
        runtime_log("world_map: too many unique tile sizes, falling back to group 0");
        group_index = 0;
    }

    asset->group_index = group_index;
    ++world_groups[group_index].asset_count;
    return true;
}

static bool world_map_parse_entries_from_stream(const char *stream)
{
    char sprite[JO_MAX_FILENAME_LENGTH];
    char x_buf[WORLD_MAP_FIELD_BUFFER_SIZE];
    char y_buf[WORLD_MAP_FIELD_BUFFER_SIZE];
    char attribute_buf[WORLD_MAP_FIELD_BUFFER_SIZE];
    int tile_capacity;
    int tile_index;
    int index;

    world_map_reset_tiles();
    tile_capacity = world_map_count_tiles(stream);
    runtime_log("world_map: tile count %d", tile_capacity);
    if (tile_capacity <= 0)
        return false;

    world_tiles = (world_map_tile_t *)jo_malloc(tile_capacity * sizeof(*world_tiles));
    if (world_tiles == JO_NULL)
    {
        runtime_log("world_map: failed to allocate %d tiles", tile_capacity);
        return false;
    }

    tile_index = 0;
    while (*stream)
    {
        int char_index;
        unsigned char attribute_value;
        unsigned char layer_value;

        attribute_value = 0;
        layer_value = 0;
        while (*stream && jo_tools_is_whitespace(*stream))
            ++stream;
        if (!*stream)
            break;

        for (char_index = 0; *stream && !jo_tools_is_whitespace(*stream); ++char_index)
        {
            if (char_index >= JO_MAX_FILENAME_LENGTH - 1)
                return false;
            sprite[char_index] = *stream++;
        }
        sprite[char_index] = '\0';

        while (*stream && jo_tools_is_whitespace(*stream))
            ++stream;
        for (char_index = 0; *stream && !jo_tools_is_whitespace(*stream); ++char_index)
        {
            if (char_index >= WORLD_MAP_FIELD_BUFFER_SIZE - 1)
                return false;
            x_buf[char_index] = *stream++;
        }
        x_buf[char_index] = '\0';

        while (*stream && jo_tools_is_whitespace(*stream))
            ++stream;
        for (char_index = 0; *stream && !jo_tools_is_whitespace(*stream); ++char_index)
        {
            if (char_index >= WORLD_MAP_FIELD_BUFFER_SIZE - 1)
                return false;
            y_buf[char_index] = *stream++;
        }
        y_buf[char_index] = '\0';

        /* Ignore malformed lines with missing coordinates. */
        if (x_buf[0] == '\0' || y_buf[0] == '\0')
        {
            runtime_log("world_map: malformed tile line (missing coords): %s", sprite);
            while (*stream && *stream != '\n')
                ++stream;
            continue;
        }

        /* Marker by sprite name: set player start position and skip tile creation. */
        /* Marker by sprite name: set player start position and skip tile creation. */
        if (strcmp(sprite, "SNC_SRT.TGA") == 0)
        {
            int start_x = jo_tools_atoi(x_buf);
            int start_y = jo_tools_atoi(y_buf);
            for (int player = 0; player < 2; ++player)
            {
                for (int group = 0; group < 3; ++group)
                {
                    world_map_player_start_x[player][group] = start_x;
                    world_map_player_start_y[player][group] = start_y;
                }
            }
            runtime_log("world_map: start marker sprite at %d,%d", start_x, start_y);
            /* Skip remaining fields on this line (e.g. attribute) so we don't reprocess it. */
            while (*stream && *stream != '\n')
                ++stream;
            continue;
        }

        /* New marker format: STP1X.TGA / STP2X.TGA where X is the player group (0-2) */
        if ((strncmp(sprite, "STP1", 4) == 0 || strncmp(sprite, "STP2", 4) == 0) && strlen(sprite) >= 6)
        {
            int player_index = (sprite[3] - '0') - 1; /* '1' -> 0, '2' -> 1 */
            int group_id = sprite[4] - '0';
            if (player_index >= 0 && player_index < 2 && group_id >= 0 && group_id < 3)
            {
                world_map_player_start_x[player_index][group_id] = jo_tools_atoi(x_buf);
                world_map_player_start_y[player_index][group_id] = jo_tools_atoi(y_buf);
                runtime_log("world_map: start marker sprite %s at %d,%d (player %d group %d)",
                            sprite,
                            world_map_player_start_x[player_index][group_id],
                            world_map_player_start_y[player_index][group_id],
                            player_index + 1,
                            group_id);
            }
            /* Skip remaining fields on this line */
            while (*stream && *stream != '\n')
                ++stream;
            continue;
        }

        /* Bot start position marker format: STB<bot#><group>.TGA where bot# is 1-4 and group is 0-2 */
        if (strncmp(sprite, "STB", 3) == 0 && strlen(sprite) >= 6)
        {
            int bot_index = (sprite[3] - '0') - 1; /* '1' -> 0 */
            int group_id = sprite[4] - '0';
            if (bot_index >= 0 && bot_index < 4 && group_id >= 0 && group_id < 3)
            {
                world_map_bot_start_x[bot_index][group_id] = jo_tools_atoi(x_buf);
                world_map_bot_start_y[bot_index][group_id] = jo_tools_atoi(y_buf);
                runtime_log("world_map: start marker sprite %s at %d,%d (bot %d group %d)",
                            sprite,
                            world_map_bot_start_x[bot_index][group_id],
                            world_map_bot_start_y[bot_index][group_id],
                            bot_index + 1,
                            group_id);
            }
            /* Skip remaining fields on this line */
            while (*stream && *stream != '\n')
                ++stream;
            continue;
        }

        if (*stream && *stream != '\r' && *stream != '\n')
        {
            size_t attribute_len;

            while (*stream && jo_tools_is_whitespace(*stream))
                ++stream;
            for (char_index = 0; *stream && !jo_tools_is_whitespace(*stream); ++char_index)
            {
                if (char_index >= WORLD_MAP_FIELD_BUFFER_SIZE - 1)
                    return false;
                attribute_buf[char_index] = *stream++;
            }
            attribute_buf[char_index] = '\0';

            /* Marker by attribute: 255 (captures start position and skip tile). */
            if (strcmp(attribute_buf, "255") == 0)
            {
                int start_x = jo_tools_atoi(x_buf);
                int start_y = jo_tools_atoi(y_buf);
                for (int player = 0; player < 2; ++player)
                {
                    for (int group = 0; group < 3; ++group)
                    {
                        world_map_player_start_x[player][group] = start_x;
                        world_map_player_start_y[player][group] = start_y;
                    }
                }
                runtime_log("world_map: start marker attr at %d,%d", start_x, start_y);
                continue;
            }

            attribute_len = strlen(attribute_buf);
            if (attribute_len == 3 && attribute_buf[0] >= '0' && attribute_buf[0] <= '9' &&
                attribute_buf[1] >= '0' && attribute_buf[1] <= '9' &&
                attribute_buf[2] >= '0' && attribute_buf[2] <= '9')
            {
                char attribute_part[3];

                layer_value = (unsigned char)(attribute_buf[2] - '0');
                attribute_part[0] = attribute_buf[0];
                attribute_part[1] = attribute_buf[1];
                attribute_part[2] = '\0';
                attribute_value = (unsigned char)jo_tools_atoi(attribute_part);
            }
            else
            {
                attribute_value = (unsigned char)jo_tools_atoi(attribute_buf);
            }

            world_tiles[tile_index].layer = layer_value;
        }

        if (tile_index >= tile_capacity)
        {
            runtime_log("world_map: tile_index %d >= capacity %d (map too large)", tile_index, tile_capacity);
            return false;
        }

        world_tiles[tile_index].x = (short)jo_tools_atoi(x_buf);
        world_tiles[tile_index].y = (short)jo_tools_atoi(y_buf);
        world_tiles[tile_index].attribute = attribute_value;
        world_tiles[tile_index].layer = layer_value;
        world_tiles[tile_index].tile_index = (unsigned short)tile_index;
        world_tiles[tile_index].applied_sprite_id = -1;
        world_tiles[tile_index].is_animated = (sprite[0] == '@');
        world_tiles[tile_index].anim_id = 0;
        world_tiles[tile_index].asset_index = -1;
        world_tiles[tile_index].width = 0;
        world_tiles[tile_index].height = 0;

        if (world_tiles[tile_index].is_animated)
        {
            world_tiles[tile_index].anim_id = (unsigned char)jo_tools_atoi(sprite + 1);
        }
        else
        {
            index = world_map_find_asset_index(sprite);
            if (index < 0)
            {
                runtime_log("world_map: missing tile asset '%s' (using placeholder)", sprite);
                if (world_asset_count > 0)
                    index = 0;
                else
                    return false;
            }

            world_tiles[tile_index].asset_index = (short)index;
            world_tiles[tile_index].width = (short)world_assets[index].width;
            world_tiles[tile_index].height = (short)world_assets[index].height;
        }

        ++tile_index;
    }

    world_map_tile_count = tile_index;
    runtime_log("world_map: parsed tiles %d", world_map_tile_count);

    /* Debug: log the configured start markers for both players/groups */
    for (int player = 0; player < 2; ++player)
    {
        for (int group = 0; group < 3; ++group)
        {
            runtime_log("world_map: start marker (P%d G%d) = %d,%d", player + 1, group,
                        world_map_player_start_x[player][group],
                        world_map_player_start_y[player][group]);
        }
    }

    return world_map_tile_count > 0;
}

static bool world_map_tile_is_visible(const world_map_tile_t *tile, int screen_x, int screen_y)
{
    return jo_square_intersect(tile->x,
                               tile->y,
                               tile->width,
                               tile->height,
                               screen_x,
                               screen_y,
                               JO_TV_WIDTH,
                               JO_TV_HEIGHT);
}

static void world_map_collect_candidate_positions(int *positions, int *count, int value)
{
    int index;

    for (index = 0; index < *count; ++index)
        if (positions[index] == value)
            return;
    positions[*count] = value;
    ++(*count);
}

static void world_map_compute_group_visibility(void)
{
    int x_positions[512];
    int y_positions[512];
    int x_count;
    int y_count;
    int tile_index;
    int x_index;
    int y_index;

    x_count = 0;
    y_count = 0;
    world_map_collect_candidate_positions(x_positions, &x_count, 0);
    world_map_collect_candidate_positions(y_positions, &y_count, 0);

    for (tile_index = 0; tile_index < world_map_tile_count; ++tile_index)
    {
        world_map_collect_candidate_positions(x_positions, &x_count, world_tiles[tile_index].x);
        world_map_collect_candidate_positions(y_positions, &y_count, world_tiles[tile_index].y);
    }

    for (x_index = 0; x_index < x_count; ++x_index)
    {
        for (y_index = 0; y_index < y_count; ++y_index)
        {
            bool seen_assets[JO_MAX_FILE_IN_IMAGE_PACK];
            int group_counts[WORLD_MAP_MAX_GROUPS];

            jo_memset(seen_assets, 0, sizeof(seen_assets));
            jo_memset(group_counts, 0, sizeof(group_counts));

            for (tile_index = 0; tile_index < world_map_tile_count; ++tile_index)
            {
                int asset_index;
                int group_index;

                if (world_tiles[tile_index].is_animated)
                    continue;
                if (!world_map_tile_is_visible(&world_tiles[tile_index], x_positions[x_index], y_positions[y_index]))
                    continue;

                asset_index = world_tiles[tile_index].asset_index;
                if (asset_index < 0 || seen_assets[asset_index])
                    continue;

                seen_assets[asset_index] = true;
                group_index = world_assets[asset_index].group_index;
                if (group_index >= 0)
                    ++group_counts[group_index];
            }

            for (tile_index = 0; tile_index < WORLD_MAP_MAX_GROUPS; ++tile_index)
                if (world_groups[tile_index].used && group_counts[tile_index] > world_groups[tile_index].max_visible_assets)
                    world_groups[tile_index].max_visible_assets = group_counts[tile_index];
        }
    }
}

static bool world_map_create_pools(void)
{
    int group_index;

    world_map_compute_group_visibility();
    for (group_index = 0; group_index < WORLD_MAP_MAX_GROUPS; ++group_index)
    {
        int slot_count;

        if (!world_groups[group_index].used)
            continue;

        slot_count = world_groups[group_index].max_visible_assets;
        if (slot_count <= 0)
            slot_count = 1;
        if (slot_count > world_groups[group_index].asset_count)
            slot_count = world_groups[group_index].asset_count;
        if (slot_count > 16)
            slot_count = 16;

        world_groups[group_index].pool_id = rotating_sprite_pool_create(world_groups[group_index].width,
                                                                        world_groups[group_index].height,
                                                                        slot_count);
        if (world_groups[group_index].pool_id < 0)
            return false;
    }

    for (group_index = 0; group_index < world_asset_count; ++group_index)
    {
        int pool_id;

        pool_id = world_groups[world_assets[group_index].group_index].pool_id;
        world_assets[group_index].item_id = rotating_sprite_pool_register_tga(pool_id,
                                                                              "BLK",
                                                                              world_assets[group_index].filename,
                                                                              JO_COLOR_Red);
        if (world_assets[group_index].item_id < 0)
            return false;

        world_assets[group_index].fallback_sprite_id = rotating_sprite_pool_get_fallback_sprite_id(world_assets[group_index].item_id);
        if (world_assets[group_index].fallback_sprite_id < 0)
            return false;
    }

    return true;
}

static bool world_map_build_runtime_map(void)
{
    int tile_index;
    int map_index;
    int *tile_order;
    int layer_counts[10] = {0};
    int layer_offsets[10];

    if (!jo_map_create_ext(WORLD_MAP_ID, (unsigned short)world_map_tile_count, 500))
        return false;

    tile_order = (int *)jo_malloc(world_map_tile_count * sizeof(*tile_order));
    if (tile_order == JO_NULL)
        return false;

    /* Build stable ordering by layer (9 first -> 0 last), so layer 0 renders on top. */
    for (tile_index = 0; tile_index < world_map_tile_count; ++tile_index)
    {
        unsigned char layer = world_tiles[tile_index].layer;
        if (layer > 9)
            layer = 0;
        layer_counts[layer]++;
    }

    map_index = 0;
    for (int layer = 9; layer >= 0; --layer)
    {
        layer_offsets[layer] = map_index;
        map_index += layer_counts[layer];
    }

    for (tile_index = 0; tile_index < world_map_tile_count; ++tile_index)
    {
        unsigned char layer = world_tiles[tile_index].layer;
        if (layer > 9)
            layer = 0;
        tile_order[layer_offsets[layer]++] = tile_index;
    }

    for (map_index = 0; map_index < world_map_tile_count; ++map_index)
    {
        int tile_i = tile_order[map_index];
        int tile_height;
        int tile_bottom;

        world_tiles[tile_i].tile_index = (unsigned short)map_index;
        if (world_tiles[tile_i].is_animated)
        {
            int sprite_id = jo_get_anim_sprite(world_tiles[tile_i].anim_id);
            tile_height = __jo_sprite_def[sprite_id].height;

            jo_map_add_animated_tile_ext(WORLD_MAP_ID,
                                         world_tiles[tile_i].x,
                                         world_tiles[tile_i].y,
                                         world_tiles[tile_i].anim_id,
                                         world_tiles[tile_i].attribute);
        }
        else
        {
            tile_height = world_tiles[tile_i].height;
            world_tiles[tile_i].applied_sprite_id = world_assets[world_tiles[tile_i].asset_index].fallback_sprite_id;
            jo_map_add_tile_ext(WORLD_MAP_ID,
                    world_tiles[tile_i].x,
                    world_tiles[tile_i].y,
                    world_tiles[tile_i].applied_sprite_id,
                    world_tiles[tile_i].attribute);
        }

        tile_bottom = world_tiles[tile_i].y + tile_height;
        if (tile_bottom > world_map_max_tile_bottom)
        {
            world_map_max_tile_bottom = tile_bottom;
            world_map_max_tile_top = world_tiles[tile_i].y;
        }
    }

    jo_free(tile_order);
    return true;
}

void world_map_prepare_visible_tiles(int screen_x, int screen_y)
{
    int tile_index;

    if (!deferred_load_completed)
        return;

    ++world_prepare_tick;
    for (tile_index = 0; tile_index < world_map_tile_count; ++tile_index)
    {
        int sprite_id;
        int asset_index;

        if (world_tiles[tile_index].is_animated)
            continue;
        if (!world_map_tile_is_visible(&world_tiles[tile_index], screen_x, screen_y))
            continue;

        asset_index = world_tiles[tile_index].asset_index;
        if (asset_index < 0)
            continue;

        if (world_assets[asset_index].prepared_tick != world_prepare_tick)
        {
            sprite_id = rotating_sprite_pool_request(world_assets[asset_index].item_id);
            vram_cache_request_tile(asset_index,
                                    rotating_sprite_pool_get_wram_ptr(world_assets[asset_index].item_id));
            if (sprite_id < 0)
                sprite_id = world_assets[asset_index].fallback_sprite_id;
            world_assets[asset_index].prepared_sprite_id = sprite_id;
            world_assets[asset_index].prepared_tick = world_prepare_tick;
        }

        sprite_id = world_assets[asset_index].prepared_sprite_id;
        if (sprite_id >= 0 && world_tiles[tile_index].applied_sprite_id != sprite_id)
        {
            jo_map_set_tile_sprite_ext(WORLD_MAP_ID, world_tiles[tile_index].tile_index, sprite_id);
            world_tiles[tile_index].applied_sprite_id = sprite_id;
        }
    }
}

void world_map_load(void)
{
    runtime_log("world_map: deferred reg");
    jo_map_free_ext(WORLD_MAP_ID);
    world_map_reset_streams();
    deferred_load_started = true;
    deferred_load_completed = false;
    deferred_load_step = 0;
    vram_cache_clear();
    rotating_sprite_pool_reset();
    world_map_reset_groups();
    world_map_reset_assets();
    world_map_reset_tiles();
    world_prepare_tick = 0;
}

bool world_map_is_ready(void)
{
    return deferred_load_completed;
}

static int world_map_sanitize_group(int group)
{
    if (group < 0)
        group = 0;
    if (group > 2)
        group = 2;
    return group;
}

int world_map_get_player_start_x(int player_index, int group)
{
    player_index = (player_index < 0) ? 0 : (player_index > 1 ? 1 : player_index);
    group = world_map_sanitize_group(group);
    return world_map_player_start_x[player_index][group];
}

int world_map_get_player_start_y(int player_index, int group)
{
    player_index = (player_index < 0) ? 0 : (player_index > 1 ? 1 : player_index);
    group = world_map_sanitize_group(group);
    return world_map_player_start_y[player_index][group];
}

int world_map_get_bot_start_x(int bot_index, int group)
{
    if (bot_index < 0)
        bot_index = 0;
    if (bot_index > 3)
        bot_index = 3;
    group = world_map_sanitize_group(group);
    return world_map_bot_start_x[bot_index][group];
}

int world_map_get_bot_start_y(int bot_index, int group)
{
    if (bot_index < 0)
        bot_index = 0;
    if (bot_index > 3)
        bot_index = 3;
    group = world_map_sanitize_group(group);
    return world_map_bot_start_y[bot_index][group];
}

int world_map_get_max_tile_bottom(void)
{
    return world_map_max_tile_bottom;
}

int world_map_get_max_tile_top(void)
{
    return world_map_max_tile_top;
}

bool world_map_do_load_step(void)
{
    if (!deferred_load_started || deferred_load_completed)
        return deferred_load_completed;

    if (deferred_load_step == 0)
    {
        runtime_log("world_map: blk.tex -> WRAM");
        if (world_tiles_stream == JO_NULL)
            world_tiles_stream = jo_fs_read_file_in_dir("BLK.TEX", "BLK", JO_NULL);
        if (world_tiles_stream == JO_NULL)
            return false;
        deferred_load_step = 1;
    }

    if (deferred_load_step == 1)
    {
        runtime_log("world_map: parse blk.tex");
        if (!world_map_parse_tiles_manifest(world_tiles_stream))
            return false;
        deferred_load_step = 2;
    }

    if (deferred_load_step == 2)
    {
        if (world_probe_index < world_asset_count)
        {
            runtime_log("world_map: probe %d/%d", world_probe_index + 1, world_asset_count);
            if (!world_map_probe_asset_dimensions(&world_assets[world_probe_index]))
                return false;
            ++world_probe_index;
            return false;
        }
        deferred_load_step = 3;
    }

    if (deferred_load_step == 3)
    {
        runtime_log("world_map: map -> WRAM");
        if (world_map_stream == JO_NULL)
            world_map_stream = jo_fs_read_file_in_dir(world_map_filename, "MAP", JO_NULL);
        if (world_map_stream == JO_NULL)
            return false;
        deferred_load_step = 4;
    }

    if (deferred_load_step == 4)
    {
        runtime_log("world_map: parse map");
        if (!world_map_parse_entries_from_stream(world_map_stream))
            return false;
        if (!world_map_create_pools())
            return false;
        deferred_load_step = 5;
    }

    if (deferred_load_step == 5)
    {
        if (world_prefetch_index < world_asset_count)
        {
            runtime_log("world_map: prefetch %d/%d", world_prefetch_index + 1, world_asset_count);
            if (!rotating_sprite_pool_prefetch(world_assets[world_prefetch_index].item_id))
                return false;
            ++world_prefetch_index;
            return false;
        }
        deferred_load_step = 6;
    }

    if (deferred_load_step == 6)
    {
        runtime_log("world_map: build map");
        if (!world_map_build_runtime_map())
            return false;
        deferred_load_completed = true;
        runtime_log("world_map: map ready");
        deferred_load_step = 7;
        return true;
    }

    return deferred_load_completed;
}