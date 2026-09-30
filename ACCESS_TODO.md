# Accessibility roadmap

This file is the authoritative plan for unfinished accessibility work.  Stable
architecture, workflow, and completion rules are in `AGENTS.md`; completed work
is in `ACCESS_DONE.md`, and scenarios awaiting screen-reader confirmation are in
`ACCESS_USERTESTS.md`.

The first supported configuration is the Windows x64 tiles build with NVDA.
Stage-specific inventories below are starting points and must be corrected when
implementation discovers a better integration point.

Stages 06–10 establish the minimum independent survival and combat loop.  Stages 11–21A
cover the major progression, management, and setup interfaces.  Stages 22, 22A, and 23
are native release gates: coverage, the complete tutorial, and packaging.  Stage 24
starts only after the native semantic model is stable.

Unless a stage lists an exception, a stage is complete when its possible
automated tests are implemented, `test_access.bat` passes, and
`build_win_x64.bat` succeeds.  Its user-test scenarios then move to
`ACCESS_USERTESTS.md` and do not block the next stage.

## Interaction conventions

Accepted by the maintainer on 2026-09-29.  A stage whose behavior these
conventions fully cover needs no three-option review; its entry names the
conventions it applies.  A stage's selected interaction overrides them where
it says so.

1. **Keys.**  Existing game keys are unchanged.  New accessibility actions use
   `Alt+letter`, are added to `data/raw/keybindings.json`, and appear in the
   keybinding help.  Shared actions: `Alt+R` repeats the last announcement,
   `Alt+D` speaks details of the focused element, and `Alt+S` speaks the
   screen summary.
2. **Lists and selectors.**  Opening a list speaks its name and item count
   once ("Inventory, 12 items") followed by the focused row.  Moving focus
   speaks only the concise row: name and state, without a position.  Wrapping
   past the end says "top" or "bottom" before the row.  The current position
   is available through `Alt+S` or `Alt+D`.
3. **Tabs, columns, and panes.**  Switching speaks the tab or pane name, its
   item count, and the focused row, not the whole content.
4. **Value changes.**  Adjusting a value (stats, quantities, options, points)
   speaks the new value and its direct consequence ("Strength 10, 4 points
   left") without rereading the row label when it is unchanged.
5. **Long text.**  Descriptions, recipe details, and similar long content open
   in the Stage 03C document view with sentence and line navigation instead of
   being spoken in full on focus.
6. **Map cursors.**  Targeting, placement, zone, and overmap cursors speak a
   concise description with distance and direction, as look-around does;
   details come on request.
7. **Multi-step and long actions.**  One summary at start, silence while it
   runs, and one aggregated result at the end.  Progress is available through
   `Alt+R`.
8. **Confirmations and warnings.**  Use the Stage 05 shared controls; the
   spoken question names what it concerns.  State is never conveyed by color
   or position alone: speak words such as "Over weight limit" or
   "Unavailable: <reason>".

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

**Selected interaction (option A, changes spoken, status on request):**

- Keys are unchanged.  Conventions 1 and 8 apply; vehicle menus use the
  shared list speech.
- Each control change speaks the new state: cruise speed, heading, engine
  on or off, handbrake.
- A newly visible obstacle on the current heading within a few tiles is
  announced once, like a threat transition ("wall ahead, 4 tiles"), and again
  only after it leaves and re-enters the path.  Collisions and warnings come
  from game messages.
- The terrain under the vehicle is not spoken per step.
- `Alt+S` while driving speaks actual and cruise speed, heading, engine
  state, fuel and power with estimated time, and handbrake.

**Success criteria:** Entry position, control focus, speed, heading,
turn/movement result, cruise state, fuel/power, and warnings are announced.
Movement feedback distinguishes character movement from vehicle motion.

**Possible automated tests:** Reuse vehicle fixtures for entry/control, start,
speed/heading change, blocked motion, cruise, fuel/power, and exit.

**User-test scenarios:** Enter, start, steer, accelerate, stop, and
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

**Selected interaction (option B, part list instead of the grid):**

- Keys are unchanged; the part list is added.  Conventions 2, 5, and 8
  apply.
- Entering the screen opens a flat list of all parts grouped by location:
  "headlight, front left, damaged 40%".  `FILTER` narrows it by name.
- Enter on a part moves the grid cursor to its tile and opens a menu of the
  available tasks (repair, remove, install here, and others) with blocked
  tasks and their reasons as words.  Requirements open in the document view.
- The grid cursor stays available with the existing keys and speaks the
  tile's location and parts summary when moved.  `Alt+S` speaks the vehicle
  statistics.

**Success criteria:** Part selection, status, install/remove/repair choices,
disabled reasons, skill/tool requirements, and results are announced.  The
tile cursor and task sub-screens are keyboard operable.

**Possible automated tests:** Capture cursor movement, parts at tile, install
tab/part selection, repair part selection, invalid action, and return focus.

**User-test scenarios:** Inspect a vehicle, navigate parts, and
perform one part action (repair or remove); exercise an invalid/blocked case.

## Stage 20 - Character creation: scenario and profession

**Outcome:** The first four character creation tabs (points, scenario, profession,
background/hobbies) are independently usable.

**NVDA observation (2026-09-26):** After entering Custom Character, the Points
tab did not announce arrow-key focus changes.  Its existing screen-reader-mode
branch changes rendered content but does not provide semantic announcements.

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

**Selected interaction (conventions 2, 3, 4, and 5):**

- Keys are unchanged.  Changing a tab speaks its name, item count, and focused
  row ("Profession, 42 items").
- A row speaks name and point cost ("Evacuee, 0 points"), or unavailability
  with its reason ("Unavailable: conflicts with scenario").  Selecting speaks
  the new point balance.
- `Alt+D` opens the description in the document view.  Filter, sort, and
  randomize speak their result.
- The Points tab's existing screen reader branch gains normal focus speech.

**Success criteria:** Each tab reports identity, focus, selection, point costs,
conflicts, and detail.  Scenario, profession, and background selection,
filtering, sorting, and randomization are announced and focus-stable.

**Possible automated tests:** Capture every tab entry, selection changes, point
updates, filter, sort, and return focus using character fixtures.

**User-test scenarios:** Create a character using each of the first four
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

**Selected interaction (conventions 2, 3, 4, 5, and 8):**

- Keys are unchanged.  Stats: Left and Right speak "Strength 10, 4 points
  left"; `Alt+D` opens the derived effects in the document view.
- Traits: changing a column speaks "Negative traits, 30 items"; a row speaks
  name, cost, and state ("Fast learner, 2 points, selected", "locked by
  profession", "conflicts with Slow learner").
- Skills: a level change speaks the new level and point balance; entering a
  category speaks its name.
- Description: each field speaks its label and value; editing uses the Stage
  05 text input.  Templates, validation errors, and the final confirmation
  use the shared controls.

**Success criteria:** Stats, traits, skills, description, start location,
randomization, templates, naming, and final confirmation are complete and
focus-stable.  Point totals and conflicts are announced on change.

**Possible automated tests:** Capture stat adjustment, trait selection/conflict,
skill level change, description fields, template save/load, validation errors,
and final confirmation.

**User-test scenarios:** Complete character creation using every
remaining tab, correct point/conflict errors, save/load a template, and start
a new game.

## Stage 21 - World and mod management

**Outcome:** World selection, creation, copying, deletion, and mod configuration
are independently usable.

**Observed blocker (2026-09-26):** After the last world was deleted, selecting
Random Character opened the basic world-creation screen without an NVDA
announcement.  Its default name and Finish shortcut allow keyboard completion,
but the screen still needs the Stage 21 interaction review and implementation.

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

**Selected interaction (conventions 2, 3, 5, and 8):**

- Keys are unchanged.  A world row speaks its name and character count.  The
  world-creation screens get focus speech, resolving the observed blocker.
- In the mod manager the "Mod List" and "Load Order" columns behave as tabs.
  A row speaks name, state, and category.  Add, remove, and move speak the
  new position; missing dependencies speak their reason as words.
- `Alt+D` opens mod information in the document view.  Copy, reset, and
  delete confirmations use the Stage 05 controls.

**Success criteria:** World selection/create/copy/reset/delete, naming, mod
category/order/dependencies, and confirmation are announced.

**Possible automated tests:** Capture world and mod focus/reorder/dependency
errors, destructive confirmation, save/cancel, and return focus.

**User-test scenarios:** Create a configured world, alter mod order,
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

**Selected interaction (conventions 2, 3, 4, and 8):**

- Keys are unchanged.  Option pages behave as tabs.  A group header speaks
  "expanded" or "collapsed"; an option row speaks name and value, and a
  change speaks the new value.  `Alt+D` speaks the option tooltip.
- Safe-mode, autopickup, and distraction rule lists use the shared list and
  input speech for editing.
- The panel manager's panel and layout columns behave as tabs; a row speaks
  name and visibility, and moving a panel speaks its new position.

**Success criteria:** Options pages, group expand/collapse, value editing,
defaults, safe-mode/autopickup rules, panel configuration, and distraction
settings are announced without relying on columns or color.

**Possible automated tests:** Capture option search/value/reset, rule editing,
panel changes, save/cancel, and return focus.

**User-test scenarios:** Change and restore options, add/edit rules,
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

**User-test scenarios:** Complete the coverage-manifest journey and a
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
`tutorial_game::init()` (which must enable gameplay speech itself because the
tutorial bypasses `game::start_game()`), `per_turn()`, `pre_action()`,
`post_action()`, and `add_message()` in `src/gamemode_tutorial.cpp`; C++ lessons in
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

**User-test scenarios:** A blind NVDA user plays the full tutorial from
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

**User-test scenarios:** A blind tester who did not prepare the source
tree obtains and launches the artifact, discovers controls from its documentation,
creates or loads a game, plays, saves, exits, and restarts.  Beta testers work
through `ACCESS_USERTESTS.md`; it must be empty or every remaining scenario
must be accepted as a known limitation in the release notes.  Record the
commit and artifact version; this is the native release gate.

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

**User-test scenarios:** A blind screen-reader user completes the Stage
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

**User-test scenarios:** A blind NVDA user unfamiliar with the guide
follows it from a clean configuration without coaching, completes the quick
start using keyboard input only, deliberately interrupts speech and uses
repeat-last, recovers from at least one mistaken or cancelled action, saves,
exits, and resumes.  Record confusing, missing, excessive, or inaccurate
instructions and revise the guide before accepting the stage.

## Known blockers and required follow-up

- The NVDA Controller Client DLL and license remain local ignored artifacts until
  Stage 23 establishes packaging.
- The current host may require single-process MSBuild invocations.
- AccessKit integration is a post-roadmap item that depends on the stable native
  semantic model from Stage 22.
- Upstream synchronization should occur between stages, re-verifying
  `test_access.bat` after each merge.
- Translation catalog updates for accessibility strings are deferred to Stage 23
  packaging.
