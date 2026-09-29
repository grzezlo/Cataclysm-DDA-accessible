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

## Stage 07: Advanced inventory and multiselect transfers

Wear a backpack.  Spawning… > Spawn an item places a hammer, 5 rocks, and a
plastic bag or other container on your tile; spawn a second backpack too.

- **Opening.**  Press `/`.  You hear "Advanced inventory", the source pane
  with its item count and capacity, the destination pane, and the focused
  row, in one announcement.  Pressing `Alt+R` repeats it.
- **Rows.**  Move with Up and Down, Page Up, Page Down, Home, and End.  Each
  key speaks one row: name, count when above one, weight, and volume.
  `Alt+D` adds the category and other details.  Interrupt a row with Control;
  the next key speaks only the new row.
- **Panes and areas.**  Press Tab: "Source:" with the other pane's area and
  item count is spoken once, then its row, without the capacity.  Press `5`
  for the tile below you and `I` for your inventory: each area change adds
  the capacity.  Press `Alt+S`: both panes, capacity, filter, and sort in one
  announcement.
- **Single, stacked, and partial moves.**  With the ground as source and the
  inventory as destination, press Enter on the hammer: when the screen
  returns you hear "Moved hammer to Inventory" and the next row.  Press `m`
  on the rocks and enter 2: "Moved rocks, 2, to Inventory".  Press `M` to
  move the rest of the stack.  Repeat from the inventory back to the ground.
- **Container.**  Select the plastic bag and press `C`: the source becomes
  "Container:" with its name and capacity.  Move a rock into and out of it.
- **Move all.**  Press `,` on the ground pane: "Moved N stacks to Inventory".
- **Failures.**  Take off the backpack and try to move the second backpack
  and then all rocks to the inventory; move an item to a wall tile with the
  direction keys.  The refusal popup is spoken and closing it returns
  silently to the same row.
- **Filter and sort.**  Press `F`, type "rock", and confirm: "Filter: rock"
  and the matching count, then the row.  Press `R` to reset: "Filter
  cleared".  Change the sort with `s`: "Sort:" and the new order.
- **Multiselect drop.**  Close the screen and press `d` (drop).  Rows say
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

- **Targets in the prompt.**  Press `o` (open) next to the closed door with
  another direction also valid, and auto-selection off.  You hear "Open
  where? Targets:", the door with its direction, then the input hint, in one
  announcement.  Tiles with the same target are grouped.  `Alt+R` repeats it.
- **Auto-selection.**  With auto-selection on and only one door, press `o`.
  You hear "Auto-selected:", the door, and its direction, then the result
  message.
- **No valid target.**  Press `c` (close) where nothing can be closed.  The
  failure message is spoken; with auto-selection off the prompt says "No
  valid targets".
- **Invalid direction.**  In the open prompt press a direction toward a
  wall.  You hear the wall, its direction, and "not a valid target".
- **Cancel.**  Open any direction prompt and press Escape: "Never mind." is
  spoken once and gameplay continues.
- **Examine.**  Press `e` next to furniture with items and a container such
  as a fridge.  The prompt names each target with its item count; choose one
  and the following menu or message is spoken.
- **Grab and release.**  Press `G` next to a chair.  The prompt names the
  furniture; choose it and hear the grab message; press `G` again to hear
  the release.
- **Smash.**  Press `s` next to a window and a corpse.  Both are listed as
  targets; choose one and the result and any confirmation are spoken.
- **Butcher.**  Stand on the corpse and press `B`.  The butcher menu is read
  through the shared menu speech.
- **Darkness.**  Map… > Change time to night, then open a prompt: tiles you
  cannot see are named "unseen".

## Stage 08A: Auto-travel

Start outdoors in daylight.  Debug menu > Map… > Change time helps if it is
dark.  Spawning… > Spawn a monster places a monster to interrupt travel.

- **Start summary.**  Press `x` (look around), move the cursor to a visible
  tile about ten tiles away, and press `Shift+t`.  You hear one announcement:
  "Traveling to", the target, its distance and direction, the step count, and
  the terrain along the route with counts, for example "grass 6, dirt road
  5".
- **Silent steps and arrival.**  During the same travel no step is spoken.
  On arrival you hear "Arrived:" and the tile name, once, and `Alt+R`
  repeats it.
- **Nearby item list.**  Press `V`, select an item a few tiles away, and
  press `Shift+t`.  The start summary and arrival are spoken as above.
- **Obstruction.**  Spawn a hostile monster on the route before starting.
  When it blocks the way you hear "Monster in the way.  Auto move canceled."
  followed by "Travel stopped:", the destination with its remaining distance
  and direction, and your position.
- **Danger query.**  Spawn a monster that comes into view during travel.  The
  cancel question names the reason and is read with its choices.  Answer no
  and travel continues silently; answer yes and you hear the cancel message
  and your position, without "Travel stopped".
- **Cancel with a key.**  During a long travel press `.` (pause) and answer
  yes.  Only the cancel message and your position are spoken.
- **Overmap travel.**  Press `m`, move the cursor a few map tiles away, press
  `Shift+w` twice, and confirm.  You hear "Traveling to", the overmap
  terrain, the distance in map tiles with direction, and the overmap terrain
  along the path.  Steps and local route changes are silent, and arrival
  speaks "You have reached your destination." and your position.  The
  overmap itself is made accessible in Stage 18; until then, choose the
  destination with sighted help or skip this scenario.
- **Speech interruption.**  Press `Shift` while the start summary is spoken.
  Speech stops and is not repeated; travel continues silently and the
  arrival is still spoken.

## Stage 09: Pickup, drop, and container transfers

Wear a backpack.  Debug menu > Spawning… > Spawn an item places items on
your tile; spawn two hammers, a stack of rocks, and something bulky such as
an anvil.

- **Single pickup.**  Press `g` and choose your tile.  Mark one hammer.  You
  hear the hammer, its selection state, the selection total, and "Weight …
  of … kg, volume … of … L".  Press Enter.  After the pickup you hear one
  sentence, "Picked up: hammer into backpack.", and no separate "You pick
  up" line.  `Alt+R` repeats it.
- **Stack and multiple items.**  Press `g`, mark the second hammer and the
  rocks with a count (type a number, then mark), and confirm.  One sentence
  names both items with their containers.
- **Predicted container.**  In the pickup list, press `Alt+D` on a row.  The
  details end with "Goes into:" and the container.
- **Capacity warning.**  Spawn several anvils and mark them in the pickup
  list.  Once the marked weight or volume passes the limit, the marking
  speech ends with "Over weight limit" or "Over volume limit".  `Alt+S`
  repeats the warning with the statistics.
- **Left behind.**  Take off the backpack, then pick up a hammer.  You hear
  "Not picked up: hammer, no room." once, without a second "Some items were
  not picked up!".
- **Drop several items.**  Press `d`, mark a hammer and the rocks, and
  confirm.  You hear "Dropped on", the ground type, and both items instead
  of "several items".
- **Insert into a container.**  Open the inventory, choose the backpack,
  pick Insert from its item menu, and mark an item.  Marking speaks the
  backpack's weight and volume against its limits; the result is the game
  message "You put your … into the backpack."
- **Ownership.**  Debug menu > Map… > teleport to a refugee center or
  another shop where items have an owner, stand next to a shelf, and press
  `g`.  Owned rows say "Not owned", and `Alt+D` names the owner.  Mark one
  and confirm: the theft question starts with "Not owned:" and the item
  name, then offers its choices.
- **Speech interruption.**  Press `Shift` while a marking or result sentence
  is spoken.  Speech stops and is not repeated; the next key speaks
  normally.

## Stage 09A: List position speech

In every list below, check that opening speaks the name and item count once,
that moving with the arrow keys speaks only the row with no "3 of 12", and
that `Alt+R` repeats the last row.

- **Menu list.**  Press `Enter` in gameplay to open the action menu.  You
  hear its title, "… items", the navigation hint, and the first row.  Press
  Down several times quickly: each press speaks only the row.  Press Up on
  the first row: you hear "Bottom." and the last row.  Press Down: you hear
  "Top." and the first row.  Press `Alt+D`: the details (or "No details
  available.") end with the position, for example "1 of 12".
- **Menu categories and filter.**  In a list with category tabs, such as
  Debug menu > Spawning… > Spawn an item, press Left or Right: you hear the
  category name, its item count, and the focused row.  Press `/`, type a
  filter, and confirm: you hear "Filter: …", the number of matching items,
  and the focused row.
- **Inventory.**  Press `i`.  You hear the title, the column name, "…
  items", and the first item.  Move with Up and Down; wrap past both ends
  and hear "Top." or "Bottom." before the item.  Press `Alt+S`: the summary
  ends with the position.  Switch columns with Tab where available: the new
  column's name and item count are spoken once.
- **Advanced inventory.**  Press `/`.  Wrap past both ends of the source
  pane and hear "Top." or "Bottom.".  Press `Alt+S`: the pane summary ends
  with the position, for example "2 of 5".
- **Nearby items and creatures.**  Spawn a few items and monsters nearby
  through the debug menu, then press `V`.  Moving speaks the item without
  "5 of 9" or "Location 1 of 3"; an item on several tiles says "3
  locations".  Wrap past both ends and hear "Top." or "Bottom.".  Press
  `Alt+S`: you hear the list name, the result count, the position, and for
  items on several tiles "Location 2 of 3".  Repeat on the creature tab.
- **Message log.**  Press `P`.  You hear "Message log" and the message
  count once.  Moving speaks only the message.  `Alt+D` or `Enter` speaks
  "Message 3 of 40" with the type and text.
- **Help and keybindings.**  Press `?`.  The topic count is spoken once;
  moving speaks only topics.  Press `Alt+S`: "Help, 23 topics, 4 of 23".
  Then open the keybinding list with `?` in any menu: opening says
  "Keybindings, … actions".  Wrap past both ends and hear "Top." or
  "Bottom.".  `Alt+D` ends with the position.
- **Main menu.**  On the New Game or World tab, press Left or Right to reach
  it: the tab name and its item count are spoken once.  Press Up on the
  first item to hear "Bottom.".  Press `Alt+S`: "Main menu, Tab …, 3 of 10,
  Item 1 of 4".
- **Speech interruption.**  Hold Down in a long list and press `Control`
  while a row is spoken.  Speech stops and is not repeated; the next key
  speaks only its row.
