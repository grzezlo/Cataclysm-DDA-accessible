# Accessibility roadmap

This file is the authoritative plan for unfinished accessibility work.  Stable
architecture, workflow, and completion rules are in `AGENTS.md`; completed work
and its verification record are in `ACCESS_DONE.md`.

The first supported configuration is the Windows x64 tiles build with NVDA.
Stage-specific inventories below are starting points and must be corrected when
implementation discovers a better integration point.

Stage 03C closes the help gap.  Stages 04–10 establish the minimum independent
survival and combat loop.  Stages 11–21A cover the major progression, management,
and setup interfaces.  Stages 22, 22A, and 23 are native release gates: coverage,
the complete tutorial, and packaging.  Stage 24 starts only after the native
semantic model is stable.

Unless a stage lists an exception, its automated gate includes
`test_access.bat` and a successful `build_win_x64.bat`.  The human gate for
native stages includes the general NVDA checks required by `AGENTS.md` plus the
scenarios listed for the stage.  Each stage's possible automated tests are a
hard gate: they must be implemented and passing before requesting human
verification.

## Stage 03C - Help browser

**Outcome:** The in-game Help browser and its topic documents can be navigated
and read without relying on two-column placement, colors, or memorized hotkeys.

**Code inventory:** `help::display_help()`, `draw_menu()`, dynamic keybinding and
direction substitutions, and `scrollable_text()` use in `src/help.cpp`;
`src/help.h`; help topics in `data/core/help.json`.

**Selected interaction design:**
- **Topic navigation:** Linear 1D cursor navigation across both visual columns using `Up` and `Down`. Opening Help announces its purpose, topic count, controls, and initial topic (`"Help. 23 topics. Use Up and Down to browse, hotkey or Enter to open, Escape to exit. a: Introduction"`). `Up` and `Down` move sequentially through all topics, speaking hotkey and topic title (`"b: Movement"`). `Page Up` and `Page Down` jump by 5 topics. Direct hotkeys (`a`–`w`) jump to and open the topic. `Enter` opens the focused topic. `Alt+R` repeats the current topic announcement. `Escape` exits Help and restores focus to the originating caller.
- **Document reader:** Reuses the Stage 03A document-reading model (`accessibility::move_document_cursor` and `accessibility::format_document_focus`). Opening a topic announces its title and first paragraph. `Down` and `Up` move between paragraphs and speak the paragraph text directly without index prefixes (matching MOTD/Credits). `Page Down` and `Page Up` jump by 3 paragraphs. `Alt+R` repeats the current paragraph. `Escape` or `Enter` returns to the topic list, preserving cursor position and announcing the returned topic.
- **Diagrams and bindings:** `<HELP_DRAW_DIRECTIONS>` is expanded into a semantic compass description for speech, listing each direction and Pause with its bound primary and secondary keys. Keybinding tokens (`<press_action>`) are formatted as clean spoken key names without color tags.
- **Scope adjustment:** The dedicated Accessibility help topic is deferred to the final stages (tutorial/release gate) as directed by the user.

**Success criteria:** Opening Help announces its purpose and controls.  A player
can browse all topics, hear each topic and hotkey, open and navigate a document,
understand expanded diagrams and bindings semantically, repeat content, and
return first to the topic list and then to the originating screen.

**Possible automated tests:** Capture Help open, topic navigation across the
visual column boundary, direct hotkeys, topic entry, expanded directions and
bindings, document scrolling, repeat-last, topic return, and Help exit.

**Required human verification:** With NVDA, enter Help from the main menu and
gameplay, find topics by navigation and direct hotkey, read short and long
documents, interrupt and recover speech with Alt+R, and verify both return
paths.

## Stage 04A - Message log review

**Outcome:** A player can browse, search, and read the full scrollable message
history to catch up on missed or interrupted speech.

**Code inventory:** `Messages::dialog` in `src/messages.cpp`; the message
history deque in `messages_impl`; filter and scroll logic.

**Implementation hints:**

- The message log dialog class is `Messages::dialog` in `src/messages.cpp`.
- Entry point: `Messages::display_messages()` in `src/messages.cpp`, triggered
  by `ACTION_MESSAGES` (the `P` key) in `src/handle_action.cpp`.
- Input context: `"MESSAGE_LOG"` in `Messages::dialog::init()`.  Actions: `"UP"`,
  `"DOWN"`, `"PAGE_UP"`, `"PAGE_DOWN"`, `"FILTER"`, `"RESET_FILTER"`, `"QUIT"`.
- The dialog currently uses a viewport offset model in `Messages::dialog::input()`
  rather than a per-message cursor.  For accessibility, track an active cursor
  index within `folded_filtered` and announce each message line on UP/DOWN.
- Message text is in `game_message::get_with_count()` in `src/messages.cpp`.
- Elapsed time per message: `to_string_clipped(calendar::turn - msg_time, ...)`
  in `Messages::dialog::show()`.
- Filter: `Messages::dialog::do_filter()` supports `<type>:<text>` prefix
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
`src/string_input_popup.cpp`, `src/ui_iteminfo.cpp`, `src/input_context.cpp`, and
`data/raw/keybindings.json`.

**Implementation hints:**

- `uilist` already has accessibility hooks in `uilist::query()` (`src/uilist.cpp`).
  This stage extends coverage to disabled-entry reasons (the `uilist_entry::enabled`
  flag and `desc_enabled` description text), desc text on focus, and
  empty/error states.
- `query_yn` is implemented in `src/popup.cpp` as a `query_popup` with two
  options.  Hook the existing `query_popup::query_once()` path in `src/popup.cpp`
  to announce the question and options.
- `query_int` / numeric input uses `string_input_popup` in
  `src/string_input_popup.cpp`.  Announce the prompt, current value, and
  bounds on open and on value change.
- `iteminfo_window::execute()` in `src/ui_iteminfo.cpp` is a scrollable
  document viewer.  Announce the full text on open via
  `format_item_info()` in `src/output.cpp` with color tags removed.
- For keybinding discovery: `input_context` in `src/input_context.cpp`
  manages the keybinding UI invoked by `HELP_KEYBINDINGS`.

**Success criteria:** Lists, disabled entries, yes/no prompts, text and numeric
fields, quantity prompts, and scrolling detail announce label, role, value,
state, bounds, errors, and focus return.  The keybinding interface exposes every
accessibility action.  Intentionally hidden input remains private.

**Possible automated tests:** Widget-level capture tests for entry, focus/value
change, disabled reason, bounds, editing, accept/cancel, scrolling, and focus
restoration; keybinding action-presence tests.

**Required human verification:** Exercise every control type, find and rebind
each accessibility action, resolve a binding conflict, and confirm there are no
silent controls or keyboard traps.

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

- Class hierarchy: `inventory_selector` in `src/inventory_ui.cpp` is the base
  multi-column controller.  It manages `inventory_column` instances, each holding
  `inventory_entry` items.
- Focus/selection change hook: `inventory_column::highlight(size_t, scroll_direction)`
  sets `highlighted_index`.  After highlight, read `inventory_column::get_highlighted()`
  to get the focused entry.
- Semantic text for an entry: `inventory_selector_preset::get_caption(entry)` returns
  the item name with stack count.  Extra column cells from
  `inventory_selector_preset::get_cell_text(entry, i)` provide weight, volume, etc.
  Denial reason is `entry.denial` string.
- Category navigation: `inventory_selector::toggle_navigation_mode()` switches between
  `ITEM` and `CATEGORY` modes.
- Column switching: `inventory_selector::set_active_column(index)` and
  `inventory_selector::toggle_active_column(dir)`.
- Stats header: `inventory_selector::get_stats()` returns weight, volume,
  and holster usage.
- Item examination: `inventory_selector::action_examine(item_location)` opens
  the `iteminfo_window`.
- Input context: registered in `inventory_selector::inventory_selector()` — register
  `"ACCESSIBILITY_REPEAT_LAST"` there.
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

- All single-item actions go through `inv_internal()` in `src/game_inventory.cpp`,
  which wraps `inventory_pick_selector::execute()` with title, hint, and
  `none_message`.  Hook announcements in `inv_internal()` for the shared flow.
- Action-specific presets in `src/game_inventory.cpp` define extra columns:
  - Wield: `weapon_inventory_preset` — DAMAGE, MOVES, TO HIT.
  - Wear: `wear_inventory_preset`.
  - Consume: `comestible_inventory_preset` — CALORIES, QUENCH, JOY, HEALTH,
    SHELF LIFE.
  - Read: `read_inventory_preset` — READ TIME, SKILL.
  - Reload: `select_ammo_inventory_preset` — DAMAGE, AP, RANGE.
- Item action menu: `game::item_action_menu()` in `src/item_action.cpp` uses
  `uilist` with `desc_enabled = true`.  The uilist already speaks entry text
  and `ctxt` column, but the `desc` (method description) is not spoken on
  focus.  Add it to the focus announcement when `desc_enabled` is active.
- Consume needs hint: `get_consume_needs_hint()` in `src/game_inventory.cpp`
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

- Entry point: `create_advanced_inv()` in `src/advanced_inv.cpp` instantiates
  `advanced_inventory` and calls `advanced_inventory::display()`.
- Two-pane structure: `std::array<advanced_inventory_pane, NUM_PANES> panes`
  in `src/advanced_inv.h`, `src = left/right`, `dest` is always opposite.
- Pane switching: `"LEFT"` / `"RIGHT"` / `"TOGGLE_TAB"` in
  `advanced_inventory::display()`.
- Area selection: 15 `ITEMS_*` actions (inventory, worn, ground tiles, vehicle,
  container, parent).
- Item rows: name, charges, weight, volume, category rendered in
  `advanced_inventory::print_items()`.
- Pane header: area title, weight/volume capacity rendered in
  `advanced_inventory::print_items()`.
- Transfer: `advanced_inventory::action_move_item()` and
  `advanced_inventory::move_all_items()` in `src/advanced_inv.cpp`.
- Quantity prompt: `advanced_inventory::query_charges()` in `src/advanced_inv.cpp`.
- Input context: `"ADVANCED_INVENTORY"` registered in
  `advanced_inventory::display()`.
- Multiselect in standard inventory: `inventory_multiselector` in
  `src/inventory_ui.cpp`.  `inventory_multiselector::toggle_entry()` and
  `inventory_multiselector::toggle_entries()` manage selection marks
  (`'-'`, `'+'`, `'#'` formatted in `selection_column::print_cell()`).

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

- Examination dispatch: `game::examine(tripoint, bool)` in `src/game.cpp`
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

- Route storage: `Character::set_destination(route, ...)` in `src/character.cpp`.
- Step generation: `Character::get_next_auto_move_direction()` in
  `src/character.cpp` converts delta to next waypoint into a movement action.
- Route abort: `Character::abort_automove()` in `src/character.cpp`.
- Local auto-travel mode: toggled via `"auto_travel_mode"` in
  `game::handle_action()` (`src/handle_action.cpp`).  Pressing a direction calculates
  a ray of up to 12 tiles via `map::route()` and calls
  `Character::set_destination()`.
- Look-around travel: `"TRAVEL_TO"` in `game::look_around()` (`src/game.cpp`) calls
  `game::safe_route_to()` and `Character::set_destination()`.
- Overmap fast travel: `create_path` helper in `src/overmap_ui.cpp` prompts
  "Travel to this point?", sets `omt_path`, and assigns `ACT_TRAVELLING`.
- Travel per-turn: `activity_handlers::travel_do_turn()` in
  `src/activity_handlers.cpp` routes local submap path and calls
  `Character::set_destination()`.
- Interruptions: `cancel_auto_move()` in `src/game.cpp` queries
  `query_yn("Cancel auto move?")`.
- Arrival message: `_("You have reached your destination.")` in
  `activity_handlers::travel_do_turn()`.
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

- Interactive pickup entry: `game_menus::inv::pickup()` in `src/game_inventory.cpp`
  creates `pickup_selector` in `src/inventory_ui.cpp`.
- `pickup_selector::execute()` in `src/inventory_ui.cpp` runs the multi-select
  loop.  Additional actions: `"WEAR"` and `"WIELD"` for direct equip from ground.
- Transfer execution: `Pickup::do_pickup()` in `src/pickup.cpp` processes
  selected items, calling `pick_one_up()`.
- Problematic pickup: `handle_problematic_pickup()` in `src/pickup.cpp` shows
  spill/wield/cancel choices.
- Ownership warning: `Pickup::query_thief()` in `src/pickup.cpp`.
- Summary message: `show_pickup_message()` in `src/pickup.cpp` — already
  reaches the live message system.
- Drop: `game_menus::inv::multidrop()` in `src/game_inventory.cpp` uses
  `inventory_drop_selector::execute()` in `src/inventory_ui.cpp`.
- Insert into container: `game_menus::inv::holster()` in `src/game_inventory.cpp`
  uses `inventory_insert_selector` in `src/inventory_ui.cpp`.
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

- Core class: `target_ui` in `src/ranged.cpp` and `src/ranged.h`.
- Entry points: `target_handler::mode_fire()`, `target_handler::mode_throw()`,
  `target_handler::mode_reach()`, and `target_handler::mode_spell()` in `src/ranged.cpp`.
- Main targeting loop: `target_ui::run()` in `src/ranged.cpp`.
- Input context: `"TARGET"` initialized in `target_ui::run()`.  Actions include
  `register_directions()`, `"FIRE"`, `"NEXT_TARGET"`, `"PREV_TARGET"`,
  `"SWITCH_MODE"`, `"SWITCH_AMMO"`, `"AIM"`, `"AIMED_SHOT"`,
  `"CAREFUL_SHOT"`, `"PRECISE_SHOT"`.
- Cursor movement: `target_ui::handle_cursor_movement()` and
  `target_ui::set_cursor_pos()`.
- Target cycling: `"NEXT_TARGET"` / `"PREV_TARGET"` cycle through `targets` vector
  in `target_ui::handle_cursor_movement()`.
- Semantic text:
  - Title: `target_ui::uitext_title()` — "Firing %s", "Throwing %s".
  - Range/elevation/target count: `target_ui::panel_range_elevation_and_target_count()`.
  - Gun & ammo: `target_ui::panel_gun_info()`.
  - Recoil: `target_ui::panel_recoil()` ("None", "Low", "Medium", "High").
  - Target info: `target_ui::panel_target_info()` (creature name/health via
    `dst_critter->print_info()`, vehicle name via `dst_veh->name`, terrain info).
  - Hit probability: `print_aim()` for firearms, `draw_throw_aim()` for thrown
    weapons in `src/ranged.cpp`.
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

- Entry point and main loop: `select_crafter_and_crafting_recipe()` in
  `src/crafting_gui.cpp`.
- Input context: `"CRAFTING"` initialized in
  `select_crafter_and_crafting_recipe()`.
- Category/subcategory tabs: drawn by `draw_recipe_tabs()` and
  `draw_recipe_subtabs()` in `src/crafting_gui.cpp`.
- Recipe list entry: `current[line]->result_name()` in
  `select_crafter_and_crafting_recipe()`.
- Detailed recipe info: `recipe_info()` in `src/crafting_gui.cpp` computes
  all attributes — skill requirements, proficiencies, success/failure chance,
  duration, exertion, light conditions, ingredients, tools, components.
- Search: `filter_recipes()` in `src/crafting_gui.cpp`.
- Batch cycling: `"CYCLE_BATCH"` action.
- Related recipes: `"RELATED_RECIPES"` action.
- Disassembly: `game_menus::inv::disassemble()` in `src/game_inventory.cpp` uses
  `disassemble_inventory_preset`.

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

- Entry point and main loop: `construction_menu()` in `src/construction.cpp`.
- Input context: `"CONSTRUCTION"` in `construction_menu()`.
- Category tabs: `construct_cat[tabindex].name()`.
- Project name: `group->name()`.
- Requirement details computed in `recalc_buffer` lambda inside
  `construction_menu()`:
  - Stage title and resulting terrain.
  - Skill requirements.
  - Precursor terrain.
  - Duration: `current_con->get_folded_time_string()`.
  - Tools: `current_con->requirements->get_folded_tools_list()`.
  - Components: `current_con->requirements->get_folded_components_list()`.
- Placement: `place_construction()` in `src/construction.cpp` uses
  `choose_adjacent()` for directional cursor.
- Validation: `can_construct()` in `src/construction.cpp`.
- Failure reason: `con.explain_failure(pnt)`.
- On confirm: starts `player_activity(ACT_BUILD)`.

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
  - Start: `player_activity::start_or_resume()` in `src/player_activity.cpp`
    fires `event_type::character_starts_activity`.
  - Per-turn: `player_activity::do_turn()` in `src/player_activity.cpp`.
  - Completion: fires `event_type::character_finished_activity` in
    `player_activity::do_turn()`.
  - Cancel: `player_activity::canceled()` in `src/player_activity.cpp` fires
    the same event with `canceled = true`.
- Progress: `player_activity::get_progress_message()` in `src/player_activity.cpp`
  returns verb + percentage or verb + extra_info.  Use this for on-demand progress
  via `Alt+R`.
- Stamina exhaustion: `uilist tired_query` in `player_activity::do_turn()`
  prompts "Keep trying?" with continue/stop/force options.
- Interruptions: `game::cancel_activity_or_ignore_query()` in `src/game.cpp`
  uses `query_popup` with context `"CANCEL_ACTIVITY_OR_IGNORE_QUERY"` and
  options YES/NO/IGNORE/MANAGER.
- Distraction types: `enum class distraction_type` in `src/enums.h` (noise, pain,
  hostile spotted, weather, etc.).
- Distraction settings UI: `distraction_manager_gui::show()` in
  `src/distraction_manager.cpp`, context `"DISTRACTION_MANAGER"`.
- Sleep/rest: `ACT_WAIT` and `ACT_WAIT_WEATHER` with finish handler
  `activity_handlers::wait_finish()` in `src/activity_handlers.cpp`.
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

- Entry point: `Character::disp_info()` in `src/player_display.cpp`.
- Input context: `"PLAYER_INFO"` in `Character::disp_info()`.
- Tab enum: `enum class player_display_tab` in `src/player_display.cpp` — `stats`,
  `encumbrance`, `speed`, `skills`, `traits`, `bionics`, `effects`,
  `proficiencies` (8 tabs).
- Tab switching: `"LEFT"` / `"RIGHT"` / `"NEXT_TAB"` / `"PREV_TAB"` and direct
  jump actions (`"SELECT_STATS_TAB"`, etc.) handled in `Character::disp_info()`.
- Row navigation: `navigate_ui_list()` in `Character::disp_info()`.  Skills tab
  skips category headers via `skip_skill_headers`.
- Per-tab semantic text:
  - Stats: `display_stat` lambda inside `Character::disp_info()` — current vs base.
    Info pane: `draw_stats_info()`.
  - Encumbrance: body part name, base + layer penalty, temperature in
    `draw_encumbrance_tab()`.  Info pane: `get_encumbrance_description()` — this
    is the layering review screen.
  - Speed: `get_speedlist_entries()` — cause, type (S/M), signed value.
  - Skills: `Character::get_HeaderSkills()` / `disp_skills_tab()` — category
    headers, name, level, exercise %, rust, training status.
  - Traits: `Character::get_mutations_variants()` — name, description,
    purifier status.
  - Bionics: `struct bionic_grouping` / `disp_bionics_tab()` — name, count,
    power.
  - Effects: `effect_name_and_text` gathered in `Character::disp_info()`.
  - Proficiencies: `Character::display_proficiencies()` / `disp_proficiencies_tab()`
    — name, progress %.
- Info pane scrolling: `"SCROLL_INFOBOX_UP"` / `"SCROLL_INFOBOX_DOWN"` handled in
  `Character::disp_info()`.

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

- Bionics entry: `avatar::power_bionics()` in `src/bionics_ui.cpp`.
  Context: `"BIONICS"`.
- Bionic tabs: `TAB_ACTIVE` / `TAB_PASSIVE` with counts in
  `avatar::power_bionics()`.
- Bionic entry text: `build_bionic_powerdesc_string()` in `src/bionics_ui.cpp`
  — name, power cost, per-turn drain, ON/OFF status.
- Power display: `draw_bionics_titlebar()` in `src/bionics_ui.cpp` — current
  power in kJ/J/mJ vs max, available fuels.
- Examine mode: `"TOGGLE_EXAMINE"` switches between activating and examining;
  `draw_description()` in `src/bionics_ui.cpp` renders detail.
- Mutations entry: `avatar::power_mutations()` in `src/mutation_ui.cpp`.
  Context: `"MUTATIONS"`.
- Two columns: `"Passive:"` and `"Active:"` switched via `"NEXT_TAB"` / `"PREV_TAB"`
  in `avatar::power_mutations()`.
- Active trait text: name + cost + resource + cooldown + active status in
  `avatar::power_mutations()`.
- Description: `mutation_desc()` in `src/mutation_ui.cpp`.

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

- Spell selection: `known_magic::select_spell()` in `src/magic.cpp`
  creates `uilist spell_menu` with category tabs.
- Spell menu context: `"SPELL_MENU"` registered in `known_magic::select_spell()`.
- Spell callback: `class spellcasting_callback` in `src/magic.cpp` handles the
  side panel.
- Spell details: `spellcasting_callback::display_spell_info()` in `src/magic.cpp`
  — class, flags, cast time, targets, range, AoE, damage, duration, requirements.
  Not currently spoken on selection change — add it.
- Spell entry: `spell::name()`, hotkey, can-cast status, favorite marker.
- Categories: "all", "favorites", and spell classes in `known_magic::select_spell()`.
- Style picker: `character_martial_arts::pick_style()` in `src/character.cpp`
  uses `uilist` with context `"MELEE_STYLE_PICKER"`.
- Style details: `class ma_details_ui` / `class ma_details_ui_impl` in
  `src/martialarts.cpp`, context `"MA_DETAILS_UI"`.  Shows general info, buffs,
  techniques, and compatible weapons.
- `"SHOW_DESCRIPTION"` in the style uilist opens the details window via
  `show_ma_details_ui()`.

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

- Entry / response loop: `dialogue::gen_responses()` in `src/npctalk.cpp`.
- Input context: `"DIALOGUE_CHOOSE_RESPONSE"` initialized in `dialogue::gen_responses()`.
- History scrolling: `dialogue_window::set_up_scrolling()` registers scrolling actions.
- Speaker name: `actor(true)->disp_name()`.
- Responses: `response_lines` generated via `talk_response::create_option_line()`
  with hotkey `evt` and text `td.text`.  Currently selected response is
  `d_win.sel_response`.
- New dialogue lines: `dialogue_window::add_to_history()` handles incoming dialogue
  text.  Hook speech to announce new speaker text on topic transitions, and announce
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

- Two panes: `_panes[_you]` and `_panes[_trader]` in `class trade_ui` in
  `src/trade_ui.cpp` (derived from `inventory_multiselector`).
- Active pane switching: `_cpane = -_cpane + 1` in `trade_ui::_process()`.
- Balance & Acceptance: `_balance`, `cost_str` ("Credit %s" / "Debt %s"),
  evaluated by `npc_trading::npc_will_accept_trade()` in `src/npctrade.cpp`.
- Confirmation check: `trade_ui::_confirm_trade()` validates credit limits,
  pocket capacity, and prompts `query_yn()`.
- Unit price: `trade_preset` in `src/trade_ui.cpp` formats price via
  `format_money(npc_trading::trading_price(...))`.
- Autobalance action: `trade_ui::autobalance()` in `src/trade_ui.cpp`.
- Bank balance: `trade_ui::bank_balance()` in `src/trade_ui.cpp`.

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

- Entry point: `mission_ui::draw_mission_ui()` in `src/mission_ui.cpp`.
- Input context: `ctxt` in `mission_ui::draw_mission_ui()` with actions
  `register_navigate_ui_list()`, `"NEXT_TAB"`, `"PREV_TAB"`, `"CONFIRM"` ("Set
  selected mission as current objective"), `"QUIT"`.
- Tabs: `ACTIVE`, `COMPLETED`, `FAILED` (`enum class mission_ui_tab_enum` in
  `src/mission_ui.cpp`).
- Missions list: `get_avatar().get_active_missions()`,
  `get_completed_missions()`, `get_failed_missions()`.
- Mission attributes: title `m->get_type().name()`, description
  `m->get_description()`, deadline `m->get_deadline()`, target location
  `m->get_target()`.
- Confirmation: `"CONFIRM"` sets tracked mission.

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

- Entry point: `ui::omap::display()` in `src/overmap_ui.cpp` invokes static
  `overmap_ui::display()`.
- Input context: `"OVERMAP"` registered in `overmap_ui::display()`.
- Main loop: interaction loop in `overmap_ui::display()`.
- Cursor movement: `get_direction_rel_omt(action)` adjusts `curs`.  Z-level:
  `"LEVEL_UP"` / `"LEVEL_DOWN"`.
- Semantic text at cursor: `draw_om_sidebar()` in `src/overmap_ui.cpp` computes:
  - Terrain: `overmap_buffer.get_description_at(sm_pos)`.
  - Explored/unexplored: "unexplored" flag.
  - Notes: `overmap_buffer.has_note(curs)` / `overmap_buffer.note(curs)`.
  - Weather: `get_weather_at_point(cursor_pos)->name.translated()`.
  - Mission distance: formatted in `draw_om_sidebar()`.
- Search: `search()` in `src/overmap_ui.cpp` scans radius and enters cycling
  loop.  Result info: index/total, distance, direction.
- Notes: `draw_notes()` in `src/overmap_ui.cpp` is a `uilist` list.
  `create_note()` in `src/overmap_ui.cpp` is a note editor dialog.
- Route: `get_overmap_path_to()` in `src/overmap_ui.cpp` generates A* path.
  `try_travel_to_destination()` initiates travel.
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

- Zone manager entry and main loop: `game::zones_manager()` in `src/game.cpp`.
- Input context: `"ZONES_MANAGER"` in `game::zones_manager()`.
- Zone list row text: enabled status `[X]`/`[ ]`, displayed marker, name,
  distance, type rendered in `game::zones_manager()`.
- Zone type selector: `zone_manager::query_type()` in `src/clzones.cpp` uses
  `uilist` — benefits from existing hooks.
- Zone name: `zone_manager::query_name()` in `src/clzones.cpp` uses
  `string_input_popup`.
- Zone options descriptions: `zone_options::get_descriptions()` in
  `src/clzones.cpp` — construct type, filter, seed, unload settings.
- Zone creation sub-flow: calls `game::look_around()` twice for start/end points,
  then queries type, options, and name in `game::zones_manager()`.
- Zone editing: `zone_data::set_name()`, `zone_data::set_type()`, and
  `zone_data::set_enabled()` in `src/clzones.cpp`.

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

- Vehicle interaction entry: `vehicle::interact_with()` in `src/vehicle_use.cpp`
  builds and queries a `veh_menu` (wraps `uilist`).
- `veh_menu::query()` in `src/veh_utils.cpp` creates `uilist` with context
  `"VEHICLE"`.  Existing uilist hooks provide basic focus announcements.
- Electronics menu: `vehicle::build_electronics_menu()` in `src/vehicle_use.cpp`
  — toggles for lights, stereo, climate, kitchen.  State changes via
  `add_electronic_toggle()`.
- Handbrake: `handbrake()` in `src/vehicle_use.cpp` zeroes cruise velocity.
- Engine control: `vehicle::control_engines()` in `src/vehicle_use.cpp`.
- Start/stop: `vehicle::start_engines()` / `vehicle::stop_engines()` in
  `src/vehicle_use.cpp`.
- Driving: `pldrive()` in `src/handle_action.cpp` calls `vehicle::pldrive()`
  in `src/vehicle_move.cpp`.
- Cruise control: `vehicle::cruise_change()` in `src/vehicle_move.cpp`.
- Speed gauge: `vehicle::print_speed_gauge()` in `src/vehicle_display.cpp` —
  cruise target, actual speed, strain coloration (normal/yellow/red).
- Fuel gauge: `vehicle::print_fuel_indicator()` in `src/vehicle_display.cpp` —
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

- Entry: `veh_interact::run()` in `src/veh_interact.cpp` instantiates the screen
  and calls `veh_interact::do_main_loop()`.
- Input context: `"VEH_INTERACT"` registered in `veh_interact::do_main_loop()`.
- Cursor movement: `veh_interact::move_cursor()` shifts tile, recalculates
  `parts_here`, `cpart`, and repair/mount status.
- Part list at tile: `vehicle::print_part_list()` in `src/vehicle_display.cpp`.
- Part descriptions: `vehicle::print_vparts_descs()` in `src/vehicle_display.cpp`.
- Vehicle stats: `veh_interact::display_stats()` in `src/veh_interact.cpp` —
  speeds, mass, cargo volume, durability, most damaged parts.
- Action bar: `veh_interact::display_mode()` in `src/veh_interact.cpp` —
  enabled/disabled tasks with reasons via `veh_interact::cant_do()` and
  `veh_act_desc()`.
- Task sub-loops in `src/veh_interact.cpp`:
  - Install: `veh_interact::do_install()` — tabs/categories, installable parts.
  - Repair: `veh_interact::do_repair()` — parts on tile.
  - Remove: `veh_interact::do_remove()`.
  - Overview: `veh_interact::overview()`.
- Requirements format: `veh_interact::format_reqs()`.
- Accessibility hooks: on `veh_interact::move_cursor()` announce tile position
  and parts summary; on task selection announce available action and requirements.

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

- Tab manager: `class tab_manager` in `src/newcharacter.cpp` tracks position,
  handles `"PREV_TAB"`, `"NEXT_TAB"`, `"SELECT"`, `"QUIT"`.
- Tabs declared in `avatar::create()` in `src/newcharacter.cpp`: POINTS (0),
  SCENARIO (1), PROFESSION (2), BACKGROUND (3), STATS (4), TRAITS (5),
  SKILLS (6), DESCRIPTION (7).
- Tab 0 (Points): `set_points()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_POINTS"`.  Already has `screen_reader_mode` branch — extend it.
- Tab 1 (Scenario): `set_scenario()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_SCENARIOS"`.  Left pane: scenario list with point costs.  Right
  pane: `scrolling_text_view details` with description, locations, missions.
- Tab 2 (Profession): `set_profession()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_PROFESSIONS"`.  Same left/right layout.
- Tab 3 (Background): `set_hobbies()` in `src/newcharacter.cpp`, context
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

- Tab 4 (Stats): `set_stats()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_STATS"`.  Four stats with LEFT/RIGHT to adjust values.  Detail
  pane: `assemble_stat_details()` shows derived effects (HP, carry, damage,
  throw distance, etc.).
- Tab 5 (Traits): `set_traits()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_TRAITS"`.  2–3 columns (positive, negative, neutral).  Locked
  traits by scenario/profession/hobby.  Conflict highlighting.
- Tab 6 (Skills): `set_skills()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_SKILLS"`.  Skills grouped under category headers.  LEFT/RIGHT
  adjusts level.
- Tab 7 (Description): `set_description()` in `src/newcharacter.cpp`, context
  `"NEW_CHAR_DESCRIPTION"`.  Name, gender, height, age, blood type,
  start location, vehicle.  Actions: `"SAVE_TEMPLATE"`, `"REROLL_CHARACTER"`,
  `"CHOOSE_LOCATION"`.
- Final confirmation: `"NEXT_TAB"` on Description tab validates points,
  prompts confirmation, and sets `tabs.complete = true` in `set_description()`.
- Template save/load: template management in `set_description()`.

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

- World selection: `worldfactory::pick_world()` in `src/worldfactory.cpp`,
  context `"PICK_WORLD_DIALOG"`.
- World creation basic: `worldfactory::show_worldgen_basic()` in
  `src/worldfactory.cpp`, context `"WORLDGEN_CONFIRM_DIALOG"`.
- World name: `prompt_world_name()` in `src/worldfactory.cpp`.
- World deletion and reset: `main_menu::world_tab()` in `src/main_menu.cpp` —
  manage world menu with delete/reset confirmation popups.
- Mod selection: `worldfactory::show_worldgen_tab_modselection()` in
  `src/worldfactory.cpp`.
- Mod list drawing: `worldfactory::draw_mod_list()` in `src/worldfactory.cpp`.
- Mod manager context: `"MODMANAGER_DIALOG"` registered in
  `worldfactory::show_worldgen_tab_modselection()`.
- Mod info: `mod_ui::get_information()` in `src/mod_manager_ui.cpp` —
  authors, dependencies, version, description.
- Mod actions: `mod_ui::try_add()`, `mod_ui::try_rem()`, and `mod_ui::try_shift()`
  in `src/mod_manager_ui.cpp`.
- Two columns: "Mod List" (available) and "Mod Load Order" (active) in
  `worldfactory::show_worldgen_tab_modselection()`.
- Category tabs: tabs in `worldfactory::show_worldgen_tab_modselection()`.

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

- Options screen: `options_manager::show()` in `src/options.cpp`, context
  `"OPTIONS"`.
- Option tabs/pages: `pages_[i].name_` ("General", "Interface", "Graphics",
  "Sound", etc.) in `options_manager::show()`.
- Option entry text: `fmt_name_value` lambda in `options_manager::show()` — group
  header or option name + value.
- Tooltip: `curr_item.fmt_tooltip()`.
- Value editing: `on_select_option` lambda in `options_manager::show()` —
  LEFT/RIGHT cycles, CONFIRM opens input for numeric types.
- Note: the curses options screen does not have search/filter — options are
  organized by pages and collapsible groups.
- Safe-mode rules: `src/safemode_ui.cpp` — rule list with add/edit/delete.
- Autopickup rules: `src/auto_pickup.cpp` — similar rule editor pattern.
- Panel admin: `panel_manager::show_adm()` in `src/panels.cpp`, context
  `"PANEL_MGMT"`.  Two columns: panel list and layout list.  Toggle panel
  visibility, reorder, activate layout.
- Distraction settings: `distraction_manager_gui::show()` in
  `src/distraction_manager.cpp`, context `"DISTRACTION_MANAGER"`.

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

## Stage 22A - Complete tutorial acceptance

**Outcome:** A blind player can complete the entire built-in tutorial with
keyboard and NVDA, understand every lesson and continuation instruction, and
leave or resume safely.  This stage follows the gameplay, interface, and
coverage stages because its exercises use their controls.

**Code inventory:** tutorial launch in `src/main_menu.cpp`;
`tutorial_game::init()`, `per_turn()`, `pre_action()`, `post_action()`, and
`add_message()` in `src/gamemode_tutorial.cpp`; C++ lessons in
`data/mods/dda_tutorial/snippets.json`; trap lessons and map structure in
`data/mods/dda_tutorial/traps.json` and `mapgen.json`.  The preliminary trigger,
continuation, and exercise inventory is in `doc/tutorial_lesson_inventory.md`.

**Implementation hints:**

- Current C++ and EOC lessons already speak their full popup text through the
  shared popup path.  Reassess the actual NVDA experience after the exercised
  controls are accessible, including interruption, repetition, and speech from
  triggers that can fire again on reentry.
- Recheck every lesson trigger, map route, continuation instruction, and required
  action against the then-current game.  Update the inventory if upstream data
  or integration points have changed.
- Confirm that both C++ and EOC lessons expand keybinding tags and that their
  spoken instructions match the current controls.  Correct misleading tutorial
  data or accessibility gaps found during the full walkthrough.
- If a new player-facing lesson interaction is needed, present three distinct
  designs and record the selected behavior here before changing code.

**Initial assessment:** The current data has 35 C++ snippet texts, of which 33
have a trigger, and 14 EOC popup lessons on the tutorial map.  One additional
trap sets pain without showing a lesson.  Several exercises rely on Stages
06/06A, 08–12, and 13.  The bat pickup near the start depends on Stage 09.

**Success criteria:** The entire tutorial route and every required exercise can
be completed without a mouse or visual cues.  Lesson text, current instructions,
feedback, and exit/save warnings are spoken accurately; interrupted information
can be recovered; repeated triggers do not flood speech; and no lesson relies
on a control or visual detail that remains inaccessible.  The final exercise
and return to the main menu are verified.

**Possible automated tests:** Use fixed lessons and map fixtures to capture
launch, C++ and EOC delivery, keybinding substitution, repeat triggers,
interruption recovery, transitions, completion, save/exit warning, and menu
return.  Check that every referenced action and tutorial trap exists.

**Required human verification:** A blind NVDA user plays the full tutorial from
launch through the final room, completes every required exercise, checks both
lesson sources and continuation guidance, deliberately interrupts speech and
recovers the instructions, saves/exits, and confirms the menu return.  Record
the tested Windows, NVDA, and fork versions and any remaining limitations.

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

## Stage 25 - Accessible player guide and quick start

**Outcome:** `access_guide.md` gives a blind player one reliable starting point
for learning the fork's accessibility features and the basic gameplay loop.  A
short, task-oriented quick start uses the opening of a standard evacuee game to
show how to obtain information, make decisions, act, recover from mistakes, and
continue playing without assuming prior Cataclysm knowledge.

**Code inventory:** Create `access_guide.md`; link it from `README.md` and the
packaged first-run documentation.  Verify every documented command and spoken
behavior against `data/raw/keybindings.json`, the accessibility service and its
game/UI integration points, and the completed-stage record in
`ACCESS_DONE.md`.  Verify the quick-start scenario and optional guidance against
`data/json/scenarios.json`, `data/core/help.json`, the evac-shelter map and
computer data, and the Reach Refugee Center mission definition.  Record any
additional source paths discovered while writing the guide.

**Content requirements:**

- Explain supported platform and screen-reader requirements, enabling or
  detecting accessibility, keyboard discovery, announcement types, concise and
  detailed information, repeat-last, speech interruption, saving, exiting,
  known limitations, and where to report defects.
- Describe the game as an open-ended survival sandbox.  Clearly distinguish
  required mechanics from suggestions, optional missions, lore, and examples so
  the guide does not imply a single mandatory route through the game.
- Provide a concise quick start based on the standard evacuee start.  Walk the
  player through understanding the initial situation, reviewing character and
  status information, examining the shelter, finding and managing useful items,
  reading the notice board or shelter computer, deciding whether to accept the
  optional Refugee Center objective, leaving safely, handling nearby threats,
  and saving the game.
- For each step, state the goal, keyboard action or discoverable command, the
  important speech feedback, and what success or a recoverable error sounds
  like.  Avoid fixed keys where bindings may differ; give the action name and
  explain how to discover its current binding.
- Keep essential instructions in the main flow and place deeper command or
  gameplay explanations in clearly named reference sections.  Use short
  headings, lists, and plain language suitable for sequential screen-reader
  reading.

**Success criteria:** A new blind player can find the guide from the repository
and packaged build, configure and verify NVDA output, discover rather than guess
the relevant bindings, complete the documented shelter sequence, understand
that the Refugee Center objective is optional, leave the shelter with basic
supplies, respond to a visible threat, save, exit, and resume.  Every described
announcement and accessibility feature matches the shipped build; the guide
does not expose hidden gameplay information or depend on color, layout, symbols,
screenshots, or mouse operation.

**Possible automated tests:** Check internal links and referenced files; verify
that named input actions exist in `data/raw/keybindings.json`; lint headings and
required sections; detect undocumented literal key assumptions; and run the
focused accessibility suite plus the documentation/package validation from
Stage 23.

**Required human verification:** A blind NVDA user unfamiliar with the guide
follows it from a clean configuration without coaching, completes the quick
start using keyboard input only, deliberately interrupts speech and uses
repeat-last, recovers from at least one mistaken or cancelled action, saves,
exits, and resumes.  Record confusing, missing, excessive, or inaccurate
instructions and revise the guide before accepting the stage.

## Known blockers and required follow-up

- Stage 03A world announcements for zero and multiple saves passed capture tests
  but were not exercised with NVDA; the tester had one world with one save.
  Recheck both counts during the Stage 22 coverage audit.
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
