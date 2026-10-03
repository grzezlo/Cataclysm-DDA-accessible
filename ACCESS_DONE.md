# Completed accessibility work

Completed stages are moved here from `ACCESS_TODO.md`. Keep verification details
because this file also defines the accessibility regression history.

The current default binding for Repeat Last is `Alt+R`. References to `F6`
below describe earlier builds and their historical verification.

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

## 2026-09-26 — Stage 05: Reusable input controls and command discovery

- Made common input controls accessible before composite gameplay screens depend on them.
- `uilist`: arrow-key navigation with concise focus speech; `Alt+D` speaks descriptions
  up to 160 characters in place, and opens longer descriptions in a navigable document view
  sentence-by-sentence.  Disabled entries are focusable and identified with denial reasons.
- `query_yn` & `query_popup`: Yes/No prompts announce question, selected answer (default No),
  Left/Right navigation, direct Y/N and Enter/Escape bindings.  Multi-option popups announce
  question, choices, and focused option.  Repeat Last restores context.
- `string_input_popup` (numeric & text input): announces title, description, and value on entry.
  Speaks typed characters, Backspace/Delete removals, and cursor character navigation after
  Left/Right without redundant "Deleted" prefix.  `End of field` announced at end.  Repeat Last
  reads full value.
- `input_context` (keybinding screen): accessible action list with Up/Down navigation, action name,
  binding, and scope.  Uppercase shortcut letters announced with Shift (e.g. `Shift+y`, `Ctrl+Shift+y`)
  via `format_spoken_key`.  Tab switches between action list and filter with result count speech.
  `Alt+D` details announce all keys and customization status.  Enter opens operation menu;
  cancelable key capture and conflict resolution.
- `iteminfo_window` (item examination dialog): integrated with the shared document reader
  (`accessibility_ui::show_document`).  Item name and type/first stat announced on open without
  noisy positional counters ("1 of N").  Long prose descriptions split into complete sentences
  for Up/Down navigation while stat lines remain intact.  Home and End jump to beginning and end;
  Page Up/Down jump 3 paragraphs.  Color tags stripped from item name, type, and descriptions.
- Automated tests: `test_access.bat` passed with 411 assertions across 30 test cases.  C++
  formatting verified clean with AStyle.  Windows x64 tiles build succeeded with `build_win_x64.bat`.
- Human verification: A blind NVDA user confirmed keyboard navigation, speech output, casing
  announcement for uppercase shortcut keys, text and numeric input, and item examination reading with NVDA.

Limitations:

- Polish character entry in `string_input_popup` under Windows is deferred per user direction (not an accessibility gate).
- Intermediate character creation screens remain silent until Stages 20 and 20A.


## 2026-09-28 — Stage 06: Inventory selector navigation and item information

- Inventory selectors announce their purpose, carrying stats, and focused item
  with count, category when it changes, location, main values, favorite and
  contents state, and denial reason.  Column, category-mode, hierarchy, and
  filter changes are announced.  `Alt+D` reads the full row.
- Comparison selection names the first selected item when it gets focus,
  announces deselection, and names the second item first when chosen.  The
  comparison view offers paired statistics with better/worse/equal verdicts
  and full item details for both items.
- Commits: `70bfe66100`, `87ce1b6032`, `bd68c3f99a`; spoken bindings in the
  comparison hints: `640ac6d1b8`.
- Confirmed with NVDA: entering comparison, marking two items, revisiting the
  first selection, and the selection speech.  The remaining scenarios are in
  `ACCESS_USERTESTS.md`.

## 2026-09-28 — Stage 05A: Review 01 corrections

- Added the Screen reader speech option (Automatic by default, On, Off).
  Automatic detects NVDA at startup and stays active once detected.  It is
  separate from upstream `SCREEN_READER_MODE`, which only adjusts curses text.
- Coalesced game messages are spoken again after a new key press; activities
  that repeat a message without input, such as smashing, speak it once.
- Threat alerts identify creatures by object identity, so removing another
  creature does not repeat an unchanged alert.
- Invlets keep upstream behavior except for accessibility shortcuts; repeat-last
  honors its binding during blocking operations; spoken hints name current
  bindings; translation fixes for plurals, double translation, and the
  arrival greeting.
- Split pure formatters into `accessibility_format.*` and moved gameplay code
  from `game.cpp` into `accessibility_gameplay.*`; removed `std::regex` from
  routine speech; cached NVDA availability; shared injury tiers with monster
  descriptions; removed the stale partial Polish catalog.
- The tutorial now enables gameplay speech; it bypasses `game::start_game()`.
- Commits: `640ac6d1b8`, `6d4ab6eb07`, `b5f04dd579`.
- Confirmed with NVDA: Automatic mode speaks without setup, game messages are
  no longer lost, and walking reads the destination tile in a normal save and
  in the tutorial.  The remaining scenarios are in `ACCESS_USERTESTS.md`.

Limitations:

- Coalesced-message speech is not covered by automated tests because the test
  binary uses fake messages.
- The full test suite had 11 failures (translation test data missing,
  order-dependent monster spawn tests, `limit_mod_size_bonus`,
  `tname_i18n_order`) that were not compared against a pre-change baseline.

## 2026-09-28 — Stage 05B: Speech priorities and turn batching

- The first focus announcement after a key press replaces unfinished speech
  through `nvdaController_cancelSpeech`; further speech for the same key queues.
- Game messages, movement, and alerts of one turn are spoken as one utterance
  at the end of the turn or before the game waits for input, without
  interrupting speech.  Pending turn speech is spoken before a prompt it causes.
- Routine movement speech does not replace repeat-last.
- Steps with more auto-move route ahead are silent; the final step is spoken,
  and an interrupted route reports the position before the next input.
- Optional `CATA_ACCESSIBILITY_TRACE` logs queued and spoken speech to
  `debug.log`.
- Commits: `147ee0a080`, `68eb721d90`.
- Confirmed with NVDA: fighting several monsters spawned through the debug menu
  is read correctly and messages do not overlap.  The remaining scenarios are
  in `ACCESS_USERTESTS.md`.

Limitations:

- Navigation inside a menu may interrupt an alert; game messages remain in the
  message log and threats in the situation review.

## 2026-09-28 — Stage 06A: Item action flows

- The item action menu (Enter on an inventory item) names its item and speaks
  each action's availability, shown visually only by color: "Not possible
  now" or "Not applicable"; Auto pickup says whether a rule is set.  `Alt+D`
  reads the complete item information in the document reader and returns to
  the focused action.
- The activation-method menu shows method descriptions instead of the
  keybinding footer, so `Alt+D` reads them; the keybinding hint moved to the
  header.
- Selector hints, such as the eating needs, are spoken as phrases without
  box-drawing separators.  A selector reopened after an item action speaks
  only the focused item.
- `uilist` gained optional `accessibility_context`, `accessibility_details`,
  and per-entry `accessibility_state` for menus whose state is visual only.
- Selected design: option A (concise focus with state, item details on
  demand), recorded before implementation.
- Commit: `5566501203`.

Limitations:

- Quantity prompts, confirmations, results, and refusals rely on the Stage 05
  controls and game-message speech; no flow-specific wording was added.
- Multidrop and other multiselect quantity entry remain Stage 07.
- The item-menu state words follow upstream `hint_rating`, which does not give
  the reason an action is not possible.

## 2026-09-28 — Stage 07: Advanced inventory and multiselect transfers

- Selected design: option A (concise focus with transfer status on demand),
  recorded before implementation.
- The advanced inventory opens with its name, the source pane (area, item
  count, capacity), the destination pane, and the focused row.  Rows speak
  name, count, ownership, weight, and volume; the category only when it
  changes under category sorting.  `Alt+D` adds category, source square in
  the surrounding-area view, auto pickup, and whether the row is the
  container open in the other pane.
- Tab, Left, and Right speak the new source pane once without its capacity;
  area, vehicle, and container changes add capacity and the square's
  terrain and hazards.  Filter, sort, and category-navigation changes are
  spoken.  Refusals use the existing popups.
- A transfer handed to an activity speaks "Moved ... to ..." when the screen
  returns, counted from the source items that actually left; nothing is
  spoken when the move cannot be confirmed.  Move all reports stacks.
- Multiselect rows (drop, pick up, insert, haul) speak "Not selected",
  "Selected N of M", or "Selected all N"; marking speaks the row, its new
  state, and the number of selected entries.
- New global action `ACCESSIBILITY_STATUS` (`Alt+S`): in the advanced
  inventory it reads both panes with capacity, filter, and sort; in
  inventory selectors it reads the title, statistics, filter, and in
  multiselect the selection total.
- Commit: `55bd32ddf5`.

Limitations:

- A transfer interrupted before any item moved is silent; the interruption
  message is spoken by the game-message speech.
- Quantity prompts, destination menus, and failure popups rely on the
  Stage 05 controls without transfer-specific wording.
- Page position ("page 1 of 3") is not spoken.

## 2026-09-28 — Stage 08: Directional and environmental interaction

- Selected design: option A (valid targets spoken in the direction prompt,
  keys unchanged), recorded before implementation.
- Direction prompts with a validity filter (open, close, examine, butcher,
  pickup, and other highlighted prompts) speak the prompt, then the valid
  adjacent targets grouped by identical target in compass order from north,
  own tile last: "Close where? Targets: closed wood door, north. (Direction
  button or mouse)", or "No valid targets".  Grab and smash get speech-only
  filters (vehicles and furniture; bashable tiles and revivable corpses)
  without changing highlighting or selection.
- Targets name a visible creature, the displayed vehicle part, furniture, or
  terrain, plus a visible item count; tiles the player cannot see are
  "unseen".
- An auto-selected single target is spoken ("Auto-selected: ..."), and an
  invalid direction names what is there and says it is not a valid target.
  `Alt+R` repeats the prompt.
- Commit: `a0133c6734`.

Limitations:

- Prompts without a validity filter (unload, deploy, spray, dismount, and
  similar item uses) speak only the prompt.
- Examination handlers, door and vending menus, confirmations, butcher
  choices, and results rely on the Stage 05 controls and game-message
  speech; no handler-specific wording was added.
- Vehicle targets name only the part, not the vehicle.

## 2026-09-29 — Stage 08A: Auto-travel and pathfinding

- Selected design: option B (route summary at start, silent steps), recorded
  before implementation.
- Travel started from look-around or the nearby item list (`Shift+t`) speaks
  one summary: target, straight-line distance and direction, step count, and
  up to four merged terrain segments ("Traveling to wooden door, 12 tiles
  northeast, 14 steps: grass 6, dirt road 5, wooden floor 3.").  Tiles the
  player cannot see are "unseen".  Overmap travel names overmap terrain and
  distance in map tiles.
- Steps are silent, including the end of each local route during overmap
  travel.  Local arrival speaks "Arrived: <tile>."; overmap arrival keeps the
  game message and the position report.
- An early stop speaks "Travel stopped: <destination>, <remaining distance and
  direction>." with the position at the next input prompt; the reason comes
  from the game message or cancel query.  A player-confirmed cancel adds no
  travel speech.
- Commit: `5a05a885af`.

Limitations:

- `auto_travel_mode` keeps per-step movement speech: it moves one step per
  key press, so there is no route to summarize.
- Mouse-only travel and vehicle autodrive have no start summary; autodrive
  belongs to Stage 19.
- The overmap cursor and path selection stay inaccessible until Stage 18.
- A player-confirmed cancel does not repeat the remaining distance.

## 2026-09-29 — Stage 09: Pickup, drop, and container transfers

- Selected design: option A (inline capacity), recorded before
  implementation.
- Inventory selector rows whose item the player does not own say "Not
  owned"; `Alt+D` adds "Owner: <faction>" and, in the pickup selector, the
  predicted container ("Goes into: backpack").  Per-row fit warnings are the
  existing denial reasons.
- Marking in pickup, drop, and insert selectors adds the resulting weight
  and volume against the limit, and "Over weight limit" or "Over volume
  limit" when the screen shows the value in red.  The opening statistics and
  `Alt+S` add the same warnings.
- A pickup speaks one result in place of the per-item "You pick up" lines:
  "Picked up: 2 hammers into backpack.  Not picked up: rock, no room."  The
  closing "Some items were not picked up!" is logged but not spoken.  Auto
  pickup reports only what it picked up, as before.
- A deliberate drop of several kinds of items names them ("Dropped on grass:
  2 hammers, rocks (2).") instead of "several items"; vehicle storage does
  the same when everything fits.
- The theft prompt speaks "Not owned: <item>" before its question.
- New service helper `game_message_speech_suppression` keeps messages in the
  log while a caller speaks a summary; `query_popup::accessibility_context`
  adds speech-only context to a prompt.
- Commit: `e76d09bcf2`.

Limitations:

- Stacks counted in charges that the game spreads over several pockets are
  reported with the best pocket's container, not every container.
- Insert keeps the per-item "You put your X into the Y" messages; spill,
  liquid-drop, and unseal prompts rely on the Stage 05 controls unchanged.
  The liquid-drop warning does not name the liquid.
- Suppression of the per-item pickup messages is not covered by automated
  tests because the test binary uses fake messages.

## 2026-09-29 — Stage 09A: List position speech unification

- Applied interaction convention 2 to the lists completed before it, as
  selected by the maintainer: the item count is spoken once on opening, no
  position while focus moves, "Top" or "Bottom" before the row on a wrap,
  and the position ("3 of 12") on request.
- Position keys: `Alt+D` in uilist (appended to details or "No details
  available"), the message log (same as `Enter`), and the keybinding list;
  `Alt+S` in the inventory selector, advanced inventory, nearby item and
  creature lists, the help topic menu, and the main menu.
- uilist speaks its count on opening, the category name and count on a
  category switch, and "Filter: …, N matching items" after filtering.  The
  inventory selector speaks the active column's item count on opening and on
  a column switch.  The keybinding list says "Keybindings, N actions"; main
  menu tabs with a submenu add its item count.
- Nearby items no longer say "Location x of y" or "x of y" on focus; an
  item on several tiles says "N locations", and `Alt+S` gives both
  positions.
- New formatters `format_list_count`, `format_list_wrap`, and
  `format_list_position` are shared by every list.
- Commit: `53d01fccc7`.

Limitations:

- The message log and help topic menu clamp at their ends instead of
  wrapping, so they never say "Top" or "Bottom"; reaching an end is silent,
  as before.
- Main menu tab switches (Left and Right) wrap without "Top" or "Bottom",
  because tabs are not a vertical list.
- The inventory count covers the active column's items, including disabled
  ones that focus may skip.

## 2026-09-29 — Stage 10: Ranged and thrown targeting

- Implemented the selected option A (concise target, detail on request) in
  `target_ui` for firing, throwing, blind throwing, reach, manual turrets,
  vehicle turrets, and spell targeting.
- Entering target mode speaks the title, the ammunition with its count, the
  target count, and the focused target: name, attitude, distance and
  direction (with the level difference), and the hit chance of the aim
  level the fire key uses.  Re-entering after a multi-turn aim speaks only
  the focus.
- Target cycling speaks the target and says "Top" or "Bottom" on a wrap;
  manual cursor movement speaks the look-around tile description.  Pushing
  past the range says "Edge of range".
- Problems the panel shows only by color or drawing are spoken: out of
  range, out of ammo, not a valid target, no turret in range, the first
  visible obstacle on the line of fire, visible creatures in the line of
  fire, and a spell area that covers the player.  Fire and aim keys that do
  nothing say why.
- `Alt+D` opens the document view with the target description and health,
  the tile, the weapon status, and every aim level with its key, hit
  chance, outcome chances, and moves (and the spell description).  `Alt+S`
  speaks firing mode, aim, ammunition, recoil, steadiness, and range, or the
  spell cost, failure chance, and damage.
- Mode and aim selection speak only the changed value and the new hit
  chance; ammunition changes speak the ammunition and range; aiming speaks
  the new recoil and hit chance; the snap, view-shift, turret-line, and
  unload toggles speak their state; Escape says "Targeting canceled."
- Hit chances come from the same predictions as the panel; `print_aim` and
  `draw_throw_aim` now share `fire_aim_predictions` and
  `throw_aim_predictions`.  New formatters: `format_target_focus`,
  `format_target_problems`, `format_target_open`, `format_target_status`,
  `format_target_aim_level`; gameplay helpers `target_focus` and
  `target_details`.
- Commit: `6a5481d569`.

Limitations:

- The targeting loop has no input injection in tests; its speech is covered
  through the formatters and the gameplay helpers, not by driving the UI.
- The line-of-fire obstacle is the first visible impassable tile, so a
  window is reported as blocking although a bullet may break through it.
- A creature seen only through special senses is called "sensed creature";
  the special-vision description is not spoken.
- Moving the view in view-shift mode is silent, since only the display
  changes.

## 2026-09-29 — Stage 11: Crafting and disassembly

- Implemented the selected option A with missing counts in
  `select_crafter_and_crafting_recipe()`; existing keys are unchanged.
- Opening speaks "Crafting", the category (and subcategory unless it is
  All), the recipe count, the hidden count, "new recipes" when the tab has
  unread ones, and the focused recipe.  Category, subcategory, filter,
  filter reset, and related-recipe searches speak the new list the same way.
- A recipe row speaks its name and "craftable" or "N missing" (unmet
  requirement groups, plus missing primary-skill knowledge, required
  proficiencies, or practice skills; "not craftable" when none can be
  named).  The states the list shows by color are words: low skill, uses
  rotten ingredients, no skill gain, uses favorite items, too dark, too
  slow; favorite and new also.  Groups say group, expanded or collapsed.
  Moving focus says "Top" or "Bottom" on a wrap.
- `Alt+D` opens the recipe document: availability and position, batch
  size, crafting speed, skills, proficiencies, failure chances, time,
  batch savings, activity, dark crafting, nearby count, warnings,
  byproducts, one line per tool and component group with a state word for
  each alternative ("have", "missing", "not enough", "have, uses a
  favorite", …), and where the recipe is written down.  `Alt+S` speaks the
  result item information (practice and group descriptions for those).
- Batch mode rows speak "Batch size N", the time, and the availability;
  leaving it says "Batch off".  Favorite, hide/show, read/unread, mark all
  read, unread-first sorting, and crafter changes speak their new state.
  Returning from details, comparison, related recipes, or the crafter menu
  repeats the focused row.  Related recipes drop the tree drawing and speak
  the favorite star as "favorite".
- The disassembly selector's `Alt+D` opens a document with the row, the
  returned components, the time, and the required tools and qualities with
  their states, through a new `inventory_selector_preset` hook.
- New formatters: `format_requirement_group`, `format_recipe_row`,
  `format_recipe_list`, `format_recipe_batch`, and related helpers; gameplay
  helpers `requirement_groups` and `disassembly_details`; test entry points
  `recipe_accessibility_row` and `recipe_accessibility_details`.
- Commit: `68ab2fe942`.

Limitations:

- The crafting loop has no input injection in tests; speech is covered
  through the formatters and the row, detail, and disassembly helpers.
- Craft start, component choice, failure, and result are spoken by the
  existing game message and list speech; nothing crafting-specific was
  added.  Long-action progress belongs to Stage 12A.
- Related-recipe headings and multi-recipe group names are disabled list
  entries, so they are spoken with "Unavailable".
- Changing subcategory in batch or filter mode does nothing and stays
  silent, as the visual screen does.
- Requirement states mirror the visual list, including its batch-size-1
  check for "not enough"; the "N missing" count uses the simple
  requirements, so overlapping requirements can make it zero on a recipe
  that is still not craftable.

## 2026-09-29 — Stage 12: Construction and placement

- The construction catalog announces its name, category, project count, and
  focused project once on entry.  Row movement speaks the project and its
  availability, disabled reason, and variant count; category, filter, and
  unavailable-project changes speak their new list.  `Alt+S` reviews the
  category, position, and whether unavailable projects are hidden.  `Alt+R`
  repeats the last announcement.
- `Alt+D` opens a navigable requirements document for every variant: result,
  description, skills with current levels, precursor terrain, annotation,
  duration, tools and components with explicit availability words, and
  adjacent placement validity.  Closing it restores project focus.
- Directional placement speaks the valid neighboring tiles through the Stage
  08 prompt.  Invalid choices retain their existing failure explanation;
  starting a build announces the selected project.
- Added pure catalog formatters and capture-suite assertions.  `test_access.bat`
  passed 915 assertions in 64 cases; `build_win_x64.bat` passed.
- Implementation commit: `43e45cf49e`.

Limitations:

- The interactive construction loop has no input injection in tests; focus,
  document navigation, placement, and interruption quality await blind NVDA
  verification in `ACCESS_USERTESTS.md`.
- Activity progress, interruption, and completion are Stage 12A.

## 2026-09-29 — Stage 12A: Activities, interruptions, and long actions

- Implemented the selected start/result interaction, conventions 7 and 8,
  and conventions 2, 4, and 5 for the distraction manager.  Starts and resumes
  name the activity; crafting, construction, reading, and waiting include the
  duration already available in their interfaces.  `Alt+R` reads live progress
  during activities, sleep, loading, and saving.
- Handler messages are collected and deduplicated into one final result.
  Failures retain the game's explanation; interruption cancellations retain
  the triggering reason.  Successful crafting cleanup does not announce a
  cancellation, and continuous batches/chapters retain one start and final
  result.  Sleep uses its existing effect messages for falling asleep and
  waking, avoiding duplicate lifecycle speech; loaded sleep resumes progress.
- Shared monotonic-clock progress reporters announce changed text after five
  real seconds of silence, never interrupt speech, prioritize queued warnings,
  and stop with their owning operation.  The Interface option **Progress
  announcement interval** sits next to **Screen reader speech**; 0 disables
  automatic updates while preserving repeat-last.  Prompts suspend automatic
  progress until the operation updates it again.
- The distraction manager speaks its list and focused name/state, changes,
  wrapping, and position; `Alt+D` opens the description document, `Alt+S`
  reviews the list, and returning restores row focus.  Interruption and stamina
  prompts use the existing shared controls and keys.
- Lifecycle coordination is in `accessibility_gameplay`, pure lifecycle
  formatters in `accessibility_format`, and timers/message capture in the
  service.  Narrow hooks cover player activities, craft completion, character
  sleep/backlog, the turn/input loop, construction, and interruption queries.
  Blocking progress uses loading/popups and `pump_events()` in `sdltiles.cpp`.
- Capture tests cover timing, changed-text deduplication, priorities, disabled
  and custom intervals, repeat, source teardown, real wait/read/sleep handlers,
  failures, cancellation/resume, and continuous activity replacement.  An
  opt-in observer in the test message stub exposes non-debug handler messages.
  Activity formatters strip color markup from item names and progress/reasons.
  `test_access.bat` passed 1035 assertions in 73 cases; the Windows x64 tiles
  build and AStyle 3.1 checks passed.
- Implementation commit: `259fa2c448`.
- Completing commit: `c5296f9c09` (color-markup regression checks).

Limitations:

- NVDA interaction quality is unverified.  Keyboard input through live activity
  and sleep loops, popup focus/choices, distraction settings, and blocking
  progress await the scenarios in `ACCESS_USERTESTS.md`.
- Automatic progress is polled at game turns and event-pump points.  Work that
  does not return to either point cannot speak or process repeat until it
  returns.  Unchanged progress stays silent, including sleeping without a
  visible remaining-time estimate.

## 2026-09-29 — Stage 13: Character information and layering

- Implemented the selected interaction with conventions 2, 3, and 5; keys
  are unchanged and the gameplay `Alt+S` status review is untouched.
- Opening the sheet speaks "Character sheet", the name, the Stats tab, its
  count, and the focused row.  Tab changes (Left/Right, Tab/Shift+Tab, and
  the direct tab keys) speak the tab name, its count, and the focused row;
  Speed adds speed and move cost, Bionics adds power.  Row movement speaks
  the concise row, "Top" or "Bottom" on a wrap, and the skill category when
  focus enters a new one.  Info-box scrolling stays silent.
- Rows: stats with base ("Strength 10, base 8"); weight, lifestyle, height,
  age, and blood type; body part, encumbrance, layering when nonzero, and the
  warmth band that the sheet shows only by color; speed causes with signed
  speed or move cost; skills with level, progress, effective dodge, "rusty",
  "training"/"not training", or "set by bionic"; traits with their colored
  kind (positive, negative, neutral, mixed, threshold, profession,
  attunement, debug); bionics with installed counts; effects by name;
  proficiencies as known, learning with progress, or not known.
- `Alt+D` opens the info pane in the document view: the row (encumbrance
  adds the total and the temperature number), its position, and the pane
  text without color markup, including the encumbrance effects and limb
  scores.  `Alt+S` speaks name, gender, profession, tab, position, and row.
  Returning from Alt+D, morale, body status, medical, proficiencies,
  customization, trait variant, sprite, profession name, keybinding help,
  or a stat upgrade repeats the focused row.
- Pure formatters in `accessibility_format`; the sheet speech and test entry
  points (`src/player_display.h`) are in `player_display.cpp`, which now
  shares its info-pane text helpers between drawing and speech.
- `test_access.bat` passed 1148 assertions in 75 cases; the Windows x64 tiles
  build and AStyle 3.1 checks passed.
- Implementation commit: `728656c44b`.

Limitations:

- NVDA interaction quality is unverified; the sheet's input loop has no
  input injection in tests, so key handling is covered through the speech
  helpers and formatters.
- The morale window, body status graph, medical menu, proficiencies window,
  and the armor layering (sort armor) screen reached from the sheet are not
  made accessible by this stage; no roadmap stage currently owns them.
- Test entry points omit speed causes from effects, which need the
  character's protected effect map.
- Body part names and skill names are spoken as the sheet shows them,
  including lowercase names from the game data.

## 2026-09-30 — Stage 14: Bionics and mutations

- Implemented the selected interaction with conventions 2, 3, 5, and 8; keys
  are unchanged.
- Opening speaks "Bionics" or "Mutations", the tab or column name, its count,
  and the focused row.  Tab changes speak the new tab or column the same way;
  the mutations screen repeats the current column when the other one is
  empty.  Row movement speaks the concise row, "Top" or "Bottom" on a wrap.
- Bionic rows: name, on or off (toggled and charging bionics; others only
  while on), "power source", energy to activate, deactivate, per trigger,
  and per turn, incapacitated, sprite hidden, fuel saving threshold and
  inactive state, and "Unavailable: not enough power".  Mutation rows: name,
  on or off, cost with its resources, cooldown, "starting trait", sprite
  hidden, and "Unavailable: not enough <resources>" using the game's own
  activation check (now shared as `mutation_resources_available()`).
- The examine toggle, reassign, and hide modes speak their new mode.
  Reassigning speaks the mode, row, and new key.  Sprite, fuel-saving, and
  sort changes speak the changed state and row.  Returning from activation
  (game messages first), popups, the sort menu, or the document repeats the
  row.  Screens closed by activation leave speech to the game messages.
- `Alt+D` opens the document view: row, position, key, then the examine-pane
  text (bionic power usage, description, occupied body parts, installed
  weapon; mutation description and purifier note).  `Alt+S` speaks power and
  fuel (bionics), mode, tab, position, and key.
- Pure formatters in `accessibility_format`; speech helpers and test entry
  points (`src/bionics_ui.h`, `src/mutation_ui.h`) in the owning UI files.
  The bionics title bar shares its power and fuel helpers with speech.
- `test_access.bat` passed 1215 assertions in 78 cases; the Windows x64 tiles
  build and AStyle 3.1 checks passed.
- Implementation commit: `72d046f544`.

Limitations:

- NVDA interaction quality is unverified; the input loops have no input
  injection in tests, so key handling is covered through the speech helpers
  and formatters.
- Row letters are spoken only in `Alt+S`, `Alt+D`, and after reassigning.
- "Not enough power" reflects only the activation cost check; fuel and spell
  failures are reported by game messages after an attempt.
- The weapon install/uninstall menus and popups use the shared list and popup
  speech without bionic-specific additions.
- The maintainer expects play testing may suggest a better design; reported
  problems are a new review rather than a regression fix.

## 2026-09-30 — Stage 14A: Spells and martial arts

- Implemented the selected interaction with conventions 2, 3, and 5; keys
  are unchanged.
- The spell menu opens with "Choose a Spell", "Ignore Distractions" when that
  is on, the category, its spell count, and the focused row.  Category
  changes speak "Favorites, 3 spells" and the focused row.  Rows speak name,
  the spell class ("Classless" when it has none), "castable", the cost as a
  number with its energy ("50 mana", "no cost"), the casting time, "cost
  impeded" or "time impeded" when encumbrance raises them, and "favorite"; a disabled spell ends with "Unavailable: not enough
  mana" (or bionic power, stamina, health), "cannot speak", "missing
  components", or "cannot cast now", following `spell::can_cast()`.
  Toggling a favorite speaks the changed row; the distraction toggle speaks
  its new state; returning from hotkey assignment repeats the row.
- `Alt+D` opens the spell document: row and key, class, description,
  properties, level, failure chance, difficulty, experience, cost, casting
  time, targets, range, area, damage or effect, duration, and component
  groups with state words.  Escape returns and speaks the row and position.
- The style picker opens with "Select a style" and the details key.  Style
  rows speak name, "current" for the style in use (shown only by color),
  and the first description sentence; `Alt+D` keeps the full description.
  The existing `SHOW_DESCRIPTION` key opens general info, buffs,
  techniques, and weapons by category in the document view, naming wielded
  and carried weapons; returning repeats the row.
- `uilist` gained a per-entry spoken focus, a spoken count formatter, and
  `accessibility_repeat_focus()`.  Pure formatters in `accessibility_format`;
  spell speech and test entry points in `magic.cpp`/`magic.h`; the style
  details text is shared between the window and the document in
  `martialarts.cpp`.
- `test_access.bat` passed 1350 assertions in 81 cases; the Windows x64 tiles
  build and AStyle 3.1 checks passed.
- Implementation commit: `d82965dd56`.
- Follow-up commit "Speak spell class in spell menu rows" adds the
  translated class after the name ("Pew, Pew, Magus, castable, 50 mana,
  100 moves"), as the success criterion required.
  `test_access.bat` passed 1352 assertions in 81 cases.
- Follow-up commit "Speak Classless for spells without a class" speaks the
  menu's own translated "Classless" label for spells without a class, as the
  class tabs and details pane show it.  `test_access.bat` passed 1352
  assertions in 81 cases.

Limitations:

- NVDA interaction quality is unverified; the menu loops have no input
  injection in tests.
- The spell row omits the failure chance, as selected; it is in the `Alt+D`
  document.  The spell document duplicates the side panel's text logic
  rather than sharing it with the ImGui drawing.
- The style picker's STR/DEX/PER/INT header is not spoken; the character
  sheet has these values.  Style details ignore the window's collapsed groups.
- Hotkey assignment popups, spell targeting, and cast results use the
  existing popup, targeting, and message speech.
- Spells in the base game come mainly from mods; testing needs a world with
  Magiclysm, whose setup is accessible only after Stage 21.

## 2026-09-30 — Stage 13A: Character sheet subscreens and armor layering

- Implemented the selected interaction (option A) with conventions 1, 2, 3,
  5, and 8; game keys are unchanged.  Every screen registers `Alt+D`,
  `Alt+S`, and `Alt+R`.
- Morale: the window opens directly in the document view: total morale,
  total positive morale and each source with its percentage in the window's
  order, the negative sources, pain and sleepiness levels, and the focus
  trend.  The document and the window share the same line list
  (`player_morale::build_window_lines()`).
- Body status: opening and returning from a nested graph speak the graph
  name ("Body status" or the part), part count, and focused part.  Rows
  speak the name, "missing", health in tenths at the bar's precision
  ("health 4 of 10", "under 1 of 10"), and "has details".  `Alt+D` reads the
  info pane, with effects marked "good" or "bad" and protection as "worst,
  median, best"; info-box scrolling stays silent.
- Medical: HEALTH, EFFECTS, and STATS are panes; opening adds the title bar
  pain, hunger, thirst, and sleepiness words.  Health rows speak part,
  health or "mending N of 5", and conditions (bleeding intensity, BROKEN,
  SPLINTED, BITTEN, SEPTIC); stats speak "Strength 8, base 8".  `Alt+D`
  reads the description and detail text; apply and keybinding help return
  to the focused row.
- Proficiencies: categories are tabs; rows speak "known" or "learning,
  40%"; filtering speaks the query, matching count, and first row; `Alt+D`
  reads the details pane and `Alt+S` adds the category description.
- Sort armor: filter changes speak the body part, count, and row.  Rows
  speak name, side, "hidden", "layering conflict", and "out of order".  The
  move key speaks "Moving …", Up and Down while moving speak the
  neighbours ("above …, below …", "innermost", "outermost"), and the move
  key again speaks "Placed" and the row.  Side change, sprite toggle, sort,
  wear, take off, invlet assignment, and help speak the result or row.
  `Alt+D` reads the item statistics and the encumbrance and warmth block
  with "covered by this item"; `Alt+S` reads the filter's items with
  encumbrance, conflicts, and the total.
- Pure formatters in `accessibility_format`; speech helpers and test entry
  points in the owning files (`bodygraph.h`, the new `medical_ui.h`,
  `proficiency.h`, `outfit` in `character_attire.h`, `morale.h`,
  `player_display.h`).
- `test_access.bat` passed 1486 assertions in 87 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `7e3f4ba1ba`.

Limitations:

- NVDA interaction quality is unverified; the input loops have no input
  injection in tests, so key handling is covered through speech helpers and
  formatters.
- Morale sources speak their percentage but not their bonus, because the
  window shows only the percentage; the selection's example "+10, 25%" was
  reduced to what the window displays.
- Filter speech punctuates as "Torso. 3 items." rather than "Torso, 3
  items".  Medical condition words and column names are spoken in the
  window's capitals.
- Body status speaks a precise temperature without its color band.
- The sort armor right pane (items grouped by body part) is not navigable;
  `Alt+S` covers the focused filter's part.  Integrated armor refusals are
  spoken by the existing popup.

## 2026-09-30 — Stage 15: NPC dialogue

- Implemented the selected interaction (chat history with a response list)
  with conventions 2, 5, 7, and 8; letter hotkeys and the screen action keys
  are unchanged.  The dialogue registers `Alt+D`, `Alt+S`, and `Alt+R`.
- Conversation level: the history holds one element per utterance ("Rubik:
  "Hello"", "You: Ask about the job", NPC actions and terminal lines
  without a speaker) and ends in "Your response, 5 options" ("Your input"
  on terminals, "Your choice" for non-conversation uses).  Each topic speaks
  its new lines once, never the player's own choice, and focuses the
  prompt.  Up and Down move through the history without wrapping; Home,
  End, Page Up, and Page Down jump.  Enter on an utterance rereads it;
  Escape keeps the game's best quit response.
- Response list: Enter on the prompt speaks the prompt and the focused row.
  Rows speak the hotkey and text with the trial ("c, [LIE 50%] …"), what
  the color shows ("May provoke an attack", "Leaves you helpless",
  "Action"), and "Unavailable: <reason>".  Look at, Size up stats, Assess
  personality, Yell, and Check opinion follow the responses with their
  keys.  Up and Down wrap with "Top" and "Bottom"; the visible cursor
  follows the responses.  Enter confirms; Escape returns to the prompt.
  Choosing an unavailable response speaks its row; declining a hostile or
  helpless confirmation speaks the focused row again.
- `Alt+D` opens the utterance or row in the document view by sentence and
  speaks the focus on return; `Alt+S` speaks the dialogue title, level, and
  position.
- The pure `dialogue_speech_model` and formatters are in
  `accessibility_format`; `dialogue_window` feeds it from
  `add_to_history()` and `set_responses()`; `npctalk.cpp` has four narrow
  calls and builds the spoken row in `talk_response::create_option_line()`.
  Tags in the response and reason are now parsed before coloring, which
  leaves the displayed text unchanged.  An unknown hotkey no longer reads
  past the response condition list.
- A closing quote after a sentence end now ends the sentence in
  `format_semantic_sequence()`, so quoted lines are not followed by an extra
  period.
- `test_access.bat` passed 1567 assertions in 90 cases and `[npc_talk]`
  passed 551 assertions in 35 cases; the Windows x64 tiles build and AStyle
  3.1 checks passed.
- Implementation commit: `f55952560d`.

Limitations:

- NVDA interaction quality is unverified; the dialogue loop has no input
  injection in tests, so key handling is covered through the speech model
  and the spoken rows of real talk topics.
- History scrolling with `<` and `>` and mouse input still move only the
  visible views; the accessible history focus does not scroll the window.
- Escape on the conversation level stays silent when the game finds no
  safe quit response, as the visible dialogue does.
- Effects of a chosen response (items, messages, the farewell line) are
  spoken through the existing message speech, not by the dialogue.
- Nested menus opened by responses (trading, missions, training choices)
  use their own screens; trading and missions are Stages 16 and 17.

## 2026-09-30 — Stage 16: Trading

- Implemented the selected interaction (conventions 2, 3, 4, and 8 with the
  Stage 07 and 09 selector speech); trade keys are unchanged.
- Opening speaks the purpose, trader, and balance once ("Trade: Rubik.
  Balance: credit $0.00, acceptable."), then "Trader's goods", the item
  count, and the focused row.  Tab speaks only the new pane ("Your goods,
  N items") and its focused row, never the other side.
- Rows speak name, unit price ("$2.00 each"), marked count ("Marked", "3
  marked"), "Not owned" when it applies, and "Unavailable: <reason>";
  `Alt+D` adds location, favorite, contents state, and owner.
- Marking, count changes, and auto balance (`F1`) speak the row's new
  state ("Unmarked" when cleared), then "Balance: you owe $5.00, trader
  refuses" or "Balance: credit $3.00, acceptable" ("Free exchange,
  acceptable" with followers), and "Over weight limit" or "Over volume
  limit" from the pane statistics.  Auto balance with nothing to change
  speaks the balance.  Bank balancing (`F2`) speaks the balance and the
  bank balance.
- `Alt+S` speaks the purpose, trader, balance and acceptance, the value of
  the marked goods on each side ("Buying", "Selling"), the bank balance
  when banking is enabled, and the pane status with statistics, selection
  total, and position.
- Refusals and the confirmation use the existing popups; afterwards the
  focused row is spoken again.  Closing speaks "Trade complete" or "Trade
  cancelled" before the dialogue continues.
- The inventory selector gained narrow virtual hooks (`accessibility_row`,
  `accessibility_opening_context`, `accessibility_column_label`,
  `accessibility_selection_event`) that `trade_selector` overrides; the
  trade loop now announces focus, handles `Alt+R`, and reports wraps.
- `test_access.bat` passed 1627 assertions in 92 cases; `[trade]`,
  `[inventory]`, and `[npc_talk]` passed 776 assertions in 45 cases; the
  Windows x64 tiles build and AStyle 3.1 checks passed.
- Implementation commit: `c6504e1278`.

Limitations:

- NVDA interaction quality is unverified; the trade loop has no input
  injection in tests, so pane switching, marking, and status are covered
  through the panes and the trade state directly.
- The standard "Care to trade?" responses lead to `TALK_DONE`, so the game
  ends the conversation after the trade; the dialogue continues only for
  trades started from other topics.
- The trader's pocket check happens only on confirmation, as on screen; its
  refusal is spoken by the popup.
- Pane statistics describe each party's own load after giving away the
  marked goods, as the visible statistics do; incoming goods are not
  counted.
- The "Trade complete" result is not asserted in tests because the trade
  loop needs real input.

## 2026-09-30 — Stage 17: Missions

- Implemented the selected interaction (conventions 2, 3, and 5); mission
  screen keys are unchanged.
- Opening speaks "Your missions", the tab with its count, and the focused
  row ("Your missions. Active, 3 missions. Reach Refugee Center, current
  objective, deadline in about 2 days.").  Left, Right, Tab, and Shift+Tab
  speak only the new tab ("Failed, no missions.").
- Rows speak the title, "current objective", and "deadline in <time>" or
  "deadline passed" for active missions, or "completed <time> ago" and
  "failed <time> ago" on the other tabs.  Times use the same watch-based
  precision as the screen.
- Enter on an active mission speaks "Current objective: <title>"; on the
  other tabs or an empty tab it speaks "Unavailable: <reason>".
- `Alt+D` opens the document view: row with category and position, giver
  and faction, giver location and target as distance in map tiles,
  direction, and approximate meters ("Target: 5 map tiles north, 120 m"),
  the description split into sentences starting with "Objective:", and the
  deadline or end time lines; "No known location." when there is no
  target.  Closing it speaks the focused row again.  `Alt+S` speaks the tab,
  count, position, and current objective; `Alt+R` repeats.
- The description parser moved into a shared helper so the screen and the
  document view read the same expanded snippets.
- Mission acceptance, completion, and failure need no new hook: the Stage 15
  dialogue speaks the giver's offer and responses, and the existing game
  log messages ("Mission "<title>" is successfully completed.", "... is
  failed.") are spoken as gameplay messages.
- `test_access.bat` passed 1668 assertions in 94 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `c413e5259c`.

Limitations:

- NVDA interaction quality is unverified.  The ImGui mission loop has no
  input injection in tests, so opening, tab, confirm, and focus speech are
  covered through the formatters and the row and detail builders.
- The missions screen has no map action; the selected interaction left the
  map handoff to Stage 18, whose known-places list includes mission
  targets.
- The screen shows no separate reward field; the reward is spoken where the
  mission description states it.
- The list does not wrap, so Up or Down at either end repeats the row
  instead of saying "Top" or "Bottom".
- Distances mirror the screen, which assumes targets on z=0; the direction
  is horizontal only.

## 2026-09-30 — Stage 18: Overmap navigation

- Implemented the selected interaction (option B, known-places list with the
  cursor; conventions 2, 6, and 8).  Existing overmap keys are unchanged;
  `Alt+L` ("List known places", `OVERMAP` context) is new, and `Alt+D`,
  `Alt+S`, and `Alt+R` work on the overmap.
- Opening speaks the prompt or "Overmap" and the cursor tile.  Cursor moves
  speak the known terrain or "unexplored", "note: <text>", danger marks,
  "mission target: <title>", "route destination" or "on route", and the
  offset from the player ("forest, 3 north 2 east", "here").  A one-tile
  step onto the same terrain speaks the terrain and markers without the
  offset; a jump speaks the whole tile.  Level changes speak "Level -1."
  before the tile, and off-level tiles add "1 level below".
- `Alt+L` opens "Known places" as a shared list sorted by distance: seen
  cities and camps, notes on every level, active mission targets with
  "current objective", and the route destination, merged per position.
  Rows speak name, distance in map tiles, direction, level difference, and
  markers ("evac shelter, 12 map tiles south, note: supplies").  The shared
  list filter narrows it.  Enter moves the cursor and speaks the tile;
  Escape speaks the unchanged cursor tile.
- The first route-selection press speaks "Route to <tile>, N map tiles
  <direction>.  M map tiles of travel.  Press <key> again to travel." or
  "No route to <tile>." (with the same-level reason); the second press uses
  the existing confirmation and Stage 08A travel summary.  Travel and
  center-on-destination without a route speak "No route selected."
- Search results speak "Search results, N results" and the tile, then each
  result with "Top"/"Bottom" on wrap; `Alt+S` speaks "Result 2 of 5".  Note
  list rows speak the note, distance, direction, danger, and terrain.
  Creating, editing, deleting, or marking a note speaks the tile again;
  toggling explored speaks "Marked explored/unexplored".
- `Alt+D` speaks the tile, the sidebar description with the nearest known
  city, the vision level, the current objective's distance from the cursor,
  visible weather, explored state, and level.  `Alt+S` speaks the cursor
  tile, level, and route.
- `test_access.bat` passed 1740 assertions in 96 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `5b2c0bf4e5`.

Limitations:

- NVDA interaction quality is unverified.  The overmap loop needs real input,
  so tests cover the formatters, tile and place builders, the speech state,
  and route speech rather than key-driven sessions.
- The known-places list covers cities and camps within five overmaps of the
  player and notes and missions everywhere loaded.  Seen specials without a
  note, NPCs, and hordes shown on the map are not listed.
- Overlay toggles other than explored (notes, hordes, city labels, fast
  travel, and so on) are not spoken; they change only the drawn map.
- Starting autodrive has no travel summary; vehicle travel belongs to
  Stage 19.
- The NPC path animation and mouse edge scrolling move the cursor silently;
  the next key press speaks the new position.
- The overmap sidebar does not show the `Alt+L` hint; it is listed in the
  keybinding help.

## 2026-09-30 — Stage 18A: Zone management

- Implemented the selected interaction (conventions 2, 6, and 8).  Zones
  manager keys are unchanged; `Alt+D`, `Alt+S`, and `Alt+R` work in it.
- Opening speaks "Zones manager", the zone count, and the focused row.  Rows
  speak the name, the type when it differs from the name, "personal" or
  "vehicle", "enabled" or "disabled", and the distance and direction from
  the player with any level difference ("Pantry, Loot: Food, enabled, 5
  tiles north").  Focus moves speak the row with "Top"/"Bottom" on wrap.
- Zone corner selection, position edits, and zone moves speak the cursor
  tile with the Stage 02 look-around description; the existing "Select
  first point." and "Moving zone." popups name the step.  The hook in
  `game::look_around()` covers every zone-selecting caller.
- Creation speaks "Zone 4 by 3." (with levels when it spans several) and the
  new row; type, options, and name use the shared list, prompt, and text
  input speech.  Edits speak the new row, the row with "Options: ..." after
  an options change, the new size after a position edit, or "Zone moved."
  after a move.  Reorder speaks "<name>, 2 of 5"; enable, disable, map
  display, personal zone toggles, the distance filter, the debug faction
  filter, and removal ("Removed <name>." and the new focus) speak the new
  state.  Focus returns to the row after a canceled dialog, the overmap,
  keybinding help, or a canceled close.
- `Alt+D` speaks the row, position, size, map display, and options; `Alt+S`
  speaks the filter, faction when not the player's, and position.
- `zones_manager_speech` in `accessibility_gameplay` compares the state
  before and after each action, so `game::zones_manager()` has only an open,
  a before, and an after call.
- `test_access.bat` passed 1802 assertions in 98 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `88592116ea`.

Limitations:

- NVDA interaction quality is unverified.  The manager and look-around loops
  need real input, so tests drive the speech state with direct zone changes
  and do not cover the corner-selection cursor speech.
- The second corner does not speak the growing size while the cursor moves;
  the size is spoken once the zone is created or its position changed.
- Row distances are horizontal, rounded as the screen shows them; the
  screen's own distance also counts the level difference.
- Temporarily disabled zones are reset when the manager opens, so no
  temporary state is spoken.

## 2026-09-30 — Stage 19: Vehicle controls, driving, and status

- Implemented the selected interaction (option A, changes spoken, status on
  request; conventions 1 and 8).  Keys are unchanged; vehicle menus use the
  shared list speech.
- Driving inputs speak the new cruise speed ("Cruise 20 mph."), with "above
  safe speed" when the running engine would strain and "maximum" when the
  request is past the limit, and the new steering target ("Heading east, 105
  degrees.").  Engine start and stop, handbrake, electronics, and collision
  results keep their existing game messages.
- Before each input while the player controls a vehicle, the nearest newly
  visible obstacle on the vehicle's swept path (impassable terrain or
  furniture, or another vehicle) is spoken once ("wall ahead, 3 tiles"),
  looking behind when reversing, until it leaves the path.  The lookahead
  is five tiles beyond the distance covered in one turn at the current speed.
- Vehicle motion is silent; walking onto a vehicle tile names the vehicle,
  the displayed part, and "controls" when present.
- `Alt+S` while controlling a vehicle speaks name, speed, cruise, engine
  strain (the speed gauge colors), heading with the steering target while
  turning, engine state, each fuel and the battery with percentage and time
  until empty or full, and skidding.  The keybinding name says so.
- Vehicle menus that reopen after a kept-open action speak only the queued
  result and the focused row (`uilist::accessibility_reopen()`).  Engine
  selection rows speak "<engine>, <fuel>, on/off" instead of "[x]".
- `test_access.bat` passed 1848 assertions in 100 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `2075dfa1dd`.

Limitations:

- NVDA interaction quality is unverified.  Tests drive the speech with
  direct vehicle changes; the menu reopen speech and the full `pldrive`
  input path are not covered by automated tests.
- There is no persistent handbrake state in the game, so `Alt+S` does not
  report one; the handbrake message and the zero cruise speed describe it.
- While controlling a vehicle, `Alt+S` no longer opens the status review;
  let go of the controls to reach it.
- Creatures on the path are not obstacle announcements; hostile ones use
  the threat alerts.  Passable but damaging terrain is not announced.
- Remote-controlled vehicles get the same speech; obstacles are judged from
  the player's own view.

## 2026-09-30 — Stage 19B: Tutorial lesson documents

- Implemented option A, selected by the maintainer on 2026-09-30: C++ lessons
  and all 14 map EOC lessons automatically open the shared document reader
  when accessibility is active.  The original popups remain in inactive mode.
- Entry speaks "Tutorial" and the first fragment.  Up/Down, Page Up/Page Down,
  Home/End, and Alt+R provide line/sentence reading and repetition; Enter or
  Escape closes the lesson.  Keybinding help is available and returns to the
  same fragment with a focus announcement.  NVDA interruption does not requeue
  speech.  No lesson archive or reopen command was added.
- Kept C++ seen flags, EOC conditions, trap removal, lesson text, and EOC
  message-log entries.  Logging an EOC lesson no longer rereads its entire text
  after the document closes.  The introductory close instruction uses the
  document's actual keybinding before expansion.
- Added the opt-in translated EOC field `popup_document_title`, its JSON
  documentation and translation extraction, and shared semantic text splitting
  that also preserves existing item-info formatting.
- Corrected `doc/tutorial_lesson_inventory.md`: trap activation removes its
  tile, so another tile with the same trap is required for repeat delivery.
- `test_access.bat` passed 1867 assertions in 103 cases; loading `dda_tutorial`
  and running the three new tutorial tests passed 19 assertions.  The Windows
  x64 tiles build, AStyle 3.1, JSON formatting, translation extraction, and
  `git diff --check` passed.  A data comparison confirmed that only document
  opt-in changed in the tutorial traps.
- Implementation commit: `3d1d43acae`.

Limitations:

- NVDA interaction quality is unverified; T151–T152 are in
  `ACCESS_USERTESTS.md`.  Automated tests cover semantic formatting, bindings,
  cursor boundaries, repeat-last, and speech suppression, not actual document
  keyboard input or a full tutorial walkthrough.
- Stage 22A still owns complete tutorial acceptance and any corrections to
  the exercises and continuation guidance.

## 2026-09-30 — Stage 19A: Vehicle examination and maintenance

- Implemented the selected interaction (option B, part list instead of the
  grid; conventions 2, 5, and 8).  Existing keys are unchanged.
- With a screen reader active, examining a vehicle opens "Parts of <vehicle>"
  with its part count: every part, front to rear, left to right, and as
  stacked on each tile, spoken as "headlight, front left, damaged 40%"
  (broken and leaking in words).  Locations are thirds of the vehicle
  ("front left", "center", "rear right").  The shared list filter narrows it
  by name; `Alt+D` opens part details (exact row and column, fuel or cargo,
  faults, label, description) in the document view.
- Enter on a part moves the grid cursor to its tile and opens "Tasks for
  <part>, <location>": Repair, Mend (with faults), Remove, Install here,
  Refill, Siphon, Assign crew, Change shape (with variants), and Label.
  Unavailable tasks stay listed as "Remove. Unavailable: <reason>";
  choosing one speaks "Remove frame. Unavailable: ..." and keeps focus.
  `Alt+D` on a task opens its requirements document: time, skills with
  have/missing, tools and components with their states, lifting, removal
  yield, extra engine or axle skills, and blocking reasons.  Escape returns
  to the part list; Escape there returns to the grid ("Vehicle grid. <tile>")
  and `Alt+L` (new `ACCESSIBILITY_PART_LIST` binding) reopens the list.
- The grid cursor speaks the tile location, its parts with condition, and
  its label, or "outside, front: no parts, obstructed" beyond the vehicle.
  `Alt+D` opens the tile details, `Alt+S` speaks the vehicle statistics
  (speeds, mass, cargo, status, wheels, most damaged and needs-repair parts,
  fuels, drag, offroad, draft).  Grid task keys use the same accessible
  lists: repair and remove choose among the tile's parts, install opens an
  install list with the part categories as uilist categories, and the
  overview-based tasks (mend, refill, siphon, crew) and
  `veh_interact::select_part()` open a part list.  Task messages or the
  cursor tile are spoken after each grid action.
- Shared change: `uilist::accessibility_resume()` reopens a list silently
  after the caller spoke why the chosen entry cannot be used.
- `test_access.bat` passed 2007 assertions in 105 cases; `[vehicle]` tests,
  the Windows x64 tiles build, and AStyle 3.1 checks passed.
- Implementation commit: `ab75f6ef86`.

Limitations:

- NVDA interaction quality is unverified; T153–T156 are in
  `ACCESS_USERTESTS.md`.  Automated tests cover formatters, part list order,
  tile speech, task availability and requirement lines, and statistics, not
  the interactive menus or keyboard input.
- The part list closes after a task starts the vehicle activity; its
  progress and result use the existing activity messages.
- Unload solid fuel and Rename vehicle are reachable only by their grid keys,
  and the grid `CHANGE_SHAPE` key still opens the unconverted map-cursor
  shape tool; the part task menu's Change shape uses the cosmetic variant
  list instead.
- The install list marks parts the visual list grays out as "requirements
  not met"; the exact missing count is computed on `Alt+D` or selection.

## 2026-09-30 — Stage 20: Character creation - scenario and profession

- Implemented the selected interaction (conventions 2, 3, 4, and 5).
  Existing keys are unchanged; the scenario tab now registers its existing
  `RANDOMIZE` binding (`*`), which upstream handled but never registered.
- Entering the Points, Scenario, Profession, or Background tab speaks the
  tab, item count, and focused row; the first tab adds "Character creation".
  Rows speak "Evacuee, 0 points, selected", "earns 1 point", "Unavailable:
  <reason>" (achievement requirement, world city size, background trait
  conflict), and "Not enough points" where the list shows these by color.
  Moves speak only the row, with "Top" or "Bottom" on wrap.
- Enter speaks "Selected <choice>" (backgrounds also "Removed <choice>")
  and the new point balance: "4 points left", "3 points over budget", or
  the legacy multi-pool breakdown.  Filter, reset filter, sort, randomize,
  identity, outfit, and the scenario calendar keys speak their result; a
  declined quit speaks the row again.  `Alt+D` opens the focused
  description in the document view; `Alt+S` speaks the tab, position,
  current choices, balance, and difficulty summary.
- Formatters are in `accessibility_format`, row builders in
  `accessibility_gameplay`, and the per-tab speech state
  (`choice_tab_speech`) in `src/newcharacter.cpp`.
- `test_access.bat` passed 2046 assertions in 107 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `704cbb929b`.

Limitations:

- NVDA interaction quality is unverified; T157–T160 are in
  `ACCESS_USERTESTS.md`.  Automated tests cover formatters, row builders
  with game data, and key resolution, not the interactive tab loops.
- In the Survivor (freeform) pool rows omit point costs and selection
  speaks no balance, matching the screen, which shows neither.
- "Not enough points" mirrors the visual header color; for backgrounds it
  inherits upstream's comparison against the profession cost.
- The difficulty summary in `Alt+S` is the header text with colors removed
  and may read awkwardly.  Scrolling the visual detail pane is silent;
  `Alt+D` is the accessible route.  The Stats tab onward is Stage 20A.

## 2026-09-30 — Stage 20A: Character creation - stats, traits, skills, and finish

- Implemented the selected interaction (conventions 2, 3, 4, 5, and 8);
  existing keys are unchanged.  Each tab speaks its name, item count, and
  focused row on entry, reusing Stage 20's `choice_tab_speech`, which now
  supports a pane (trait column) and a section (skill category).
- Stats: rows speak "Strength 8, average human" plus the header's limit or
  cost note; Left and Right speak "Strength 10, 4 points left".  `Alt+D`
  opens the derived effects in the document view.
- Traits: the tab and Left or Right speak "Negative traits, 30 items" and
  the row.  Rows speak cost, "selected", "locked by scenario/profession/
  background", "conflicts with <traits>", and "forbidden by scenario" or
  "profession".  Enter speaks "Selected" or "Removed" with the balance;
  refusal popups and the variant menu return to the row.  Filter, reset
  filter, sort ("Sorted by points" or "by name"), and randomize speak
  their result.
- Skills: entering a category speaks its name before the row ("Melee,
  level 3, 1 from profession"); Left and Right speak "Level 2" with the
  balance.  `Alt+S` adds the next upgrade cost.
- Description: fields speak "Name: Ada", "Blood type: O positive", and the
  start location with its variant count; value changes speak only the new
  value.  Name, gender, randomize, reroll ("Character randomized", then
  the re-entered tab), template save ("Saved template <name>"), city,
  location, and calendar keys speak their result.  Edits use the Stage 05
  text input and uilists; validation popups and the final confirmations
  use the Stage 05 controls and return to the field when declined.
  `Alt+D` opens a character summary of everything the tab shows outside
  its fields.
- The Description tab's skill-level calculation moved into
  `description_skill_level()` so the drawing and the summary share it.
- `test_access.bat` passed 2089 assertions in 109 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `a91cee218d`.

Limitations:

- NVDA interaction quality is unverified; T161–T165 are in
  `ACCESS_USERTESTS.md`.  Automated tests cover formatters, trait rows
  with game data, and key resolution, not the interactive tab loops.
- Trait rows do not speak profession bionics that block a trait; the popup
  after Enter names them.
- A loaded template has no separate announcement: character creation opens
  on the Description tab and speaks it.  Template saving reports success
  when the template file exists afterwards.
- Visual detail-pane scrolling and scrollbar dragging remain silent;
  `Alt+D` is the accessible route.

## 2026-09-30 — Stage 21: World and mod management

- Implemented the selected interaction (conventions 2, 3, 5, and 8);
  existing keys are unchanged.
- World selection speaks "World selection, 4 items" and rows such as
  "Survival, 3 characters"; page keys speak the page, `Alt+D` opens the
  world's characters and load order, and `Alt+S` speaks the position.  The
  main menu's World and Load rows now use the same character wording
  (`format_world_row`, formerly `format_main_menu_world` with "saves").
- Create World speaks its name, item count, and focused row on entry,
  resolving the silent screen reached from Random Character after the last
  world was deleted.  Rows speak "World name: <name>", "<slider>:
  <level>", Finish, Reset, and Randomize; slider changes speak only the new
  level.  Reset, randomize, the random-name key, declined confirmations,
  and returning from the mod manager or advanced options speak their
  result and the row.  `Alt+D` opens the slider level description or the
  screen's key hints; `Alt+S` speaks every setting and the load order size.
- With a screen reader, `prompt_world_name()` uses the Stage 05 text input
  instead of the silent ImGui input, keeping the randomize key and the
  cancel behavior (cancel keeps the previous name).
- The mod manager treats Mod List (with its category tab) and Mod Load
  Order as tabs.  Rows speak name, state (active, not installed, obsolete,
  incompatible with an active mod, or "Unavailable: missing dependency
  <id>"), and category.  Add speaks the load order position and any
  dependencies added or core mod replaced; refusals speak their reason.
  Remove speaks the remaining count and removed dependents; moves speak
  "Position 2 of 3" or why the mod cannot move.  Category tabs, filter,
  save default, and the description popup speak their result; `Alt+D`
  opens mod information in the document view.  The active world mods list
  speaks the same rows.
- Main menu delete, reset, and compression confirmations name the world
  through the popup's spoken context; create, copy, delete, reset, and
  compression changes speak their result before the main menu focus.
- Formatters are in `accessibility_format`, mod row and refusal builders in
  `accessibility_gameplay`, and the screen hooks in `src/worldfactory.cpp`
  and `src/main_menu.cpp`.
- `test_access.bat` passed 2153 assertions in 111 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `3d49cf1158`.

Limitations:

- NVDA interaction quality is unverified; T166–T170 are in
  `ACCESS_USERTESTS.md`, and T1 now expects the character wording.
  Automated tests cover formatters, mod rows and refusals with game data,
  and key resolution, not the interactive loops.
- The World Options tab and the Advanced options screen are Stage 21A.
- Mouse movement over the lists is silent.  Typing in the mod filter speaks
  each character but not the live match count; the count is spoken when
  the filter closes.
- Mod categories are spoken as the list headers show them, in capitals
  ("CORE CONTENT PACKS").  "Cannot move" reasons are derived from the same
  checks as the visual +/- marks.
- Character to Template from the manage-world menu has no added result
  speech; its load list uses the shared uilist.

## 2026-09-30 — Stage 21A: Options, rules, and panel configuration

- Implemented the selected interaction (conventions 2, 3, 4, and 8);
  existing keys are unchanged.
- Options: opening speaks "Options: General", the count of selectable rows,
  and the focused row; pages behave as tabs.  Group headers speak
  "Graphics, collapsed, 12 options"; Enter, Left, or Right speak "Expanded,
  12 options" or "Collapsed".  Option rows speak "Use tiles: True", with
  "Unavailable: requires <option>" for the prerequisite shown only in gray;
  value changes speak only the new value, and a refused change returns to
  the row after its popup.  With a screen reader, int and float options use
  the Stage 05 text input instead of the silent ImGui number input ("Not a
  number" on invalid input).  `Alt+D` speaks the tooltip with its default
  and the in-game world options note; `Alt+S` speaks position and page.
  The Create World "World Options" tab and "Advanced world options" use the
  same speech, resolving the Stage 21 limitation.  The save question names
  "Options", and "Options saved" or "Option changes discarded" follows.
- Safe mode and autopickup managers: Global and Character behave as tabs.
  Safe mode rows speak "zombie, Blacklist, Category: Hostile, Attitude:
  Hostile, Distance: 5, Mode: Both", omitting columns the rule does not use;
  autopickup rows speak "wooden arrow, Include".  Left and Right speak the
  focused column; editing speaks only the new value.  Add, default ruleset,
  remove, copy, move to the other tab, enable, disable, reorder ("Position
  3 of 5" or "Cannot move up: already last"), and the manager's enable
  switch speak their result.  The autopickup rule input uses the Stage 05
  text input with a screen reader.  `Alt+D` opens the rule fields and the
  wildcard examples; `Alt+S` speaks position, manager state, and column.
  Rule test lists speak their title and matches (`match_list_speech`).
  Save questions name the manager, and the result is spoken.
- Sidebar options: the panel and layout columns behave as tabs.  Rows speak
  "Stats, shown" or "hidden" and "Classic, active"; toggling speaks "Shown"
  or "Hidden".  Moving speaks "Moving Stats, position 3 of 20", then each
  target "Position 4 of 20", then "Moved Stats to position 4 of 20".
  `Alt+D` opens the description and key hints.
- The distraction manager already met the selected interaction through
  Stage 12A and was not changed.
- `query_yn_about()` in `src/output.cpp` adds the spoken context to a
  yes/no question without showing it.
- `test_access.bat` passed 2246 assertions in 114 cases; the Windows x64
  tiles build and AStyle 3.1 checks passed.
- Implementation commit: `1ef09f6797`.

Limitations:

- NVDA interaction quality is unverified; T171–T175 are in
  `ACCESS_USERTESTS.md`.  Automated tests cover formatters, the rule test
  list speech, and key resolution, not the interactive loops.
- Options changed by Left or Right on an int option still wrap from the
  maximum to the minimum as upstream does; speech gives only the new value.
  An out-of-range typed value falls back to the default, and the spoken
  value reflects that without an explanation.
- The safe mode "move up" key moves a rule toward the end of the list, as
  upstream does; speech reports the resulting position.
- Mouse movement over these screens is silent.

## 2026-10-03 — Review 02: speech work only with an active screen reader

- Screens and gameplay hooks build speech only when `accessibility::is_active()`:
  look-around, the medical menu (rows format speech on demand), multi-item
  drops, pickup results, `inventory_multiselector` selection changes, `uilist`,
  static and progress popups, `query_popup` prompts, the V menu, armor sorting,
  missions, the overmap cursor, the main menu, the mod manager, dialogue
  response options, bionics and mutations opening, and game log messages.
- Vehicle workshop: moving the cursor no longer builds the list of mountable
  part types; `ensure_can_mount()` builds it when the install list opens, and
  task menus focus each tile once.  Part lists compute the vehicle bounds once.
- `requirement_groups()` reads the availability that one
  `can_make_with_inventory()` call caches, for the selected batch size, so the
  visual list and speech agree for batches.
- Activity progress is formatted only when its text changes; regular
  expressions in progress normalization and markup removal became scanners
  (a test compares them with the old expressions); sentence punctuation is
  translated once per language.
- Inventory focus builds its entry once per key; targeting computes hit
  predictions once for the details document.
- Fixes found by the review: speech for mutations no longer inserts missing
  entries, monster threat ids include the monster type, and the automatic
  screen-reader probe uses the replaceable test clock.
- Deferred items are listed in Stage 22.

Limitations:

- No profiling was done; the changes remove work identified by reading the
  code, and the speech captured by tests is unchanged.
