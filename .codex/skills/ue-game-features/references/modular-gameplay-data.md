# ModularGameplayData (project plugin)

`Plugins/ModularGameplayData` — the bottom layer of the Modular stack: PawnData and its Fragments, the asset manager, InputConfig, a world-scoped GameFeatureAction base, cosmetic character parts and tagged physical materials. It knows nothing about Experiences or GAS; those layers add their own Fragments.

Depends on: `ModularGameplay`, `ModularGameplayActors`, `GameFeatures`, `EnhancedInput`, `CommonUI`. Depended on by every other Modular plugin and `EquipManager`.

## PawnData = PawnClass + Fragments

`UModularPawnData` (const asset) holds only `PawnClass`; **every capability is a Fragment**. Project heroes use `UYanPawnData`, which adds no fields. Add a Fragment for a new capability, never a PawnData field.

- `FragmentSets` (`UModularPawnDataFragmentSet`, reusable, one level only — sets cannot reference sets) are applied first, then the inline `Fragments`. Revoke runs in reverse.
- `FragmentSets` has `IncludeAssetBundles=true`, because AssetBundles metadata is not collected across asset references.
- Query: `FindFragment<T>()` (first match) / `FindFragments<T>()`.
- PawnData's own AssetBundles are inert (its PrimaryAssetType is not registered); Fragment soft refs effectively load synchronously.

### Writing a Fragment

```cpp
UCLASS(MinimalAPI, meta = (DisplayName = "My Capability"))
class UPawnDataFragment_MyCapability : public UPawnDataFragment
{
    GENERATED_BODY()
public:
    //~Begin UPawnDataFragment Interface
    virtual EPawnDataFragmentScope GetScope() const override { return EPawnDataFragmentScope::Pawn; }
    virtual bool RequiresAuthority() const override { return true; }
    virtual bool AllowsMultipleInstances() const override { return false; }
    UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
    UE_API virtual void Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
    //~End UPawnDataFragment Interface
};
```

Contract:

- **Stateless asset.** `Apply` must not write members; return a `UPawnDataFragmentState` subclass holding handles/created objects. The applier owns it and passes it back to `Revoke`. The same PawnData is shared by many players.
- **Scope** decides lifetime: `PlayerState` and `Controller` survive respawn, so `Revoke` must undo explicitly; `Pawn` is reclaimed with the pawn. Player-persistent hosts (inventory, quick bar) belong on `Controller`.
- **`RequiresAuthority`**: true for granting abilities / creating replicated components; false for input bindings and UI that must run on the owning client.
- **`AllowsMultipleInstances`** must return false for anything queried with `FindFragment` (a duplicate in a set would silently win over the inline specialisation).
- `FPawnDataFragmentContext`: `TargetActor`, `Pawn` (may be null for PlayerState scope), `Controller`.
- Revoke by recorded handles, never by clearing everything — Experience-level grants share the same targets.

Appliers: `AModularExperiencePlayerState` (PlayerState + Controller scope) and `UModularPawnComponent` (Pawn scope), both in `ModularGameplayExperiences`.

### Shipped Fragments

| Fragment | Plugin | Scope | Authority |
|---|---|---|---|
| `PawnDataFragment_AddComponents` | Data | configurable (default Pawn) | no — each entry has `bServerComponent` / `bClientComponent`, created per end without component replication |
| `PawnDataFragment_InputConfig` | Data | Pawn | no; single instance. Holds `InputConfig`, `AdditionalAbilityInputConfigs`, `InputMappings`. Not applied by itself: `UModularInputComponent` / `UModularAbilityExtensionComponent` query it so registration stays in step with `ClearAllMappings` |
| `PawnDataFragment_AddAbilities` | Abilities | PlayerState | yes |
| `PawnDataFragment_TagRelationship` | Abilities | PlayerState | no |
| `PawnDataFragment_AddWidgets` | UI | Pawn | no; no-op without a LocalPlayer |

## Asset manager

`UModularAssetManager` (the project's `AssetManagerClassName` is its subclass `UYanAssetManager`):

- `GetAsset(TSoftObjectPtr)` / `GetSubclass(TSoftClassPtr)` — synchronous resolve, optionally kept in memory.
- `GetModularGameData()` (config `ModularGameDataPath`) and `GetDefaultPawnData()` (config `DefaultPawnData`, last-resort PawnData).
- `GetOrLoadTypedGameData<T>(DataPath)` — lazily loads a `UPrimaryDataAsset` subclass into `GameDataMap`; failure is **Fatal**.
- Startup jobs run in `StartInitialLoading`; `DumpLoadedAssets` logs tracked assets.

Bundle names: `FModularBundles::Equipped`. Every new GameFeature directory must be in its GameFeatureData `PrimaryAssetTypesToScan`, otherwise soft-referenced widgets/actions resolve to null with no error.

## Input

- `UModularInputConfig`: `NativeInputActions` and `AbilityInputActions` — arrays of `{InputAction, InputTag}` (`Categories="InputTag"`). `FindNativeInputActionForTag` / `FindAbilityInputActionForTag`.
- `UModularInputConfigComponent` (subclass of `UEnhancedInputComponent`, set as the Pawn's input component class): `BindNativeAction`, `BindAbilityActions`, `RemoveBinds`, `AddInputMappings`. It registers every active InputConfig (ref-counted) so `FindInputActionForTag` can reverse-look-up — Ability table wins over Native on duplicates. Runtime-injected configs require this component class.
- `UModularSettingBasedScalar` — input modifier scaling each axis by a named `double` property on the shared `ULocalPlayerSaveGame`, clamped.
- `GameFeatureAction_AddInputContextMapping` — adds IMCs to local players during activation; `bRegisterWithSettings` registers them with `UEnhancedInputUserSettings` at GameFeature registration (needed for key rebinding UI).
- IMC key mappings live in `DefaultKeyMappings`; the old `Mappings` field reads empty — writing it back wipes bindings.

## GameFeatureAction base

`UGameFeatureAction_WorldActionBase` — subclass and implement `AddToWorld(const FWorldContext&, const FGameFeatureStateChangeContext&)`; the base handles worlds that already exist and GameInstances that start later. Keep per-context data in a `TMap<FGameFeatureStateChangeContext, FPerContextData>` and reset it in `OnGameFeatureDeactivating` (all shipped actions follow this).

## Cosmetics

- `UModularControllerComponent_CharacterParts` (Controller) — the *requested* parts; follows the controller across respawn / hero swap. Server-only `AddCharacterPart` / `RemoveCharacterPart` / `RemoveAllCharacterParts`.
- `UModularPawnComponent_CharacterParts` (Pawn) — replicated FastArray of applied parts; every client spawns `UChildActorComponent`s, swaps body mesh by part tags (`GetCombinedTags`), broadcasts `OnCharacterPartsChanged` (team colours subscribe here).
- Parts attach to `ACharacter::Mesh`, otherwise the **first** skeletal mesh component — a pawn with two meshes must declare the main one first.
- Child actors get no owner by default; enable `SetChildActorOwnerOnCreation` where `OwnerNoSee` matters.
- `UModularCosmeticDeveloperSettings` + `ModularCosmeticCheats` override parts in development builds.

## Misc

`UPhysicalMaterialWithTags` — physical material with a `Tags` container for surface-type logic (footsteps, impacts).
