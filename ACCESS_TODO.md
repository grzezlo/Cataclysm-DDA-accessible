# Accessibility roadmap

This file is the authoritative plan for unfinished accessibility work.  Stable
architecture, workflow, and completion rules are in `AGENTS.md`; completed work
and its verification record are in `ACCESS_DONE.md`.

The first supported configuration is the Windows x64 tiles build with NVDA.
Stage-specific inventories below are starting points and must be corrected when
implementation discovers a better integration point.

Stages 01-10 establish the minimum independent survival and combat loop.  Stages
11-21 cover the major progression, management, and setup interfaces.  Stages
22-23 are native release gates.  Stage 24 starts only after the native semantic
model is stable.

Unless a stage lists an exception, its automated gate includes
`test_access.bat` and a successful `build_win_x64.bat`.  The human gate for
native stages includes the general NVDA checks required by `AGENTS.md` plus the
scenarios listed for the stage.  Stage 24 uses the declared browser and screen
reader combinations instead.

## Stage 01 - On-demand situation review

**Outcome:** A player can efficiently inspect the current position and immediate
surroundings and return to gameplay without moving the character.

**Selected interaction:** A rebindable gameplay command opens a standard
keyboard-navigable `uilist` containing normally visible situation elements as a
single flat list from the current z-level and within the engine's maximum
60-tile visibility range, ordered by distance from the player with a stable
tie-breaker.  The range includes only state the character can currently see but
is independent of visual zoom, window size, panels, and camera offset; there is
no separate entry-count limit.  The current position,
movement state, obstacles, exits, items, creatures, hazards, and furniture are
represented as semantic entries rather than visual map cells or category
headings.  Ordinary ground such as grass, pavement, and floors is omitted;
mechanically significant terrain is represented as a hazard, exit, or obstacle.
Connected tiles with the same obstacle or area-hazard semantics form one entry,
using eight-directional adjacency and reporting the region's tile count and
nearest location.  Creatures, items, exits, traps, furniture, and other discrete
objects remain separate entries.  Moving focus speaks a concise, self-contained entry with the
information needed to understand that element without a second detail mode.
Enter or Escape closes the list and returns directly to unchanged gameplay.  The
visible list mirrors the spoken content, passive redraws remain silent, and
Alt+R repeats the most recent focus announcement; moving away and back repeats an
entry as well.  The reusable list-building approach should support later
nearby-object interfaces without embedding map or NVDA dependencies in the
accessibility service.

**Selected command access:** Accessibility commands use Alt as a familiar,
compact-keyboard-friendly modifier.  Alt+R repeats the last screen-reader
announcement in gameplay and every supported menu context; Alt+V opens the
situation review from gameplay.  F6 and F7 are not retained as aliases.  The
existing Enter action menu has a top-level `Accessibility` category whose
entries can be selected with arrows and Enter or with menu-local R and V.  A
repeat selected through that menu repeats the meaningful announcement from
before the menu opened rather than the menu's own focus announcement.

**Code inventory:** `src/game.cpp` and `src/game.h` for visible local state and
look helpers; `src/map.cpp` for terrain/furniture names; `src/uilist.cpp` for the
shared semantic list behavior; `src/handle_action.cpp` and
`data/raw/keybindings.json` for dispatch; the accessibility service and test.

**Success criteria:**

- The interaction design selected through the required three-option review
  provides efficient access to visible obstacles, exits, items, creatures,
  hazards, mechanically significant terrain, furniture, and movement state
  without listing every ordinary ground tile.
- Information volume and detail are under the player's control according to the
  accepted design; the player is not forced to wait through irrelevant detail.
- Focus, ordering, direction/distance wording, return to gameplay, and
  repeat-last behavior are stable and translatable.  Passive redraws are silent.
- The interaction does not reveal occluded, invisible, remembered-only, or
  otherwise hidden state.

**Possible automated tests:** Exercise the selected interaction with fixed empty,
indoor, outdoor, furniture, item, hazard, and creature snapshots; verify its
accepted focus/order and detail behavior, visibility, gameplay gating,
deduplication, repeat-last, and cancel/return.  Add a map-backed visibility test
if pure snapshots cannot prove parity.

**Required human verification:** Review indoor and outdoor positions, furniture,
an item pile, a hazard, and a visible creature.  Confirm the desired information
can be reached efficiently, navigation and return are predictable, hidden state
is not disclosed, and passive redraws are silent.

## Stage 02 - Look-around exploration

**Outcome:** A player can inspect visible map tiles with a semantic cursor without
moving the character.

**Code inventory:** `game::look_around`, `game::draw_look_around_cursor`,
`game::peek`, direction selection, and the look-information helpers in
`src/game.cpp`/`src/game.h`;
`src/ui_extended_description.cpp` for detailed tile/entity descriptions.

**Success criteria:** Entering the mode identifies it and the cursor origin.
Each cursor move reports relative direction, distance, z-level, terrain,
furniture, fields/hazards, vehicle, visible creatures, and visible items in a
consistent order.  Extended detail and exit restore meaningful focus.  Rapid
movement suppresses redraw duplicates without losing the final position.
Peeking identifies its direction and observation origin, preserves crouching
and character position correctly, and makes its return and available peek
actions unambiguous.

**Possible automated tests:** Capture entry, unchanged focus, cardinal/diagonal
movement, z-level changes, empty/occupied tiles, extended detail, hidden tiles,
rapid movement, and return to gameplay.  Cover peek direction selection,
cancellation, crouched peeking, z-level peeking, position restoration, and
available peek actions.

**Required human verification:** Explore a room, street, multiple z-levels,
vehicle, item pile, creature, hazard, and obscured area.  Confirm the cursor is
spatially understandable and fast navigation leaves the correct final focus.
Peek from standing and crouched positions, cancel during direction selection,
and confirm that its origin, time cost, available actions, and return to the
unchanged character position are understandable.

## Stage 03 - Nearby item and creature lists

**Outcome:** The `V` interface provides efficient list-based exploration of
normally visible nearby items and creatures.

**Code inventory:** `game::list_items_monsters`, `game::list_items`, and monster
list handling in `src/game.cpp`/`src/game.h`; `src/map_item_stack.cpp` for grouped
items; `src/uilist.cpp` only where the shared list actually owns focus.

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

**Code inventory:** `src/panels.cpp` for HUD data; `src/game.cpp` and
`src/handle_action.cpp` for gameplay dispatch; `src/player_display.cpp` for
detailed status; character state providers; `src/messages.cpp` to avoid
duplicating existing game-log announcements.

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

## Stage 05 - Reusable input controls and command discovery

**Outcome:** Common controls expose consistent semantics before more bespoke
screens depend on them, and accessibility commands can be discovered in-game.

**Code inventory:** `src/uilist.cpp`, `src/popup.cpp`, `src/input_popup.cpp`,
`src/string_input_popup.cpp`, `src/ui_iteminfo.cpp`, `src/input_context.cpp`,
`src/help.cpp`, `data/core/help.json`, and `data/raw/keybindings.json`.

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

## Stage 06 - Standard inventory and item information

**Outcome:** Common inventory selectors, item details, comparisons, and item
actions are usable without visual columns, colors, or highlights.

**Code inventory:** `src/inventory_ui.cpp`, `src/game_inventory.cpp`,
`src/item_action.cpp`, `src/ui_iteminfo.cpp`, and `src/iteminfo_query.cpp`.

**Success criteria:** A selector reports its purpose, category/column, item,
count, location, relevant state, and disabled reason.  Categories, filtering,
quantity, comparison, detail scrolling, action menus, confirmation, and cancel
are announced.  Wield, wear/take off, use, consume, read, reload/unload, and drop
flows expose their results and errors.

**Possible automated tests:** Capture selector entry, category and item changes,
stacks, disabled entries, filters, quantity, comparison, details, action choice,
failure, and return focus; reuse item-info fixtures.

**Required human verification:** Complete every common item flow above, including
a stack, comparison, a disabled item, and a failed action.  Confirm details are
ordered usefully and verbosity remains manageable.

## Stage 07 - Advanced inventory and multiselect transfers

**Outcome:** Two-pane and multiselect item transfers are independently usable.

**Code inventory:** `src/advanced_inv.cpp`, `src/advanced_inv_pane.cpp`, other
`src/advanced_inv_*` helpers, and multiselect paths in `src/inventory_ui.cpp`.

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

## Stage 09 - Pickup, drop, and container transfers

**Outcome:** Local item transfer workflows expose item, quantity, source,
destination, capacity, ownership, and result.

**Code inventory:** `src/pickup.cpp`, pickup/drop selectors in
`src/game_inventory.cpp` and `src/inventory_ui.cpp`, item locations and container
selection paths reached by those workflows.

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

**Code inventory:** targeting and aim UI in `src/ranged.cpp`/`src/ranged.h`;
combat dispatch in `src/game.cpp` and `src/handle_action.cpp`; existing result
messages in `src/messages.cpp`; safe-mode warnings in `src/safemode_ui.cpp`.

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

**Success criteria:** Category, recipe, variant, availability, missing
requirements, skill, time, batch size, related recipe, search, confirmation,
start, failure, and result are announced.  Disassembly exposes target, returned
components, requirements, confirmation, and result.

**Possible automated tests:** Capture categories, filters, available/unavailable
recipes, requirement detail, variant/batch changes, related recipes, start,
failure, disassembly, and return focus using crafting fixtures.

**Required human verification:** Craft single and batch recipes, inspect a recipe
with missing requirements, search/filter, and disassemble an item with NVDA.

## Stage 12 - Construction, activities, and interruptions

**Outcome:** Projects and long-running actions expose selection, placement,
progress on request, interruptions, resumption, and completion.

**Code inventory:** `src/construction.cpp`, `src/player_activity.cpp`,
`src/activity_actor.cpp`, `src/activity_handlers.cpp`, and
`src/distraction_manager.cpp`.

**Success criteria:** Construction category/project, requirements, variants,
placement, and invalid reason are announced.  Activities announce start,
meaningful status on demand, interruption reason and choices, resume/cancel,
failure, and completion without turn-by-turn flooding.

**Possible automated tests:** Capture project focus, missing requirements,
placement cursor/invalid tile, activity start/status, deduplicated progress,
interruption policy, resume/cancel, failure, and completion.

**Required human verification:** Place and complete construction, attempt an
invalid project, run a long activity, trigger an interruption, resume it, and
cancel another.

## Stage 13 - Character information

**Outcome:** All read-only character information presented in tabs, rows, colors,
and detail panes is semantically reviewable.

**Code inventory:** `src/player_display.cpp` and the character/body/morale/skill
providers it uses.

**Success criteria:** Tab/section identity and focused row are explicit.  Stats,
body condition, encumbrance, skills, traits, effects, morale, speed, needs, and
proficiencies expose the values and explanations visible in the standard UI.
Scrolling and tab changes preserve focus and do not reread unchanged content.

**Possible automated tests:** Snapshot representative healthy, injured,
encumbered, effect-heavy, skilled, and morale states; capture tab, row, detail,
scroll, and return-focus changes.

**Required human verification:** Navigate every character-information tab in
simple and information-dense characters and find specified values without visual
assistance.

## Stage 14 - Bionics, mutations, spells, and martial arts

**Outcome:** Character capabilities can be inspected, selected, activated, and
configured without relying on layout or color.

**Code inventory:** `src/bionics_ui.cpp`, `src/mutation_ui.cpp`, `src/magic.cpp`,
`src/martialarts.cpp`, and their owning data/state modules.

**Success criteria:** Each screen reports section, selection, active/passive
state, cost, cooldown, chance, description, binding, and disabled reason where
normally shown.  Activation/deactivation, spell choice, martial-style selection,
configuration, confirmation, result, and focus return are complete.

**Possible automated tests:** Capture screen entry, selection/detail changes,
activation/deactivation, insufficient-resource and disabled states, spell/style
selection, confirmation, result, and return focus.

**Required human verification:** Inspect and operate at least one bionic,
mutation, spell, and martial style, including a disabled or insufficient-resource
case for each available system.

## Stage 15 - NPC dialogue

**Outcome:** Conversation text and response selection support complete branching
dialogue without repeated rereading.

**Code inventory:** dialogue rendering, response input, and nested menus in
`src/npctalk.cpp`/`src/npctalk.h`; shared lists and popups where actually used.

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

**Success criteria:** Mission category/state, title, giver, objective, deadline,
reward, tracked state, normally known location, success/failure, and focus return
are announced.  The mission screen exposes a clear map action, but its semantic
map handoff is completed and accepted in Stage 18.

**Possible automated tests:** Reuse mission fixtures to capture categories,
selection/details, accept, track/untrack, known/unknown locations, deadline,
completion/failure, and invocation of the map action.

**Required human verification:** Accept, review, track, locate, complete, and turn
in a mission; inspect one mission without a known location and one failed state.

## Stage 18 - Overmap navigation and zones

**Outcome:** Known-world exploration, route planning, notes, and zone management
are spatially understandable without revealing unexplored information.

**Code inventory:** `src/overmap_ui.cpp`, known-state providers in
`src/overmapbuffer.cpp` and `src/overmap.cpp`, and zone UI in `src/clzones.cpp`.

**Success criteria:** Cursor movement reports relative direction/distance,
z-level, known terrain, notes, mission markers, and route state.  Search, notes,
route selection/travel, zone list, creation, placement, editing, and return are
keyboard operable.  Unknown map content stays hidden.

**Possible automated tests:** Capture cursor and z-level movement, known/unknown
tiles, search, notes, mission handoff, route creation/failure/travel, zone focus,
placement/editing, and visibility filtering.

**Required human verification:** Find a known destination, add/edit a note,
locate a mission, plan/follow a route, change z-level, and create/edit a zone.

## Stage 19 - Vehicles

**Outcome:** Vehicle entry, controls, driving, status, parts, and maintenance are
accessible through semantic direction and state information.

**Code inventory:** `src/vehicle_use.cpp`, `src/vehicle_display.cpp`, vehicle
movement/state modules, and vehicle action dispatch in `src/game.cpp`.

**Success criteria:** Entry position, control focus, speed, heading, turn/movement
result, cruise state, fuel/power, warnings, part selection/status, install/remove/
repair choices, disabled reasons, and result are announced.  Movement feedback
distinguishes character movement from vehicle motion.

**Possible automated tests:** Reuse vehicle fixtures for entry/control, start,
speed/heading change, blocked motion, cruise, fuel/power, part focus, repair or
install, invalid action, stop, and exit.

**Required human verification:** Enter, inspect, start, steer, accelerate, stop,
and leave a vehicle; inspect and perform one part action; exercise a warning and
blocked/invalid case.

## Stage 20 - Custom character creation

**Outcome:** A player can create and start a custom character using every
creation tab instead of relying on quick start.

**Code inventory:** creation tabs, state, and validation in
`src/newcharacter.cpp`; existing fixtures in `tests/new_character_test.cpp`;
main-menu entry and template handoff in `src/main_menu.cpp`.

**Success criteria:** Each tab reports identity, focus, value/selection, point
totals, conflicts, validation, and detail.  Scenario, profession, traits, stats,
skills, description, start location, randomization, templates, naming, and final
confirmation are complete and focus-stable.

**Possible automated tests:** Capture every tab and representative changes,
point updates, conflicts, filters, randomization, validation errors, template
save/load, final confirmation, and focus restoration using character fixtures.

**Required human verification:** Create and start a custom character using every
tab, correct point/conflict errors, randomize selectively, and save/load a
template.

## Stage 21 - World, mod, and application configuration

**Outcome:** World management, mods, options, rule editors, and presentation
configuration are independently usable.

**Code inventory:** `src/worldfactory.cpp`, `src/mod_manager_ui.cpp`,
`src/options.cpp`, `src/safemode_ui.cpp`, `src/auto_pickup.cpp`, `src/panels.cpp`,
and main-menu world management in `src/main_menu.cpp`.

**Success criteria:** World selection/create/copy/reset/delete, naming, mod
category/order/dependencies, world and global options, panel configuration,
safe-mode/autopickup rules, search, values, defaults, conflicts, confirmation,
save, cancel, and errors are announced without relying on columns or color.

**Possible automated tests:** Reuse world/options fixtures to capture world and
mod focus/reorder/dependency errors, option search/value/reset, rule editing,
panel changes, destructive confirmation, save/cancel, and return focus.

**Required human verification:** Create a configured world, alter mod order,
change and restore options/panels, add/edit rules, and safely exercise copy,
reset, and delete confirmations.

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
exists.  Save/load, death, errors, debug-disabled normal play, and exit are
covered by an end-to-end keyboard journey.

**Possible automated tests:** Table-driven service tests for categories,
collisions, mode transitions, backend failure/reset, deduplication, and history;
coverage-manifest consistency checks; `test_access.bat`, `test_all.bat`, Windows
test build, tiles build, and the branch's static checks.

**Required human verification:** Complete the coverage-manifest journey and a
long play session, deliberately navigating rapidly and interrupting speech.
Confirm every unverified popup noted in `ACCESS_DONE.md`, including standard,
developer-error, static-instruction, and progress-popup integration, and record
all accepted limitations.

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
