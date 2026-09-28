# Screen reader user tests

This checklist holds the accessibility scenarios that still need confirmation
by a blind screen-reader user.  The maintainer works through it during
development, and beta testers receive it before a release.  Development does
not wait for these tests; a stage is closed once its automated tests pass.

## How to test

- Use the Windows x64 tiles build with NVDA running and default options.
  Options > Interface > Screen reader speech should stay on Automatic.
- `Alt+R` repeats the last meaningful announcement.  `Alt+D` reads details in
  lists and the inventory.
- For every scenario, also check: every action works from the keyboard, focus
  moves in a sensible order, each key produces one clear announcement rather
  than several, short speech comes first and details on request, `Alt+R`
  repeats the right thing, and after you silence NVDA with Control the game
  does not repeat the interrupted speech by itself.
- Some situations are hard to reach in normal play.  Set them up through the
  debug menu: open it from the action menu, or bind the "Debug menu" action in
  the keybindings menu.  The debug menu only prepares the situation; what is
  tested is the speech that follows.
- Report a problem with the scenario name, what you did, what you heard, and
  what you expected.  To help diagnosis, start the game with the environment
  variable `CATA_ACCESSIBILITY_TRACE=1`; the speech then also goes to
  `config\debug.log`.

## Stage 03A: Main menu

- **World save counts.**  Create worlds with no save and with two saves, then
  browse the World and Load tabs.  Each world names its number of saves.

## Stage 05: Popups

- **Standard, error, and progress popups.**  Trigger a normal message popup, a
  developer error popup, and a long save or load.  Each is announced once,
  `Alt+R` repeats it, and progress is not read at every percent.

## Stage 05A: Review corrections

- **Repeated sounds while smashing.**  Smash a piece of furniture until it
  breaks.  The repeated sound, such as "You hear whack!", is spoken once, then
  the result.
- **Repeated attack messages.**  Spawning… > Spawn monster next to the
  character, then attack until you miss several times.  Each miss is spoken.
- **Unchanged threats after another monster dies.**  Spawning… > Spawn monster
  twice, one far and one near.  Kill the near one.  The far monster is not
  announced as newly spotted.
- **Inventory letters and Alt+D.**  In the inventory, select an item by its
  letter, then use `Alt+D` on an item.  The letter selects the item; `Alt+D`
  reads details and does not select anything.
- **Rebound repeat key while saving.**  Bind "Repeat last screen reader
  message" to another key, save the game, and press the new key during saving.
  The progress message is repeated.
- **Speech off.**  Set Screen reader speech to Off: the game is silent.  Set it
  back to Automatic: speech returns.

## Stage 05B: Speech priorities and turn batching

- **Fast list navigation.**  Hold Down in a long list and in the inventory,
  then release.  Only the entry where you stop is read in full; speech does not
  lag behind.
- **Walking during an alert.**  Spawning… > Spawn monster several tiles away,
  then step at once.  The alert is not cut off by the step.
- **Repeat after a step.**  After an alert or message, take one step and press
  `Alt+R`.  The alert or message is repeated, not the tile name.
- **Auto-travel.**  Travel to a distant spot, for example from look-around.
  Individual steps are silent; the arrival or the reason for stopping is
  spoken.
- **Alert before a prompt.**  Player… > Bleed self, then keep playing until a
  prompt appears.  The pending alert is spoken before the prompt.

## Stage 06: Inventory selector

A varied inventory comes from Spawning… > Spawn an item.

- **Browsing.**  Open the inventory and move through items.  Each item names
  its count, category when it changes, location, and main values.
- **Categories and columns.**  Switch between category and item selection, and
  between inventory, worn items, and nearby items.  The new mode or column is
  announced.
- **Filter.**  Filter the inventory, then clear the filter.  The filter and
  the number of matching items are announced.
- **Item details.**  Examine an item and read its details by line, then return.
  Focus returns to the same item.
- **Comparison hints.**  Compare two items.  The hints name the keys you
  actually have bound.

## Stage 06A: Item action flows

Spawning… > Spawn an item gives a knife, a jacket, a book, a canned food item,
a loaded gun with spare ammo, and a flashlight.

- **Item action menu.**  Open the inventory and press Enter on the knife.  You
  hear "Actions for" with the item name, the details key, and the focused
  action.  Move through the actions: "wear" says "Not possible now" or "Not
  applicable" when it cannot be done, and available actions have no suffix.
  "Auto pickup" says "Rule set" or "No rule".
- **Item details from the action menu.**  In the same menu press `Alt+D`.  The
  complete item information opens and is read by line.  Leave it: the focused
  action is spoken again.
- **Return after an action.**  In the action menu choose "favorite".  The
  inventory reopens on the same item and speaks only the item, now marked
  Favorite, without the title and hotkey count.
- **Activation methods.**  Choose "activate" on an item with several uses, or
  use the Apply command on the flashlight.  Each method is read with its item;
  `Alt+D` reads the method's description when it has one.
- **Common flows.**  Wield the knife, wear and take off the jacket, read the
  book, eat the food, reload and unload the gun, and drop an item, using both
  the direct commands and the item action menu.  Each selector names its
  purpose and the action's main values; each result or refusal is spoken once.
- **Eating hint.**  Open the eat menu.  The opening lists food, drink, pain,
  rest, weight, and intake as separate phrases, without naming line symbols.
- **Failure.**  Try to wear a second pair of the same boots, or read in
  darkness (Map… > Change time to night).  The refusal is spoken.
