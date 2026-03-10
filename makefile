JO_COMPILE_WITH_VIDEO_MODULE = 0
JO_COMPILE_WITH_BACKUP_MODULE = 0
JO_COMPILE_WITH_TGA_MODULE = 1
JO_COMPILE_WITH_AUDIO_MODULE = 1
JO_COMPILE_WITH_3D_MODULE = 0
JO_COMPILE_WITH_PSEUDO_MODE7_MODULE = 0
JO_COMPILE_WITH_EFFECTS_MODULE = 0
JO_DEBUG = 1
JO_COMPILE_USING_SGL=1
JO_MAX_SPRITE_ANIM=64
SRCS=main.c src/class/player.c src/class/sonic.c src/class/amy.c src/class/tails.c src/class/knuckles.c src/class/shadow.c src/class/character_registry.c src/class/bot.c src/class/control.c src/class/input_mapping.c src/class/ai_control.c src/class/ui_control.c src/class/game_flow.c src/class/game_loop.c src/class/world_collision.c src/class/world_camera.c src/class/world_background.c src/class/world_map.c src/class/world_physics.c src/class/game_audio.c src/class/debug.c src/class/damage_fx.c src/class/ram_cart.c src/jo_audio_ext/jo_audio_ext.c
OBJDIR=build
override OBJS = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))
JO_ENGINE_SRC_DIR=../../jo_engine
COMPILER_DIR=../../Compiler
include $(COMPILER_DIR)/COMMON/jo_engine_makefile

# Include headers moved into src/class plus jo_audio_ext in src/
CCFLAGS += -Isrc/class -Isrc

VPATH += $(sort $(dir $(SRCS)))

$(OBJDIR)/%.o: %.c
	$(CC) $< $(DFLAGS) $(CCFLAGS) $(_CCFLAGS) -o $@


