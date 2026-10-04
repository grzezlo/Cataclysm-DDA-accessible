# Cataclysm: DDA Accessible

Cataclysm: DDA Accessible is an independent fork of [Cataclysm: Dark Days Ahead](https://github.com/CleverRaven/Cataclysm-DDA) focused on making the game playable by blind players. It is based on the maintained upstream 0.I branch. The first target is the Windows x64 tiles build with NVDA, using semantic announcements and keyboard controls.

**Work in progress:** The accessibility implementation is incomplete. Automated checks cover parts of the code, and some interactions have been exercised with NVDA, but many player scenarios still await testing by a blind screen-reader user. This source tree is not yet a verified, complete accessible release. Do not treat an automated build or a passing test suite as confirmation that the game is accessible in play.

## Development status

- [ACCESS_TODO.md](ACCESS_TODO.md) is the numbered roadmap for unfinished work. Its first stage is the current stage.
- [ACCESS_DONE.md](ACCESS_DONE.md) records completed stages, their verification, and important limitations.
- [ACCESS_USERTESTS.md](ACCESS_USERTESTS.md) lists player scenarios still awaiting a complete result from a blind screen-reader user.

The [project instructions](AGENTS.md) describe the accessibility requirements, architecture, and development workflow. The first playable target remains Windows x64 with NVDA; other screen readers and platforms are future work.

## Build and test from source

The Windows command-line workflow requires Visual Studio 2022 with the C++ tools and vcpkg component. From the repository root, use [build_win_x64.bat](build_win_x64.bat) for the tiles game, [build_tests_win_x64.bat](build_tests_win_x64.bat) for the test executable, and [test_access.bat](test_access.bat) for the focused accessibility suite. [test_all.bat](test_all.bat) runs the full test suite as the Windows CI does; run it and make it pass before you push changes or open a pull request. It takes about half an hour. These scripts locate the installed tools; the Visual Studio GUI is not required.

Run [clang_tidy_win_x64.bat](clang_tidy_win_x64.bat) to check the accessibility source and test files, or pass specific C++ file paths to check them. This requires the Visual Studio LLVM tools and gettext. The script uses CMake and Ninja to generate a local tiles compilation database without installing vcpkg packages; sound is disabled for this analysis configure because the current vcpkg SDL2_mixer CMake target does not resolve. Use the regular MSBuild script to verify the full tiles build. GitHub Actions runs the project's LLVM 18 clang-tidy checks, including its custom plugin, on the `access` branch.

Translations come in layers. The game uses the first translated, reviewed (non-fuzzy) entry from:

1. `lang/access/<language>.po`: strings added by this fork. AI may draft them; a draft carries the translator comment `# AI draft` until a human reviews it and removes the comment.
2. `lang/machine/<language>-overrides.po`: corrections of wrong upstream translations, so the fork need not wait for upstream to fix them.
3. `lang/po/<language>.po`: upstream catalogs, kept identical to upstream and updated only by upstream synchronization.
4. `lang/machine/<language>.po`: AI translations of upstream strings that the upstream catalog leaves untranslated.

Never submit entries from `lang/machine` to Transifex or upstream. Fix a wrong upstream translation in the overrides file rather than in `lang/po`, and report it upstream if you can.

Run [update_translations_win_x64.bat](update_translations_win_x64.bat) with language codes, for example `pl`, to extract the strings this tree adds compared with the `upstream-base` branch and merge them into `lang/access`; without arguments it updates every existing catalog. Add `--machine`, typically once per release, to also merge `lang/machine/<language>.po` with the upstream strings that neither `lang/po` nor `lang/access` translates (also written to the ignored `lang/po/gaps-<language>.pot`). A machine entry whose string upstream has since translated disappears, and an entry whose source string changed becomes fuzzy until it is translated again. The same run drops overrides that upstream now matches or whose source string is gone. Any tool or agent may then fill the empty machine entries; give it the `msgctxt`, `#.` comments, the existing translations of the language as translation memory, and the glossary in `lang/notes/<language>.txt` if there is one. The update script validates every catalog: format arguments and tags such as `<color_red>`, `</color>`, or `<npcname>` must match the original, and machine entries that fail are cleared. It also reports untranslated entries and AI drafts awaiting review.

Run [compile_translations_win_x64.bat](compile_translations_win_x64.bat) to build `lang/mo` with that precedence; `--no-machine` leaves out both `lang/machine` catalogs. [test_translations_win_x64.bat](test_translations_win_x64.bat) checks these scripts with fixture catalogs. The scripts require Git for Windows, gettext, and Python 3 with `polib` or `uv`. Generated templates in `lang/po` and compiled catalogs in `lang/mo` are ignored build artifacts.

NVDA speech currently depends on the NVDA Controller Client DLL. The DLL and its license are not yet included in this repository or a release package. Packaging, first-run guidance, and further NVDA acceptance testing remain on the roadmap. Until a release is prepared, this repository is intended for source builds and development testing.

The Windows GitHub Actions workflow builds the tiles game and tests on pushes to the access branch once Actions are enabled for the fork. CI cannot test the quality of the spoken experience; the pending scenarios are in [ACCESS_USERTESTS.md](ACCESS_USERTESTS.md).

## AI-assisted development

AI coding tools have assisted with implementation, tests, and documentation in this fork. Their output is reviewed and checked through builds and automated tests. Interaction quality is assessed through testing with blind NVDA users; AI output and automated checks alone are not evidence that a feature is accessible.

## About the upstream game

Cataclysm: Dark Days Ahead is a turn-based survival game set in a persistent, procedurally generated post-apocalyptic world. Its original project, documentation, community, and releases are available from [CleverRaven/Cataclysm-DDA](https://github.com/CleverRaven/Cataclysm-DDA). Upstream releases do not include this fork's accessibility changes.

This fork retains the upstream license and attribution files. Cataclysm: Dark Days Ahead was created by its many contributors; see the [upstream contributors](https://github.com/CleverRaven/Cataclysm-DDA/graphs/contributors) and [license files](LICENSE.txt) for attribution and license terms. Some included components have their own license notices.
