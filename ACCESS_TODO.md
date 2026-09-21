# Accessibility roadmap

This file is the authoritative plan for unfinished accessibility work.  Stable
architecture, workflow, and completion rules are in `AGENTS.md`; completed work
and its verification record are in `ACCESS_DONE.md`.

The first supported configuration is the Windows x64 tiles build with NVDA.
Stage-specific inventories below are starting points and must be corrected when
implementation discovers a better integration point.

Stages 03–10 establish the minimum independent survival and combat loop.  Stages
11–21A cover the major progression, management, and setup interfaces.  Stages
22–23 are native release gates.  Stage 24 starts only after the native semantic
model is stable.

Unless a stage lists an exception, its automated gate includes
`test_access.bat` and a successful `build_win_x64.bat`.  The human gate for
native stages includes the general NVDA checks required by `AGENTS.md` plus the
scenarios listed for the stage.  Each stage's possible automated tests are a
hard gate: they must be implemented and passing before requesting human
verification.

## Stage 03 - Nearby item and creature lists

**Outcome:** The `V` interface provides efficient list-based exploration of
normally visible nearby items and creatures.

**Selected interaction:** Preserve the existing `V` list and its visible UI.
Opening announces the active item or creature mode, result count, and current
selection.  Focus movement gives a concise semantic row containing name, count,
direction, distance, and the most important item or creature state.  Existing
examine and compare commands provide detail on demand.  Tab changes mode and
announces its count and restored selection; filtering, sorting, paging, and
item-stack position changes announce the resulting state without rereading the
whole list.  Look handoff preserves the selected target and reannounces it on
return.  Escape restores and announces gameplay focus.  Empty modes are
explicit.  This interaction adds no accessibility-only visible screen.

**Code inventory:** `game::list_items_monsters`, `game::list_items`, and monster
list handling in `src/game.cpp`/`src/game.h`; `src/map_item_stack.cpp` for grouped
items.  The two `V` lists are bespoke `ui_adaptor` loops rather than `uilist`, so
their focus announcements belong in `src/game.cpp`.

**Implementation hints:**

- Entry point: `game::list_items_monsters` dispatches to `game::list_items` and
  the monster list.  The `V` action is `ACTION_LIST_ITEMS` in
  `src/handle_action.cpp`.
- Both lists use bespoke `ui_adaptor` loops.  Follow the focus-announcement
  policy used by `uilist`, but keep the hooks local to these list loops.
- `map_item_stack` in `src/map_item_stack.cpp` groups identical items by
  position; the accessibility announcement should use the stack's
  `display_name()` and `totalcount`.
- The relationship to `Alt+V` situation review: `Alt+V` is a spatial overview
  while `V` is the game's standard item/creature list with filtering, sorting,
  and look handoff.  Both coexist.
- Follow the announcement pattern in `game::accessibility_situation_review`
  (`game.cpp:8969-9047`) for distance/direction formatting.

**Success criteria:** Opening announces active item/creature mode and selection.
Navigation reports name, count, direction, distance, attitude, and meaningful
secondary detail.  Mode switching, sorting, filtering, paging, look handoff, and
return are keyboard operable and restore focus.  Empty results are explicit.

**Possible automated tests:** Capture both modes, focus changes, stacks, empty
results, mode switch, filter/sort/page changes, look handoff, and visibility
filtering.

**Required human verification:** Locate a chosen item and creature in a crowded
area, use filters and both list modes, hand off to look mode, and return without
losing context.

## Stage 04 - Player condition and gameplay status

**Outcome:** All important sidebar/HUD information is available on demand, while
urgent changes are announced once without turn-by-turn chatter.

**Code inventory:** `src/widget.cpp` and `src/display.cpp` for HUD data
extraction; `src/game.cpp` and `src/handle_action.cpp` for gameplay dispatch;
`src/player_display.cpp` for detailed status; character state providers;
`src/messages.cpp` to avoid duplicating existing game-log announcements.

**Implementation hints:**

- HUD data is computed by `display::` namespace functions in `src/display.cpp`:
  - Health: `display::health_text_color` (display.cpp:1439).
  - Stamina: `Character::get_stamina()` / `get_stamina_max()`.
  - Needs: `display::hunger_text_color` (:1417), `display::thirst_text_color`
    (:1406), `display::sleepiness_text_color` (:1449).
  - Pain: `display::pain_text_color` (:1463).
  - Temperature: `display::temp_text_color` (:1528).
  - Time: `display::time_string` (:218), `display::date_string` (:198).
  - Weather: `display::weather_text_color` (:1506).
  - Weariness: `display::weariness_text_color` (:1367).
- These functions return `std::pair<std::string, nc_color>` — strip the color,
  keep the text for accessibility.
- Widget rendering goes through `widget::color_text_function_string`
  (widget.cpp:1210-1239) which dispatches to the `display::` functions above.
- For urgent transitions: compare the text label between turns and announce only
  on change.  Threshold categories already exist in the `display::` functions
  (e.g., hunger goes through "Satisfied" → "Hungry" → "Very Hungry").
- Register a new action (e.g., `ACTION_ACCESSIBILITY_STATUS`) for the on-demand
  status command that reads all HUD values in sequence.

**Success criteria:** Concise and detailed commands cover health/pain, stamina,
focus, needs, temperature, effects, wielded item/ammunition, movement mode,
time/weather, and safe mode to the same precision as the HUD.  Critical
transitions are timely; routine value changes stay on demand.

**Possible automated tests:** Snapshot healthy, injured, exhausted, hungry,
effect-heavy, armed, and unarmed states; test transition thresholds, priority,
deduplication, and overlap with game messages.

**Required human verification:** Play through exertion, hunger/thirst,
pain/damage, effects, and weapon use.  Confirm urgent changes are timely and
concise/detail commands replace the need to inspect the sidebar visually.

## Stage 04A - Message log review

**Outcome:** A player can browse, search, and read the full scrollable message
history to catch up on missed or interrupted speech.

**Code inventory:** `Messages::dialog` in `src/messages.cpp`; the message
history deque in `messages_impl`; filter and scroll logic.

**Implementation hints:**

- The message log dialog is `Messages::dialog` (messages.cpp:529-885).
- Entry point: `Messages::display_messages()` (messages.cpp:925-930), triggered
  by `ACTION_MESSAGES` (the `P` key) in handle_action.cpp.
- Input context: `"MESSAGE_LOG"` (messages.cpp:620-630).  Actions: `"UP"`,
  `"DOWN"`, `"PAGE_UP"`, `"PAGE_DOWN"`, `"FILTER"`, `"RESET_FILTER"`, `"QUIT"`.
- The dialog currently uses a viewport offset model (messages.cpp:839-857)
  rather than a per-message cursor.  For accessibility, track an active cursor
  index within `folded_filtered` and announce each message line on UP/DOWN.
- Message text is in `game_message::get_with_count()` (messages.cpp:72-78).
- Elapsed time per message: `to_string_clipped(calendar::turn - msg_time)`
  (messages.cpp:725).
- Filter: `do_filter()` (messages.cpp:781-817) supports `<type>:<text>` prefix
  filtering.
- Register `"ACCESSIBILITY_REPEAT_LAST"` in the input context.
- Follow the `uilist.cpp` pattern: announce on open (title + message count),
  announce on cursor move (message text + elapsed time), announce filter
  activation/results.

**Success criteria:** Opening announces the log title and message count.
Navigation reads one message at a time with its elapsed time.  Filtering
announces the active filter and result count.  Clearing the filter and closing
restore gameplay context.

**Possible automated tests:** Capture log open, cursor movement, page
navigation, filter activation, filter results, filter clear, and close using
deterministic message fixtures.

**Required human verification:** Open the log after combat, scroll through
messages, apply and clear a filter, and confirm that interrupted speech can be
recovered by browsing history.

## Stage 05 - Reusable input controls and command discovery

**Outcome:** Common controls expose consistent semantics before more bespoke
screens depend on them, and accessibility commands can be discovered in-game.

**Code inventory:** `src/uilist.cpp`, `src/popup.cpp`, `src/input_popup.cpp`,
`src/string_input_popup.cpp`, `src/ui_iteminfo.cpp`, `src/input_context.cpp`,
`src/help.cpp`, `data/core/help.json`, and `data/raw/keybindings.json`.

**Implementation hints:**

- `uilist` already has accessibility hooks (uilist.cpp:978-984).  This stage
  extends coverage to disabled-entry reasons (the `uilist_entry::enabled`
  flag and `desc_enabled` description text), desc text on focus, and
  empty/error states.
- `query_yn` is implemented in `src/popup.cpp` as a `query_popup` with two
  options.  Hook the existing `query_popup::query_once()` (popup.cpp:321)
  path to announce the question and options.
- `query_int` / numeric input uses `string_input_popup` in
  `src/string_input_popup.cpp`.  Announce the prompt, current value, and
  bounds on open and on value change.
- `iteminfo_window::execute()` (ui_iteminfo.cpp:64-96) is a scrollable
  document viewer.  Announce the full text on open via
  `format_item_info()` (output.cpp:1156-1232) with color tags removed.
- Help: `src/help.cpp` and `data/core/help.json` define the help screen.
  Add an `Accessibility` category listing all accessibility actions.
- For keybinding discovery: `input_context` in `src/input_context.cpp`
  manages the keybinding UI invoked by `HELP_KEYBINDINGS`.

**Success criteria:** Lists, disabled entries, yes/no prompts, text and numeric
fields, quantity prompts, and scrolling detail announce label, role, value,
state, bounds, errors, and focus return.  Help and keybinding interfaces contain
an accessibility section and expose every accessibility action.  Intentionally
hidden input remains private.

**Possible automated tests:** Widget-level capture tests for entry, focus/value
change, disabled reason, bounds, editing, accept/cancel, scrolling, and focus
restoration; help/action-presence tests.

**Required human verification:** Exercise every control type, find and rebind
each accessibility action, read its help, resolve a binding conflict, and confirm
there are no silent controls or keyboard traps.

## Stage 06 - Inventory selector navigation and item information

**Outcome:** Inventory selector navigation, categories, item details, and
comparisons are usable without visual columns, colors, or highlights.

Stage 06 owns the shared `inventory_selector` navigation, category switching,
column switching, item information, and comparison flows.  Stage 06A owns
the specific item action flows (wield, wear, consume, etc.).  Stage 07 owns
the two-pane advanced inventory.  Stage 09 owns ground pickup and drop
transfers.

**Code inventory:** `src/inventory_ui.cpp` (classes `inventory_selector`,
`inventory_column`, `inventory_entry`, `inventory_pick_selector`,
`inventory_compare_selector`, `inventory_examiner`);
`src/inventory_ui.h` for the class hierarchy;
`src/game_inventory.cpp` (`game_menus::inv::common`, `game_menus::inv::compare`);
`src/ui_iteminfo.cpp` for detail scrolling.

**Implementation hints:**

- Class hierarchy: `inventory_selector` is the base multi-column controller
  (inventory_ui.cpp:2889-2918 for context registration).  It manages
  `inventory_column` instances, each holding `inventory_entry` items.
- Focus/selection change hook: `inventory_column::highlight(size_t, scroll_direction)`
  (inventory_ui.cpp:905-916) sets `highlighted_index`.  After highlight, read
  `get_highlighted()` (inventory_ui.cpp:1179) to get the focused entry.
- Semantic text for an entry: `preset.get_caption(entry)` (inventory_ui.cpp:735-751)
  returns the item name with stack count.  Extra column cells from
  `preset.get_cell_text(entry, i)` (inventory_ui.cpp:758-765) provide weight,
  volume, etc.  Denial reason is `entry.denial` string.
- Category navigation: `toggle_navigation_mode()` (inventory_ui.cpp:3330-3336)
  switches between `ITEM` and `CATEGORY` modes.
- Column switching: `set_active_column(index)` (inventory_ui.cpp:3172-3179)
  and `toggle_active_column(dir)` (inventory_ui.cpp:3308-3328).
- Stats header: `get_stats()` (inventory_ui.cpp:2732) returns weight, volume,
  and holster usage.
- Item examination: `action_examine(item_location)` (inventory_ui.cpp:3544)
  opens the `iteminfo_window`.
- Input context: registered in the `inventory_selector` constructor
  (inventory_ui.cpp:2889-2918) — register `"ACCESSIBILITY_REPEAT_LAST"` there.
- Follow `uilist.cpp` focus pattern: announce on open (title + stats), announce
  on selection change (caption + cells + denial), announce on column change
  (column name).

**Success criteria:** A selector reports its purpose, category/column, item,
count, location, relevant state, and disabled reason.  Categories, filtering,
comparison, detail scrolling, and cancel are announced.

**Possible automated tests:** Capture selector entry, category and item changes,
stacks, disabled entries, filters, comparison, details, and return focus; reuse
item-info fixtures.

**Required human verification:** Browse inventory, switch categories and
columns, examine an item, compare two items, and confirm details are ordered
usefully and verbosity remains manageable.

## Stage 06A - Item action flows

**Outcome:** Common item action flows expose their choices, confirmations,
quantity prompts, results, and errors through speech.

**Code inventory:** `src/game_inventory.cpp` (action-specific presets and
`inv_internal` loop); `src/item_action.cpp` (`game::item_action_menu`);
inventory selectors from Stage 06 where they serve specific actions.

**Implementation hints:**

- All single-item actions go through `inv_internal` (game_inventory.cpp:139-215),
  which wraps `inventory_pick_selector::execute()` with title, hint, and
  `none_message`.  Hook announcements in `inv_internal` for the shared flow.
- Action-specific presets define extra columns:
  - Wield: `weapon_inventory_preset` (game_inventory.cpp:1897) — DAMAGE, MOVES,
    TO HIT.
  - Wear: `wear_inventory_preset` (game_inventory.cpp:424).
  - Consume: `comestible_inventory_preset` (game_inventory.cpp:662) — CALORIES,
    QUENCH, JOY, HEALTH, SHELF LIFE.
  - Read: `read_inventory_preset` (game_inventory.cpp:1406) — READ TIME, SKILL.
  - Reload: `select_ammo_inventory_preset` (game_inventory.cpp:3007) — DAMAGE,
    AP, RANGE.
- Item action menu: `game::item_action_menu` (item_action.cpp:258-383) uses
  `uilist` with `desc_enabled = true`.  The uilist already speaks entry text
  and `ctxt` column, but the `desc` (method description) is not spoken on
  focus.  Add it to the focus announcement when `desc_enabled` is active.
- Consume needs hint: `get_consume_needs_hint()` (game_inventory.cpp:988-1012)
  formats hunger/thirst/fatigue — include in the selector's opening
  announcement.

**Success criteria:** Wield, wear/take off, use, consume, read, reload/unload,
and drop flows expose their action-specific columns, quantity prompts,
confirmation, results, and errors.

**Possible automated tests:** Capture selector entry for each action type,
action-specific column content, quantity input, failure, and return focus.

**Required human verification:** Complete every common item flow above, including
a stack, a disabled item, and a failed action.

## Stage 07 - Advanced inventory and multiselect transfers

**Outcome:** Two-pane and multiselect item transfers are independently usable.

**Code inventory:** `src/advanced_inv.cpp`, `src/advanced_inv_pane.cpp`, other
`src/advanced_inv_*` helpers, and multiselect paths in `src/inventory_ui.cpp`
(`inventory_multiselector`, `selection_column`).

**Implementation hints:**

- Entry point: `create_advanced_inv()` (advanced_inv.cpp:2480) instantiates
  `advanced_inventory` and calls `display()` (advanced_inv.cpp:2011-2112).
- Two-pane structure: `std::array<advanced_inventory_pane, NUM_PANES> panes`
  (advanced_inv.h:122), `src = left/right`, `dest` is always opposite.
- Pane switching: `"LEFT"` / `"RIGHT"` / `"TOGGLE_TAB"` (advanced_inv.cpp:1982-1986).
- Area selection: 15 `ITEMS_*` actions (inventory, worn, ground tiles, vehicle,
  container, parent).
- Item rows: name, charges, weight, volume, category (advanced_inv.cpp:395-460).
- Pane header: area title, weight/volume capacity (advanced_inv.cpp:321-360).
- Transfer: `action_move_item` (advanced_inv.cpp:1516-1600) and
  `move_all_items` (advanced_inv.cpp:1034-1180).
- Quantity prompt: `query_charges()` (advanced_inv.cpp:1602-1670).
- Input context: `"ADVANCED_INVENTORY"` (advanced_inv.cpp:1285).
- Multiselect in standard inventory: `inventory_multiselector` in
  inventory_ui.cpp.  `toggle_entry()` (inventory_ui.cpp:3874) and
  `toggle_entries()` (inventory_ui.cpp:3962) manage selection marks
  (`'-'`, `'+'`, `'#'` at inventory_ui.cpp:1796-1804).

**Success criteria:** Speech identifies source/destination pane, area, category,
selection, marks, quantity, weight/volume capacity, destination validity, and
transfer result.  Pane/area switching, filtering, paging, bulk selection, and
cancel preserve context without rereading both panes.

**Possible automated tests:** Capture pane and area changes, mark/unmark,
quantity, capacity failure, invalid destination, filters, bulk transfer, result,
and focus restoration; reuse advanced-inventory fixtures.

**Required human verification:** Move single, stacked, and multiple items between
inventory, ground, and a container in both directions; include capacity and
invalid-destination failures.

## Stage 08 - Directional and environmental interaction

**Outcome:** Direction selection and terrain/furniture interaction clearly expose
the target, choices, warnings, and outcome.

**Code inventory:** action dispatch in `src/handle_action.cpp`; adjacent-target
helpers in `src/game.cpp`; terrain/furniture interactions in `src/iexamine.cpp`;
shared prompts and lists.

**Implementation hints:**

- Examination dispatch: `game::examine(tripoint, bool)` (game.cpp:6369-6508)
  evaluates candidates in priority order: creatures → vehicles → partial
  constructions → traps → computers → furniture → terrain → items.
- Each terrain/furniture type can have an `examine` callback registered in its
  JSON definition, dispatching to functions in `src/iexamine.cpp`.
- `iexamine.cpp` is ~10,000 lines with dozens of handlers.  The accessibility
  approach should be generic: hook announcements at the dispatch level in
  `game::examine()` rather than per-handler.
- Common patterns in iexamine: `uilist` choice dialogs (doors, elevators,
  vending), `query_yn` prompts, and activity assignment.  These benefit from
  Stage 05 shared control hooks.
- Direction selection uses `choose_adjacent` which has directional input and
  announces the target tile.
- Smash: `ACTION_SMASH` in handle_action.cpp → `game::smash()`.
- Butcher: `ACTION_BUTCHER` → `game::butcher()` which uses `uilist`.
- Grab/release: `ACTION_GRAB` → `game::grab()`.

**Success criteria:** Direction selection reports direction and normally visible
target.  Examine, open/close, grab/release, haul, smash, butcher, and common
terrain/furniture actions announce valid choices, disabled reasons, warnings,
result, cancel, and restored gameplay context.

**Possible automated tests:** Map fixtures for doors, furniture, appliances,
containers, corpses, hazards, valid/invalid directions, nested choices,
confirmation, cancellation, and completion.

**Required human verification:** Examine and manipulate a door, container,
furniture, appliance, hazard, and corpse; deliberately choose invalid directions
and cancel nested prompts.

## Stage 08A - Auto-travel and pathfinding

**Outcome:** A player can travel to distant tiles without step-by-step manual
movement, with clear announcements of destination, progress, interruptions, and
arrival.

**Code inventory:** `src/character.cpp` (route storage and step generation);
`src/activity_handlers.cpp` (`travel_do_turn`); `src/game.cpp`
(`cancel_auto_move`); `src/handle_action.cpp` (auto-travel mode toggle);
`src/overmap_ui.cpp` (overmap path selection — the semantic overmap itself
is Stage 18, but travel initiation is in scope here).

**Implementation hints:**

- Route storage: `Character::set_destination(route, ...)` (character.cpp:11546).
- Step generation: `Character::get_next_auto_move_direction()`
  (character.cpp:11609-11636) converts delta to next waypoint into a movement
  action.
- Route abort: `Character::abort_automove()` (character.cpp:11562-11568).
- Local auto-travel mode: toggled via `"auto_travel_mode"` in
  handle_action.cpp:2812.  Pressing a direction calculates a ray of up to
  12 tiles via `here.route()` and calls `set_destination()`
  (handle_action.cpp:2371-2384).
- Look-around travel: `"TRAVEL_TO"` in look-around calls `safe_route_to()`
  (game.cpp:8488) and `u.set_destination()` (game.cpp:8493).
- Overmap fast travel: `create_path` (overmap_ui.cpp:1900-1928) prompts
  "Travel to this point?", sets `omt_path`, and assigns `ACT_TRAVELLING`.
- Travel per-turn: `travel_do_turn` (activity_handlers.cpp:1640-1657) routes
  local submap path and calls `set_destination()`.
- Interruptions: `cancel_auto_move(Character, text)` (game.cpp:1586-1598)
  queries `query_yn("Cancel auto move?")`.
- Arrival message: `_("You have reached your destination.")` in travel_do_turn
  (activity_handlers.cpp:1653).
- For accessibility: announce destination name/direction on start, announce
  terrain changes during travel (reuse `announce_movement`), announce
  interruption reason, announce arrival.

**Success criteria:** Starting auto-travel announces destination and direction.
Each meaningful terrain change during travel is announced without flooding.
Obstruction and danger interruptions announce the reason and offer clear
choices.  Arrival is announced.  Cancel is immediate and silent.

**Possible automated tests:** Capture travel start, terrain changes during
travel (deduplicated), obstruction interrupt, danger interrupt, arrival, and
cancel using deterministic map/route fixtures.

**Required human verification:** Auto-travel to a visible destination via
look-around, trigger an obstruction, cancel auto-travel, and confirm speech
remains manageable during movement.

## Stage 09 - Pickup, drop, and container transfers

**Outcome:** Local item transfer workflows expose item, quantity, source,
destination, capacity, ownership, and result.

**Code inventory:** `src/pickup.cpp`, `src/game_inventory.cpp`
(`game_menus::inv::pickup`, `game_menus::inv::multidrop`),
`src/inventory_ui.cpp` (`pickup_selector`, `inventory_drop_selector`,
`inventory_insert_selector`).

**Implementation hints:**

- Interactive pickup entry: `game_menus::inv::pickup()` (game_inventory.cpp:2425)
  creates `pickup_selector` (inventory_ui.cpp:4244-4396).
- `pickup_selector::execute()` (inventory_ui.cpp:4278-4315) runs the
  multi-select loop.  Additional actions: `"WEAR"` and `"WIELD"` for direct
  equip from ground (inventory_ui.cpp:4248-4249).
- Transfer execution: `Pickup::do_pickup()` (pickup.cpp:357-408) processes
  selected items, calling `pick_one_up()` (pickup.cpp:181-355).
- Problematic pickup: `handle_problematic_pickup()` (pickup.cpp:84-110) shows
  spill/wield/cancel choices.
- Ownership warning: `Pickup::query_thief()` (pickup.cpp:112-143).
- Summary message: `show_pickup_message()` (pickup.cpp:55-68) — already
  reaches the live message system.
- Drop: `game_menus::inv::multidrop()` (game_inventory.cpp:2401) uses
  `inventory_drop_selector` (inventory_ui.cpp:4079).
- Insert into container: `game_menus::inv::holster()` (game_inventory.cpp:2008)
  uses `inventory_insert_selector` (inventory_ui.cpp:4197).
- Pickup builds on Stage 06 `inventory_selector` hooks.

**Success criteria:** Single and multi-item pickup/drop, quantities, container
choice, spill/unseal warnings, capacity, ownership/theft warnings, success, and
failure are keyboard operable and announced.  Returning restores local context.

**Possible automated tests:** Capture ground stacks, multiselect, quantities,
container choice, insufficient capacity, ownership warning, cancel, and result
using deterministic item/map fixtures.

**Required human verification:** Pick up and drop single, stacked, and multiple
items; transfer to/from a container; exercise capacity and ownership warnings.

## Stage 10 - Ranged and thrown targeting

**Outcome:** A player can inspect targets, choose an aim action, and fire or throw
with the same normally visible tactical information.

**Code inventory:** `target_ui` class in `src/ranged.cpp`/`src/ranged.h`;
combat dispatch in `src/game.cpp` and `src/handle_action.cpp`; existing result
messages in `src/messages.cpp`.

**Implementation hints:**

- Core class: `target_ui` (ranged.cpp:210-469).
- Entry points: `target_handler::mode_fire()` (ranged.cpp:482),
  `target_handler::mode_throw()` (ranged.cpp:496),
  `target_handler::mode_reach()` (ranged.cpp:509),
  `target_handler::mode_spell()` (ranged.cpp:570).
- Main targeting loop: `target_ui::run()` (ranged.cpp:2673-2948).
- Input context: `"TARGET"` (ranged.cpp:2981).  Actions include
  `register_directions()`, `"FIRE"`, `"NEXT_TARGET"`, `"PREV_TARGET"`,
  `"SWITCH_MODE"`, `"SWITCH_AMMO"`, `"AIM"`, `"AIMED_SHOT"`,
  `"CAREFUL_SHOT"`, `"PRECISE_SHOT"`.
- Cursor movement: `handle_cursor_movement()` (ranged.cpp:3033-3090) and
  `set_cursor_pos()` (ranged.cpp:3092-3150).
- Target cycling: `NEXT_TARGET` / `PREV_TARGET` (ranged.cpp:3075-3078) cycle
  through the `targets` vector.
- Semantic text:
  - Title: `uitext_title()` (ranged.cpp:3946-3959) — "Firing %s", "Throwing %s".
  - Range/elevation/target count: ranged.cpp:4130-4151.
  - Gun & ammo: ranged.cpp:4153-4181.
  - Recoil: ranged.cpp:4183-4200 ("None", "Low", "Medium", "High").
  - Target info: creature name/health via `dst_critter->print_info()`
    (ranged.cpp:4270), vehicle name (ranged.cpp:4300), terrain (ranged.cpp:4315).
  - Hit probability: `print_aim()` (ranged.cpp:2169-2325) for firearms,
    `draw_throw_aim()` (ranged.cpp:2327-2420) for thrown.
- Safe-mode warnings are handled by `game.cpp` threat announcements (already
  completed).  The safe-mode configuration UI (`src/safemode_ui.cpp`) is a
  rule editor owned by Stage 21A.

**Success criteria:** Target mode reports selected target, attitude, direction,
distance, visible description, hit information, obstruction, ammunition, and aim
mode when shown visually.  Target cycling, cursor movement, aim-mode selection,
aiming, firing, throwing, dangerous-action confirmation, and cancel are complete.
Threat and combat messages do not duplicate or stale the target announcement.

**Possible automated tests:** Capture entry, target cycling, empty target set,
manual cursor, obstruction, out-of-range, aim/ammunition change, confirmation,
fire/throw, result, cancel, visibility, and deterministic ordering.

**Required human verification:** Use melee context, a firearm, and a thrown weapon
against one and several targets, including obstruction and out-of-range cases.
Confirm target choice is reliable under rapid navigation and interrupted speech.

## Stage 11 - Crafting and disassembly

**Outcome:** Recipe selection, requirements, batch choices, crafting, and
disassembly are independently usable.

**Code inventory:** `src/crafting_gui.cpp`, crafting/disassembly selectors in
`src/game_inventory.cpp`, and crafting state providers used by those screens.

**Implementation hints:**

- Entry point: `select_crafter_and_crafting_recipe()` (crafting_gui.cpp:1246-2045).
- Input context: `"CRAFTING"` (crafting_gui.cpp:622).
- Main loop: crafting_gui.cpp:1758-2044.
- Category/subcategory tabs: drawn by `draw_recipe_tabs` (crafting_gui.cpp:2438)
  and `draw_recipe_subtabs` (crafting_gui.cpp:2495).
- Recipe list entry: `current[line]->result_name()` (crafting_gui.cpp:1704).
- Detailed recipe info: `recipe_info()` (crafting_gui.cpp:398-615) computes
  all attributes — skill requirements, proficiencies, success/failure chance,
  duration, exertion, light conditions, ingredients, tools, components.
- Search: `filter_recipes()` (crafting_gui.cpp:948-1010).
- Batch cycling: `"CYCLE_BATCH"` action.
- Related recipes: `"RELATED_RECIPES"` action.
- Disassembly: `game_menus::inv::disassemble()` (game_inventory.cpp:655) uses
  `disassemble_inventory_preset` (game_inventory.cpp:600).

**Success criteria:** Category, recipe, variant, availability, missing
requirements, skill, time, batch size, related recipe, search, confirmation,
start, failure, and result are announced.  Disassembly exposes target, returned
components, requirements, confirmation, and result.

**Possible automated tests:** Capture categories, filters, available/unavailable
recipes, requirement detail, variant/batch changes, related recipes, start,
failure, disassembly, and return focus using crafting fixtures.

**Required human verification:** Craft single and batch recipes, inspect a recipe
with missing requirements, search/filter, and disassemble an item with NVDA.

## Stage 12 - Construction and placement

**Outcome:** Construction project selection and directional placement expose
category, requirements, placement validity, and activity start.

**Code inventory:** `src/construction.cpp` (catalog UI, placement cursor,
validation); `src/handle_action.cpp` (action dispatch).

**Implementation hints:**

- Entry point: `construction_menu(bool)` (construction.cpp:480-1099).
- Input context: `"CONSTRUCTION"` (construction.cpp:531-544).
- Main loop: construction.cpp:920-1094.
- Category tabs: `construct_cat[tabindex].name()` (construction.cpp:843).
- Project name: `group->name()` (construction.cpp:857).
- Requirement details computed in `recalc_buffer` lambda
  (construction.cpp:568-745):
  - Stage title and resulting terrain: construction.cpp:613-662.
  - Skill requirements: construction.cpp:669-684.
  - Precursor terrain: construction.cpp:692-707.
  - Duration: `current_con->get_folded_time_string()` (construction.cpp:714).
  - Tools: `current_con->requirements->get_folded_tools_list()`
    (construction.cpp:716).
  - Components: `get_folded_components_list()` (construction.cpp:719).
- Placement: `place_construction()` (construction.cpp:1233-1335) uses
  `choose_adjacent` (construction.cpp:1261-1283) for directional cursor.
- Validation: `can_construct()` (construction.cpp:1425-1442).
- Failure reason: `con.explain_failure(pnt)` (construction.cpp:1290).
- On confirm: starts `player_activity(ACT_BUILD)` (construction.cpp:1330).

**Success criteria:** Construction category/project, requirements, variants,
placement direction, valid/invalid tile, and disabled reason are announced.
Selecting a project and placing it starts the activity with confirmation.

**Possible automated tests:** Capture project focus, category switching,
missing requirements, placement cursor, invalid tile, activity start, and
return focus.

**Required human verification:** Browse construction projects, attempt an
invalid placement, complete a valid placement, and confirm the activity starts.

## Stage 12A - Activities, interruptions, and long actions

**Outcome:** Long-running activities expose progress on demand, interruption
reasons and choices, resume/cancel, completion, and failure without
turn-by-turn flooding.  Sleep, wait, and book reading are included.

**Code inventory:** `src/player_activity.cpp`, `src/activity_actor.cpp`,
`src/activity_handlers.cpp`, and `src/distraction_manager.cpp`.

**Implementation hints:**

- Activity lifecycle:
  - Start: `player_activity::start_or_resume()` (player_activity.cpp:222-233)
    fires `event_type::character_starts_activity`.
  - Per-turn: `player_activity::do_turn()` (player_activity.cpp:235-418).
  - Completion: fires `event_type::character_finished_activity`
    (player_activity.cpp:394).
  - Cancel: `player_activity::canceled()` (player_activity.cpp:420-428) fires
    the same event with `canceled = true`.
- Progress: `get_progress_message()` (player_activity.cpp:170-220) returns
  verb + percentage or verb + extra_info.  Use this for on-demand progress
  via `Alt+R`.
- Stamina exhaustion: `uilist tired_query` (player_activity.cpp:351-369)
  prompts "Keep trying?" with continue/stop/force options.
- Interruptions: `game::cancel_activity_or_ignore_query()` (game.cpp:1600-1653)
  uses `query_popup` with context `"CANCEL_ACTIVITY_OR_IGNORE_QUERY"` and
  options YES/NO/IGNORE/MANAGER.
- Distraction types: `distraction_type` enum in enums.h:334-356 (noise, pain,
  hostile spotted, weather, etc.).
- Distraction settings UI: `distraction_manager_gui::show()`
  (distraction_manager.cpp:49-175), context `"DISTRACTION_MANAGER"`.
- Sleep/rest: `ACT_WAIT` and `ACT_WAIT_WEATHER` with finish handler
  `wait_finish` (activity_handlers.cpp:1480-1488).
- Book reading: dispatched through the modern `activity_actor` system;
  `read_activity_actor` handles chapter progress and skill gain.
- Pattern: hook `start_or_resume` to announce activity start, use
  `set_last_announcement` with `get_progress_message()` for on-demand
  repeat (following the Stage 02A pattern), and announce completion/cancel
  via `announce_status`.

**Success criteria:** Activities announce start, meaningful status on demand,
interruption reason and choices, resume/cancel, failure, and completion without
turn-by-turn flooding.  Sleep, wait, and book reading are covered.

**Possible automated tests:** Capture activity start/status, deduplicated
progress, interruption policy, resume/cancel, failure, completion, and
on-demand progress repeat.

**Required human verification:** Run a long activity (crafting or construction),
trigger an interruption, resume it, cancel another, sleep, wait, and read a
book chapter.

## Stage 13 - Character information and layering

**Outcome:** All read-only character information presented in tabs, rows, colors,
and detail panes is semantically reviewable, including the encumbrance and
clothing layering view.

**Code inventory:** `src/player_display.cpp` and the character/body/morale/skill
providers it uses.

**Implementation hints:**

- Entry point: `Character::disp_info(bool)` (player_display.cpp:1520-1977).
- Input context: `"PLAYER_INFO"` (player_display.cpp:1675-1704).
- Tab enum: `player_display_tab` (player_display.cpp:289-299) — `stats`,
  `encumbrance`, `speed`, `skills`, `traits`, `bionics`, `effects`,
  `proficiencies` (8 tabs).
- Tab switching: `"LEFT"` / `"RIGHT"` / `"NEXT_TAB"` / `"PREV_TAB"`
  (player_display.cpp:1338-1350).  Direct jump: `"SELECT_STATS_TAB"` etc.
  (player_display.cpp:1441-1490).
- Row navigation: `navigate_ui_list()` (player_display.cpp:1293-1306).
  Skills tab skips category headers via `skip_skill_headers`
  (player_display.cpp:1302).
- Per-tab semantic text:
  - Stats: `display_stat()` (player_display.cpp:427-448) — current vs base.
    Info: `draw_stats_info` (player_display.cpp:483-570).
  - Encumbrance: body part name, base + layer penalty, temperature
    (player_display.cpp:572-589).  Info: `get_encumbrance_description`
    (player_display.cpp:228-272) — this is the layering review screen.
  - Speed: `get_speedlist_entries()` (player_display.cpp:955-988) — cause,
    type (S/M), signed value.
  - Skills: `get_HeaderSkills()` (player_display.cpp:1653) — category
    headers, name, level, exercise %, rust, training status.
  - Traits: `get_mutations_variants()` (player_display.cpp:1628) — name,
    description, purifier status.
  - Bionics: `bionic_grouping` (player_display.cpp:664-668) — name, count,
    power.
  - Effects: `effect_name_and_text` gathered at player_display.cpp:1525-1624.
  - Proficiencies: `display_proficiencies()` (player_display.cpp:325) —
    name, progress %.
- Info pane scrolling: `"SCROLL_INFOBOX_UP"` / `"SCROLL_INFOBOX_DOWN"`
  (player_display.cpp:1431-1439).

**Success criteria:** Tab/section identity and focused row are explicit.  Stats,
body condition, encumbrance with layering detail, skills, traits, effects,
morale, speed, needs, and proficiencies expose the values and explanations
visible in the standard UI.  Scrolling and tab changes preserve focus and do not
reread unchanged content.

**Possible automated tests:** Snapshot representative healthy, injured,
encumbered, effect-heavy, skilled, and morale states; capture tab, row, detail,
scroll, and return-focus changes.

**Required human verification:** Navigate every character-information tab in
simple and information-dense characters and find specified values without visual
assistance.

## Stage 14 - Bionics and mutations

**Outcome:** Bionic and mutation capabilities can be inspected, activated, and
configured without relying on layout or color.

**Code inventory:** `src/bionics_ui.cpp`, `src/mutation_ui.cpp`, and their
owning data/state modules.

**Implementation hints:**

- Bionics entry: `avatar::power_bionics()` (bionics_ui.cpp:575-1049).
  Context: `"BIONICS"` (bionics_ui.cpp:659).
- Bionic tabs: `TAB_ACTIVE` / `TAB_PASSIVE` with counts
  (bionics_ui.cpp:381-385).
- Bionic entry text: `build_bionic_powerdesc_string()` (bionics_ui.cpp:364-374)
  — name, power cost, per-turn drain, ON/OFF status.
- Power display: `draw_bionics_titlebar` (bionics_ui.cpp:212-311) — current
  power in kJ/J/mJ vs max, available fuels.
- Examine mode: `"TOGGLE_EXAMINE"` switches between activating and examining;
  `draw_description` (bionics_ui.cpp:400-425) renders detail.
- Mutations entry: `avatar::power_mutations()` (mutation_ui.cpp:100-673).
  Context: `"MUTATIONS"` (mutation_ui.cpp:217).
- Two columns: `"Passive:"` and `"Active:"` (mutation_ui.cpp:253-254),
  switched via `"NEXT_TAB"` / `"PREV_TAB"`.
- Active trait text: name + cost + resource + cooldown + active status
  (mutation_ui.cpp:310-349).
- Description: `mutation_desc(id)` (mutation_ui.cpp:358-362).

**Success criteria:** Each screen reports tab/column, selection, active/passive
state, cost, cooldown, description, and disabled reason.  Activation,
deactivation, examine toggle, and focus return are complete.

**Possible automated tests:** Capture screen entry, tab/column changes,
selection/detail, activation/deactivation, insufficient-resource, and return.

**Required human verification:** Inspect and operate at least one bionic and
mutation, including a disabled or insufficient-resource case.

## Stage 14A - Spells and martial arts

**Outcome:** Spell and martial-style selection, inspection, and activation are
independently usable.

**Code inventory:** `src/magic.cpp` (spell UI), `src/martialarts.cpp` and
`src/character.cpp` (style picker), and their owning data modules.

**Implementation hints:**

- Spell selection: `known_magic::select_spell(Character)` (magic.cpp:3246-3348)
  creates `uilist spell_menu` with category tabs.
- Spell menu context: `"SPELL_MENU"` (magic.cpp:3286).
- Spell callback: `spellcasting_callback` (magic.cpp:2745-2827) handles the
  side panel.
- Spell details: `display_spell_info(index)` (magic.cpp:2916-3148) — class,
  flags, cast time, targets, range, AoE, damage, duration, requirements.
  Not currently spoken on selection change — add it.
- Spell entry: `spell::name()`, hotkey, can-cast status, favorite marker.
- Categories: "all", "favorites", and spell classes (magic.cpp:3295-3314).
- Style picker: `character_martial_arts::pick_style(Character)` in
  character.cpp:13690-13774, uses `uilist` with context
  `"MELEE_STYLE_PICKER"`.
- Style details: `ma_details_ui` class (martialarts.cpp:2209-2532), context
  `"MA_DETAILS_UI"` (martialarts.cpp:2255).  Shows general info, buffs,
  techniques, and compatible weapons.
- `"SHOW_DESCRIPTION"` in the style uilist opens the details window.

**Success criteria:** Spell list reports name, class, can-cast, cost, and key
detail on focus.  Spell details are available on demand.  Style picker reports
name and description; style details show techniques, buffs, and weapon
compatibility.  Activation, selection, and focus return are complete.

**Possible automated tests:** Capture spell/style list entry, selection changes,
detail access, category switching, can-cast/disabled states, and return.

**Required human verification:** Inspect and cast a spell, select a martial
style, and view its details with NVDA.

## Stage 15 - NPC dialogue

**Outcome:** Conversation text and response selection support complete branching
dialogue without repeated rereading.

**Code inventory:** dialogue rendering, response input, and nested menus in
`src/npctalk.cpp`/`src/npctalk.h`; shared lists and popups where actually used.

**Implementation hints:**

- Entry / response loop: `dialogue::gen_responses()` (npctalk.cpp:2950-3050).
- Input context: `"DIALOGUE_CHOOSE_RESPONSE"` (npctalk.cpp:2953).
- History scrolling: `d_win.set_up_scrolling(ctxt)` registers scrolling actions.
- Speaker name: `actor(true)->disp_name()` (npctalk.cpp:2988).
- Responses: `response_lines` generated via `talk_response::create_option_line`
  with hotkey `evt` and text `td.text`.  Currently selected response is
  `d_win.sel_response`.
- New dialogue lines: `d_win.add_to_history()` handles incoming dialogue text.
  Hook speech to announce new speaker text on topic transitions, and announce
  focused response option text on cursor movement.

**Success criteria:** Speaker, newly presented text, focused response, visible
consequence hints, unavailable reason, topic transition, confirmation, and exit
are announced once.  Scrolling history and nested menus retain dialogue context.

**Possible automated tests:** Reuse NPC talk fixtures to capture entry, response
movement, disabled response, topic transition, repeated redraw, history scroll,
nested menu, confirmation, and exit.

**Required human verification:** Complete a branching conversation with a
disabled response, nested choice, long text, and clean return to gameplay.

## Stage 16 - Trading

**Outcome:** Buying, selling, quantity selection, balance, and capacity are clear
in the two-party trade interface.

**Code inventory:** `src/trade_ui.cpp`, `src/npctrade.cpp`, inventory selectors,
and item information used by trade.

**Implementation hints:**

- Two panes: `_panes[_you]` and `_panes[_trader]` in `trade_ui`
  (trade_ui.cpp:48-60).  Derived from `inventory_multiselector`.
- Active pane switching: `_cpane = -_cpane + 1` in `trade_ui::_process`
  (trade_ui.cpp:266-268).
- Balance & Acceptance: `_balance`, `cost_str` ("Credit %s" / "Debt %s"),
  evaluated by `npc_trading::npc_will_accept_trade()` (trade_ui.cpp:308-316).
- Confirmation check: `trade_ui::_confirm_trade()` (trade_ui.cpp:276-302)
  validates credit limits, pocket capacity, and prompts `query_yn()`.
- Unit price: `trade_preset` formats price via
  `format_money(npc_trading::trading_price(...))` (trade_ui.cpp:54).
- Autobalance action: `trade_ui::autobalance()` (trade_ui.cpp:215-228).
- Bank balance: `trade_ui::bank_balance()` (trade_ui.cpp:230-244).

**Success criteria:** Active party/pane, item, marks, quantity, unit/total price,
running balance, capacity, acceptance state, ownership, invalid reason,
confirmation, and result are announced.  Pane changes do not reread both sides.

**Possible automated tests:** Deterministic fixtures for pane/item movement,
marking, stack quantity, balance updates, insufficient funds/capacity, acceptance,
cancel, result, and restored dialogue focus.

**Required human verification:** Buy and sell single and stacked items, inspect
details, correct an unfavorable/invalid trade, accept it, and return to dialogue.

## Stage 17 - Missions

**Outcome:** Mission discovery, acceptance, tracking, details, and completion are
usable without visual tabs or map markers alone.

**Code inventory:** `src/mission_ui.cpp`, `src/mission.cpp`, and mission dialogue
in `src/npctalk.cpp`.  Stage 18 owns the later overmap handoff.

**Implementation hints:**

- Entry point: `mission_ui::draw_mission_ui()` (mission_ui.cpp:91-120).
- Input context: `ctxt` (mission_ui.cpp:93-108) with actions
  `register_navigate_ui_list()`, `NEXT_TAB`, `PREV_TAB`, `CONFIRM` ("Set
  selected mission as current objective"), `QUIT`.
- Tabs: `ACTIVE`, `COMPLETED`, `FAILED` (`mission_ui_tab_enum` in
  mission_ui.cpp:168-185).
- Missions list: `get_avatar().get_active_missions()`,
  `get_completed_missions()`, `get_failed_missions()`.
- Mission attributes: title `m->get_type().name()`, description
  `m->get_description()`, deadline `m->get_deadline()`, target location
  `m->get_target()`.
- Confirmation: `CONFIRM` sets tracked mission.

**Success criteria:** Mission category/state, title, giver, objective, deadline,
reward, tracked state, normally known location, success/failure, and focus return
are announced.  The mission screen exposes a clear map action, but its semantic
map handoff is completed and accepted in Stage 18.

**Possible automated tests:** Reuse mission fixtures to capture categories,
selection/details, accept, track/untrack, known/unknown locations, deadline,
completion/failure, and invocation of the map action.

**Required human verification:** Accept, review, track, locate, complete, and turn
in a mission; inspect one mission without a known location and one failed state.

## Stage 18 - Overmap navigation

**Outcome:** Known-world exploration and route planning are spatially
understandable without revealing unexplored information.

**Code inventory:** `src/overmap_ui.cpp`, known-state providers in
`src/overmapbuffer.cpp` and `src/overmap.cpp`.

**Implementation hints:**

- Entry point: `ui::omap::display()` (overmap_ui.cpp:2506) invokes static
  `display()` (overmap_ui.cpp:1930).
- Input context: `"OVERMAP"` (overmap_ui.cpp:1981-2025).
- Main loop: overmap_ui.cpp:2041-2246.
- Cursor movement: `get_direction_rel_omt(action)` adjusts `curs`
  (overmap_ui.cpp:2070-2075).  Z-level: `"LEVEL_UP"` / `"LEVEL_DOWN"`
  (overmap_ui.cpp:2096-2099).
- Semantic text at cursor: `draw_om_sidebar` (overmap_ui.cpp:968) computes:
  - Terrain: `overmap_buffer.get_description_at(sm_pos)` (overmap_ui.cpp:1047).
  - Explored/unexplored: "unexplored" (overmap_ui.cpp:1078).
  - Notes: `overmap_buffer.has_note(curs)` / `overmap_buffer.note(curs)`.
  - Weather: `get_weather_at_point(cursor_pos)->name.translated()`
    (overmap_ui.cpp:1088).
  - Mission distance: overmap_ui.cpp:1152-1167.
- Search: `search()` (overmap_ui.cpp:1359) scans radius and enters cycling
  loop (overmap_ui.cpp:1486-1503).  Result info: index/total, distance,
  direction (overmap_ui.cpp:1463-1470).
- Notes: `draw_notes(origin)` (overmap_ui.cpp:462) is a `uilist` list.
  `create_note()` (overmap_ui.cpp:1262) is a note editor dialog.
- Route: `get_overmap_path_to()` (overmap_ui.cpp:1807) generates A* path.
  `try_travel_to_destination()` (overmap_ui.cpp:1863) initiates travel.
- Hook: on cursor movement announce terrain, note, distance from player,
  and direction.

**Success criteria:** Cursor movement reports relative direction/distance,
z-level, known terrain, notes, mission markers, and route state.  Search, notes,
route selection/travel, and return are keyboard operable.  Unknown map content
stays hidden.

**Possible automated tests:** Capture cursor and z-level movement, known/unknown
tiles, search, notes, mission handoff, route creation/failure/travel, and
visibility filtering.

**Required human verification:** Find a known destination, add/edit a note,
locate a mission, plan/follow a route, change z-level, and return.

## Stage 18A - Zone management

**Outcome:** Zone listing, creation, placement, editing, and deletion are
independently usable.

**Code inventory:** `src/clzones.cpp` (zone types, options, queries),
`src/game.cpp` (`game::zones_manager`).

**Implementation hints:**

- Zone manager entry: `game::zones_manager()` (game.cpp:7447).
- Input context: `"ZONES_MANAGER"` (game.cpp:7503-7523).
- Main loop: game.cpp:7675-7890.
- Zone list row text: enabled status `[X]`/`[ ]`, displayed marker, name,
  distance, type (game.cpp:7715-7735).
- Zone type selector: `zone_manager::query_type()` (clzones.cpp:583) uses
  `uilist` — benefits from existing hooks.
- Zone name: `zone_manager::query_name()` (clzones.cpp:568) uses
  `string_input_popup`.
- Zone options descriptions: `zone_options::get_descriptions()`
  (clzones.cpp:451-487) — construct type, filter, seed, unload settings.
- Zone creation sub-flow: calls `look_around` twice for start/end points,
  then queries type, options, and name (game.cpp:7610).
- Zone editing: `zone_data::set_name()` (clzones.cpp:630),
  `set_type()` (clzones.cpp:647), `set_enabled()` (clzones.cpp:727).

**Success criteria:** Zone list reports name, type, enabled state, and distance.
Creation walks through placement, type selection, options, and naming.
Editing, reordering, enabling/disabling, and deletion are announced.

**Possible automated tests:** Capture zone list focus, creation flow, edit,
reorder, enable/disable, delete, and return focus.

**Required human verification:** Create a zone with placement and options,
edit its name and type, reorder, disable, and delete a zone.

## Stage 19 - Vehicle controls, driving, and status

**Outcome:** Vehicle entry, controls, driving feedback, and status readouts are
accessible through speech.

**Code inventory:** `src/vehicle_use.cpp` (controls, electronics, interact menu),
`src/vehicle_display.cpp` (fuel gauges, speed gauge), vehicle movement in
`src/vehicle_move.cpp`, and vehicle action dispatch in `src/game.cpp`.

**Implementation hints:**

- Vehicle interaction entry: `vehicle::interact_with()` (vehicle_use.cpp:2600)
  builds and queries a `veh_menu` (wraps `uilist`).
- `veh_menu::query()` (veh_utils.cpp:424) creates `uilist` with context
  `"VEHICLE"`.  Existing uilist hooks provide basic focus announcements.
- Electronics menu: `vehicle::build_electronics_menu()` (vehicle_use.cpp:289)
  — toggles for lights, stereo, climate, kitchen.  State changes via
  `add_electronic_toggle()` (vehicle_use.cpp:250).
- Handbrake: `handbrake()` (vehicle_use.cpp:113) zeroes cruise velocity.
- Engine control: `vehicle::control_engines()` (vehicle_use.cpp:384).
- Start/stop: `vehicle::start_engines()` (vehicle_use.cpp:795) /
  `stop_engines()` (vehicle_use.cpp:762).
- Driving: `game::pldrive` (handle_action.cpp:501) calls `veh->pldrive()`
  (vehicle_move.cpp:1427).
- Cruise control: `vehicle::cruise_change()` (vehicle_move.cpp:645).
- Speed gauge: `vehicle::print_speed_gauge()` (vehicle_display.cpp:477) —
  cruise target, actual speed, strain coloration (normal/yellow/red).
- Fuel gauge: `vehicle::print_fuel_indicator()` (vehicle_display.cpp:404) —
  percentage, fuel name, rate, estimated time remaining.
- For accessibility: create `vehicle::get_accessibility_status_summary()`
  using `print_speed_gauge` and `print_fuel_indicators` data for on-demand
  status readout during driving.

**Success criteria:** Entry position, control focus, speed, heading,
turn/movement result, cruise state, fuel/power, and warnings are announced.
Movement feedback distinguishes character movement from vehicle motion.

**Possible automated tests:** Reuse vehicle fixtures for entry/control, start,
speed/heading change, blocked motion, cruise, fuel/power, and exit.

**Required human verification:** Enter, start, steer, accelerate, stop, and
leave a vehicle; toggle electronics; exercise a warning and blocked case.

## Stage 19A - Vehicle examination and maintenance

**Outcome:** Vehicle part inspection, installation, repair, and removal are
accessible through the examination grid interface.

**Code inventory:** `src/veh_interact.cpp`/`src/veh_interact.h` (examination
screen, part grid, task handlers).

**Implementation hints:**

- Entry: `veh_interact::run()` (veh_interact.cpp:234) instantiates the screen
  and calls `do_main_loop()` (veh_interact.cpp:496-604).
- Input context: `"VEH_INTERACT"` (veh_interact.cpp:272-300).
- Cursor movement: `veh_interact::move_cursor()` (veh_interact.cpp:2199)
  shifts tile, recalculates `parts_here`, `cpart`, and repair/mount status.
- Part list at tile: `vehicle::print_part_list()` (vehicle_display.cpp:148).
- Part descriptions: `vehicle::print_vparts_descs()` (vehicle_display.cpp:247).
- Vehicle stats: `display_stats()` (veh_interact.cpp:2422-2608) — speeds,
  mass, cargo volume, durability, most damaged parts.
- Action bar: `display_mode()` (veh_interact.cpp:2666) — enabled/disabled
  tasks with reasons via `cant_do()` and `veh_act_desc` (veh_interact.cpp:2643).
- Task sub-loops:
  - Install: `do_install()` (veh_interact.cpp:969) — tabs/categories,
    installable parts (veh_interact.cpp:1014-1095).
  - Repair: `do_repair()` (veh_interact.cpp:1122) — parts on tile
    (veh_interact.cpp:1168-1219).
  - Remove: `do_remove()` (veh_interact.cpp:1809-1913).
  - Overview: `overview()` (veh_interact.cpp:1631-1715).
- Requirements format: `format_reqs()` (veh_interact.cpp:359-407).
- Accessibility hooks: on `move_cursor` announce tile position and parts
  summary; on task selection announce available action and requirements.

**Success criteria:** Part selection, status, install/remove/repair choices,
disabled reasons, skill/tool requirements, and results are announced.  The
tile cursor and task sub-screens are keyboard operable.

**Possible automated tests:** Capture cursor movement, parts at tile, install
tab/part selection, repair part selection, invalid action, and return focus.

**Required human verification:** Inspect a vehicle, navigate parts, and
perform one part action (repair or remove); exercise an invalid/blocked case.

## Stage 20 - Character creation: scenario and profession

**Outcome:** The first four character creation tabs (points, scenario, profession,
background/hobbies) are independently usable.

**Code inventory:** `src/newcharacter.cpp` tabs 0–3; main-menu entry in
`src/main_menu.cpp`.

**Implementation hints:**

- Tab manager: `tab_manager` class (newcharacter.cpp:136-203) tracks position,
  handles `PREV_TAB`/`NEXT_TAB`/`SELECT`/`QUIT`.
- Tabs declared in `avatar::create` (newcharacter.cpp:755-766): POINTS (0),
  SCENARIO (1), PROFESSION (2), BACKGROUND (3), STATS (4), TRAITS (5),
  SKILLS (6), DESCRIPTION (7).
- Tab 0 (Points): `set_points()` (newcharacter.cpp:1163-1299), context
  `"NEW_CHAR_POINTS"`.  Already has `screen_reader_mode` branch
  (newcharacter.cpp:1245-1251) — extend it.
- Tab 1 (Scenario): `set_scenario()` (newcharacter.cpp:3590-4044), context
  `"NEW_CHAR_SCENARIOS"`.  Left pane: scenario list with point costs.  Right
  pane: `scrolling_text_view details` with description, locations, missions.
- Tab 2 (Profession): `set_profession()` (newcharacter.cpp:2524-2877), context
  `"NEW_CHAR_PROFESSIONS"`.  Same left/right layout.
- Tab 3 (Background): `set_hobbies()` (newcharacter.cpp:2879-3219), context
  `"NEW_CHAR_PROFESSIONS"`.  Same layout.
- All tabs support `"FILTER"`, `"SORT"`, and `"RANDOMIZE"`.
- Announce tab identity on switch, entry name + point cost on selection change,
  detail text on demand.

**Success criteria:** Each tab reports identity, focus, selection, point costs,
conflicts, and detail.  Scenario, profession, and background selection,
filtering, sorting, and randomization are announced and focus-stable.

**Possible automated tests:** Capture every tab entry, selection changes, point
updates, filter, sort, and return focus using character fixtures.

**Required human verification:** Create a character using each of the first four
tabs, use filter and sort, randomize, and confirm tab transitions work smoothly.

## Stage 20A - Character creation: stats, traits, skills, and finish

**Outcome:** The remaining character creation tabs and the final confirmation
flow are independently usable.

**Code inventory:** `src/newcharacter.cpp` tabs 4–7; template and validation
logic.

**Implementation hints:**

- Tab 4 (Stats): `set_stats()` (newcharacter.cpp:1491-1703), context
  `"NEW_CHAR_STATS"`.  Four stats with LEFT/RIGHT to adjust values.  Detail
  pane: `assemble_stat_details` shows derived effects (HP, carry, damage,
  throw distance, etc.).
- Tab 5 (Traits): `set_traits()` (newcharacter.cpp:1705-2522), context
  `"NEW_CHAR_TRAITS"`.  2–3 columns (positive, negative, neutral).  Locked
  traits by scenario/profession/hobby.  Conflict highlighting.
- Tab 6 (Skills): `set_skills()` (newcharacter.cpp:3221-3588), context
  `"NEW_CHAR_SKILLS"`.  Skills grouped under category headers.  LEFT/RIGHT
  adjusts level.
- Tab 7 (Description): `set_description()` (newcharacter.cpp:4046-5143),
  context `"NEW_CHAR_DESCRIPTION"`.  Name, gender, height, age, blood type,
  start location, vehicle.  `"SAVE_TEMPLATE"`, `"REROLL_CHARACTER"`,
  `"CHOOSE_LOCATION"`.
- Final confirmation: `"NEXT_TAB"` on Description tab validates points,
  prompts confirmation, and sets `tabs.complete = true`
  (newcharacter.cpp:4503-4543).
- Template save/load: template management in Description tab.

**Success criteria:** Stats, traits, skills, description, start location,
randomization, templates, naming, and final confirmation are complete and
focus-stable.  Point totals and conflicts are announced on change.

**Possible automated tests:** Capture stat adjustment, trait selection/conflict,
skill level change, description fields, template save/load, validation errors,
and final confirmation.

**Required human verification:** Complete character creation using every
remaining tab, correct point/conflict errors, save/load a template, and start
a new game.

## Stage 21 - World and mod management

**Outcome:** World selection, creation, copying, deletion, and mod configuration
are independently usable.

**Code inventory:** `src/worldfactory.cpp` (world selection, creation, deletion),
`src/mod_manager_ui.cpp` (mod list logic), and main-menu world management in
`src/main_menu.cpp`.

**Implementation hints:**

- World selection: `worldfactory::pick_world()` (worldfactory.cpp:454-690),
  context `"PICK_WORLD_DIALOG"` (worldfactory.cpp:622).
- World creation basic: `worldfactory::show_worldgen_basic()`
  (worldfactory.cpp:1494-1834), context `"WORLDGEN_CONFIRM_DIALOG"`
  (worldfactory.cpp:1502).
- World name: `prompt_world_name()` (worldfactory.cpp:207-225).
- World deletion: `main_menu.cpp:1240-1310` — manage world menu with
  delete/reset confirmation popups.
- Mod selection: `show_worldgen_tab_modselection` (worldfactory.cpp:978-1492).
- Mod list drawing: `draw_mod_list` (worldfactory.cpp:761-905).
- Mod manager context: `"MODMANAGER_DIALOG"` (worldfactory.cpp:994).
- Mod info: `mod_ui::get_information(mod)` (mod_manager_ui.cpp:18-69) —
  authors, dependencies, version, description.
- Mod actions: `try_add` (mod_manager_ui.cpp:84-157), `try_rem`
  (mod_manager_ui.cpp:159-184), `try_shift` (mod_manager_ui.cpp:186-225).
- Two columns: "Mod List" (available) and "Mod Load Order" (active)
  (worldfactory.cpp:1068-1069).
- Category tabs: worldfactory.cpp:1220-1234.

**Success criteria:** World selection/create/copy/reset/delete, naming, mod
category/order/dependencies, and confirmation are announced.

**Possible automated tests:** Capture world and mod focus/reorder/dependency
errors, destructive confirmation, save/cancel, and return focus.

**Required human verification:** Create a configured world, alter mod order,
and safely exercise copy, reset, and delete confirmations.

## Stage 21A - Options, rules, and panel configuration

**Outcome:** Global and world options, safe-mode and autopickup rule editors,
and sidebar panel configuration are independently usable.

**Code inventory:** `src/options.cpp`, `src/safemode_ui.cpp`,
`src/auto_pickup.cpp`, `src/panels.cpp` (sidebar admin), and
`src/distraction_manager.cpp`.

**Implementation hints:**

- Options screen: `options_manager::show()` (options.cpp:3342-3945), context
  `"OPTIONS"` (options.cpp:3379).
- Option tabs/pages: `pages_[i].name_` ("General", "Interface", "Graphics",
  "Sound", etc.) (options.cpp:3603-3620).
- Option entry text: `fmt_name_value` (options.cpp:3500-3540) — group header
  or option name + value.
- Tooltip: `curr_item.fmt_tooltip()` (options.cpp:3625).
- Value editing: `on_select_option` (options.cpp:3657-3699) — LEFT/RIGHT
  cycles, CONFIRM opens input for numeric types.
- Note: the curses options screen does not have search/filter — options are
  organized by pages and collapsible groups.
- Safe-mode rules: `src/safemode_ui.cpp` — rule list with add/edit/delete.
- Autopickup rules: `src/auto_pickup.cpp` — similar rule editor pattern.
- Panel admin: `panel_manager::show_adm()` (panels.cpp:711-881), context
  `"PANEL_MGMT"` (panels.cpp:713-722).  Two columns: panel list and layout
  list.  Toggle panel visibility, reorder, activate layout.
- Distraction settings: `distraction_manager_gui::show()`
  (distraction_manager.cpp:49-175), context `"DISTRACTION_MANAGER"`.

**Success criteria:** Options pages, group expand/collapse, value editing,
defaults, safe-mode/autopickup rules, panel configuration, and distraction
settings are announced without relying on columns or color.

**Possible automated tests:** Capture option search/value/reset, rule editing,
panel changes, save/cancel, and return focus.

**Required human verification:** Change and restore options, add/edit rules,
configure panels, and confirm all value changes are announced.

## Stage 22 - Coverage and announcement-policy audit

**Outcome:** Every keyboard-reachable interface is classified and tested, speech
policy is consistent, and remaining bespoke-screen gaps are closed.

**Code inventory:** all `accessibility::` call sites; all registered
`input_context` categories and bespoke UI entry points; the accessibility service
and feature tests.  Create and maintain a concise coverage manifest during this
stage so later upstream merges do not require rediscovering every screen.

**Success criteria:** The manifest maps each reachable screen to a shared
accessible control, a feature hook, or an explicit accepted limitation.  Event
categories define focus, screen, status, message, movement, threat, urgent alert,
and on-demand detail priority/deduplication.  Rapid events preserve the newest
meaningful state; repeat-last is correct; no speech-cancel or automatic requeue
exists.  Save/load, death screen, errors, debug-disabled normal play, and exit
are covered by an end-to-end keyboard journey.  No perceptible gameplay slowdown
with accessibility enabled in a scenario with 50+ visible creatures and rapid
movement.

**Possible automated tests:** Table-driven service tests for categories,
collisions, mode transitions, backend failure/reset, deduplication, and history;
coverage-manifest consistency checks; `test_access.bat`, `test_all.bat`, Windows
test build, tiles build, and the branch's static checks.

**Required human verification:** Complete the coverage-manifest journey and a
long play session, deliberately navigating rapidly and interrupting speech.
Confirm every unverified popup noted in `ACCESS_DONE.md`, including standard,
developer-error, static-instruction, and progress-popup integration.  Confirm
the death screen and record all accepted limitations.

## Stage 23 - Windows packaging and playable-release acceptance

**Outcome:** A new blind player can obtain, start, understand, and play the
Windows x64 tiles build without assembling development dependencies.

**Code inventory:** repository Windows build/package scripts;
`src/accessibility_nvda.cpp` runtime lookup; `README.md`, `doc/`, release notes,
licenses, translations, and distribution contents.

**Success criteria:** A reproducible clean package contains the matching NVDA
Controller Client DLL and LGPL 2.1 license, preserves upstream attribution,
starts safely with NVDA running or absent, and provides accessible first-run,
command, limitation, diagnostic, AI-development, and defect-reporting guidance.
All accessibility source strings are wrapped in `_()` / `pgettext()` and the
Polish catalog (the tested locale) includes translations for critical
accessibility strings.

**Possible automated tests:** Validate package contents, architecture, licenses,
version, clean configuration, missing-DLL fallback, smoke launch, focused/full
tests, and final tiles build from a clean checkout.

**Required human verification:** A blind tester who did not prepare the source
tree obtains and launches the artifact, discovers controls from its documentation,
creates or loads a game, plays, saves, exits, and restarts.  Record exact Windows,
NVDA, commit, and artifact versions; this is the native release gate.

## Stage 24 - Browser backend and web acceptance

**Outcome:** The Emscripten build exposes the stable semantic event model through
browser accessibility APIs without parsing canvas pixels or rendered text.

**Code inventory:** the backend-neutral accessibility service; the NVDA backend
as a boundary reference only; Emscripten build and web-shell paths to be recorded
here when this stage begins.

**Success criteria:** A browser backend maps existing semantic events to suitable
focus semantics and ARIA live regions.  Keyboard operation, information parity,
deduplication, repeat-last, localization, and hidden-state rules match native
behavior.  Null/capture/NVDA and native builds remain unaffected.

**Possible automated tests:** Reuse the backend contract suite; add DOM/ARIA
event and browser keyboard tests for focus, status, rapid updates, teardown, and
restart; build and smoke-test Emscripten and Windows targets.

**Required human verification:** A blind screen-reader user completes the Stage
23 acceptance journey in every declared supported browser/screen-reader pair.
Record browser, screen reader, operating system, artifact versions, results, and
limitations before claiming web accessibility.

## Known blockers and required follow-up

- Standard, developer-error, and static popup announcements are implemented but
  still need the remaining dedicated NVDA checks recorded in `ACCESS_DONE.md`.
  Stage 22 owns those checks unless an earlier stage relies on a popup type.
- The NVDA Controller Client DLL and license remain local ignored artifacts until
  Stage 23 establishes packaging.
- The current host may require single-process MSBuild invocations.
- AccessKit integration is a post-roadmap item that depends on the stable native
  semantic model from Stage 22.
- Upstream synchronization should occur between stages, re-verifying
  `test_access.bat` after each merge.
- Translation catalog updates for accessibility strings are deferred to Stage 23
  packaging.
