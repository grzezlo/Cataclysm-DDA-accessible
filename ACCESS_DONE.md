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
