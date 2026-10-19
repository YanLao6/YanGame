# Experience and PawnData

Experience decides what a match loads; PawnData decides what one player plays. Experience code: `Plugins/ModularGameplayExperiences`; hero assignment: `Plugins/YanGameCore/Source/HeroAssignmentSystem`.

## Two layers

- **Experience, per class:** GameFeatureActions inject components, widgets and ability sets into GameState, Controller and Pawn for all players.
- **PawnData, per player:** abilities, input, components and UI come from `UModularPawnData` Fragments.

`UYanPawnData` adds no fields; it exists as a stable asset type for hero assets. **Add a Fragment for new hero capability, never a PawnData field.**

## Assignment flow

`UHeroAssignmentComponent` (GameStateComponent, injected by Experience via AddComponents):

```
Experience loaded → OnExperienceLoaded: assign players already present
  → OnPlayerInitialized: assign later joiners
  → AssignHeroForController decides and records in AssignedHeroes
  → pawn spawn and PlayerState write both look up the same table
```

Assignment happens once at player init, then is lookup-only, so both paths agree. `UHeroCatalog` candidates are **hard references** loaded with the component class, so the decision never loads synchronously.

## First-write lock trap

A player who logs in after the Experience is ready makes PlayerState query PawnData **before identity exists**. Falling back to the Experience default there writes and locks the first write; real assignment can never overwrite it (every player becomes the default hero while assignment code looks fine).

So while `IsManagingHeroAssignment()` is true, an unregistered player must be **deferred**, not defaulted. Fall back only when the component does not manage this match (returns false).

## Changing hero

`ChangeHeroForController` is Authority-only: it rewrites the record and PlayerState PawnData, then requests a **respawn next frame**. PawnClass depends on the hero and PawnData is read at spawn; there is no in-place swap. `bRestartInPlace` reuses the old pawn's location and facing, but velocity is not inherited and spawn may be nudged for a larger capsule. Constrain timing if swaps are allowed mid-match (it can bypass movement restrictions).

## Asset loading traps

- **Every new plugin directory must be added to `PrimaryAssetTypesToScan`.** Otherwise its ActionSet/Experience AssetBundles do not load and soft-referenced widgets return null from `.Get()` with **no error**: the UI just never appears. Check this first when UI is missing.
- **PawnData AssetBundles are inert.** Its PrimaryAssetType is not registered, so Fragment soft references go through `LoadSynchronous`. Do not judge load regressions by bundle config.

## Boundaries

- Assignment is fully server-authoritative; clients read PlayerState only.
- `AssignedHeroes` PawnData lifetime is held by `HeroCatalog` hard refs; that table is not GC-tracked.
- The lobby selection UI must reference the **same** `UHeroCatalog` asset as `UHeroAssignmentComponent`, or the chosen `PrimaryAssetId` is not found server-side and `FindSelectableHero` returns null.

## See also

`HeroAssignmentComponent.h`, `HeroCatalog.h`, `HeroSelectionBoardComponent.h` (broadcasts `OnBoardChanged`), `YanHeroSelectWidget.as`, `YanPawnData.h`.
