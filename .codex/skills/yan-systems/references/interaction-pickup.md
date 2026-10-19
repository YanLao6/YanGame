# Interaction / Pickup / Indicator Framework

Lyra-derived flow: approach a world item → on-screen indicator → key press → item enters inventory. Spans `EquipManager` and `ModularGameplayUI`; C++ supplies mechanics, assembly happens in editor/AngelScript.

## Capabilities

Periodic spherical scan around the pawn; interactables carry their own ability, granted dynamically when near; pickup writes the item list into the inventory component; interactables project to screen-space indicators (edge clamping with arrow).

## Layout

Dependency is one-way: `EquipManager → ModularGameplayUI` (one Build.cs entry, one .uplugin entry).

| Class | Module | Role |
|---|---|---|
| `UGameplayAbility_Interact` | EquipManager | Always-on (OnSpawn) ability; starts scan tasks, `UpdateInteractions` refreshes indicators, `TriggerInteraction` fires pickup. **Abstract, needs a BP subclass** |
| `UAbilityTask_GrantNearbyInteraction` | EquipManager | Scans the Interaction channel, grants carried abilities |
| `UAbilityTask_WaitForInteractableTargets_SingleLineTrace` | EquipManager | Single-line trace, broadcasts option changes |
| `IInteractableTarget` / `IPickupable` | EquipManager | C++-only interfaces (see below) |
| `UPickupableStatics::AddPickupToInventory` | EquipManager | The pickup action; `BlueprintAuthorityOnly` |
| `AWorldCollectable` | EquipManager | World item base; collision defaults to Overlap on Interaction |
| `UGameplayAbility_Pickup` | EquipManager | `ServerOnly`; takes the item from `TriggerEventData->Target` |
| `UIndicatorManagerComponent` / `UIndicatorDescriptor` / `UIndicatorLayer` | ModularGameplayUI | Controller-side manager / indicator data / screen projection canvas |

Pickup chain: `GA_Interact` (OnSpawn) → `GrantNearbyInteraction` scan → `GatherInteractionOptions()` → `GiveAbility`; `WaitForInteractableTargets` → `UpdateInteractions` → `UIndicatorDescriptor`; key → `TriggerInteraction` → `GA_Pickup` → `AddPickupToInventory` → `InventoryManagerComponent.AddItemDefinition()`.

## Constraints

- **Interfaces are C++-only.** `CannotImplementInterfaceInBlueprint` and non-UFUNCTION methods mean neither Blueprint nor AngelScript can implement them; world item bases must derive from `AWorldCollectable`.
- **Dedicated channel.** `ECC_GameTraceChannel1` is named `Interaction` in `Config/DefaultEngine.ini` (default Ignore); constant `EquipManager_TraceChannel_Interaction` in `Interaction/InteractionChannels.h`. Interactables must Overlap it.
- **Pickup is server-side** (`BlueprintAuthorityOnly`, `ServerOnly` + `IsNetAuthority()`).
- **Components sit on the PlayerController** (`InventoryManagerComponent`, `IndicatorManagerComponent` are `UControllerComponent`) so they outlive pawn death.
- **C++ gives bricks, Blueprint orchestrates:** the `GA_Interact` BP subclass wires detection task and input event to `UpdateInteractions`/`TriggerInteraction`.

## Assembly checklist

1. PlayerController gets `UInventoryManagerComponent` + `UIndicatorManagerComponent`.
2. `BP_GA_Interact` (subclass of `UGameplayAbility_Interact`) goes into the pawn's `ABS_DefaultAbilitySet`: `WaitForInteractableTargets_SingleLineTrace` → `InteractableObjectsChanged` → `UpdateInteractions`; interact key → `TriggerInteraction`.
3. Pickup uses `UGameplayAbility_Pickup` directly (or a BP subclass).
4. HUD hosts `UIndicatorLayer` (set `ArrowBrush`) plus a hint widget implementing `IIndicatorWidgetInterface`.
5. `BP_Collectable` (subclass of `AWorldCollectable`): `Option.InteractionAbilityToGrant = UGameplayAbility_Pickup`, `StaticInventory.Templates += {ItemDef, count}`, a visible mesh.
6. PIE check: approach → indicator → key → inventory +1.

## Extension

Armed characters use the full Inventory → QuickBar → Equipment chain (see `equipment-quickbar.md`); the commented-out `AbilitySetsToGrant` in `EquipmentDefinition` must be restored with `UModularAbilitySet` in place of `ULyraAbilitySet`.
