# OpenUnrealUtilities (v1.5.0)

General utility plugin. Read the headers for signatures; this is a map of what exists so you do not rewrite it.

| Module | Type | Contents |
|---|---|---|
| `OUURuntime` | Runtime | C++ utilities: scene projection, credits, flow control (`FRecursionGuard`, request queue), game entitlements subsystem, GAS extensions (`UOUUGameplayAbility`, `UOUUAbilitySystemComponent`), gameplay-debugger categories, localization helpers, message log, math, `UOUUActorPool` + `IOUUPoolableActor`, `FSemanticVersion`, `FSequentialFrameScheduler`, C++ templates |
| `OUUBlueprintRuntime` | Runtime | Blueprint-callable libraries: clipboard, CDO/world lookup, config ini reads, runtime DataTable row add/remove, build/world-type queries (`UEngineGlobalsLibrary`), file I/O, project settings, message dialogs, property-path get/set, platform process launch, Slate notifications |
| `OUUUMG` | Runtime | `UOUUWindow`: Blueprint-subclassable standalone OS window |
| `OUUEditor` | Editor | `UOUUEditorLibrary` (rerun construction scripts, instance components), animation/material editing helpers, slow-task dialogs, asset validators (hard references, restricted names, text), reference viewer, content browser extensions, `OUUValidateAssetListCommandlet` |
| `OUUDeveloper`, `OUUTests`, `OUUTestUtilities` | Developer | Map window, world stats overlay, test framework |

C++ templates worth knowing: `ScopedAssign<T>` (temporary value, restored on scope exit), `RWLockedVariable<T>`, `ScopedMultiRWLock`, circular buffers, `SubclassWithInterfaces<Base, ...>`, `ArrayUtils`/`StringUtils`/`BitmaskUtils`.

Related: the tag extensions in this plugin family are documented in `ue-gameplay-tags-messaging/references/ouutags.md`.
