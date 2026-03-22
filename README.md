# Sonic SS

Sonic project for the Sega Saturn using Jo Engine.

Compatible with:

- Emulators (tested on Ymir, Yabause and YabaSanshiro).
- CD-R via a disc drive.
- SAROO.
- Needs 4MB Cart RAM.

Available features:

- Battles against bots (4 bots)
- Player vs Player battles (also up to 4 bots)
- Audio test menu
- Joystick mapping menu
- 5 char
- Map select

Available characters (v0.1.X):

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

## v0.2.X

Available features:

- 7 char

Available characters (v0.1.X):

- Sonic — NOK
- Amy — NOK
- Tails — NOK
- Knuckles — NOK
- Shadow — NOK
- Cream — NOK
- Rouge — NOK

### Checklist (Road to v0.2) (60%)

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
- ⬜ (0%) Auto camera that stays at a midpoint between the players.
- ✅ Allow the camera to follow player 1 or player 2.

- ✅ Debug menu to adjust stun, knockback, pulse, and damage in-game.
- - ✅ Adjust stun, damage and knockback debugging for PvP (currently only works correctly on bots).

- ⬜ (90%) Tiles DMA on Work RAM: “Direct Memory Access transfer” (copy from ROM → WRAM or WRAM → VRAM).
- ⬜ (90%) Tiles packing: grouping multiple sprites into the same block to reduce overhead.

- ⬜ New FULL sprite sheet.
- - ⬜ (80%) Sonic.
- - ⬜ (0%) Amy.
- - ⬜ (0%) Tails.
- - ⬜ (80%) Knuckles.
- - ⬜ (0%) Shadow.
- - ⬜ (0%) Cream.
- - ⬜ (0%) Rouge.
- - ⬜ (50%) Sprites DMA on Cart RAM: copy from Cart → VRAM.
- - ✅ Tiles packing: grouping multiple sprites into the same block to reduce overhead.

- ⬜ (25%) Bot strategy.
- ✅ Make the bots follow the same sprite control flow as the players.
- ✅ Check why debug_battle_add_damage doesn’t get called for all IDs.
- ⬜ Create spectator mode to watch bots fight.

- ✅ Problem with tiles that suddenly change; this started happening after implementing FREE CAM.