#include <jo/jo.h>
#include "sonic.h"
#include "knuckles.h"

#define character_ref player
extern jo_sidescroller_physics_params physics;
#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 40
#define DEFEATED_SPRITE_HEIGHT 32
#define KNUCKLES_HIT_RANGE_PUNCH1 10
#define KNUCKLES_HIT_RANGE_PUNCH2 11
#define KNUCKLES_HIT_RANGE_KICK1 13
#define KNUCKLES_HIT_RANGE_KICK2 18
#define KNUCKLES_ATTACK_FORWARD_IMPULSE_LIGHT 0.85f
#define KNUCKLES_ATTACK_FORWARD_IMPULSE_HEAVY 1.75f
#define KNUCKLES_KNOCKBACK_PUNCH1 1.8f
#define KNUCKLES_KNOCKBACK_PUNCH2 2.3f
#define KNUCKLES_KNOCKBACK_KICK1 2.4f
#define KNUCKLES_KNOCKBACK_KICK2 3.6f
#define KNUCKLES_FRAME_COUNT 4
#define KNUCKLES_COMBO2_START_FRAME 2
#define KNUCKLES_CHARGED_KICK_PHASE1_FRAMES 6
#define KNUCKLES_CHARGED_KICK_PHASE2_FRAMES 10
#define KNUCKLES_KICK_PART3_INDEX 3
#define KNUCKLES_KICK_PART4_INDEX 2
#define KNUCKLES_KICK_PART3_WIDTH_PIXELS CHARACTER_WIDTH

static bool knuckles_loaded = false;
static int knuckles_walking_base_id;
static int knuckles_running1_base_id;
static int knuckles_running2_base_id;
static int knuckles_stand_base_id;
static int knuckles_punch_base_id;
static int knuckles_kick_base_id;
static int knuckles_walking_anim_id;
static int knuckles_running1_anim_id;
static int knuckles_running2_anim_id;
static int knuckles_stand_anim_id;
static int knuckles_punch_anim_id;
static int knuckles_kick_anim_id;
static int knuckles_spin_sprite_id;
static int knuckles_jump_sprite_id;
static int knuckles_damage_sprite_id;
static int knuckles_defeated_sprite_id;

static const jo_tile KnucklesWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesRunning2Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesStandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesKickTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void knuckles_reset_animation_lists_except(int active_anim_id)
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

inline void knuckles_running_animation_handling(void)
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

    // Punch handling: 2-stage punch (simplificado)
    if (character_ref.punch) {
        int anim_frame = jo_get_sprite_anim_frame(character_ref.punch_anim_id);
        if (anim_frame > (KNUCKLES_FRAME_COUNT - 1) || jo_is_sprite_anim_stopped(character_ref.punch_anim_id)) {
            jo_set_sprite_anim_frame(character_ref.punch_anim_id, 0);
            jo_start_sprite_anim(character_ref.punch_anim_id);
        }
        if (anim_frame >= KNUCKLES_COMBO2_START_FRAME) {
            if (character_ref.punch2_requested) {
                character_ref.punch = false;
                character_ref.punch2 = true;
                character_ref.punch2_requested = false;
                character_ref.perform_punch2 = true;
                jo_set_sprite_anim_frame(character_ref.punch_anim_id, KNUCKLES_COMBO2_START_FRAME);
                jo_start_sprite_anim(character_ref.punch_anim_id);
            } else if (anim_frame >= (KNUCKLES_FRAME_COUNT - 1)) {
                character_ref.punch = false;
                character_ref.attack_cooldown = ATTACK_COOLDOWN_FRAMES;
                jo_reset_sprite_anim(character_ref.punch_anim_id);
            }
        }
    } else if (character_ref.punch2) {
        int anim_frame = jo_get_sprite_anim_frame(character_ref.punch_anim_id);
        if (anim_frame < KNUCKLES_COMBO2_START_FRAME) {
            jo_set_sprite_anim_frame(character_ref.punch_anim_id, KNUCKLES_COMBO2_START_FRAME);
            jo_start_sprite_anim(character_ref.punch_anim_id);
        }
        if (anim_frame >= (KNUCKLES_FRAME_COUNT - 1) && jo_is_sprite_anim_stopped(character_ref.punch_anim_id)) {
            character_ref.punch2 = false;
            character_ref.attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
            jo_reset_sprite_anim(character_ref.punch_anim_id);
        }
    }

    if (character_ref.charged_kick_enabled && character_ref.kick && !character_ref.kick2)
    {
        jo_set_sprite_anim_frame(character_ref.kick_anim_id, 0);
        jo_start_sprite_anim(character_ref.kick_anim_id);
    }
    else if (character_ref.charged_kick_enabled && character_ref.kick2 && character_ref.charged_kick_active)
    {
        character_ref.charged_kick_phase_timer++;

        if (character_ref.charged_kick_phase == 1)
        {
            jo_set_sprite_anim_frame(character_ref.kick_anim_id, 1);
            jo_start_sprite_anim(character_ref.kick_anim_id);
            if (character_ref.charged_kick_phase_timer >= KNUCKLES_CHARGED_KICK_PHASE1_FRAMES)
            {
                character_ref.charged_kick_phase = 2;
                character_ref.charged_kick_phase_timer = 0;
            }
        }
        else
        {
            jo_set_sprite_anim_frame(character_ref.kick_anim_id, 2);
            jo_start_sprite_anim(character_ref.kick_anim_id);
            if (character_ref.charged_kick_phase_timer >= KNUCKLES_CHARGED_KICK_PHASE2_FRAMES)
            {
                character_ref.kick2 = false;
                character_ref.charged_kick_active = false;
                character_ref.charged_kick_ready = false;
                character_ref.charged_kick_hold_ms = 0;
                character_ref.charged_kick_phase = 0;
                character_ref.charged_kick_phase_timer = 0;
                character_ref.attack_cooldown = ATTACK_COOLDOWN_KICK2_FRAMES;
                jo_reset_sprite_anim(character_ref.kick_anim_id);
            }
        }
    }
    else if(character_ref.kick){
        /* Knuckles ground kick has no combo path; only charged release enters kick2. */
        jo_set_sprite_anim_frame(character_ref.kick_anim_id, 0);
        jo_start_sprite_anim(character_ref.kick_anim_id);
        character_ref.kick2_requested = false;
    } else if (character_ref.kick2 && !character_ref.charged_kick_active && physics.is_in_air) {
        int anim_frame = jo_get_sprite_anim_frame(character_ref.kick_anim_id);
        if (anim_frame < KNUCKLES_COMBO2_START_FRAME) {
            jo_set_sprite_anim_frame(character_ref.kick_anim_id, KNUCKLES_COMBO2_START_FRAME);
            jo_start_sprite_anim(character_ref.kick_anim_id);
        }
        if (anim_frame >= (KNUCKLES_FRAME_COUNT - 1) && jo_is_sprite_anim_stopped(character_ref.kick_anim_id)) {
            character_ref.kick2 = false;
            character_ref.attack_cooldown = ATTACK_COOLDOWN_KICK2_FRAMES;
            jo_reset_sprite_anim(character_ref.kick_anim_id);
        }
    } else if (character_ref.kick2) {
        int anim_frame = jo_get_sprite_anim_frame(character_ref.kick_anim_id);
        if (anim_frame < KNUCKLES_COMBO2_START_FRAME) {
            jo_set_sprite_anim_frame(character_ref.kick_anim_id, KNUCKLES_COMBO2_START_FRAME);
            jo_start_sprite_anim(character_ref.kick_anim_id);
        }
        if (anim_frame >= (KNUCKLES_FRAME_COUNT - 1) && jo_is_sprite_anim_stopped(character_ref.kick_anim_id)) {
            character_ref.kick2 = false;
            character_ref.attack_cooldown = ATTACK_COOLDOWN_KICK2_FRAMES;
            jo_reset_sprite_anim(character_ref.kick_anim_id);
        }
    }
}

inline void display_knuckles(void)
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
        knuckles_reset_animation_lists_except(-1);
        jo_sprite_draw3D_and_rotate2(character_ref.spin_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, character_ref.angle);
        if (character_ref.flip)
            character_ref.angle -= CHARACTER_SPIN_SPEED;
        else
            character_ref.angle += CHARACTER_SPIN_SPEED;
    } else if (character_ref.life <= 0 && knuckles_defeated_sprite_id >= 0) {
        knuckles_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(knuckles_defeated_sprite_id, character_ref.x, character_ref.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    } else if (character_ref.stun_timer > 0 && knuckles_damage_sprite_id >= 0) {
        knuckles_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(knuckles_damage_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.punch || character_ref.punch2){
        knuckles_reset_animation_lists_except(character_ref.punch_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.punch_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.kick || character_ref.kick2){
        knuckles_reset_animation_lists_except(character_ref.kick_anim_id);
        if (character_ref.charged_kick_enabled && character_ref.kick && !character_ref.kick2)
        {
            jo_sprite_draw3D2(knuckles_kick_base_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.charged_kick_enabled && character_ref.kick2 && character_ref.charged_kick_active)
        {
            int front_offset = character_ref.flip ? -KNUCKLES_KICK_PART3_WIDTH_PIXELS : KNUCKLES_KICK_PART3_WIDTH_PIXELS;
            if (character_ref.charged_kick_phase <= 1)
                jo_sprite_draw3D2(knuckles_kick_base_id + 1, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
            else
            {
                jo_sprite_draw3D2(knuckles_kick_base_id + KNUCKLES_KICK_PART4_INDEX, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
                jo_sprite_draw3D2(knuckles_kick_base_id + KNUCKLES_KICK_PART3_INDEX,
                                  character_ref.x + front_offset,
                                  character_ref.y,
                                  CHARACTER_SPRITE_Z);
            }
        }
        else if (character_ref.charged_kick_enabled && character_ref.kick2 && physics.is_in_air)
        {
            int front_offset = character_ref.flip ? -KNUCKLES_KICK_PART3_WIDTH_PIXELS : KNUCKLES_KICK_PART3_WIDTH_PIXELS;
            jo_sprite_draw3D2(knuckles_kick_base_id + KNUCKLES_KICK_PART4_INDEX, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
            jo_sprite_draw3D2(knuckles_kick_base_id + KNUCKLES_KICK_PART3_INDEX,
                              character_ref.x + front_offset,
                              character_ref.y,
                              CHARACTER_SPRITE_Z);
        }
        else
        {
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.kick_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
    } else if (character_ref.jump){
        knuckles_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(character_ref.jump_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else {
        if (character_ref.walk && character_ref.run == 0)
        {
            knuckles_reset_animation_lists_except(character_ref.walking_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.walking_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 1)
        {
            knuckles_reset_animation_lists_except(character_ref.running1_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running1_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 2)
        {
            knuckles_reset_animation_lists_except(character_ref.running2_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running2_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else
        {
            knuckles_reset_animation_lists_except(character_ref.stand_sprite_id);
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
    jo_printf(1, 26, "P1 : [%s] %d%%", bar, life_percent);
}

void load_knuckles(void)
{
    if (!knuckles_loaded)
    {
        knuckles_walking_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_WLK.TGA", JO_COLOR_Green, KnucklesWalkingTiles, JO_TILE_COUNT(KnucklesWalkingTiles));
        knuckles_walking_anim_id = jo_create_sprite_anim(knuckles_walking_base_id, JO_TILE_COUNT(KnucklesWalkingTiles), 4);

        knuckles_running1_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_RUN1.TGA", JO_COLOR_Green, KnucklesRunning1Tiles, JO_TILE_COUNT(KnucklesRunning1Tiles));
        knuckles_running1_anim_id = jo_create_sprite_anim(knuckles_running1_base_id, JO_TILE_COUNT(KnucklesRunning1Tiles), 4);

        knuckles_running2_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_RUN1.TGA", JO_COLOR_Green, KnucklesRunning2Tiles, JO_TILE_COUNT(KnucklesRunning2Tiles));
        knuckles_running2_anim_id = jo_create_sprite_anim(knuckles_running2_base_id, JO_TILE_COUNT(KnucklesRunning2Tiles), 4);

        knuckles_stand_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_STD.TGA", JO_COLOR_Green, KnucklesStandTiles, JO_TILE_COUNT(KnucklesStandTiles));
        knuckles_stand_anim_id = jo_create_sprite_anim(knuckles_stand_base_id, JO_TILE_COUNT(KnucklesStandTiles), 4);

        knuckles_spin_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "KNK_SPN.TGA", JO_COLOR_Green);
        knuckles_jump_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "KNK_JMP.TGA", JO_COLOR_Green);
        knuckles_damage_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "KNK_DMG.TGA", JO_COLOR_Green);
        knuckles_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_DFT.TGA", JO_COLOR_Green, KnucklesDefeatedTile, JO_TILE_COUNT(KnucklesDefeatedTile));

        knuckles_punch_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_PNC.TGA", JO_COLOR_Green, KnucklesPunchTiles, JO_TILE_COUNT(KnucklesPunchTiles));
        knuckles_punch_anim_id = jo_create_sprite_anim(knuckles_punch_base_id, JO_TILE_COUNT(KnucklesPunchTiles), 4);

        knuckles_kick_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_KCK.TGA", JO_COLOR_Green, KnucklesKickTiles, JO_TILE_COUNT(KnucklesKickTiles));
        knuckles_kick_anim_id = jo_create_sprite_anim(knuckles_kick_base_id, JO_TILE_COUNT(KnucklesKickTiles), 4);

        knuckles_loaded = true;
    }

    character_ref.walking_anim_id = knuckles_walking_anim_id;
    character_ref.running1_anim_id = knuckles_running1_anim_id;
    character_ref.running2_anim_id = knuckles_running2_anim_id;
    character_ref.stand_sprite_id = knuckles_stand_anim_id;
    character_ref.spin_sprite_id = knuckles_spin_sprite_id;
    character_ref.jump_sprite_id = knuckles_jump_sprite_id;
    character_ref.damage_sprite_id = knuckles_damage_sprite_id;
    character_ref.defeated_sprite_id = knuckles_defeated_sprite_id;
    character_ref.punch_anim_id = knuckles_punch_anim_id;
    character_ref.kick_anim_id = knuckles_kick_anim_id;
    character_ref.hit_range_punch1 = KNUCKLES_HIT_RANGE_PUNCH1;
    character_ref.hit_range_punch2 = KNUCKLES_HIT_RANGE_PUNCH2;
    character_ref.hit_range_kick1 = KNUCKLES_HIT_RANGE_KICK1;
    character_ref.hit_range_kick2 = KNUCKLES_HIT_RANGE_KICK2;
    character_ref.attack_forward_impulse_light = KNUCKLES_ATTACK_FORWARD_IMPULSE_LIGHT;
    character_ref.attack_forward_impulse_heavy = KNUCKLES_ATTACK_FORWARD_IMPULSE_HEAVY;
    character_ref.knockback_punch1 = KNUCKLES_KNOCKBACK_PUNCH1;
    character_ref.knockback_punch2 = KNUCKLES_KNOCKBACK_PUNCH2;
    character_ref.knockback_kick1 = KNUCKLES_KNOCKBACK_KICK1;
    character_ref.knockback_kick2 = KNUCKLES_KNOCKBACK_KICK2;
    character_ref.charged_kick_enabled = true;
    character_ref.charged_kick_hold_ms = 0;
    character_ref.charged_kick_ready = false;
    character_ref.charged_kick_active = false;
    character_ref.charged_kick_phase = 0;
    character_ref.charged_kick_phase_timer = 0;
    character_ref.character_id = CHARACTER_ID_KNUCKLES;
    character_ref.hit_done_punch1 = false;
    character_ref.hit_done_punch2 = false;
    character_ref.hit_done_kick1 = false;
    character_ref.hit_done_kick2 = false;
    character_ref.attack_cooldown = 0;

    // Inicializa vida
    character_ref.life = 50;
}

void unload_knuckles(void)
{
    if (!knuckles_loaded)
        return;

    if (knuckles_walking_anim_id >= 0) jo_remove_sprite_anim(knuckles_walking_anim_id);
    if (knuckles_running1_anim_id >= 0) jo_remove_sprite_anim(knuckles_running1_anim_id);
    if (knuckles_running2_anim_id >= 0) jo_remove_sprite_anim(knuckles_running2_anim_id);
    if (knuckles_stand_anim_id >= 0) jo_remove_sprite_anim(knuckles_stand_anim_id);
    if (knuckles_punch_anim_id >= 0) jo_remove_sprite_anim(knuckles_punch_anim_id);
    if (knuckles_kick_anim_id >= 0) jo_remove_sprite_anim(knuckles_kick_anim_id);

    knuckles_walking_base_id = -1;
    knuckles_running1_base_id = -1;
    knuckles_running2_base_id = -1;
    knuckles_stand_base_id = -1;
    knuckles_punch_base_id = -1;
    knuckles_kick_base_id = -1;
    knuckles_walking_anim_id = -1;
    knuckles_running1_anim_id = -1;
    knuckles_running2_anim_id = -1;
    knuckles_stand_anim_id = -1;
    knuckles_punch_anim_id = -1;
    knuckles_kick_anim_id = -1;
    knuckles_spin_sprite_id = -1;
    knuckles_jump_sprite_id = -1;
    knuckles_damage_sprite_id = -1;
    knuckles_defeated_sprite_id = -1;
    knuckles_loaded = false;
}

/*
** END OF FILE
*/

