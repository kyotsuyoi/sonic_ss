# Sonic SS

Sonic project for the Sega Saturn using Jo Engine.

Compatible with:

- Emulators (tested on Ymir, Yabause and YabaSanshiro).
- CD-R via a disc drive.
- SAROO.

Available features:

- Battles against bots (4 bots)
- Player vs Player battles (also up to 4 bots)
- Audio test menu
- Joystick mapping menu
- 5 char

Available characters:

- Sonic — OK
- Amy — OK
- Tails — OK
- Knuckles — OK
- Shadow — OK

## Screenshots

### 1. Splash Screen (Press Start)
![Splash Screen](readme_src/spl_scr.png)

### 2. Character Select
![Character Select](readme_src/slc_chr.png)

### 3. Battlefield
![Battlefield](readme_src/btl_fld01.gif)
![Battlefield](readme_src/btl_fld02.gif)


Checklist:

- OK - Knuckles bot misses the range of the aerial attack and the super punch.
- OK - Knuckles incorrectly allows jumping while holding the super punch.
- OK - Reduce the number of Sonic and Amy sprites.
- OK - Fix Tails's punch combo sequence.
- OK - Fix Amy's kick cooldown timing.
- NOK - Camera control.
- NOK - Auto camera that stays at a midpoint between the players.
- NOK - Allow the camera to follow player 1 or player 2.
- OK - Debug menu to adjust stun, knockback, pulse, and damage in-game.
- - OK - Adjust stun, damage and knockback debugging for PvP (currently only works correctly on bots).
- NOK - Sprite DMA: “DMA transfer” (fast way to copy from ROM → WRAM or WRAM → VRAM).
- NOK - Sprite packing: grouping multiple sprites into the same block to reduce overhead.
- NOK - Bot strategy.
- NOK - Equalize Shadow’s class.
- NOK - Knuckles’ charged attack should allow up to 3 hits before being canceled.
- NOK - Sonic, Amy, Tails and Shadow need a unique ability like Knuckles’ charged attack.
- OK - Assign characters to specific locations on the map using the Map Editor (only P1/P2 is currently working).
- OK - Character classes are duplicated in class and character.
- NOK - Check why debug_battle_add_damage doesn’t get called for all IDs.
- NOK - Create spectator mode to watch bots fight.
- OK - Physical hardware (SAROO) became extremely slow, need to find out why (emulator is OK).
- - OK The method jo_clear_background is called in a loop and wipes the entire VRAM.
- OK - Some logs and debug output stay permanently on and need to be disabled by default (causes a bug in jo_print).