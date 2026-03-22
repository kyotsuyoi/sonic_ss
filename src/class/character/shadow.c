#include <jo/jo.h>
#include "shadow.h"
#include "sonic.h"
#include "player.h"
#include "game_constants.h"
#include "character_registry.h"
#include "character_common.h"

extern jo_sidescroller_physics_params physics;

static character_t *shadow_ref = &player;
static jo_sidescroller_physics_params *shadow_physics = &physics;

#define character_ref (*shadow_ref)
#define physics (*shadow_physics)

void shadow_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != NULL)
        shadow_ref = chr;
    if (phy != NULL)
        shadow_physics = phy;
}

#define SPRITE_DIR "SPT"

/*
Speed -> Animation mapping:
- `speed_step = (int)JO_ABS(physics.speed)` converts the physics speed to a discrete level.
- The code maps speed ranges to an animation variant and a sprite frame rate:
    * `speed_step >= 6`: use `running2` with `frame_rate = 3` (fast)
    * `speed_step >= 5`: use `running1` with `frame_rate = 4`
    * `speed_step >= 4`: use `running1` with `frame_rate = 5`
    * `speed_step >= 3`: use `running1` with `frame_rate = DEFAULT_SPRITE_FRAME_DURATION` (medium)
    * `speed_step >= 2`: use `running1` with `frame_rate = 7`
    * `speed_step >= 1`: use `walking` with `frame_rate = 8`
    * `else`: use `walking` with `frame_rate = 9`
- Lower `frame_rate` values advance animation frames faster (so `3` animates quicker than `9`).
- `run` selects which animation variant is drawn (0 = walking, 1 = running1, 2 = running2).
This mapping keeps the visual animation speed consistent with the character's physical speed.
*/
static bool shadow_loaded = false;
static int shadow_walking_base_id;
static int shadow_running1_base_id;
static int shadow_running2_base_id;
static int shadow_stand_base_id;
static int shadow_punch_base_id;
static int shadow_kick_base_id;
static int shadow_walking_anim_id;
static int shadow_running1_anim_id;
static int shadow_running2_anim_id;
static int shadow_stand_anim_id;
static int shadow_punch_anim_id;
static int shadow_kick_anim_id;
static int shadow_spin_sprite_id;
static int shadow_jump_sprite_id;
static int shadow_damage_sprite_id;
static int shadow_defeated_sprite_id;

static const jo_tile ShadowWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile ShadowRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile ShadowRunning2Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile ShadowStandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile ShadowPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile ShadowKickTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile ShadowDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void shadow_reset_animation_lists_except(int active_anim_id)
{
    if (character_ref.walking_anim_id >= 0 && character_ref.walking_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.walking_anim_id);
    if (character_ref.running1_anim_id >= 0 && character_ref.running1_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.running1_anim_id);
    if (character_ref.running2_anim_id >= 0 && character_ref.running2_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.running2_anim_id);
    if (character_ref.stand_sprite_id >= 0 && character_ref.stand_sprite_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.stand_sprite_id);
    if (character_ref.punch_anim_id >= 0 && character_ref.punch_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.punch_anim_id);
    if (character_ref.kick_anim_id >= 0 && character_ref.kick_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.kick_anim_id);
}

void shadow_running_animation_handling(void)
{
    character_running_animation_handling(&character_ref, &physics);
}

void display_shadow(void)
{
    if (!physics.is_in_air)
    {
        character_ref.spin = false;
        character_ref.jump = false;
        character_ref.angle = 0;
    }

    if (character_ref.flip)
        jo_sprite_enable_horizontal_flip();

    if (character_ref.spin)
    {
        shadow_reset_animation_lists_except(-1);
        jo_sprite_draw3D_and_rotate2(shadow_spin_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, character_ref.angle);
        if (character_ref.flip)
            character_ref.angle -= CHARACTER_SPIN_SPEED;
        else
            character_ref.angle += CHARACTER_SPIN_SPEED;
    }
    else if (character_ref.life <= 0 && shadow_defeated_sprite_id >= 0)
    {
        shadow_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(shadow_defeated_sprite_id, character_ref.x, character_ref.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    }
    else if (character_ref.stun_timer > 0 && shadow_damage_sprite_id >= 0)
    {
        shadow_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(shadow_damage_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else if (character_ref.punch || character_ref.punch2)
    {
        shadow_reset_animation_lists_except(character_ref.punch_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.punch_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else if (character_ref.kick || character_ref.kick2)
    {
        shadow_reset_animation_lists_except(character_ref.kick_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.kick_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else if (character_ref.jump)
    {
        shadow_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(character_ref.jump_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else
    {
        if (character_ref.walk && character_ref.run == 0)
        {
            shadow_reset_animation_lists_except(character_ref.walking_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.walking_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 1)
        {
            shadow_reset_animation_lists_except(character_ref.running1_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running1_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 2)
        {
            shadow_reset_animation_lists_except(character_ref.running2_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running2_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else
        {
            shadow_reset_animation_lists_except(character_ref.stand_sprite_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.stand_sprite_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
    }

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();
}

void shadow_draw(character_t *chr)
{
    if (chr == JO_NULL)
        return;

    shadow_set_current(chr, NULL);
    display_shadow();
}

void load_shadow(void)
{
    if (!shadow_loaded)
    {
        shadow_walking_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_WLK.TGA", JO_COLOR_Green, ShadowWalkingTiles, JO_TILE_COUNT(ShadowWalkingTiles));
        shadow_walking_anim_id = jo_create_sprite_anim(shadow_walking_base_id, JO_TILE_COUNT(ShadowWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);

        shadow_running1_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_RUN1.TGA", JO_COLOR_Green, ShadowRunning1Tiles, JO_TILE_COUNT(ShadowRunning1Tiles));
        shadow_running1_anim_id = jo_create_sprite_anim(shadow_running1_base_id, JO_TILE_COUNT(ShadowRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        shadow_running2_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_RUN2.TGA", JO_COLOR_Green, ShadowRunning2Tiles, JO_TILE_COUNT(ShadowRunning2Tiles));
        shadow_running2_anim_id = jo_create_sprite_anim(shadow_running2_base_id, JO_TILE_COUNT(ShadowRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        shadow_stand_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_STD.TGA", JO_COLOR_Green, ShadowStandTiles, JO_TILE_COUNT(ShadowStandTiles));
        shadow_stand_anim_id = jo_create_sprite_anim(shadow_stand_base_id, JO_TILE_COUNT(ShadowStandTiles), DEFAULT_SPRITE_FRAME_DURATION);

        shadow_spin_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "SDW_SPN.TGA", JO_COLOR_Green);
        shadow_jump_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "SDW_JMP.TGA", JO_COLOR_Green);
        shadow_damage_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "SDW_DMG.TGA", JO_COLOR_Green);
        shadow_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_DFT.TGA", JO_COLOR_Green, ShadowDefeatedTile, JO_TILE_COUNT(ShadowDefeatedTile));

        shadow_punch_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_PNC.TGA", JO_COLOR_Green, ShadowPunchTiles, JO_TILE_COUNT(ShadowPunchTiles));
        shadow_punch_anim_id = jo_create_sprite_anim(shadow_punch_base_id, JO_TILE_COUNT(ShadowPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);

        shadow_kick_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SDW_KCK.TGA", JO_COLOR_Green, ShadowKickTiles, JO_TILE_COUNT(ShadowKickTiles));
        shadow_kick_anim_id = jo_create_sprite_anim(shadow_kick_base_id, JO_TILE_COUNT(ShadowKickTiles), DEFAULT_SPRITE_FRAME_DURATION);

        shadow_loaded = true;
    }

    player.walking_anim_id = shadow_walking_anim_id;
    player.running1_anim_id = shadow_running1_anim_id;
    player.running2_anim_id = shadow_running2_anim_id;
    player.stand_sprite_id = shadow_stand_anim_id;
    player.spin_sprite_id = shadow_spin_sprite_id;
    player.jump_sprite_id = shadow_jump_sprite_id;
    player.damage_sprite_id = shadow_damage_sprite_id;
    player.defeated_sprite_id = shadow_defeated_sprite_id;
    player.punch_anim_id = shadow_punch_anim_id;
    player.kick_anim_id = shadow_kick_anim_id;

    character_registry_apply_combat_profile(&player, UiCharacterShadow);
    player.charged_kick_hold_ms = 0;
    player.charged_kick_ready = false;
    player.charged_kick_active = false;
    player.charged_kick_phase = 0;
    player.charged_kick_phase_timer = 0;
    player.hit_done_punch1 = false;
    player.hit_done_punch2 = false;
    player.hit_done_kick1 = false;
    player.hit_done_kick2 = false;
    player.attack_cooldown = 0;

    /* Inicializa vida */
    player.life = 50;
}

void unload_shadow(void)
{
    if (!shadow_loaded)
        return;

    if (shadow_walking_anim_id >= 0) jo_remove_sprite_anim(shadow_walking_anim_id);
    if (shadow_running1_anim_id >= 0) jo_remove_sprite_anim(shadow_running1_anim_id);
    if (shadow_running2_anim_id >= 0) jo_remove_sprite_anim(shadow_running2_anim_id);
    if (shadow_stand_anim_id >= 0) jo_remove_sprite_anim(shadow_stand_anim_id);
    if (shadow_punch_anim_id >= 0) jo_remove_sprite_anim(shadow_punch_anim_id);
    if (shadow_kick_anim_id >= 0) jo_remove_sprite_anim(shadow_kick_anim_id);

    shadow_walking_base_id = -1;
    shadow_running1_base_id = -1;
    shadow_running2_base_id = -1;
    shadow_stand_base_id = -1;
    shadow_punch_base_id = -1;
    shadow_kick_base_id = -1;
    shadow_walking_anim_id = -1;
    shadow_running1_anim_id = -1;
    shadow_running2_anim_id = -1;
    shadow_stand_anim_id = -1;
    shadow_punch_anim_id = -1;
    shadow_kick_anim_id = -1;
    shadow_spin_sprite_id = -1;
    shadow_jump_sprite_id = -1;
    shadow_damage_sprite_id = -1;
    shadow_defeated_sprite_id = -1;
    shadow_loaded = false;
}
