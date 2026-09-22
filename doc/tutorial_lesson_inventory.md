# Tutorial lesson inventory for Stage 03B

This inventory describes the tutorial data and triggers as assessed before the
gameplay and interface stages.  A lesson's instructions are not proof that its
gameplay control is accessible.  Stage 03B will recheck the entire tutorial
with NVDA after those controls are implemented.

## C++ snippet lessons

`tutorial_game::add_message()` shows a lesson only once per tutorial session.
It expands keybinding tags before opening a popup.  The following groups cover
all 35 entries in `snippets.json`.

- Every turn: `LESSON_INTRO` explains the tutorial and popup dismissal;
  `LESSON_MOVE` gives movement keys and sends the player east to a door.
- Light, equipment, and condition checks during `per_turn()`:
  `LESSON_DARK`, `LESSON_DARK_NO_FLASH`, `LESSON_GUN_FIRE`, `LESSON_PAIN`,
  `LESSON_RESTORE_STAMINA`, and `LESSON_BUTCHER`.  `LESSON_GUN_FIRE` asks the
  player to shoot the dummy and proceed west; the others give contextual advice.
- Adjacent terrain or furniture during `per_turn()`: `LESSON_OPEN`,
  `LESSON_CLOSE`, `LESSON_LOCKED_DOOR`, `LESSON_WINDOW`, `LESSON_EXAMINE`,
  `LESSON_STAIRS`, `LESSON_PICKUP_WATER`, and `LESSON_ACT_BUBBLEWRAP`.
  `LESSON_LOCKED_DOOR` asks for a smash; `LESSON_EXAMINE` asks for wielding the
  spear and moving west; `LESSON_PICKUP_WATER` asks for filling a bottle,
  drinking, and moving north.  The other lessons describe the nearby feature.
- Items on the player's tile during `per_turn()`: `LESSON_PICKUP` asks for the
  baseball bat to be picked up before moving south.
- After opening or closing: `LESSON_CLOSE` and `LESSON_SMASH`.  After firing:
  `LESSON_RECOIL`.  After using an item: `LESSON_ACT_GRENADE` and
  `LESSON_TOOK_PAINKILLER`.  After eating or drinking: `LESSON_TOOK_CIG` and
  `LESSON_DRANK_WATER`.  These give contextual advice without a fixed route.
- After wearing: `LESSON_HOLSTERS_ACTIVATE` asks for putting the spear in the
  strap and proceeding down the corridor; `LESSON_WORE_STORAGE` asks for
  moving north and wearing clothing on the racks; `LESSON_WORE_MASK` and
  `LESSON_WORE_ARMOR` explain the equipped item.
- After pickup, according to item type: `LESSON_GOT_ARMOR`,
  `LESSON_GOT_GUN`, `LESSON_GOT_AMMO`, `LESSON_GOT_TOOL`, `LESSON_GOT_FOOD`,
  and `LESSON_GOT_WEAPON`.  These give contextual advice.
- No current call site: `LESSON_WEAPON_INFO` and `LESSON_HIT_MONSTER` remain
  in the snippet data but are not currently delivered.

## Map trap lessons

All numbered traps are placed in `mapgen.json`; traps 9 and 10 are in the
underground map.  A trap EOC shows its message as a popup.  `parse_tags()`
resolves keybinding tags before that popup.  Traps 3, 4, and 11 guard against
repeat delivery with EOC state; the other message traps can fire again on
reentry.

- 1, look around: use Look Around; no explicit route instruction.
- 2, movement modes: inspect or change mode, then reach the corridor end.
- 3, melee dummy: attack it and pass through the door behind it.
- 4, reach attack: attack the dummy across the counters, then move south.
- 5, spear strap: wear it to continue.
- 6, gun loading: load the magazine and gun; no explicit route instruction.
- 7, inventory: inspect clothing and pockets, then wear the western backpack.
- 8, flashlight: reload it, take it, and move to the stairs.
- 9, interaction setup: take the plastic bottle and approach the dispenser.
- 10, remote use: light the lantern, then move east.
- 11, food crafting: make and eat a peanut butter sandwich.
- 12, construction: paint walls, then move south.
- 13, pain trigger: no popup lesson; `per_turn()` sets pain and can cause the
  C++ `LESSON_PAIN` lesson.
- 14, throwing: select a stack quantity, throw baseballs at the dummy, then
  move north.
- 15, finale: explains action and keybinding menus and saving to leave; the
  player may keep exploring.

## Exercise ownership

- Movement and looking use existing gameplay movement and Stage 02 look-around.
- Status and movement modes use the completed Stage 04 status work and the
  shared control work in Stage 05.
- Inventory browsing and item detail belong to Stage 06.  Wielding, wearing,
  reloading, using, consuming, and holster actions belong to Stage 06A.
- Doors, smashing, stairs, the dispenser, and remote terrain or furniture use
  belong to Stage 08.
- Pickup, stack quantity, dropping, and container transfers belong to Stage 09.
  The first required bat pickup needs verification during the full walkthrough.
- Gun firing, reach targeting, and throwing belong to Stage 10; melee also
  needs gameplay feedback checked during the Stage 03B NVDA assessment.
- Food crafting belongs to Stage 11, construction to Stage 12, and detailed
  character or encumbrance inspection to Stage 13.
- Keybinding discovery and editing belong to Stage 05; Help browsing belongs
  to Stage 03C.
