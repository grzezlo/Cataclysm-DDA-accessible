# Completed accessibility work

Completed stages are moved here from `ACCESS_TODO.md`. Keep verification details
because this file also defines the accessibility regression history.

## 2026-09-18 — Fork foundation and Windows baseline

- Created the local `access` branch from upstream `0.I-branch` at commit
  `ae8b56eb566775d2a74f6dd2a92917310ff2c232`.
- Added repository-wide contribution and accessibility guidance in `AGENTS.md`.
- Installed the manifest dependencies for the official Visual Studio project.
- Built the Windows x64 Release tiles executable with Visual Studio 2022.
- Fixed the executable project so its startup code is compiled with `SDL_SOUND`,
  matching the game library and the intended full-feature build.
- Verified `cataclysm-tiles.exe --version` exits successfully and reports
  `+tiles, +sound` for `cdda-0.I-2026-09-10-0827-dirty`.
- Started the executable from the repository root. The process remained alive and
  responsive and created its initial configuration files.

Limitations:

- The current automation session could verify the native process but could not
  inspect its SDL window, so the visible main menu still needs a direct user check.
- This is the unchanged upstream interface apart from the sound build correction;
  no NVDA output or playable screen-reader flow exists yet.
- A host environment issue requires single-process MSBuild invocations for now.

## 2026-09-18 — Localized audio-enabled runtime

- Installed the compiled Polish catalog locally under `lang/mo/` and selected `pl`.
- Verified in `config/debug.log` that the Windows build selected `pl_PL.UTF-8`
  and scanned the core translations successfully.
- Installed the full CC-Sounds 2025-11-15 release under the Git-ignored `sound/`
  directory and selected `CC-Sounds` in the local configuration.
- Verified the official SHA-256 checksum
  `adf730636ae6b148bf0da8646e92a3ce44cc9f092a498a50f7299d8e5bfef2ee`.
- Verified that the installed pack contains 724 files, including 459 Ogg Vorbis
  audio files, and occupies approximately 134.2 MiB after extraction.
- User-verified that the Windows build displays the main menu in Polish and plays
  CC-Sounds background music.

Limitations:

- The soundpack and compiled translation catalog are local ignored artifacts and
  are not part of the repository. A future distributable build must fetch or
  package them explicitly.

## 2026-09-18 — NVDA announcement proof of concept

- Added a renderer-independent accessibility service with announcement history,
  a no-op fallback, and an injectable backend seam for tests.
- Added an isolated Windows backend that dynamically loads the official signed
  NVDA 2026.1 Controller Client from the application directory. It does not add
  a link-time NVDA dependency and fails silently when NVDA or the DLL is absent.
- Restricted NVDA output to the normal Windows desktop to avoid announcing game
  information on locked or secure screens.
- Added a localized main-menu announcement using the game's existing `Main menu`
  translation.
- Added the configurable `Repeat last screen reader message` action, bound to F6
  by default. It repeats speech without cancelling or otherwise controlling NVDA.
- Added a capture-backend test covering empty messages, announcement history,
  and repetition. The focused test passed with 11 assertions.
- Built the Windows x64 Release tiles executable successfully with Visual Studio
  2022 after the implementation.
- User-verified that NVDA speaks the localized `Menu główne` announcement on
  Windows version 25H2, build 26200.9457, with NVDA 2026.1.
- User-verified that F6 repeats the last announcement without cancelling speech.

Limitations:

- The NVDA DLL is currently a local Git-ignored runtime file. Release packaging
  must include the matching architecture and its LGPL 2.1 license.

## 2026-09-18 — Semantic main-menu focus

- Added a semantic focus operation to the renderer-independent accessibility
  service.
- The initial announcement identifies the main menu, selected tab, and selected
  submenu item.
- Left and right navigation announces the new tab and its selected item. Up and
  down navigation announces only the changed submenu item to avoid repetition.
- Reused the menu's translated labels after removing visual shortcut markup.
- Included world names and the create-world action in focus announcements.
- The focused accessibility test passed with 11 assertions, and the Windows x64
  Release tiles build completed successfully.
- User-verified the announcement content, navigation behavior, and F6 repetition
  with NVDA 2026.1 without requesting changes.

## 2026-09-18 — Accessible quick-start path

- Identified `Play Now! (Default Scenario)` as the shortest new-game path because
  it bypasses the interactive character creator and template dialogs.
- Added semantic status announcements for core-data loading, save loading, and
  world generation using the game's existing translated strings.
- Added a ready announcement containing an existing translated welcome, the
  character name, and the translated name of the current map tile.
- Applied the same ready announcement to loaded games with `Welcome back`.
- Extended the capture-backend test to cover status announcements; all 13 focused
  assertions passed, and the Windows x64 Release tiles build completed.
- User-verified the complete keyboard path from the main menu through world
  generation to the ready announcement with NVDA 2026.1.

## 2026-09-18 — Live game messages

- Added a semantic game-message operation that is disabled in menus and during
  world initialization, then enabled when a new or loaded game is ready.
- Hooked announcements into message insertion rather than sidebar rendering, so
  display redraws cannot repeat speech.
- Suppressed developer debug messages, messages hidden by the game's cooldown,
  and repetitions already coalesced by the game.
- Removed visual color markup before sending messages to a screen reader.
- Extended the capture-backend test to cover the gameplay gate and repeat-last
  behavior; all 17 focused assertions passed, and the Windows x64 Release tiles
  build completed.
- User-verified a movement-related game message with NVDA 2026.1. Ordinary steps
  correctly remained silent because they do not create game-log messages.

## 2026-09-19 — Movement and current-tile announcements

- Added a semantic movement operation gated to active gameplay.
- Successful movement announces the translated direction and the translated
  terrain or furniture name at the destination.
- Blocked movement remains silent, avoiding false position feedback.
- Extended the capture-backend test to cover movement and repeat-last behavior;
  all 19 focused assertions passed, and the Windows x64 Release tiles build
  completed successfully.
- User-verified normal floor and furniture movement, as well as silence at a
  blocked map edge, with NVDA 2026.1.

## 2026-09-19 — Standard list navigation and menu return

- Added semantic focus announcements to the shared `uilist` control. Opening a
  list announces its title and selected entry; navigation announces only the
  newly selected entry and includes meaningful second-column text.
- Added one-time announcements for standard popup text, with visual color tags
  removed and repeated redraws deduplicated.
- Restored the main-menu context announcement after returning from a child
  screen such as character loading, settings, help, or world management.
- Built the Windows x64 Release tiles executable successfully after the changes.
- User-verified the saved-character list, navigation between its entries, and
  the announcement on returning to the main menu with NVDA 2026.1.

Limitations:

- Standard popup announcements are implemented but have not yet received a
  dedicated NVDA acceptance test.

## 2026-09-19 — Nearby threat changes

- Added semantic threat-set tracking that activates only during gameplay.
- New or changed hostile groups announce up to the three nearest creatures with
  translated names, distance, and direction; additional creatures are counted.
- Gameplay-scoped monster/NPC identifiers rather than raw object addresses or
  positions are tracked, so allocator address reuse cannot suppress a new
  threat and movement by the same visible enemies does not repeat the entire
  announcement.
- The first empty scan stays silent. Losing the final visible threat announces
  the existing translated `Nothing` message.
- Extended the capture-backend test to cover initial silence, deduplication,
  identifier changes, threat appearance, disappearance, and repeat-last
  behavior; the focused test now contains 32 assertions.
- User-verified spawning and approaching a zombie without repeated threat
  announcements, then removing it and hearing the expected clear message, with
  NVDA 2026.1.
- Combat messages encountered during the test, including an injured body part
  and an attack causing no damage, were already announced by the live-message
  integration.

## 2026-09-19 — Startup and modal notices

- Added a concise semantic announcement for the dedicated developer-error
  screen, including its available keyboard actions.
- Added `D` to speak the full technical report on demand and `F6` to repeat the
  most recent summary or detail.  Space, `I`, and `C` retain their existing
  continue, ignore, and copy behavior.
- Added shared announcements for non-interactive static popup instructions and
  statuses.  Progress percentages, item counts, spinner frames, unchanged
  redraws, and repeated animation waits are suppressed while meaningful phase
  changes remain available.
- Reused the shared static-popup path for world loading and generation instead
  of maintaining duplicate feature hooks.
- Added fixed-input tests for whitespace cleanup and progress normalization.
  The focused accessibility suite passed with 52 assertions in 4 test cases,
  and the Windows x64 Release tiles build completed successfully.
- User-verified that changing data `keybindings.json` produces an NVDA-readable,
  usable stale-data warning before the main menu.  The test used Windows 25H2,
  build 26200.9457, NVDA 2026.1, and a working tree based on commit
  `1bc81a560cd1` (`cdda-0.I-2026-09-10-0827-12-g1bc81a560c-dirty`).

Limitations:

- Immediate developer errors outside startup, representative static
  instructions and progress popups, and repeat behavior after interrupted
  detail speech were not separately exercised with NVDA.  They remain explicit
  regression scenarios for Stage 22 rather than blocking the current roadmap.

## 2026-09-20 — On-demand situation review

- Added a rebindable gameplay action `Alt+V` to open an on-demand situation
  review list, and `Alt+R` to repeat the last announcement in gameplay and menu
  contexts.
- Represented visible obstacles, exits, items, creatures, hazards, furniture,
  and current position as semantic entries sorted by distance with stable
  tie-breaking.
- Connected tiles sharing the same obstacle or area-hazard semantics are
  grouped into a single entry reporting tile count and nearest location.
- Omitted ordinary ground tiles and respected character line of sight and
  maximum visibility range without revealing occluded or remembered state.
- Added an `Accessibility` category to the action menu (`Enter`), allowing
  navigation or direct `R` / `V` selection, with repeat preserving the prior
  gameplay context announcement.
- Added automated tests in `tests/accessibility_test.cpp` covering stable
  distance ordering and connected matching region grouping; all 56 assertions
  in 5 test cases passed.
- Built the Windows x64 Release tiles executable successfully.
- User-verified indoor and outdoor positions, furniture, item piles, hazards,
  and visible creatures with NVDA 2026.1 on Windows 25H2, build 26200.9457,
  testing commit `bfd9be513b` (`cdda-0.I-2026-09-10-0827-14-gbfd9be513b`).
  Confirmed efficient navigation, predictable return with `Enter` or `Escape`,
  silence on passive redraws, and reliable repeat with `Alt+R`.

## 2026-09-20 — Stage 02: Look-around exploration

- Added a layered semantic cursor to look-around.  Each actual cursor or
  z-level change announces visible terrain, furniture, fields and hazards,
  vehicle information, creatures, and items in a stable order while redraws
  and cursor blinking remain silent.
- Placed relative z-level information near the end of each announcement and
  direction and distance from the character's stable position last.  Peeking
  retains that origin even while the game temporarily moves the character for
  visibility calculations.
- Shared tile categories and descriptions with the `Alt+V` situation review,
  while keeping connected-region compression exclusive to that list.
- Added semantic extended descriptions with navigable creature, furniture,
  terrain, and vehicle tabs, including visible signs and inscriptions.  Closing
  detail restores the current cursor announcement, and leaving look-around
  restores gameplay focus.
- Added peek direction cancellation, observation origin, time cost, available
  action, movement-mode restoration, and return announcements without changing
  the existing position and crouching behavior.
- Added renderer-independent semantic-sequence formatting coverage.  The
  focused accessibility suite passed with 57 assertions in 6 test cases, the
  Windows x64 test executable built successfully, AStyle reported no formatting
  changes, and the Windows x64 Release tiles build completed successfully.
- A blind user accepted the interaction with NVDA 2026.1 on Windows 25H2,
  build 26200.9457, using implementation commit `afc67940ab`.  Verification
  covered spatial cursor output, rapid navigation and repeat-last behavior,
  multiple z-levels, representative terrain and entities, extended detail,
  obscured areas, standing and crouched peeking, cancellation, available peek
  actions, and return to the unchanged character position.

Limitations:

- Newly introduced source strings can fall back to English until translation
  catalogs receive updated translations.

## 2026-09-20 — Stage 02A: On-demand progress repeat during blocking operations

- Added on-demand progress repeat using `Alt+R` during world loading, map saving,
  save compression, and other blocking operations where standard input contexts
  are inactive.
- Implemented `accessibility::set_last_announcement` to update the active
  progress snapshot quietly in `last_message` without triggering premature or
  flooding speech output.
- Connected `loading_ui::show` to update the active context and step status,
  preserving informational parity with the text visible to sighted players at
  the bottom of the loading screen.
- Connected `query_popup::set_message` progress tracking to update `last_message`
  with full semantic text (including progress counters and percentages) while
  automatic speech remains deduplicated.
- Intercepted `Alt+R` in `input_manager::pump_events()` to trigger
  `accessibility::repeat_last_announcement()` immediately when requested during
  blocking loops before clearing input events.
- Added automated Catch2 tests covering quiet status updates and on-demand
  repeat; the focused accessibility test suite passed with 72 assertions in 7
  test cases, AStyle formatting checks passed cleanly, and both the test
  executable and Windows x64 Release tiles build succeeded.
- A blind user confirmed the interaction with NVDA 2026.1 on Windows 25H2,
  build 26200.9457.  Verification confirmed complete silence during loading,
  immediate spoken feedback reporting the current step upon pressing `Alt+R`,
  and normal arrival announcements upon entering gameplay.

Limitations:

- Very short sub-second operations between event pumping calls might not capture
  rapid intermediate micro-steps before execution completes.

## 2026-09-21 — Stage 03: Nearby item and creature lists

- Preserved the standard visible `V` interface while adding renderer-independent
  semantic rows and concise focus announcements to its bespoke item and creature
  list loops.
- Item focus reports display name, total count, position within a multi-location
  stack, direction, distance, category, priority, newness, and result position.
- Creature focus reports name, direction, distance, broad attitude, current
  behavior, visible injury state, whether it sees the player, safe-mode blacklist
  state, and result position without bypassing visibility or the Inattentive
  trait's information restriction.
- Opening, Tab mode changes, filtering, filter reset, sorting, paging, stack
  position changes, and empty results announce the resulting context without
  speaking on routine redraws.  `Alt+R` repeats the current row.
- Item examination and comparison restore the selected row.  Creature look
  handoff starts at the selected creature, returns to the same row, and suppresses
  the otherwise incorrect intermediate gameplay-focus announcement.  Closing
  `V` restores gameplay focus, including when `V` was opened from look-around.
- Added capture-backend contract tests for both modes, focus movement, stacks,
  empty results, filtering, sorting, paging, mode and detail return, plus a map
  fixture covering grouped visible items and blindness filtering.  The focused
  suite passed all 107 assertions in 11 test cases; the normalized-name check
  passed all 1226 assertions.
- Built the Windows x64 test executable and Release tiles executable
  successfully.  AStyle and `git diff --check` completed cleanly.
- A blind user accepted the interaction with NVDA 2026.1 on Windows 25H2,
  build 26200.9457, using
  `cdda-0.I-2026-09-10-0827-21-g26e1aa65a7-dirty`.  Verification confirmed the
  nearby item and creature modes, concise focus output, filtering and sorting,
  stack and page navigation, look handoff and focus restoration, repeat-last,
  and recovery after speech interruption.

Limitations:

- Newly introduced source strings can fall back to English until translation
  catalogs are updated during Stage 23.
- An additional full-suite run encountered unrelated repository/environment
  failures, including a missing Russian `TEST_DATA.mo` fixture, and was stopped
  after the required Stage 03 checks had passed.

## 2026-09-22 — Stage 04: Player condition and gameplay status

- Added the `Alt+S` status review with seven concise categories and navigable
  detail lists for health and perceived pain, stamina and weariness, needs,
  temperature and effects, wielded weapon, movement and safety, and time and
  environment.
- Kept semantic extraction renderer-independent by formatting concise status
  snapshots, safe-mode state, threat transitions, and condition transitions in
  the accessibility service.
- Added automatic threshold announcements for bleeding, broken limbs, critical
  stamina, pain, hunger, thirst, sleepiness, and weariness.  A simultaneous
  visible-threat change and condition change are combined into one announcement
  so NVDA and repeat-last preserve the complete warning.
- Reset transient threat, movement, and condition state when starting or loading
  a character, preventing announcements inherited from a previous game.
- Reported perceived rather than underlying pain, removed duplicated weapon
  mode/ammunition, and exposed auto-safe countdown and threat-stop state as text
  rather than color alone.
- Added capture and fixed-snapshot coverage for bindings, healthy and adverse
  status, armed and unarmed state, safe/auto-safe/threat-stop text, transition
  deduplication, session reset, combined urgent delivery, and repeat-last.
- `test_access.bat` passed with 165 assertions in 14 test cases.  The Windows x64
  Release tiles build completed successfully with `build_win_x64.bat`; AStyle
  and `git diff --check` were clean.
- A blind user confirmed the corrected interaction works with NVDA 2026.1 on
  Windows 25H2 build 26200.9457, including the updated speech behavior.  Tested
  source state: base commit `faf61e82e911fe9467c514646f92c2b0e748b3c9`
  plus stable source patch ID `4791db52fc98911f3265df5cc4a7a22dd9672f72`.

Limitations:

- Newly introduced source strings fall back to English if the translation
  catalog does not contain translations.

## 2026-09-22 — Stage 03A: Main-menu information

- Announced the title, version, tip of the day, and MOTD/reporting hint on menu
  entry.  Added a visible Tip of the day tab for keyboard retrieval.
- Announced New Game hints and world names with save counts, including semantic
  empty states.  MOTD and Credits speak source-text lines through Up/Down and
  retain independent reading positions when the player changes tabs.
- Kept long source lines intact for speech as selected by the user.  URLs in
  MOTD and Credits remain text; the visible interface does not activate them.
- Capture tests covered opening and focus changes, world counts, empty lists,
  document entry and navigation, independent cursor positions, repeat-last,
  and menu return.  `test_access.bat` passed with 198 assertions in 16 test
  cases; `build_win_x64.bat` succeeded; AStyle checks were clean.
- Tested Windows version: 25H2, build 26200.9457.  NVDA version: 2026.1.
  Tested fork version: `cdda-0.I-2026-09-10-0827-28-g87636cdcad` (commit
  `87636cdcad4ea2cb1a8ea35f592cbf686bd83e09`).  The user confirmed MOTD,
  Credits, a world with one save, and return to each document's prior line, and
  accepted closure of the stage.

Limitations:

- Worlds with zero or multiple saves were covered by capture tests but not by
  NVDA testing because the tester had only a one-save world.  Recheck them in
  Stage 22 as recorded in `ACCESS_TODO.md`.
- Individual outcomes for every other Stage 03A human scenario were not
  separately reported in the acceptance message.  The user accepted closure
  with this verification record.
- New source strings may remain in English until Stage 23 translation work.

## 2026-09-23 — Stage 03C: Help browser

- Implemented linear 1D topic list navigation across two visual columns using
  Up/Down and Page Up/Page Down, announcing topic hotkey and title.
- Preserved direct hotkey jumping and opening (`a`–`w`) and Enter activation.
- Reused the renderer-independent document reader from Stage 03A for help
  topics, reading text paragraph-by-paragraph with Up/Down and Page Up/Page Down
  without paragraph index prefixes.
- Replaced the visual ASCII compass rose (`<HELP_DRAW_DIRECTIONS>`) with a
  semantic spoken description listing all compass directions and pause along with
  their bound keys, including non-printable arrow keys (`UP`, `DOWN`, `LEFT`, `RIGHT`),
  vi keys, and numpad keys.
- Expanded keybinding tokens (`<press_action>`) to clean spoken key descriptions
  without color markup.
- Supported `Alt+R` (`ACCESSIBILITY_REPEAT_LAST`) for repeating topic focus and
  paragraph text without interrupting or fighting screen-reader controls.
- Preserved topic cursor position when returning from document reading to the
  topic list, with clean return to the caller (main menu or gameplay) on Escape.
- Added capture and formatter coverage for menu open, topic navigation across
  columns, direct hotkeys, topic entry, document scrolling, direction bindings,
  repeat-last, and return paths. `test_access.bat` passed with 234 assertions in
  18 test cases; `build_win_x64.bat` succeeded; AStyle and `git diff --check`
  were clean.
- Tested Windows version: 25H2, build 26200.9457. NVDA version: 2026.1. Tested fork
  version: `cdda-0.I-2026-09-10-0827-33-gd750c7070c` (commit
  `d750c7070ce5182c93654ccd7c436b3b400eb918`). A blind user verified topic list
  navigation, direct hotkeys, document paragraph reading, movement keys including
  arrow keys, repeat-last, and return navigation with NVDA, and confirmed the stage.

Limitations:

- Dedicated Accessibility help topic is deferred to final stages (tutorial and
  release gate) as directed by the user.
- New source strings fall back to English if the translation catalog lacks
  translations.

## 2026-09-24 — Stage 04A: Message log review

- Added a semantic message cursor to the existing log dialog.  Opening focuses
  the newest message; Up/Down move by complete messages and Page Up/Page Down
  move by a viewport of messages.  The visible selection follows focus, and
  navigation respects the configured log flow direction.
- Focus speaks the full message and elapsed time.  Enter adds the message type
  and position in the current results.  Wrapped display lines remain one
  message.  Redraws do not announce.
- Kept the shared text and localized `<type>:<text>` filter.  Its field speaks
  the current query and available type names; applying or clearing it announces
  the result count and focused message.  Empty results and invalid type prefixes
  are explicit.  Cancel restores the prior filter and focus.  Repeat Last works
  in the log and filter field, and closing restores gameplay context.
- Capture tests covered opening, cursor and page movement, detail, text and type
  filters, empty and invalid results, reset, repeat-last, and return context.
  `test_access.bat` passed with 283 assertions in 20 test cases.  The Windows
  x64 Release tiles build succeeded with `build_win_x64.bat`; AStyle and
  `git diff --check` were clean.
- A blind user confirmed the requested NVDA checks: keyboard navigation,
  focus and announcement behavior, detailed output, filtering and reset,
  repeat-last after speech interruption, and return to gameplay.  Tested
  Windows version: 25H2, build 26200.9457.  NVDA version: 2026.1.  Tested
  fork version: `cdda-0.I-2026-09-19-2324-37-g00560e743b` (commit
  `00560e743bf08c223e24f591418512a0fd4c106c`).

Limitations:

- The options screen is not yet independently usable with NVDA; Stage 21A owns
  that interface.  The default log flow remains newest at the bottom.
- New source strings may fall back to English until Stage 23 translation work.
