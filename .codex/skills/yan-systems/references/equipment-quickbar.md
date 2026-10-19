# Equipment and QuickBar

Read/write split and network authority for inventory, quick bar and equipment. Generic code: `Plugins/EquipManager`; input and request layer: `Plugins/YanGameCore/Script/Hero/`.

## Where components live

| Component | Host | Why |
|---|---|---|
| `UInventoryManagerComponent` | PlayerController | Survives pawn swaps |
| `UQuickBarComponent` | PlayerController | Slots are server-authoritative data |
| `UYanQuickBarRequestComponent` | Pawn | Borrows the pawn's client ownership for Server RPCs |

Controller replication lags, so every access point fetches lazily (`GetQuickBar()` re-fetches each call); never cache in `BeginPlay`.

## Authority model

`UQuickBarComponent::AddItemToSlot` is AuthorityOnly and accepts empty slots only, so UI picks go through the server:

```
Client UI pick → UYanQuickBarRequestComponent::ServerPlaceItemInQuickBar / ServerEquipItemFromQuickBar
  → server PlaceItem computes the slot → QuickBar replicates back
```

**The slot index is computed on the server; the client sends only the item instance.** The request is immune to slot-replication lag. This is the opposite of `UYanTechniqueComponent::ServerCommitRestore` (array index as RPC arg, a known race, see `reports/AngelScript代码审计.md`); follow the quick bar pattern for new RPCs. The server re-checks `IsInInventory` before placing, because the UI selection outlives the item.

## Placement order (`PlaceItem`)

1. Already in the quick bar → keep its slot.
2. `TryAddItem` picks an empty slot by item category tag.
3. Category full → `ReplaceSameCategorySlot` evicts one.

Eviction empties the slot first, then runs the component's own `CanAddItemToSlot`, so admission rules still apply. On failure, restore the item; emptying also unequips, so if the emptied slot was active, re-equip it too. Category tags come from `UInventoryFragment_ItemDisplay::SlotCategoryTags` (same on both ends). Owned items replace each other; unowned items replace only unowned ones.

## Input path

Input never writes the quick bar directly:

```
Key abilities (AS_QuickBarSetSlot / CycleForward / CycleBackward / AS_ItemWheel)
  → broadcast GameplayEvent → UAS_QuickBarInputHost → QuickBarComponent
```

`UAS_QuickBarInputHost` is an always-on ability: `ActivationPolicy = OnSpawn`, three `WaitGameplayEvent` with `bOnlyTriggerOnce = false`, never `CommitAbility`, never ends. It is the only ability here that deliberately skips cost/cooldown.

**SetSlot listens on the parent tag without exact match**; the child tag's last segment is the 0-based slot index, so a new slot needs only a new child tag. The parent tag itself is also delivered, so `ResolveSlotIndex` must check `IsNumeric` first or it will switch to index 0.

## Slot-switch policies

Both are driven by GameplayMessage and share one activation history.

- **Empty slot: no switch.** Switching would unequip without equipping.
- **Fallback on loss.** When the active item leaves the bar, return to the most recent slot that still holds an item.

History is recorded where the **active index changes** (so cycling and server-side equips count), one entry per slot, length ≤ slot count. Fallback triggers on the **slot-content message**: `RemoveItemFromSlot` clears the active index to -1 but does **not** broadcast it, so that message is the only reliable signal on both ends. Quick bar messages are broadcast per player; filter with `IsOwnQuickBar`.

## Listener unbinding

`UAS_QuickBarInputHost` is the model: `StartWatchingQuickBar` calls `StopWatchingQuickBar` first (a cancelled ability can reactivate on the same instance and would stack listeners), and `OnEndAbility` unbinds.

## Icons

Quick bar and inventory icons load asynchronously via `FDataRegistryId` + `PrimaryAssetId`/Bundle through `UEquipmentDisplaySubsystem`. They are not a synced field; UI must handle the "not ready yet" state.

## See also

`YanQuickBarRequestComponent.as`, `AS_QuickBarInputHost.as`, `UI/Combat/Equipment/`, and `ui-layer-framework.md`.
