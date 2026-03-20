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

## v0.1.X

### 1. Splash Screen (Press Start)
![Splash Screen](readme_src/spl_scr.png)

### 2. Character Select
![Character Select](readme_src/slc_chr.png)

### 3. Battlefield
![Battlefield](readme_src/btl_fld01.gif)
![Battlefield](readme_src/btl_fld02.gif)

### 4. Checklist (Road to v0.2) (60%)

- ✅ Knuckles bot misses the range of the aerial attack and the super punch.
- ✅ Knuckles incorrectly allows jumping while holding the super punch.
- ✅ Reduce the number of Sonic and Amy sprites.
- ✅ Fix Tails's punch combo sequence.
- ✅ Fix Amy's kick cooldown timing.
- ✅ Assign characters to specific locations on the map using the Map Editor (only P1/P2 is currently working).
- ✅ Character classes are duplicated in class and character.
- ✅ Some logs and debug output stay permanently on and need to be disabled by default (causes a bug in jo_print).

- ✅ Physical hardware (SAROO) became extremely slow, need to find out why (emulator is OK).
- - ✅ The method jo_clear_background is called in a loop and wipes the entire VRAM.

- ✅ Camera control.
- ⬜ Auto camera that stays at a midpoint between the players.
- ✅ Allow the camera to follow player 1 or player 2.

- ✅ Debug menu to adjust stun, knockback, pulse, and damage in-game.
- - ✅ Adjust stun, damage and knockback debugging for PvP (currently only works correctly on bots).

- ⬜ Sprite DMA: “DMA transfer” (fast way to copy from ROM → WRAM or WRAM → VRAM).
- - ⬜ Sprite packing: grouping multiple sprites into the same block to reduce overhead.
- - ✅ Sonic sprites -> Players (100%) - Bots (100%).
- - ✅ Amy sprites -> Players (100%) - Bots (100%).
- - ⬜ Tails sprites -> Players (50%) - Bots (50%).
- - ⬜ Knuckles sprites.
- - ⬜ Shadow sprites.
- - ⬜ All - Use vram_cache + rotating_sprite_pool to avoid excessive CPU usage during (WRAM → VRAM).

- ⬜ Bot strategy (25%).
- ✅ Make the bots follow the same sprite control flow as the players.
- ⬜ Knuckles’ charged attack should allow up to 3 hits before being canceled.
- ⬜ Sonic, Amy, Tails and Shadow need a unique ability like Knuckles’ charged attack.
- ✅ Check why debug_battle_add_damage doesn’t get called for all IDs.
- ⬜ Create spectator mode to watch bots fight.

- ✅ Problem with tiles that suddenly change; this started happening after implementing FREE CAM.
