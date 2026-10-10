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
are native release gates: coverage, the complete tutorial, and packaging.  Stage 23A
adds machine translations and does not block a release.  Stage 24
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

## Stage 22 - Coverage and announcement-policy audit

**Outcome:** Every keyboard-reachable interface is classified and tested, speech
policy is consistent, and remaining bespoke-screen gaps are closed.

**Code inventory:** all `accessibility::` call sites; all registered
`input_context` categories and bespoke UI entry points; the accessibility service
and feature tests; `tests/accessibility_benchmark_test.cpp` includes the hidden
`[accessibility_profile]` selection, activity, and changing 60-creature fixtures.
Create and maintain a concise coverage manifest during this stage so later
upstream merges do not require rediscovering every screen.

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

**Selected lesson delivery (option A, accepted 2026-09-30):** Stage 19B's
document delivery applies to both lesson sources.  Opening speaks the title
and first fragment; navigation and repeat-last work within the open lesson.
Closure preserves the original seen flags, EOC conditions, trap removal, and
message-log availability.  No lesson archive or reopen command is added.

**Implementation hints:**

- C++ and EOC lessons use the shared document reader when accessibility is
  active (Stage 19B, option A); inactive mode keeps the original popups.
  Reassess the actual NVDA experience after the exercised
  controls are accessible, including interruption, repetition, and speech from
  another tile with the same lesson trap (the original trap is removed).
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
licenses, `lang/access` translations, and distribution contents.

**Success criteria:** A reproducible clean package contains the matching NVDA
Controller Client DLL and LGPL 2.1 license, preserves upstream attribution,
starts safely with NVDA running or absent, and provides accessible first-run,
command, limitation, diagnostic, AI-development, and defect-reporting guidance.
All accessibility source strings are wrapped in `_()` / `pgettext()` and the
Polish fork catalog `lang/access/pl.po` (the tested locale) includes
translations for critical accessibility strings, and the package's `lang/mo` is
built with `compile_translations_win_x64.bat`.

**Possible automated tests:** Validate package contents, architecture, licenses,
version, clean configuration, missing-DLL fallback, smoke launch, focused/full
tests, and final tiles build from a clean checkout.

**User-test scenarios:** A blind tester who did not prepare the source
tree obtains and launches the artifact, discovers controls from its documentation,
creates or loads a game, plays, saves, exits, and restarts.  Beta testers work
through `ACCESS_USERTESTS.md`; it must be empty or every remaining scenario
must be accepted as a known limitation in the release notes.  Record the
commit and artifact version; this is the native release gate.

## Stage 23A - Machine translation layer

**Outcome:** Players of any language can use AI translations of upstream strings
that the upstream catalog lacks, while every human translation takes precedence
and machine translations retire automatically when upstream translates a string.
Contributors from other countries can add their language with the same tools.
This stage does not block the Stage 23 release and can run independently.

**Selected catalog layout (accepted 2026-10-02):**

- `lang/access/<language>.po` holds every translation of strings added by the
  fork, whatever their source.  AI may draft them; a human reviews and corrects
  them there.  AI drafts reach the game immediately so beta testers can report
  language problems found in play.  A draft carries the translator comment
  `# AI draft`, which the reviewer removes; the fuzzy flag keeps its gettext
  meaning (a stale translation of a changed source string) and stays out of the
  game.
- `lang/po/<language>.po` stays identical to upstream.
- `lang/machine/<language>.po` holds AI translations of upstream strings that
  neither `lang/po` nor `lang/access` translates.  The file is the provenance
  marker; entries carry no special flags.  Machine translations are never
  submitted to Transifex.
- `lang/machine/<language>-overrides.po` (accepted 2026-10-04) holds
  corrections of wrong upstream translations, AI-made or manual, so the fork
  need not wait for upstream to fix them.  They are never submitted to
  Transifex either.  An override stays until removed by hand, except that the
  update script drops it once the upstream translation becomes identical and
  drops it when its source string disappears or changes (the old text stays
  in history).  Fork strings are never overridden here; they belong in
  `lang/access`, and the update script reports such entries.
- Compilation takes translated, non-fuzzy entries in the order `lang/access`,
  machine overrides, `lang/po`, `lang/machine` (`msgcat --use-first`).
  Upstream fuzzy entries keep their current in-game use.  Untranslated
  entries of every layer are removed first so they cannot hide a lower layer.
  A `--no-machine` option, which skips both machine files, and missing
  machine catalogs produce the current result.

**Code inventory:** `lang/update_access_po.sh`, `lang/compile_access_mo.sh`,
`lang/access_catalog.py`, `update_translations_win_x64.bat`,
`compile_translations_win_x64.bat`, `tools/check_po_printf_format.py`,
`lang/notes/`, `.claude/agents/pl-translator.md`,
`.claude/agents/pl-reviewer.md`, `.claude/commands/translate-pl.md`,
`README.md`, and release notes.

**Progress:** The layer tooling is implemented: `--machine` in the update
script (gap step, override pruning, validation with clearing of failing
machine entries, status report with AI-draft counts), `--no-machine` and the
four-layer precedence in the compile script, and fixture tests in
`lang/test_access_po.sh` (`test_translations_win_x64.bat`).  The Polish
glossary is drafted in `lang/notes/pl.txt` from the `langbench` comparison.

**Selected Polish workflow (2026-10-10):** The `langbench` comparison chose
Opus (medium effort) to translate and Sonnet (high effort) to review.
`access_catalog.py split` divided the 50,339 empty entries of
`lang/machine/pl.po` into 204 batches of up to 250 entries in
`lang/machine/work/pl/` (0001-0007 interface, 0008-0054 items, 0055-0058
monsters, 0059-0086 other core data, 0087-0096 snippets, 0097-0125 NPCs,
0126-0204 mods).  A Claude Code cloud session runs `/translate-pl
<first>-<last>`: for each batch the `pl-translator` subagent fills it with
`show`, `lookup`, and `fill`, the `pl-reviewer` subagent corrects errors only
and logs them in `NNNN.review.md`, and the session commits each batch.  Cloud
environment: Ubuntu with `apt-get install -y gettext` in the setup script and
Trusted network access.  Afterwards, locally: `access_catalog.py merge
lang/machine/pl.po lang/machine/work/pl`, then
`update_translations_win_x64.bat --machine pl`, and remove
`lang/machine/work/pl`.  Start with a pilot of batches 0001-0003 to measure
cost and check that the reviewer fixes errors without restyling.

Remaining: the pilot and the full run, the first overrides (the validator
already flags about a dozen upstream `pl` entries with invented or
unbalanced tags), release notes, and the user test.

**Implementation hints:**

- Add a gap step to the update script: from the full template of the current
  tree, write the entries translated by neither `lang/po` nor `lang/access` to
  an ignored `lang/po/gaps-<language>.pot`, then `msgmerge` the machine
  catalog with it and drop obsolete entries.  An upstream translation then
  removes its machine entry without manual work; changed source strings return
  as fuzzy and stay out of the game until translated again.
- Keep the repository independent of an AI provider.  The scripts export empty
  entries and validate the result; any tool or agent may fill them in batches.
- Give the translator `msgctxt`, extracted `#.` comments, source files, the
  existing translations of the language as translation memory, and a glossary
  in `lang/notes/<language>.txt`.
- Translate in player-visible order: `src/` interface strings, then items and
  monsters, then snippets and NPC dialogue.
- Update the machine layer rarely, for example once per release, because each
  full refresh of a catalog of about 50,000 entries grows the history.
  Reassess keeping it as a release asset instead if the repository grows too
  much.

**Success criteria:** Compiled catalogs follow the precedence above; an
upstream translation replaces a machine entry after the next update; fuzzy and
untranslated entries never reach the game while `# AI draft` entries do; the
update script reports how many drafts await review; every machine and access
catalog
passes the validation below; and the README and release notes state which
translations are machine-made and how to add a language.

**Possible automated tests:** Fixture catalogs for the three layers verify
precedence, removal of untranslated and fuzzy entries, retirement of a machine
entry after an upstream translation, and `--no-machine`.  Validation runs
`msgfmt -c`, `tools/check_po_printf_format.py`, and a check that every
translation keeps the original's tags (`<color_*>`, `</color>`, `<npcname>`,
`<num>`, `<good>`, `<bad>`, and similar); an entry that fails stays empty.

**User-test scenarios:** A Polish NVDA user starts a game with the machine
layer enabled and checks that menus, item descriptions, and fork announcements
are spoken in Polish without tag fragments or broken placeholders, that
reviewed fork translations are used, and that a build with `--no-machine`
falls back to English for the same untranslated upstream strings.

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
- `lang/access/pl.po` holds the fork's own strings without translations yet.
  Translating them is pending Stage 23; AI drafts and their human review follow
  the Stage 23A catalog layout.
