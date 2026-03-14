#include <jo/jo.h>
#include "sonic.h"
#include "player.h"
#include "character_registry.h"

#define character_ref player
extern jo_sidescroller_physics_params physics;
#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 40
#define DEFEATED_SPRITE_HEIGHT 32

static bool sonic_loaded = false;
static int sonic_walking_base_id;
static int sonic_running1_base_id;
static int sonic_running2_base_id;
static int sonic_stand_base_id;
static int sonic_punch_base_id;
static int sonic_kick_base_id;
static int sonic_walking_anim_id;
static int sonic_running1_anim_id;
static int sonic_running2_anim_id;
static int sonic_stand_anim_id;
static int sonic_punch_anim_id;
static int sonic_kick_anim_id;
static int sonic_spin_sprite_id;
static int sonic_jump_sprite_id;
static int sonic_damage_sprite_id;
static int sonic_defeated_sprite_id;

static const jo_tile SonicWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicRunning2Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicStandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicKickTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void sonic_reset_animation_lists_except(int active_anim_id)
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
inline void sonic_running_animation_handling(void)
{
    int speed_step;

    if (jo_physics_is_standing(&physics))
    {
        
            jo_reset_sprite_anim(character_ref.running1_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);
            
        if (!jo_is_sprite_anim_stopped(character_ref.walking_anim_id)){
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.stand_sprite_id);
        }else{
            if (jo_is_sprite_anim_stopped(character_ref.stand_sprite_id)){
                jo_start_sprite_anim_loop(character_ref.stand_sprite_id);
            }
            
            if (jo_get_sprite_anim_frame(character_ref.stand_sprite_id) == 0)
                jo_set_sprite_anim_frame_rate(character_ref.stand_sprite_id, 70);
            else
                jo_set_sprite_anim_frame_rate(character_ref.stand_sprite_id, 2);
        }
    }
    else
    {        
        if (character_ref.run == 0)
        {
            jo_reset_sprite_anim(character_ref.running1_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.walking_anim_id))
                jo_start_sprite_anim_loop(character_ref.walking_anim_id);
        }
        else if (character_ref.run == 1)
        {
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.running1_anim_id))
                jo_start_sprite_anim_loop(character_ref.running1_anim_id);
        }
        else if (character_ref.run == 2)
        {
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.running1_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.running2_anim_id))
                jo_start_sprite_anim_loop(character_ref.running2_anim_id);
        }
        
        speed_step = (int)JO_ABS(physics.speed);

        if (speed_step >= 6){
            jo_set_sprite_anim_frame_rate(character_ref.running2_anim_id, 3);
            character_ref.run = 2;
        }else if (speed_step >= 5){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 4);
            character_ref.run = 1;
        }else if (speed_step >= 4){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 5);    
            character_ref.run = 1;  
        }else if (speed_step >= 3){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 6);
            character_ref.run = 1;
        }else if (speed_step >= 2){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 7);
            character_ref.run = 1;
        }else if (speed_step >= 1){
            jo_set_sprite_anim_frame_rate(character_ref.walking_anim_id, 8);
            character_ref.run = 0;
        }else{
            jo_set_sprite_anim_frame_rate(character_ref.walking_anim_id, 9);
            character_ref.run = 0;
        }
    }

    player_update_punch_state_for_character(&character_ref);
    player_update_kick_state_for_character(&character_ref);
}

inline void display_sonic(void)
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
        sonic_reset_animation_lists_except(-1);
        jo_sprite_draw3D_and_rotate2(character_ref.spin_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, character_ref.angle);
        if (character_ref.flip)
            character_ref.angle -= CHARACTER_SPIN_SPEED;
        else
            character_ref.angle += CHARACTER_SPIN_SPEED;
    } else if (character_ref.life <= 0 && sonic_defeated_sprite_id >= 0) {
        sonic_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(sonic_defeated_sprite_id, character_ref.x, character_ref.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    } else if (character_ref.stun_timer > 0 && sonic_damage_sprite_id >= 0) {
        sonic_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(sonic_damage_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.punch || character_ref.punch2){
        sonic_reset_animation_lists_except(character_ref.punch_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.punch_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.kick || character_ref.kick2){
        sonic_reset_animation_lists_except(character_ref.kick_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.kick_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.jump){
        sonic_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(character_ref.jump_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else {
        if (character_ref.walk && character_ref.run == 0)
        {
            sonic_reset_animation_lists_except(character_ref.walking_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.walking_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 1)
        {
            sonic_reset_animation_lists_except(character_ref.running1_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running1_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 2)
        {
            sonic_reset_animation_lists_except(character_ref.running2_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running2_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else
        {
            sonic_reset_animation_lists_except(character_ref.stand_sprite_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.stand_sprite_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);        
        }
    }

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();

    // Barra de vidas textual
    int life_percent = (character_ref.life * 100) / 50;
    int bar_max_width = 20; // 20 caracteres
    int bar_width = (life_percent * bar_max_width) / 100;
    char bar[bar_max_width + 1];
    for (int i = 0; i < bar_max_width; ++i)
        bar[i] = (i < bar_width) ? '#' : '-';
    bar[bar_max_width] = '\0';
    // jo_printf(1, 26, "P1 : [%s] %d%%", bar, life_percent); // debug life bar (temporário)
}

void load_sonic(void)
{
    if (!sonic_loaded)
    {
        sonic_walking_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_WLK.TGA", JO_COLOR_Green, SonicWalkingTiles, JO_TILE_COUNT(SonicWalkingTiles));

        sonic_walking_anim_id = jo_create_sprite_anim(sonic_walking_base_id, JO_TILE_COUNT(SonicWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);

        sonic_running1_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_RUN1.TGA", JO_COLOR_Green, SonicRunning1Tiles, JO_TILE_COUNT(SonicRunning1Tiles));
        sonic_running1_anim_id = jo_create_sprite_anim(sonic_running1_base_id, JO_TILE_COUNT(SonicRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        sonic_running2_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_RUN2.TGA", JO_COLOR_Green, SonicRunning2Tiles, JO_TILE_COUNT(SonicRunning2Tiles));
        sonic_running2_anim_id = jo_create_sprite_anim(sonic_running2_base_id, JO_TILE_COUNT(SonicRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        sonic_stand_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_STD.TGA", JO_COLOR_Green, SonicStandTiles, JO_TILE_COUNT(SonicStandTiles));
        sonic_stand_anim_id = jo_create_sprite_anim(sonic_stand_base_id, JO_TILE_COUNT(SonicStandTiles), DEFAULT_SPRITE_FRAME_DURATION);

        sonic_spin_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "SNC_SPN.TGA", JO_COLOR_Green);
        sonic_jump_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "SNC_JMP.TGA", JO_COLOR_Green);
        sonic_damage_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "SNC_DMG.TGA", JO_COLOR_Green);
        sonic_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_DFT.TGA", JO_COLOR_Green, SonicDefeatedTile, JO_TILE_COUNT(SonicDefeatedTile));

        sonic_punch_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_PNC.TGA", JO_COLOR_Green, SonicPunchTiles, JO_TILE_COUNT(SonicPunchTiles));
        sonic_punch_anim_id = jo_create_sprite_anim(sonic_punch_base_id, JO_TILE_COUNT(SonicPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);

        sonic_kick_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_KCK.TGA", JO_COLOR_Green, SonicKickTiles, JO_TILE_COUNT(SonicKickTiles));
        sonic_kick_anim_id = jo_create_sprite_anim(sonic_kick_base_id, JO_TILE_COUNT(SonicKickTiles), DEFAULT_SPRITE_FRAME_DURATION);

        sonic_loaded = true;
    }

    character_ref.walking_anim_id = sonic_walking_anim_id;
    character_ref.running1_anim_id = sonic_running1_anim_id;
    character_ref.running2_anim_id = sonic_running2_anim_id;
    character_ref.stand_sprite_id = sonic_stand_anim_id;
    character_ref.spin_sprite_id = sonic_spin_sprite_id;
    character_ref.jump_sprite_id = sonic_jump_sprite_id;
    character_ref.damage_sprite_id = sonic_damage_sprite_id;
    character_ref.defeated_sprite_id = sonic_defeated_sprite_id;
    character_ref.punch_anim_id = sonic_punch_anim_id;
    character_ref.kick_anim_id = sonic_kick_anim_id;
    character_registry_apply_combat_profile(&character_ref, UiCharacterSonic);
    character_ref.charged_kick_hold_ms = 0;
    character_ref.charged_kick_ready = false;
    character_ref.charged_kick_active = false;
    character_ref.charged_kick_phase = 0;
    character_ref.charged_kick_phase_timer = 0;
    character_ref.hit_done_punch1 = false;
    character_ref.hit_done_punch2 = false;
    character_ref.hit_done_kick1 = false;
    character_ref.hit_done_kick2 = false;
    character_ref.attack_cooldown = 0;

    // Inicializa vida
    character_ref.life = 50;
}

void unload_sonic(void)
{
    if (!sonic_loaded)
        return;

    if (sonic_walking_anim_id >= 0) jo_remove_sprite_anim(sonic_walking_anim_id);
    if (sonic_running1_anim_id >= 0) jo_remove_sprite_anim(sonic_running1_anim_id);
    if (sonic_running2_anim_id >= 0) jo_remove_sprite_anim(sonic_running2_anim_id);
    if (sonic_stand_anim_id >= 0) jo_remove_sprite_anim(sonic_stand_anim_id);
    if (sonic_punch_anim_id >= 0) jo_remove_sprite_anim(sonic_punch_anim_id);
    if (sonic_kick_anim_id >= 0) jo_remove_sprite_anim(sonic_kick_anim_id);

    sonic_walking_base_id = -1;
    sonic_running1_base_id = -1;
    sonic_running2_base_id = -1;
    sonic_stand_base_id = -1;
    sonic_punch_base_id = -1;
    sonic_kick_base_id = -1;
    sonic_walking_anim_id = -1;
    sonic_running1_anim_id = -1;
    sonic_running2_anim_id = -1;
    sonic_stand_anim_id = -1;
    sonic_punch_anim_id = -1;
    sonic_kick_anim_id = -1;
    sonic_spin_sprite_id = -1;
    sonic_jump_sprite_id = -1;
    sonic_damage_sprite_id = -1;
    sonic_defeated_sprite_id = -1;
    sonic_loaded = false;
}

/*
** END OF FILE
*/
