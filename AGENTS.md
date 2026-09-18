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
- Do not communicate state through color, layout, symbols, or sound alone.
- Prefer short focus announcements, with separate commands for detailed
  information.
- Deduplicate and prioritize announcements so routine redraws do not flood the
  screen reader.
- Preserve the last meaningful announcement so the player can request it
  again.
- Do not add a speech-cancel command.  NVDA already lets the user interrupt
  speech with Shift or Control.  Do not intercept those keys or immediately
  requeue an interrupted message.
- Treat the repository owner, as a blind NVDA user, as the primary acceptance
  tester for interaction quality.  Do not claim that a feature is accessible
  until it has been tested with NVDA or clearly mark it as unverified.

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

## Scope and milestones

Prioritize a playable vertical slice in this order:

1. Startup and main-menu navigation.
2. Loading a game or starting with a simple character preset.
3. Game-message announcements.
4. Movement, current-tile descriptions, and nearby threats.
5. Basic inventory and item use.
6. Accessible targeting and combat.
7. Saving and quitting.

Crafting, advanced inventory, vehicles, construction, and less common screens
come after this first playable slice.

## Progress tracking

- Keep active work in `ACCESS_TODO.md`.  It should contain only current, next,
  and later work, plus concise known blockers and acceptance criteria.
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

## Branching and upstream synchronization

- Use `access` as the public fork's primary development and release branch, and
  configure it as the default branch on GitHub.
- Base `access` on the latest maintained upstream `0.I-branch`, not upstream
  `master`.
- Keep upstream tracking branches clean.  Develop accessibility work on a
  dedicated feature branch derived from `access`, then merge completed work
  back into `access`.
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
