# Reparenting a Blueprint / WBP

Applies to: changing a Blueprint/WBP parent; moving a parent class between AngelScript and C++; porting Lyra types to local types.

## Core rule: the old parent lives until the new parent is persisted

Until reparent, save and on-disk readback are all done, the old parent must exist and load. Do not delete the `.as`, delete/rename the C++ class, or unload the module. If it disappears first, the asset loses its parent at load time:

- opening the Blueprint crashes the editor;
- SCS component nodes fail to load, and re-attaching the parent does not bring them back; saving then bakes the loss in (how `Character_Default` lost `AIPerceptionStimuliSource`);
- in a running editor, script hot reload unloads the AS class at once, leaving the in-memory asset parentless; one save writes the damage to disk.

Before starting, record the **full path of the old parent** (changelist description or a comment beside the redirect) for rollback.

## Choosing a path

| Case | Path |
|---|---|
| New parent can have a different name, or the old one is kept | A: reparent in editor (preferred) |
| Must keep the same name (AS class rewritten as same-named C++) | B: redirect migration |

An AS class and a C++ class of the same name cannot coexist (script binds C++ types by short name), so same-name migration is always B.

## A: in-editor reparent

1. New parent written and compiled. C++ needs an editor restart to be visible to Blueprint; AS hot reloads.
2. `p4 edit` the asset.
3. Monolith `blueprint_query reparent_blueprint`, then `compile_blueprint`.
4. Save with `save_loaded_asset(asset, only_if_is_dirty=False)` (MCP edits usually do not dirty the package; a plain save is a no-op).
5. Read back (see Verification).
6. Only then delete the old parent.

Blueprint variables sharing a name with a new-parent member are renamed `Name_0`; instance overrides need `PropertyRedirects` to reattach (memory note `bp-to-as-reparent-migration`).

## B: same-name redirect

1. Write the new class with identical field names so asset data matches by name.
2. Add to `[CoreRedirects]` in `Config/DefaultEngine.ini`. **An AS source requires `OverrideClassName`**; otherwise it only renames, the import entry stays `ASClass`, the native class is not found and the parent is empty.

   ```ini
   +ClassRedirects=(OldName="/Script/Angelscript.X",NewName="/Script/<Module>.X",OverrideClassName="/Script/CoreUObject.Class")
   +StructRedirects=(OldName="/Script/Angelscript.Y",NewName="/Script/<Module>.Y",OverrideClassName="/Script/CoreUObject.ScriptStruct")
   ```

   A C++ source (e.g. `/Script/LyraGame.*`) does not need it. `<Module>` is the Build.cs module name, not the plugin name.
3. **Close the editor first, then delete the old parent.** Deleting a `.as` in a live editor triggers the hot-reload unload. If already deleted, save no affected asset.
4. Build and link fully, with the same configuration as the editor (`LogInit: Build Configuration` in `YanGame.log`).
5. **Before opening the editor**, cold-load in a separate process to confirm the redirect works:

   ```powershell
   & "E:\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UnrealEngine\Projecties\YanGame\YanGame.uproject" `
     -run=pythonscript -script="<check.py>" -unattended -nullrhi -nosplash -abslog="<log>"
   ```

   The script reads the generated class CDO, tests `isinstance(cdo, unreal.<NewClass>)` and prints key config fields. Python cannot read `WidgetBlueprint.parent_class`; do not use it. With an editor open, port 8010 conflicts make the exit code 1; trust the log.
6. After the cold load passes, start the editor and do A steps 2, 4, 5 to persist the new parent.
7. `grep -rl --include=*.uasset --include=*.umap "<old path>" Content Plugins` shows no remaining references → the redirect may be removed (keeping it is harmless).

## Verification (all four)

- **In editor:** `blueprint_query get_parent_class` returns the new path and `status` is `UpToDate`.
- **On disk:** uasset timestamp updated; `strings -n 6 <uasset> | grep <old path>` returns 0; `Class'<new path>'` present.
- **Data intact:** `get_cdo_properties` config fields (arrays, colors, text) match pre-reparent.
- **Level instances** (asset placed in a level): read the instance itself, not just the CDO; writing the CDO after reparent does not propagate to existing instances (memory note `reparent-then-python-cdo-leaves-instance-stale`).

## Rollback

- Old parent still present: `reparent_blueprint` back to the recorded path.
- After path B: restore the old parent file, remove the redirect, `p4 revert` the asset, restart the editor.
