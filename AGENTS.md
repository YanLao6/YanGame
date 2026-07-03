# Repository Guidelines

## Project Structure & Module Organization

This is an Unreal Engine project rooted at `YanGame.uproject`. The primary game module lives in `Source/YanGame`, with targets in `Source/YanGame.Target.cs` and `Source/YanGameEditor.Target.cs`. Most reusable gameplay systems are plugins under `Plugins/`, including `YanGameCore`, `ModularGameplayExperiences`, `ModularGameplayUI`, `EquipManager`, `DaftMover`, and `RealtimeDestruction`. Game assets are in `Content/` (`Ability`, `Actor`, `Experiences`, `GameMode`, `Hero`, `Input`, `Maps`, `Mover`, `UI`). Project settings are in `Config/`; generated folders such as `Binaries/`, `Intermediate/`, `Saved/`, and `DerivedDataCache/` should not be edited directly.

## Build, Test, and Development Commands

- Open locally: launch `YanGame.uproject` from Unreal Editor or open `YanGame.sln` in Visual Studio/Rider.
- Build editor target:
  `UnrealBuildTool.exe YanGameEditor Win64 Development -Project="E:\UnrealEngine\Projecties\YanGame\YanGame.uproject"`
- Regenerate IDE files after module/plugin changes:
  `UnrealVersionSelector.exe /projectfiles "E:\UnrealEngine\Projecties\YanGame\YanGame.uproject"`
- Run automation tests from the editor or command line with `UnrealEditor-Cmd.exe YanGame.uproject -ExecCmds="Automation RunTests <Filter>; Quit" -unattended -nop4`.

## Coding Style & Naming Conventions

Follow Unreal C++ conventions: tabs for indentation, braces on explicit blocks, `U/A/F/I/E/T` type prefixes, PascalCase for types and functions, and camelCase only where existing local code does so. Keep public headers in `Public/`, implementation in `Private/`, and module dependencies in each `.Build.cs`. Comments should explain intent or constraints; avoid process notes. For reflected APIs, prefer concise UE-style documentation comments.

## Testing Guidelines

There is no central test suite in the root project. Add focused automation or functional tests near the plugin or module being changed, using names that include the feature under test. Always verify changed gameplay through either an automation filter, PIE/editor testing, or a targeted commandlet run, and report the exact verification used.

## Commit & Pull Request Guidelines

Recent history uses short imperative subjects, sometimes with conventional prefixes such as `fix:` and PR references like `(#651)`. Keep commits scoped to one behavior change. PRs should describe the change, list verification steps, link related issues, and include screenshots or recordings for UI, camera, movement, or asset-facing changes.

## Agent-Specific Instructions

This workspace is Perforce-managed. If a file is read-only, check it out before editing rather than changing attributes manually. Do not modify generated folders, IDE metadata, or unrelated plugin code while addressing a focused request.
