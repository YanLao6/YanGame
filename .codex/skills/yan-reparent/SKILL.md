---
name: yan-reparent
description: Workflow routing for changing a Blueprint or WBP parent class. Use for reparent, rewriting an AngelScript class as C++ (or back), porting Lyra types to local types, or adding CoreRedirects for classes/structs. Core constraint: the old parent must survive until the new parent is persisted.
type: index
tags: [blueprint, widget, angelscript, cpp, coreredirects, migration]
requires: []
---

# Reparenting (routing)

Full procedure in `references/workflow.md`; read it before starting.

Three rules to keep in mind:

- **Old parent lives until the new one is persisted.** Do not delete the old `.as` or C++ class before reparent, save and on-disk readback finish. Record the old parent's full path first, for rollback.
- **Same-name AS → C++:** the redirect needs `OverrideClassName`; close the editor before deleting the `.as`; cold-load with `UnrealEditor-Cmd` before opening the editor.
- **Save with `only_if_is_dirty=False`;** done means the old path no longer appears in the on-disk uasset.
