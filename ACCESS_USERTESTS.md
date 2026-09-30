# Screen reader user tests

This checklist holds the accessibility scenarios awaiting a result from a
blind screen-reader user.  The maintainer works through it during
development, and beta testers receive it before a release.  Development does
not wait for these tests; a stage is closed once its automated tests pass.

Last assigned test ID: T141.

- Each scenario has a permanent ID: `T1`, `T2`, and so on.  Append new tests
  at the end of the file, using the next number and updating the last assigned
  ID above.  Never renumber tests or reuse an ID, even after removing tests.
- Remove a test after the user reports a complete result.  Briefly record
  passes by ID in the stage's `ACCESS_DONE.md` entry.  Record failures with
  their IDs and reproduction details as fixes in `ACCESS_TODO.md`; after a
  fix, append a retest with a new ID referring to the original test.  Partial
  or skipped tests stay in the pool.  Remove empty stage sections.

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
- Report results by ID, for example "T7 passed" or "T8 failed: ...".  For a
  problem, say what you did, what you heard, and what you expected.  To help
  diagnosis, start the game with the environment
  variable `CATA_ACCESSIBILITY_TRACE=1`; the speech then also goes to
  `config\debug.log`.

## Stage 03A: Main menu

- **T1 - World save counts.**  Create worlds with no save and with two saves, then
  browse the World and Load tabs.  Each world names its number of saves.

## Stage 05: Popups

- **T2 - Standard, error, and progress popups.**  Trigger a normal message popup, a
  developer error popup, and a long save or load.  Each is announced once,
  `Alt+R` repeats it, and progress is not read at every percent.

## Stage 05A: Review corrections

- **T3 - Repeated sounds while smashing.**  Smash a piece of furniture until it
  breaks.  The repeated sound, such as "You hear whack!", is spoken once, then
  the result.
- **T4 - Repeated attack messages.**  Spawning… > Spawn monster next to the
  character, then attack until you miss several times.  Each miss is spoken.
- **T5 - Unchanged threats after another monster dies.**  Spawning… > Spawn monster
  twice, one far and one near.  Kill the near one.  The far monster is not
  announced as newly spotted.
- **T6 - Inventory letters and Alt+D.**  In the inventory, select an item by its
  letter, then use `Alt+D` on an item.  The letter selects the item; `Alt+D`
  reads details and does not select anything.
- **T7 - Rebound repeat key while saving.**  Bind "Repeat last screen reader
  message" to another key, save the game, and press the new key during saving.
  The progress message is repeated.
- **T8 - Speech off.**  Set Screen reader speech to Off: the game is silent.  Set it
  back to Automatic: speech returns.

## Stage 05B: Speech priorities and turn batching

- **T9 - Fast list navigation.**  Hold Down in a long list and in the inventory,
  then release.  Only the entry where you stop is read in full; speech does not
  lag behind.
- **T10 - Walking during an alert.**  Spawning… > Spawn monster several tiles away,
  then step at once.  The alert is not cut off by the step.
- **T11 - Repeat after a step.**  After an alert or message, take one step and press
  `Alt+R`.  The alert or message is repeated, not the tile name.
- **T12 - Auto-travel.**  Travel to a distant spot, for example from look-around.
  Individual steps are silent; the arrival or the reason for stopping is
  spoken.
- **T13 - Alert before a prompt.**  Player… > Bleed self, then keep playing until a
  prompt appears.  The pending alert is spoken before the prompt.

## Stage 06: Inventory selector

A varied inventory comes from Spawning… > Spawn an item.

- **T14 - Browsing.**  Open the inventory and move through items.  Each item names
  its count, category when it changes, location, and main values.
- **T15 - Categories and columns.**  Switch between category and item selection, and
  between inventory, worn items, and nearby items.  The new mode or column is
  announced.
- **T16 - Filter.**  Filter the inventory, then clear the filter.  The filter and
  the number of matching items are announced.
- **T17 - Item details.**  Examine an item and read its details by line, then return.
  Focus returns to the same item.
- **T18 - Comparison hints.**  Compare two items.  The hints name the keys you
  actually have bound.

## Stage 06A: Item action flows

Spawning… > Spawn an item gives a knife, a jacket, a book, a canned food item,
a loaded gun with spare ammo, and a flashlight.

- **T19 - Item action menu.**  Open the inventory and press Enter on the knife.  You
  hear "Actions for" with the item name, the details key, and the focused
  action.  Move through the actions: "wear" says "Not possible now" or "Not
  applicable" when it cannot be done, and available actions have no suffix.
  "Auto pickup" says "Rule set" or "No rule".
- **T20 - Item details from the action menu.**  In the same menu press `Alt+D`.  The
  complete item information opens and is read by line.  Leave it: the focused
  action is spoken again.
- **T21 - Return after an action.**  In the action menu choose "favorite".  The
  inventory reopens on the same item and speaks only the item, now marked
  Favorite, without the title and hotkey count.
- **T22 - Activation methods.**  Choose "activate" on an item with several uses, or
  use the Apply command on the flashlight.  Each method is read with its item;
  `Alt+D` reads the method's description when it has one.
- **T23 - Common flows.**  Wield the knife, wear and take off the jacket, read the
  book, eat the food, reload and unload the gun, and drop an item, using both
  the direct commands and the item action menu.  Each selector names its
  purpose and the action's main values; each result or refusal is spoken once.
- **T24 - Eating hint.**  Open the eat menu.  The opening lists food, drink, pain,
  rest, weight, and intake as separate phrases, without naming line symbols.
- **T25 - Failure.**  Try to wear a second pair of the same boots, or read in
  darkness (Map… > Change time to night).  The refusal is spoken.

## Stage 07: Advanced inventory and multiselect transfers

Wear a backpack.  Spawning… > Spawn an item places a hammer, 5 rocks, and a
plastic bag or other container on your tile; spawn a second backpack too.

- **T26 - Opening.**  Press `/`.  You hear "Advanced inventory", the source pane
  with its item count and capacity, the destination pane, and the focused
  row, in one announcement.  Pressing `Alt+R` repeats it.
- **T27 - Rows.**  Move with Up and Down, Page Up, Page Down, Home, and End.  Each
  key speaks one row: name, count when above one, weight, and volume.
  `Alt+D` adds the category and other details.  Interrupt a row with Control;
  the next key speaks only the new row.
- **T28 - Panes and areas.**  Press Tab: "Source:" with the other pane's area and
  item count is spoken once, then its row, without the capacity.  Press `5`
  for the tile below you and `I` for your inventory: each area change adds
  the capacity.  Press `Alt+S`: both panes, capacity, filter, and sort in one
  announcement.
- **T29 - Single, stacked, and partial moves.**  With the ground as source and the
  inventory as destination, press Enter on the hammer: when the screen
  returns you hear "Moved hammer to Inventory" and the next row.  Press `m`
  on the rocks and enter 2: "Moved rocks, 2, to Inventory".  Press `M` to
  move the rest of the stack.  Repeat from the inventory back to the ground.
- **T30 - Container.**  Select the plastic bag and press `C`: the source becomes
  "Container:" with its name and capacity.  Move a rock into and out of it.
- **T31 - Move all.**  Press `,` on the ground pane: "Moved N stacks to Inventory".
- **T32 - Failures.**  Take off the backpack and try to move the second backpack
  and then all rocks to the inventory; move an item to a wall tile with the
  direction keys.  The refusal popup is spoken and closing it returns
  silently to the same row.
- **T33 - Filter and sort.**  Press `F`, type "rock", and confirm: "Filter: rock"
  and the matching count, then the row.  Press `R` to reset: "Filter
  cleared".  Change the sort with `s`: "Sort:" and the new order.
- **T34 - Multiselect drop.**  Close the screen and press `d` (drop).  Rows say
  "Not selected".  Mark the rocks with the right arrow or `l`: "Selected all
  5" and "1 entry selected".  Type 2 and confirm the number: "Selected 2 of
  5".  Unmark: "Nothing selected".  `Alt+S` reads the purpose, weight and
  volume statistics, and the selection total.  Confirm the drop and hear
  the result message.

## Stage 08: Directional and environmental interaction

Stand next to a closed door and a piece of furniture, for example inside the
starting shelter.  Spawning… > Spawn an item gives items to drop around you;
Spawning… > Spawn a monster followed by killing it leaves a corpse.  Options >
Interface > "Autoselect if exactly one valid target" controls auto-selection.

- **T35 - Targets in the prompt.**  Press `o` (open) next to the closed door with
  another direction also valid, and auto-selection off.  You hear "Open
  where? Targets:", the door with its direction, then the input hint, in one
  announcement.  Tiles with the same target are grouped.  `Alt+R` repeats it.
- **T36 - Auto-selection.**  With auto-selection on and only one door, press `o`.
  You hear "Auto-selected:", the door, and its direction, then the result
  message.
- **T37 - No valid target.**  Press `c` (close) where nothing can be closed.  The
  failure message is spoken; with auto-selection off the prompt says "No
  valid targets".
- **T38 - Invalid direction.**  In the open prompt press a direction toward a
  wall.  You hear the wall, its direction, and "not a valid target".
- **T39 - Cancel.**  Open any direction prompt and press Escape: "Never mind." is
  spoken once and gameplay continues.
- **T40 - Examine.**  Press `e` next to furniture with items and a container such
  as a fridge.  The prompt names each target with its item count; choose one
  and the following menu or message is spoken.
- **T41 - Grab and release.**  Press `G` next to a chair.  The prompt names the
  furniture; choose it and hear the grab message; press `G` again to hear
  the release.
- **T42 - Smash.**  Press `s` next to a window and a corpse.  Both are listed as
  targets; choose one and the result and any confirmation are spoken.
- **T43 - Butcher.**  Stand on the corpse and press `B`.  The butcher menu is read
  through the shared menu speech.
- **T44 - Darkness.**  Map… > Change time to night, then open a prompt: tiles you
  cannot see are named "unseen".

## Stage 08A: Auto-travel

Start outdoors in daylight.  Debug menu > Map… > Change time helps if it is
dark.  Spawning… > Spawn a monster places a monster to interrupt travel.

- **T45 - Start summary.**  Press `x` (look around), move the cursor to a visible
  tile about ten tiles away, and press `Shift+t`.  You hear one announcement:
  "Traveling to", the target, its distance and direction, the step count, and
  the terrain along the route with counts, for example "grass 6, dirt road
  5".
- **T46 - Silent steps and arrival.**  During the same travel no step is spoken.
  On arrival you hear "Arrived:" and the tile name, once, and `Alt+R`
  repeats it.
- **T47 - Nearby item list.**  Press `V`, select an item a few tiles away, and
  press `Shift+t`.  The start summary and arrival are spoken as above.
- **T48 - Obstruction.**  Spawn a hostile monster on the route before starting.
  When it blocks the way you hear "Monster in the way.  Auto move canceled."
  followed by "Travel stopped:", the destination with its remaining distance
  and direction, and your position.
- **T49 - Danger query.**  Spawn a monster that comes into view during travel.  The
  cancel question names the reason and is read with its choices.  Answer no
  and travel continues silently; answer yes and you hear the cancel message
  and your position, without "Travel stopped".
- **T50 - Cancel with a key.**  During a long travel press `.` (pause) and answer
  yes.  Only the cancel message and your position are spoken.
- **T51 - Overmap travel.**  Press `m`, move the cursor a few map tiles away, press
  `Shift+w` twice, and confirm.  You hear "Traveling to", the overmap
  terrain, the distance in map tiles with direction, and the overmap terrain
  along the path.  Steps and local route changes are silent, and arrival
  speaks "You have reached your destination." and your position.  The
  overmap itself is made accessible in Stage 18; until then, choose the
  destination with sighted help or skip this scenario.
- **T52 - Speech interruption.**  Press `Shift` while the start summary is spoken.
  Speech stops and is not repeated; travel continues silently and the
  arrival is still spoken.

## Stage 09: Pickup, drop, and container transfers

Wear a backpack.  Debug menu > Spawning… > Spawn an item places items on
your tile; spawn two hammers, a stack of rocks, and something bulky such as
an anvil.

- **T53 - Single pickup.**  Press `g` and choose your tile.  Mark one hammer.  You
  hear the hammer, its selection state, the selection total, and "Weight …
  of … kg, volume … of … L".  Press Enter.  After the pickup you hear one
  sentence, "Picked up: hammer into backpack.", and no separate "You pick
  up" line.  `Alt+R` repeats it.
- **T54 - Stack and multiple items.**  Press `g`, mark the second hammer and the
  rocks with a count (type a number, then mark), and confirm.  One sentence
  names both items with their containers.
- **T55 - Predicted container.**  In the pickup list, press `Alt+D` on a row.  The
  details end with "Goes into:" and the container.
- **T56 - Capacity warning.**  Spawn several anvils and mark them in the pickup
  list.  Once the marked weight or volume passes the limit, the marking
  speech ends with "Over weight limit" or "Over volume limit".  `Alt+S`
  repeats the warning with the statistics.
- **T57 - Left behind.**  Take off the backpack, then pick up a hammer.  You hear
  "Not picked up: hammer, no room." once, without a second "Some items were
  not picked up!".
- **T58 - Drop several items.**  Press `d`, mark a hammer and the rocks, and
  confirm.  You hear "Dropped on", the ground type, and both items instead
  of "several items".
- **T59 - Insert into a container.**  Open the inventory, choose the backpack,
  pick Insert from its item menu, and mark an item.  Marking speaks the
  backpack's weight and volume against its limits; the result is the game
  message "You put your … into the backpack."
- **T60 - Ownership.**  Debug menu > Map… > teleport to a refugee center or
  another shop where items have an owner, stand next to a shelf, and press
  `g`.  Owned rows say "Not owned", and `Alt+D` names the owner.  Mark one
  and confirm: the theft question starts with "Not owned:" and the item
  name, then offers its choices.
- **T61 - Speech interruption.**  Press `Shift` while a marking or result sentence
  is spoken.  Speech stops and is not repeated; the next key speaks
  normally.

## Stage 09A: List position speech

In every list below, check that opening speaks the name and item count once,
that moving with the arrow keys speaks only the row with no "3 of 12", and
that `Alt+R` repeats the last row.

- **T62 - Menu list.**  Press `Enter` in gameplay to open the action menu.  You
  hear its title, "… items", the navigation hint, and the first row.  Press
  Down several times quickly: each press speaks only the row.  Press Up on
  the first row: you hear "Bottom." and the last row.  Press Down: you hear
  "Top." and the first row.  Press `Alt+D`: the details (or "No details
  available.") end with the position, for example "1 of 12".
- **T63 - Menu categories and filter.**  In a list with category tabs, such as
  Debug menu > Spawning… > Spawn an item, press Left or Right: you hear the
  category name, its item count, and the focused row.  Press `/`, type a
  filter, and confirm: you hear "Filter: …", the number of matching items,
  and the focused row.
- **T64 - Inventory.**  Press `i`.  You hear the title, the column name, "…
  items", and the first item.  Move with Up and Down; wrap past both ends
  and hear "Top." or "Bottom." before the item.  Press `Alt+S`: the summary
  ends with the position.  Switch columns with Tab where available: the new
  column's name and item count are spoken once.
- **T65 - Advanced inventory.**  Press `/`.  Wrap past both ends of the source
  pane and hear "Top." or "Bottom.".  Press `Alt+S`: the pane summary ends
  with the position, for example "2 of 5".
- **T66 - Nearby items and creatures.**  Spawn a few items and monsters nearby
  through the debug menu, then press `V`.  Moving speaks the item without
  "5 of 9" or "Location 1 of 3"; an item on several tiles says "3
  locations".  Wrap past both ends and hear "Top." or "Bottom.".  Press
  `Alt+S`: you hear the list name, the result count, the position, and for
  items on several tiles "Location 2 of 3".  Repeat on the creature tab.
- **T67 - Message log.**  Press `P`.  You hear "Message log" and the message
  count once.  Moving speaks only the message.  `Alt+D` or `Enter` speaks
  "Message 3 of 40" with the type and text.
- **T68 - Help and keybindings.**  Press `?`.  The topic count is spoken once;
  moving speaks only topics.  Press `Alt+S`: "Help, 23 topics, 4 of 23".
  Then open the keybinding list with `?` in any menu: opening says
  "Keybindings, … actions".  Wrap past both ends and hear "Top." or
  "Bottom.".  `Alt+D` ends with the position.
- **T69 - Main menu.**  On the New Game or World tab, press Left or Right to reach
  it: the tab name and its item count are spoken once.  Press Up on the
  first item to hear "Bottom.".  Press `Alt+S`: "Main menu, Tab …, 3 of 10,
  Item 1 of 4".
- **T70 - Speech interruption.**  Hold Down in a long list and press `Control`
  while a row is spoken.  Speech stops and is not repeated; the next key
  speaks only its row.

## Stage 10: Ranged and thrown targeting

Debug menu > Spawning… > Spawn an item gives a pistol with a loaded
magazine and rocks; wield the pistol.  Debug menu > Spawning… > Spawn
monster places zombies; put two or three at different distances, one behind
a wall or window.  Debug menu > Spawning… > Spawn NPC adds an NPC for the
neutral case.

- **T71 - Enter target mode.**  Press `f`.  You hear once "Firing pistol", the
  ammunition with its count ("Ammo: …, 8 of 15"), the number of targets,
  and the focused target with its attitude, distance, direction, and hit
  chance ("zombie, Hostile, 5 tiles northeast, 62% hit").
- **T72 - Cycle targets.**  Press Tab repeatedly, quickly.  Each press speaks only
  the next target and its hit chance.  Past the farthest target you hear
  "Top." before the nearest; Shift+Tab past the nearest says "Bottom.".
  With no visible target, Tab says "No targets".
- **T73 - Manual cursor.**  Move the cursor with the direction keys.  Each step
  speaks the tile as look-around does, ending with the distance and
  direction.  Pushing past the maximum range says "Edge of range".  `0`
  returns the cursor to you and says "Not a valid target".
- **T74 - Obstruction.**  Aim at the zombie behind the wall.  The focus ends with
  "No line of fire, blocked by" and the wall.  With a creature between you
  and the target, you hear "In line of fire:" and its name.
- **T75 - Details and status.**  Press `Alt+D`.  A document opens with the
  target, its description and health, the weapon status, and one line per
  aim level with its key, hit chance, outcomes, and moves.  Escape returns
  and repeats the focus.  `Alt+S` speaks firing mode, aim, ammunition,
  recoil, steadiness, and range.
- **T76 - Aim and mode changes.**  Press `.` to aim: you hear the new recoil and
  hit chance.  Press `s` and pick another aim or firing mode: you hear only
  what changed and the new hit chance.  `,` resets the aim and says
  "Recoil: High".
- **T77 - Fire, confirm, cancel.**  Press `f` to fire; the result is the game's
  messages.  Target the neutral NPC and fire: the question names the NPC.
  Press Escape in target mode: you hear "Targeting canceled.".
- **T78 - Blocked fire.**  Press `0` to put the cursor on yourself, then `f`: you
  hear "Not a valid target." instead of silence.
- **T79 - Throwing.**  Wield or select a rock with `t`.  You hear "Throwing rock",
  the target count, and the target with its hit chance.  On an empty tile
  the hit chance is for hitting the spot.
- **T80 - Interrupted speech.**  Press Tab and silence NVDA with Control right
  away.  Nothing is repeated by itself; `Alt+R` repeats the last target.
- **T81 - Reach and spells.**  Reach attacks and spell targeting use the same
  speech; spell casting is completed in Stage 14A, so skip spells until then.

## Stage 11: Crafting and disassembly

Debug menu > Spawning… > Spawn an item gives materials: for example a
hammer, a knife, rags, and a 2x4.  Debug menu > Player… > Unlock all recipes
makes more recipes available; Debug menu > Player… > Change all skills
raises skills.

- **T82 - Open crafting.**  Press `&`.  You hear once "Crafting", the category,
  the recipe count, and the focused recipe with "craftable" or the number
  of missing requirements ("knife, 2 missing").
- **T83 - Browse.**  Press Down and Up quickly.  Each press speaks only the new
  row, without a position.  Past the last recipe you hear "Top." before the
  first; Up past the first says "Bottom.".  Favorite recipes say
  "favorite", unread ones "new", and group rows "group, collapsed";
  Enter on a group says "expanded".
- **T84 - Categories.**  Press Tab and Shift+Tab: you hear the category name, its
  recipe count, hidden count if any, and the focused recipe.  Left and Right
  change the subcategory the same way.
- **T85 - Missing requirements.**  Focus a recipe you cannot craft and press
  `Alt+D`.  A document opens; the first line gives the availability and the
  position ("2 missing. 3 of 12."), later lines give skills, time, and one
  line per requirement group ("Tools: hammer, have; or rock, missing.").
  Escape returns and repeats the recipe row.
- **T86 - Result information.**  Press `Alt+S`: you hear the result item's
  description and statistics.  `e` opens the same information in the
  document view.
- **T87 - Search.**  Press `/`, type a name, and press Enter: you hear "Filter:",
  the text, the recipe count, and the focused recipe.  `r` says "Filter
  cleared" and the category again.  Shift+L on a recipe opens related
  recipes; choosing one speaks the filtered list.
- **T88 - Batch.**  Press `b` on a craftable recipe: you hear "Batch size 1", the
  time, and "craftable".  Down speaks the next batch size, its time, and
  whether it is still craftable.  `b` again says "Batch off" and the
  recipe.
- **T89 - Craft.**  Press Enter on a craftable recipe.  Any component choice is a
  normal list; the start, progress messages, failures, and the result are
  spoken as game messages.  Enter on an uncraftable recipe says "Crafter
  can't craft that!".
- **T90 - Toggles.**  `*` says "Favorite" or "Not favorite", `v` "Marked as read"
  or "Marked as unread", `u` the unread sorting, `c` opens the crafter list
  and a new crafter is announced by name.
- **T91 - Disassemble.**  Press `(`.  The item list speaks each item with its
  yield and time, or why it cannot be disassembled.  `Alt+D` opens a
  document with the returned components, the time, and the tools
  ("Tools: 1 tool with cutting of 2 or more, have.").  Choosing an item
  asks for confirmation through the normal question, and the result is a
  game message.
- **T92 - Interrupted speech.**  Press Down and silence NVDA with Control right
  away.  Nothing is repeated by itself; `Alt+R` repeats the last row.

## Stage 12: Construction and placement

Use Debug menu > Spawning… > Spawn an item to provide boards, nails, and a
hammer.  Use Debug menu > Player… > Change all skills if a selected project
needs a skill you lack.  Stand beside open ground and a suitable wall or
window so both valid and invalid placement directions are available.

- **T93 - Open and browse.**  Open the construction menu using its binding in the
  keybinding help.  Hear "Construction", the category, project count, and one
  focused project.  Press Down and Up rapidly: each movement speaks one row
  with "available" or "Unavailable" and its reason.  Wrapping says "Top" or
  "Bottom" before the row; routine redraws stay silent.
- **T94 - Categories and filters.**  Switch categories with Left and Right, search
  for a project, clear the search, and toggle unavailable projects.  Each
  change speaks the new category or filter, count, and focused row.  `Alt+S`
  gives the position and whether unavailable projects are hidden.
- **T95 - Requirements and variants.**  Focus a project with multiple variants and
  press `Alt+D`.  The document names each variant, result, required skill and
  current level, precursor terrain, time, tools, components, and their
  "have" or "missing" states.  Navigate by Down and Up; Escape returns to
  the same project and speaks its concise row once.
- **T96 - Invalid and valid placement.**  Select a buildable project.  The
  "Construct where?" prompt lists valid neighboring tiles by name and
  direction.  Choose an invalid direction and hear the game's failure
  explanation.  Reopen the project, choose a valid direction, and hear
  "Started: Constructing" with the project name.  Confirm the activity is
  present; progress and completion checks belong to Stage 12A.
- **T97 - Repeat and interruption.**  Interrupt a project announcement and then
  a placement prompt with NVDA's Control key.  Neither requeues itself;
  `Alt+R` repeats the most recent meaningful announcement.  Escape from
  placement returns to gameplay with the cancellation message.

## Stage 12A: Activities, interruptions, and long actions

Use the Stage 12 construction setup, or Debug menu > Spawning… > Spawn an item
to obtain the tools/components for a known long crafting recipe.  Debug menu >
Player… > Change all skills can satisfy its requirements.  Choose activity
commands through the action/keybinding menus; their original bindings remain.
These checks use the existing keyboard interface and do not require Stage 13's
full character sheet.

- **T98 - Start, progress, and result.**  Start a long craft and then a construction.
  Hear one start summary naming the work and its estimated duration.  Rapid
  game-time progress must not produce turn-by-turn speech.  `Alt+R` during the
  work reads current progress, including the percentage when the game supplies
  it.  On completion hear one result containing the item/project and any
  handler-reported skill gains or warnings, with no erroneous "Stopped" for a
  successful craft.  `Alt+R` after completion repeats the result.
- **T99 - Real-time interval and interruption of speech.**  In Options > Interface,
  leave Progress announcement interval at 5.  While an activity runs for more
  than five real seconds, changed progress may be spoken only after five
  seconds without other speech and at most once per interval.  Silence one
  update with Control or Shift: its unchanged text must not requeue itself.
  `Alt+R` explicitly repeats the current snapshot.  Set the interval to 0 and
  check that automatic progress stops while `Alt+R` still works; set it to 10
  and check the longer delay, then restore 5.  Keyboard focus in Options must
  read the option name, value, and explanation normally.
- **T100 - Threat interruption and choices.**  Use Debug menu > Spawning… > Spawn a
  monster to place a visible zombie near the work site; start long work and
  let the interruption occur.  Hear the visible threat's reason and the stop
  question once.  Navigate the shared popup choices with its shown keyboard
  actions: stop, continue, ignore, and distraction manager.  Each focus speaks
  its choice, `Alt+R` repeats the prompt, and redraws stay silent.  Stop and
  hear the activity and reason.  Use Debug menu > Map… > Kill all monsters
  in this test world to remove the zombie,
  restart resumable work, and hear "Resumed" rather than a fresh unexplained
  start.  Cancel another activity with the Pause action and its confirmation;
  declining that confirmation continues it.  Silencing prompt speech must not
  select a choice or cause an automatic repeat.
- **T101 - Distraction manager.**  Open the manager from the interruption popup.
  Hear its name, count, and first name/state row.  Up/Down browse in visible
  order, including "Toggle all"; wrapping says "Top" or "Bottom" once.  Enter
  or Left/Right toggles and speaks the new enabled/disabled state.  `Alt+S`
  reports position, `Alt+D` opens the focused explanation with line/sentence
  navigation, and Escape returns to the same row.  Escape closes the manager.
  Restore the original distraction settings before continuing other checks.
- **T102 - Stamina break.**  Use Debug menu > Player… > Edit player/NPC > Set stamina
  to set a positive value below one third of the displayed maximum, then
  perform tiring long work until "Keep trying?" appears.  Hear
  the question and concise choices for a break, stopping, and finishing.
  Choose a break: hear rest begin and the original task resume after stamina
  recovers.  In another attempt choose to stop; do not hear completion for the
  unfinished task.  The prompt's real keyboard labels and focus order remain
  the standard shared list behavior.
- **T103 - Wait and sleep.**  In a safe shelter use the Wait action and select a
  duration.  Hear one start, use `Alt+R` to obtain remaining time, and hear one
  finish.  Use Debug menu > Player… > Edit player/NPC > Set needs > Sleepiness
  to raise sleepiness, then use Sleep.  Hear the attempt start, falling asleep
  once, "Sleeping" on
  `Alt+R`, and waking once; no hidden wake-time estimate is spoken.  For loaded
  sleep, enable Autosave with a short turn interval and Real minutes between
  autosaves set to 0 in Options > General.  Sleep until an autosave occurs.
  On waking, use Debug menu > Game… > Quit to main menu and confirm quitting
  without saving, then load the autosave if it contains the sleeping state.
  Check that progress resumes without claiming a new fall-asleep event.
  Restore the original autosave settings afterward.
  Try sleeping while rested and check the keep-trying choices and the actual
  failure explanation when the attempt ends.  Silence speech and repeat it
  during both waiting and actual sleep.
- **T104 - Reading, continuous work, and failure.**  Spawn a readable book through
  Debug menu > Spawning… > Spawn an item, and a light if needed.  Read one
  chapter: hear the book and duration on start, progress/remaining time on
  `Alt+R`, and one final result.  Try continuous reading or repeated crafting;
  chapters/batches must not each announce a new start, and the final result
  preserves reported gains.  Turn off the reading light at night, or use Debug
  menu > Player… > Edit player/NPC to make vision unsuitable before resuming.
  Hear the game's actual inability-to-read reason, never a false completion.
  Skill-reading prerequisites may require debug skill adjustment.
- **T105 - Blocking loading and saving.**  Save and reload a world whose operation
  lasts more than five real seconds (a large explored world or additional mods
  may help).  Hear one start; changed progress respects the configured silence
  interval.  `Alt+R` reads the latest context/step/counter while input is
  otherwise blocked.  Interrupt an update with Control, check that unchanged
  text stays silent, and verify that old loading/saving progress never returns
  after the operation closes.  If an indivisible step does not pump events,
  speech and repeat may wait for that step to return; record the step and delay.

## Stage 13: Character information and layering

Open the character sheet with the game's character info key (`@` by default,
listed in the keybinding help).  For a dense character use Debug menu >
Player… > Edit player/NPC to edit skills, stats, and proficiencies, add an
effect, mutate, and edit bionics; wear several clothing layers from Debug menu
> Spawning… > Spawn an item.

- **T106 - Open, tabs, and rows.**  Open the sheet.  Hear "Character sheet",
  the name, "Stats", "9 items", and "Strength …, base …" once.  Press Down and
  Up through the stats: each key speaks one concise row; wrapping says "Top"
  or "Bottom" first.  Press Right/Left and Tab/Shift+Tab through all eight
  tabs, then the direct tab keys shown in the tab titles.  Each change speaks
  the tab name, its count ("No items" when empty), and the focused row; Speed
  also speaks speed and move cost, Bionics the power.  Scrolling the info box
  stays silent.
- **T107 - Encumbrance and warmth.**  Wearing several layers, open the
  Encumbrance tab.  Rows speak the body part, encumbrance, layering when
  nonzero, and a warmth word such as "comfortable" or "chilly" (eyes have no
  warmth word).  `Alt+D` reads the row with total and temperature number,
  its position, the encumbrance effects, and the limb scores line by line;
  Escape returns and speaks the row once.
- **T108 - Skills and rust.**  Use Debug menu > Player… > Edit player/NPC >
  Edit theoretical skill knowledge to raise one skill above its practical
  level.  On the Skills tab, moving into a new category speaks the category
  name before the row; moving within a category does not.  The raised skill
  says "rusty"; `Alt+D` includes its practical level and learning bonus.
- **T109 - Traits, bionics, effects, proficiencies.**  Rows speak trait names
  with positive/negative/neutral/mixed kind, bionics with installed count,
  effects by name, and proficiencies as known or learning with percent.
  `Alt+D` reads each description; the trait document states when a purifier
  cannot remove it.
- **T110 - Summary, repeat, and return.**  Press `Alt+S` on any tab: hear the
  name, gender, profession, tab, position, and row.  Interrupt speech with
  Control; nothing is repeated automatically, and `Alt+R` repeats the last
  announcement.  Open and close morale, body status, and the keybinding help;
  on return the focused row is spoken once.  The morale, body status, and
  medical screens themselves are not yet accessible.

## Stage 14: Bionics and mutations

Open the bionics screen with `p` and the mutations screen with `[` (both
listed in the keybinding help).  Install bionics through Debug menu >
Player… > Edit player/NPC > edit bionics: at least one Power Storage, a
toggled bionic such as Implanted Night Vision, and a power source.  Add
mutations from the same editor, including an active one with a cost, such as
Weak Photophore, and a passive one.  Set the bionic power level to zero and
later to full from the same menu.

- **T111 - Bionics tabs, rows, and power.**  Open the bionics screen.  Hear
  "Bionics", "Active bionics", the item count, and the focused row once.
  Rows speak name, on or off, and costs such as "10 J to activate, 10 J per
  turn"; power sources say "power source".  With zero power an inactive
  bionic adds "Unavailable: not enough power".  Down and Up speak one row;
  wrapping says "Top" or "Bottom" first.  Tab and Shift+Tab speak "Passive
  bionics", its count ("No items" when empty), and the focused row.  `Alt+S`
  speaks the power, the available fuel, the mode, the tab, the position, and
  the letter.
- **T112 - Bionic activation, examine, and details.**  With power, press Enter
  on Implanted Night Vision: hear the game's activation message, then the row
  with "on".  Press Enter again to hear it turn off.  Press `!`: hear
  "Examining"; press it again: "Activating".  `Alt+D` opens the description
  document with the row, position, letter, power usage, description, and
  occupied body parts; Escape returns and speaks the row once.  Press Enter
  on a passive bionic in Activating mode: the popup explains why, and the row
  is spoken once after closing it.  Toggle fuel saving (`S`) and sprite
  visibility (`H`) and hear the changed row.
- **T113 - Mutation columns, rows, and activation.**  Open the mutations
  screen.  Hear "Mutations", the column name, its count, and the focused
  row.  Active rows speak name, on or off, cost with its resource, and
  cooldown ("9 kcal, cooldown …"); passive rows speak the name, "starting
  trait" for traits chosen at character creation, and "sprite hidden".  Tab
  switches columns and speaks the column; with only one column the current
  one is repeated.  Starve the character (Debug menu > Player… > Edit
  player/NPC, or set hunger) and hear "Unavailable: not enough kcal" on
  Weak Photophore; Enter shows the game's popup and speaks the row after it.
  With enough food, Enter activates it, closes the screen, and the game
  message is spoken.
- **T114 - Mutation modes, details, repeat, and return.**  Press `!`, `=`,
  and `,`: hear "Examining", "Reassigning, select an entry to assign a new
  letter", and "Hiding, select an entry to hide or show its sprite".  In
  Examining mode press a mutation's letter: its row is spoken, and `Alt+D`
  opens its description.  Reassign a letter and hear the mode, row, and new
  key.  `Alt+S` speaks the mode, column, position, and key.  Interrupt speech
  with Control; nothing is repeated automatically, and `Alt+R` repeats the
  last announcement.

## Stage 14A: Spells and martial arts

Spell scenarios need a world with the Magiclysm mod; creating one depends on
Stage 21 (world and mod management), so skip them or get sighted help with
world creation until then.  Learn spells by spawning spell scrolls such as
Scroll of Magic Missile (Debug menu > Spawn > Spawn item) and reading them.
Open the spell menu with `]`.  Learn martial styles with Debug menu >
Player… > Learn all melee styles and open the style picker with `_`.

- **T115 - Spell list, tabs, and castability.**  Open the spell menu.  Hear
  "Choose a Spell", the category, its spell count, and the focused row once.
  Rows speak name, class ("Magus", or "Classless" for a spell without one),
  "castable", cost ("20 mana"), and casting time; Up and
  Down speak one row, with "Top" or "Bottom" on a wrap.  Left and Right
  switch categories and speak "All", "Favorites", or a class name with its
  spell count and the focused row.  Press `*` and hear the row with
  "favorite"; press it again to remove it.  Press `I` and hear "Ignore
  Distractions" or "Popup Distractions".  Spend mana (or set it to zero
  through Debug menu > Player… > Edit player/NPC) and hear "Unavailable: not
  enough mana" on the row.
- **T116 - Spell details, casting, and return.**  In the spell menu press
  `Alt+D`: the document starts with the row and key, then class,
  description, level, failure chance, cost, casting time, targets, range,
  and damage.  Up and Down read lines; Escape returns and speaks the row and
  position once.  Press `=`, assign a new letter, and hear the row again
  after the popups.  Press Enter on a castable spell: targeting or the cast
  result is spoken by the existing speech.  Interrupt speech with Control;
  nothing is repeated automatically, and `Alt+R` repeats the last
  announcement.
- **T117 - Martial style picker and details.**  Open the style picker.  Hear
  "Select a style", the details key, the item count, and the focused row.
  Rows speak the style name, "current" for the style in use, and the first
  sentence of its description; `Alt+D` reads the full description.  Press the
  details key named on opening (`d` by default) on a style: the document
  reads general info, buffs, techniques, and
  compatible weapons by category, with "(wielded)" and "(carried)" on
  weapons you have.  Escape returns and speaks the row once.  Press Enter
  on a style and hear the game's style message.

## Stage 13A: Character sheet subscreens and armor layering

From the character sheet (`@`) open the morale window with `m`, body status
with `s`, and the proficiencies window with `p`.  In gameplay open the medical
menu with `N`, body status with `{`, and sort armor with `+`.  Spawn items
through Debug menu > Spawning… > Spawn an item and wear them from the
inventory.  After a screen opened from the character sheet closes, the sheet
speaks its focused row once.

- **T118 - Morale document.**  Spawn and eat a comfort food such as chocolate,
  then open the morale window.  Hear "Morale" and the total morale; Down reads
  "Total positive morale, +N", each source with its percentage ("Ate something
  tasty, 100%"), the negative sources, then the pain and sleepiness lines when
  present and "Focus trends towards".  With no morale sources hear "Nothing
  affects your morale".  Escape returns to the caller.  Interrupt speech with
  Control; nothing is repeated automatically, and `Alt+R` repeats the last
  line.
- **T119 - Body status parts and nested graphs.**  Lower a body part's health
  through Debug menu > Player… > Edit player/NPC.  Open body status and hear
  "Body status", the part count, and the focused part once.  Up and Down speak
  one part: its name, "health 7 of 10" at the bar's precision, and "has
  details"; "Top" or "Bottom" on a wrap.  Page Up and Page Down scroll the
  info pane silently.  Press Enter or Right on an arm and hear the arm's name,
  its part count, and the focused part; Left or Escape returns and speaks
  "Body status", the count, and the part.  Press `Alt+D` on the damaged part
  and read health, wetness, temperature, effects with "good" or "bad", worn
  items, coverage, encumbrance, and protection as "worst, median, best";
  Escape speaks the part again.  `Alt+S` speaks the graph, position, and part.
- **T120 - Medical columns, details, and apply.**  Use Debug menu > Player… >
  Bleed self and spawn bandages.  Open the medical menu and hear "Medical",
  the pain, hunger, thirst, and tiredness words shown in the title bar,
  "HEALTH", its count, and the focused row.  Health rows speak the part,
  "health N of 10", and conditions such as the bleeding intensity, "BROKEN",
  or "SPLINTED, mending 2 of 5".  Left and Right switch to EFFECTS and STATS
  with their counts and focused row; stats speak "Strength 8, base 8".  Press
  `Alt+D` on the bleeding torso and read the bleeding description and the
  torso stats; Escape speaks the row.  Press the apply key, choose the
  bandage, and after the game's messages hear the focused row with the changed
  state.  `Alt+S` speaks the statuses, column, position, and row.
- **T121 - Proficiencies categories, filter, and details.**  Grant a
  proficiency and progress in another through Debug menu > Player… > Edit
  player/NPC.  Open the proficiencies window: hear "Proficiencies", "ALL", the
  count, and the focused row.  Rows speak "known" or "learning, 40%".  Left
  and Right speak the category, its count, and the first row.  Press `/`, type
  `l:*`, and confirm: hear "Filter: l:*", the matching count, and the first
  row.  `Alt+D` reads time to learn, progress, category, prerequisites, what
  it leads to, and the description; `Alt+S` adds the category description.
- **T122 - Sort armor conflicts and moving.**  Spawn and wear a hoodie and a
  sweatshirt (they share a layer) over a long-sleeved shirt, then open sort
  armor.  Hear "Sort Armor", "All", the count, and the focused row.  Left and
  Right switch the body part filter; on the torso hear "Torso. 3 items."  and
  the row.  The hoodie and sweatshirt rows say "layering conflict".  Press the
  move key (`s`) on the outer one and hear "Moving sweatshirt"; each Up speaks
  its new neighbours ("above hoodie, below long-sleeved shirt", "innermost,
  above …"); press `s` again and hear "Placed" with the row.  Press `H` and
  hear "Sprite hidden" and the row with "hidden"; `c` on a sided item asks and
  speaks the row with its side; `S` sorts and says "Sorted into layer order".
  `Alt+D` reads the item statistics and the encumbrance and warmth block with
  "covered by this item"; `Alt+S` reads the filter's items in layer order with
  encumbrance, conflicts, and the total encumbrance.  Interrupt speech with
  Control during a move; `Alt+R` repeats the last position.

## Stage 15: NPC dialogue

Spawn an NPC next to you through Debug menu > Spawning… > Spawn NPC and talk
to it with `C` and its direction.  Show a specific topic with Debug menu >
Dialogue… > Display talk topic, choosing the NPC and then the topic.

- **T123 - Conversation entry and history.**  Talk to the NPC.  Hear its
  name and first line once, followed by "Your response, N options".  Up
  reads the newest line, then older lines including your own ("You: …");
  Up at the oldest line reads it again.  Down returns to "Your response";
  Home and End jump to the oldest line and the prompt.  `Alt+S` speaks
  "Dialogue: <name>" and the position ("Conversation. 2 of 3").  `Alt+D` on
  a long line opens it in the document view by sentence; Escape speaks the
  line again.  Interrupt speech with Control; nothing is repeated
  automatically, and `Alt+R` repeats the last announcement.
- **T124 - Response list, branching, and return to gameplay.**  Press Enter
  on "Your response" and hear the prompt and the focused row ("a, …").  Up
  and Down speak one row with its key; after the responses hear "Shift+l,
  Look at", "Shift+s, Size up stats", and the other screen actions; a wrap
  says "Top" or "Bottom".  Escape returns to "Your response".  Enter on a
  response continues the conversation: hear only the NPC's new line and the
  new prompt, not your own choice.  Enter on "Shift+l, Look at" speaks the
  description.  Letter hotkeys choose a response from either level.  Choose
  a goodbye response and check the game returns to the map without extra
  dialogue speech.
- **T125 - Unavailable response.**  Display the topic
  `TALK_TEST_SHOW_ALWAYS2`.  Open the response list and hear "a, You should
  see this response … because. Unavailable: zero isn't greater than one."
  Press Enter or `a` on it; the row is spoken again and the conversation
  stays open.  `b` ("All done") ends it.
- **T126 - Consequence hint and confirmation.**  Display the topic
  `TALK_TEST_EFFECTS` and move to the multi-effect response: its row ends
  with "May provoke an attack".  Choose it; the confirmation asks "You may
  be attacked!  Proceed?".  Answer no and hear the focused response row
  again and stay in the response list.  Every response of this test topic
  has effects, so use a fresh NPC afterwards.
- **T127 - Non-conversation dialogue.**  Examine a slot machine, which
  opens this window without a speaker.  Place one next to you with Debug
  menu > Map… > Map editor (the map editor may need sighted help until its
  own stage), or find one in a casino.  Its text is spoken without a
  speaker name and the prompt says "Your choice, N options".  Up, Down,
  Enter, and Escape behave as in T123 and T124, without the screen actions.

## Stage 16: Trading

Spawn an NPC next to you through Debug menu > Spawning… > Spawn NPC, talk to
it with `C` and its direction, and choose "Care to trade?" when it is
offered, or show the topic `TALK_TEST_FACTION_TRUST` through Debug menu >
Dialogue… > Display talk topic and choose "Start trade.".  Give yourself
goods to sell, including a stack such as several batteries, through Debug
menu > Spawning… > Spawn item.  These responses end the conversation after
the trade.

- **T128 - Opening, rows, and pane switching.**  Open the trade.  Hear
  "Trade: <name>. Balance: credit $0.00, acceptable. Trader's goods. N
  items." and the focused row once.  Up and Down speak one row: name, price
  ("$2.00 each"), and "Unavailable: <reason>" on goods the trader keeps; a
  wrap says "Top" or "Bottom".  Tab speaks "Your goods", the item count, and
  your focused row, without the trader's goods; Tab again speaks the
  trader's pane the same way.  `Alt+D` reads the row with its location.
  `Alt+S` speaks the trader, balance, "Buying", "Selling", and the pane
  position.  Interrupt speech with Control; nothing is repeated
  automatically, and `Alt+R` repeats the last announcement.
- **T129 - Marking single and stacked goods.**  On a trader's item press
  Right and hear "<name>. Marked. Balance: you owe $…, trader refuses."
  Press Right again and hear "Unmarked" with the new balance.  On your
  stack, type a number such as 3 and Enter, or use `=` and `-`, and hear
  "3 marked" and a balance that moves toward credit.  Mark enough of your
  goods to reach a credit and hear "acceptable".  Press `F1` on a row to
  auto balance and hear the new marks and balance, or only the balance
  when nothing changes.  Mark more than you can carry and hear "Over
  weight limit" when the pane statistics show it.
- **T130 - Correcting, accepting, and cancelling.**  With the trader
  refusing, press Enter and hear the trader's refusal; after closing it,
  the focused row is spoken again.  Correct the balance and press Enter:
  hear "Looks like a deal!  Accept this trade?".  Answer no and hear the
  focused row again; press Enter and answer yes, then hear "Trade
  complete" and the game return to the map.  Open a new trade and press
  Escape: hear "Trade cancelled".
- **T131 - Bank balance.**  Press `F2` in the trade.  Without the
  `CAPITALISM` external option (the default), hear "Your promises of
  digital payment mean nothing here."  With it enabled by a mod, hear the
  new balance and "Bank balance: $…".

## Stage 17: Missions

Spawn an NPC next to you through Debug menu > Spawning… > Spawn NPC.  Give it
a mission through Debug menu > Player… > Edit player/NPC, choosing the NPC,
then "Add mission" and a type such as "Find a Book".  Talk to it with `C`,
ask about its missions, and accept.  Missions can be failed or completed
through Debug menu > Player… > Edit player/NPC, choosing yourself, then
"Edit missions (WARNING: Unstable!)", the mission, and "Fail mission" or
"Mark as complete".  Open the missions screen with `M`.

- **T132 - Opening, rows, and tabs.**  With two active missions open the
  missions screen.  Hear "Your missions. Active, 2 missions." and the
  focused row once: the title, "current objective" on the tracked one, and
  "deadline in <time>" when it has a deadline.  Up and Down speak one row;
  at either end the row repeats.  Right or Tab speaks "Completed", the
  count, and the focused row, or "no missions"; Left or Shift+Tab goes back.
  `Alt+S` speaks the tab, count, position ("2 of 2"), and the current
  objective.  Interrupt speech with Control; nothing is repeated
  automatically, and `Alt+R` repeats the last announcement.
- **T133 - Tracking a mission.**  On the active mission that is not the
  current objective press Enter and hear "Current objective: <title>".
  Move to the other mission and back; the new one says "current
  objective" and the other does not.  On the Completed or Failed tab press
  Enter and hear "Unavailable: only an active mission can be the current
  objective."
- **T134 - Details and location.**  Press `Alt+D` on an accepted mission.
  Hear the row with "Active" and its position; Down reads "Given by:" with
  the faction, the giver location in map tiles and direction, the
  description sentence by sentence starting with "Objective:", the
  deadline, and "Target: N map tiles <direction>, <meters>" or "No known
  location."  Escape returns and speaks the focused row again.
- **T135 - Completion and failure.**  Fail one mission and complete another
  through the debug route above; hear "Mission "<title>" is failed." and
  "... is successfully completed." as messages.  Open the missions screen:
  the Failed and Completed tabs list them with "failed <time> ago" and
  "completed <time> ago", and `Alt+D` reads the end time.

## Stage 18: Overmap navigation

Open the map with `m`.  To see more of the world, reveal it through Debug
menu > Map… > Overmap editor, press `r`, and choose a vision level; close
the editor with Escape and open the map again with `m`.

- **T136 - Opening and cursor movement.**  Open the map.  Hear "Overmap"
  and your tile ending in "here" once.  Move the cursor with the movement
  keys: each step speaks the terrain and its offset from you ("forest, 1
  north"); a step onto the same terrain speaks only its name; an unrevealed
  tile says "unexplored" and nothing about what is there.  `0` returns the
  cursor to you.  `Alt+S` speaks the cursor tile, level, and route;
  `Alt+D` adds the nearby city, vision level, and weather when the weather
  overlay is on.  Interrupt with Control; nothing is repeated
  automatically, and `Alt+R` repeats the last announcement.
- **T137 - Levels.**  Press `>` and `<`.  Each press speaks "Level -1." (or
  the new level) and the tile, including "1 level below" when it is not
  your level.  Return to your level and press `0`.
- **T138 - Notes.**  On a tile press `N`, type "B:Base", and confirm; hear
  the tile again with "note: Base".  Press `R` to mark it dangerous with a
  radius of 2 and hear "dangerous area, radius 2".  Press `L`: the notes
  list speaks "Map notes", the count, and rows such as "Base, 3 map tiles
  east, dangerous area, radius 2, field".  Enter moves the cursor to the
  note; Escape speaks the unchanged cursor tile.  Press `D` on the note,
  confirm, and hear the tile without the note.
- **T139 - Known places.**  With a note and a revealed town, press `Alt+L`.
  Hear "Known places", the count, and the nearest row with name, map tiles,
  direction, and markers such as "city" or "note: Base".  Up and Down speak
  one row; wrapping says "Top" or "Bottom".  Press `/`, type part of a name,
  and hear the filtered count.  Enter moves the cursor there and speaks the
  tile; `Alt+L` then Escape speaks the unchanged cursor.  An unrevealed town
  is not listed.
- **T140 - Mission location.**  Accept a mission with a target through the
  Stage 17 debug route (T132 setup).  Open the map and press `Alt+L`: the
  target row says "mission target: <title>, current objective".  Enter moves
  the cursor there and the tile says "mission target: <title>".
- **T141 - Search and routes.**  Press `/`, type "forest", and confirm.  Hear
  "Search results, N results" and the nearest match; Tab and Shift+Tab
  speak the next and previous result with "Top" or "Bottom" on wrap;
  `Alt+S` speaks "Result 2 of N"; Enter keeps the cursor there.  Press `W`
  on a revealed tile on your level and hear "Route to <tile>, N map tiles
  <direction>" with the travel length and "Press W again to travel".  Press
  `W` again, answer the travel question, and hear the Stage 08A travel
  summary.  On an unexplored tile, `W` says "No route to unexplored."  With
  no route, `g` and `c` say "No route selected."
