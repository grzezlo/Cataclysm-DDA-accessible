# Cataclysm: DDA Accessible

Cataclysm: DDA Accessible is an independent fork of [Cataclysm: Dark Days Ahead](https://github.com/CleverRaven/Cataclysm-DDA) focused on making the game playable by blind players. It is based on the maintained upstream 0.I branch. The first target is the Windows x64 tiles build with NVDA, using semantic announcements and keyboard controls.

**Work in progress:** The accessibility implementation is incomplete. Automated checks cover parts of the code, and some interactions have been exercised with NVDA, but many player scenarios still await testing by a blind screen-reader user. This source tree is not yet a verified, complete accessible release. Do not treat an automated build or a passing test suite as confirmation that the game is accessible in play.

## Development status

- [ACCESS_TODO.md](ACCESS_TODO.md) is the numbered roadmap for unfinished work. Its first stage is the current stage.
- [ACCESS_DONE.md](ACCESS_DONE.md) records completed stages, their verification, and important limitations.
- [ACCESS_USERTESTS.md](ACCESS_USERTESTS.md) lists player scenarios still awaiting a complete result from a blind screen-reader user.

The [project instructions](AGENTS.md) describe the accessibility requirements, architecture, and development workflow. The first playable target remains Windows x64 with NVDA; other screen readers and platforms are future work.

## Build and test from source

The Windows command-line workflow requires Visual Studio 2022 with the C++ tools and vcpkg component. From the repository root, use [build_win_x64.bat](build_win_x64.bat) for the tiles game, [build_tests_win_x64.bat](build_tests_win_x64.bat) for the test executable, and [test_access.bat](test_access.bat) for the focused accessibility suite. [test_all.bat](test_all.bat) runs the full test suite. These scripts locate the installed tools; the Visual Studio GUI is not required.

NVDA speech currently depends on the NVDA Controller Client DLL. The DLL and its license are not yet included in this repository or a release package. Packaging, first-run guidance, and further NVDA acceptance testing remain on the roadmap. Until a release is prepared, this repository is intended for source builds and development testing.

The Windows GitHub Actions workflow builds the tiles game and tests on pushes to the access branch once Actions are enabled for the fork. CI cannot test the quality of the spoken experience; the pending scenarios are in [ACCESS_USERTESTS.md](ACCESS_USERTESTS.md).

## AI-assisted development

AI coding tools have assisted with implementation, tests, and documentation in this fork. Their output is reviewed and checked through builds and automated tests. Interaction quality is assessed through testing with blind NVDA users; AI output and automated checks alone are not evidence that a feature is accessible.

## About the upstream game

Cataclysm: Dark Days Ahead is a turn-based survival game set in a persistent, procedurally generated post-apocalyptic world. Its original project, documentation, community, and releases are available from [CleverRaven/Cataclysm-DDA](https://github.com/CleverRaven/Cataclysm-DDA). Upstream releases do not include this fork's accessibility changes.

This fork retains the upstream license and attribution files. Cataclysm: Dark Days Ahead was created by its many contributors; see the [upstream contributors](https://github.com/CleverRaven/Cataclysm-DDA/graphs/contributors) and [license files](LICENSE.txt) for attribution and license terms. Some included components have their own license notices.
