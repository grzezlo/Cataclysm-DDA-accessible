# Cataclysm: DDA Access Fork

## Project goal

This fork aims to make Cataclysm: Dark Days Ahead genuinely playable by blind
players.  The first supported configuration is Windows x64 with NVDA and the
tiles build.

Use NVDA Controller Client for the first working implementation.  Keep the
game-facing accessibility API independent of NVDA and of the renderer so that
AccessKit or another native accessibility backend can be added later.

This is an independent accessibility-focused fork.  Compatibility with
upstream is valuable for maintenance, but working accessibility takes priority
over upstream contribution policy or exact UI parity.

## Language

- Write all repository content in English.  This includes documentation, code
  comments, identifiers, test descriptions, release notes, and commit messages.
- Write source strings in English and make user-facing strings translatable by
  following the localization conventions already used by the project.
- Keep commit subjects concise, imperative, and in English.

## Accessibility requirements

- Support complete keyboard-only operation.  Never require a mouse to reach an
  action or obtain information.
- Announce semantic information, not rendered text or pixel content.
- Preserve informational parity with the standard interface.  Accessibility
  may describe, summarize, or reorganize information that a sighted player can
  perceive, but it must not reveal hidden creatures, state, intentions, or
  other information unavailable through normal gameplay.
- Do not communicate state through color, layout, symbols, or sound alone.
- Prefer short focus announcements and user-controlled access to detail.  The
  detail interaction may be a command, a navigable semantic view, or another
  design selected through the interaction review below.
- Deduplicate and prioritize announcements so routine redraws do not flood the
  screen reader.
- Preserve the last meaningful announcement so the player can request it
  again.
- Do not add a speech-cancel command.  NVDA already lets the user interrupt
  speech with Shift or Control.  Do not intercept those keys or immediately
  requeue an interrupted message.
- Treat testing with a blind NVDA user as the primary acceptance standard for
  interaction quality.  Do not claim that a feature is accessible until it has
  been tested with NVDA or clearly mark it as unverified.

### Interaction design review

- Before implementing a new player-facing accessibility interaction, present
  the user with three genuinely distinct and feasible interaction designs and
  wait for an explicit choice.  A roadmap outcome does not authorize one
  particular interaction design.
- Describe each option from the player's perspective: keyboard operation,
  focus/navigation model, what is spoken and when, how detail is requested, how
  the player returns or cancels, and any visible UI effect.
- State the important usability, information-density, implementation, and
  maintenance tradeoffs.  Recommend one option and explain why it best supports
  blind play.
- After the user chooses, record the selected interaction and its essential
  behavior in the current `ACCESS_TODO.md` stage before changing code, so another
  agent does not have to infer the decision.
- Do not satisfy this review with superficial technical variations of the same
  interaction.  Consider patterns such as concise announcements, navigable
  semantic lists or views, and layered overview/detail access when applicable.
- A small regression fix that preserves an already accepted interaction design
  does not require a new three-option review.  Material changes to its controls,
  focus model, speech behavior, or information organization do.

## Architecture

- Put screen-reader-independent behavior behind a small accessibility service,
  with operations such as announcements, focus changes, screen changes, and
  status changes.
- Keep NVDA-specific calls inside a Windows NVDA backend.  UI code and gameplay
  code must not call NVDA Controller functions directly.
- Provide a null backend and a capture/test backend so behavior can be tested
  without NVDA running.
- Keep integration hooks narrow and localized to reduce conflicts when syncing
  upstream changes.
- Do not build a separate wxWidgets UI or duplicate the complete game UI.
- Delay AccessKit integration until the semantic model and an NVDA-backed
  playable vertical slice are working.

### Accessibility implementation map

- `src/accessibility.h` and `src/accessibility.cpp` own the renderer-independent
  accessibility service, the screen reader mode (`SCREEN_READER_SPEECH` option,
  automatic by default), announcement history, gameplay gating, and shared
  deduplication policy.  They must not depend on game UI or map objects.  UI
  code decides whether to use accessible behavior with
  `accessibility::is_active()`, never by querying the screen reader directly.
- `src/accessibility_format.h` and `src/accessibility_format.cpp` own the pure
  semantic models and speech formatters.  Keep them free of game state so they
  can be tested with fixed inputs; add new formatters here, not to the service
  header.
- `src/accessibility_gameplay.h` and `src/accessibility_gameplay.cpp` own the
  gameplay views and descriptions: situation and status review, look-around
  tile descriptions, threat and condition alerts, movement, and nearby-list
  semantics.
- `src/accessibility_nvda.cpp` is the only place for NVDA Controller Client and
  Windows desktop calls.  Other code must use the accessibility service.
- `tests/accessibility_test.cpp` contains the capture backend and service-level
  contract tests.  Prefer small semantic formatters that can be tested with
  fixed inputs over assertions against rendered windows.
- `src/main_menu.cpp`, `src/uilist.cpp`, `src/popup.cpp`, and
  `src/messages.cpp` are shared announcement integration points.  Extend a
  shared control only when the semantics are valid for every caller; otherwise
  keep the hook in the owning feature.
- `src/game.cpp` keeps only narrow calls into `accessibility_gameplay` from the
  gameplay lifecycle, movement, visible-threat, look-around, and nearby-list
  code.  Do not add accessibility logic bodies to `game.cpp`.
- `data/raw/keybindings.json` owns default accessibility bindings.  New actions
  must also be discoverable through the in-game keybinding and help interfaces.
- Before changing an integration point, inspect its current symbol and recent
  history.  Upstream synchronization may move UI code; update the relevant
  stage inventory in `ACCESS_TODO.md` when it does.

## Progress tracking

- Treat `ACCESS_TODO.md` and `ACCESS_DONE.md` as the authoritative source for
  current scope, priorities, milestones, and completed work.
- Keep the complete numbered roadmap for unfinished accessibility work in
  `ACCESS_TODO.md`.  The first stage in the file is current and the following
  stage is next; do not duplicate that ordering in a table of contents or with
  status labels.
- Keep stage numbers stable.  Insert an unplanned stage with a suffix such as
  `03A` instead of renumbering stages that have already been referenced.
- Each stage must state its outcome, feature-specific code inventory, success
  criteria, possible automated tests, and required human verification.  The
  roadmap is sequential, so do not list earlier stages as dependencies.  Record
  only exceptional external blockers or independently runnable work where it is
  relevant.  Do not duplicate the stable architecture map or repository
  workflow in every stage.
- When an item is completed, remove it from `ACCESS_TODO.md` and move it to
  `ACCESS_DONE.md`.  Completing this documentation update is part of completing
  the work item.
- Use `ACCESS_DONE.md` both as the record of completed stages and as historical
  context for regression work.
- Record the tested Windows version, NVDA version, fork version or commit, and
  important limitations for completed milestones.
- Keep both files concise and use headings and lists rather than large tables.
- Read `ACCESS_TODO.md` before starting accessibility work.  Consult
  `ACCESS_DONE.md` only when historical context or regression coverage is
  needed.
- A native stage is complete only after its implementation and automated checks
  pass, the Windows x64 tiles build succeeds, and a blind NVDA user confirms
  every required human scenario.  A non-native backend stage uses a blind user
  of each declared screen-reader/platform combination and must also pass native
  regression checks.  Human confirmation must cover keyboard operation, focus
  order, announcement count, concise and detailed output, repeat-last, and
  behavior after speech interruption.
- Reach hard-to-reach human scenarios through the in-game debug menu (for
  example spawning monsters or items, bleeding, or changing time) instead of
  progressing through normal play.  Give each such scenario a short debug-menu
  route in its stage.  Debug setup proves the accessible behavior, not that
  normal play reaches the state; Stages 22 and 22A cover normal play.
- A scenario that depends on an interaction a later stage has not yet made
  accessible does not block the current stage.  Move it explicitly to the
  verification list of the stage that enables it, and record in
  `ACCESS_DONE.md` which scenarios were verified and which were moved where.
- When closing a stage, record the Windows version, NVDA version, tested commit,
  results, and limitations in `ACCESS_DONE.md`.  Removing it from
  `ACCESS_TODO.md` makes the following stage current without another status
  update.

## Branching and upstream synchronization

- Use `access` as the public fork's primary development and release branch, and
  configure it as the default branch on GitHub.
- Base `access` on the latest maintained upstream `0.I-branch`, not upstream
  `master`.
- Keep upstream tracking branches clean.  Commit routine accessibility work
  directly to `access` in small, focused commits; the roadmap is sequential,
  so per-feature branches add no value.  Every commit on `access` must pass
  `test_access.bat` and build the Windows x64 tiles game.
- Use a separate branch derived from `access` only for experimental work or
  work that may be abandoned.  After verifying that such a branch has been
  merged into `access`, delete it locally.  Keep unmerged branches and
  branches still needed for active work.
- Do not push to or change the fork remote; the maintainer pushes `access`.
- Treat a future port to upstream `master` or 0.J as a separate experimental
  line until it is stable enough for players.
- Keep accessibility commits focused so they can be ported across upstream UI
  changes without rewriting the entire feature.

## Verification and delivery

- Test announcement generation independently through the capture backend.
- For every interactive feature, verify focus order, keyboard operation,
  announcement count, concise and detailed output, and behavior after speech is
  interrupted.
- Prefer textual diagnostics and logs over screenshot-only evidence.
- Preserve upstream license and attribution files.  Document the use of AI in
  developing this fork clearly in its public README and release information.

## Windows command-line workflow

- Use the repository-root batch files for Windows x64 MSVC work.  They locate
  Visual Studio, MSBuild, and the Visual Studio vcpkg component through
  `vswhere`; do not hardcode installation paths or edit the tracked
  `CMakePresets.json` for one machine.
- Run `build_win_x64.bat` for the tiles game and
  `build_tests_win_x64.bat` for the test executable.  Run `test_access.bat`
  for the focused accessibility suite and `test_all.bat` for the full suite.
- Use AStyle 3.1 directly on Windows instead of the Makefile targets, whose
  checks require a POSIX shell and can report success without running.  Format
  changed C and C++ files with
  `astyle --options=.astylerc -n <files>` and verify them with
  `astyle --options=.astylerc --dry-run -X -Q --ascii <files>`; a clean check
  prints no `Formatted` lines.
- Every entry-point batch file must call `setenv.bat` first.  Keep
  `VcpkgInstalledDir` pointed at `msvc-full-features\vcpkg_installed`; invoking
  MSBuild without that explicit directory can create an incorrect nested
  `x64-windows-static\x64-windows-static` package tree with some Visual Studio
  vcpkg versions.
- Do not manually copy or install packages into a nested triplet directory.
  Let the batch workflow restore missing manifest dependencies into the
  canonical install tree.
