# ModularGameplayUI (project plugin)

`Plugins/ModularGameplayUI` — CommonUI / CommonGame based UI building blocks for the Modular stack: Experience- and PawnData-driven widget injection, the gameplay layout with Escape handling, dialogs, list/tab/button bases, the world-space indicator system and a frontend flow component. Project screens built on top of it (layer stack, back action, lifecycle rules) are in `yangame-systems/references/ui-layer-framework.md`.

Depends on: `CommonUI`, `CommonGame`, `CommonUser`, `CommonLoadingScreen`, `UIExtension`, `ControlFlows`, `EnhancedInput`, `AsyncMixin`, and the other Modular plugins (Data, Experiences, Abilities, Actors). `EquipManager` depends on it one-way (indicators).

## Two ways to put widgets on screen

| Need | Use | Why |
|---|---|---|
| Full-screen / modal screen (menu, settings, HUD root layout) | Push to a CommonUI layer: `Layout` entry (`LayerID` `UI.Layer.*` + `LayoutClass`) | `UCommonActivatableWidgetStack` shows only its top widget |
| Element overlaid on the HUD (bars, indicators, spectator strip) | UIExtension slot: `Widgets` entry (`SlotID` + `WidgetClass`), slot tags `HUD.Slot.*` | Stacked pushes would hide the HUD; the extension point content root must not be a `CanvasPanel` |

Both entry kinds exist in two injectors:

- **`GameFeatureAction_AddWidget`** (Experience / ActionSet) — extends `AModularGameplayHUD` via the component manager; widgets live while the feature is active. Soft classes carry bundle `Client`.
- **`PawnDataFragment_AddWidgets`** (hero PawnData) — Pawn scope, runs only where a LocalPlayer exists (no-op on dedicated server and other clients), revoked with the pawn. Entries also have a slot `Priority`.

The owning plugin directory must be in `PrimaryAssetTypesToScan`, otherwise the soft class resolves to null and the widget silently never appears.

## Layout and HUD

| Class | Role |
|---|---|
| `AModularGameplayHUD` | HUD actor targeted by `AddWidget`; implements `IModularLayoutInterface`; optional Overlay widget + `UModularWidgetController` (`InitOverlay`) |
| `UModularGameplayLayout` | Base for the HUD layout pushed to `UI.Layer.Game`; binds `UI.Action.Escape` in `NativeOnInitialized` and pushes `EscapeMenuClass` to `UI.Layer.Menu` |
| `UModularInputActivatableWidget` | Activatable widget with `InputConfig` (`Default` / `GameAndMenu` / `Game` / `Menu`) and `GameMouseCaptureMode`; warns at compile time if gamepad focus target is missing |
| `UModularUserWidget` | Activatable widget holding a `WidgetController` (`SetWidgetController` → `WidgetControllerSet` event) |
| `UModularWidgetController` | MVC-style controller fed with `FWidgetControllerParams` (PC, PS, ASC, AttributeSet); `BroadcastInitialValues`, `BindCallbacksToDependancies` |
| `UModularTaggedWidget` | Switches to `HiddenVisibility` while the owning PC's pawn ASC (via `IAbilitySystemInterface`) has any `HiddenByTags` (hierarchical match); restores the caller's last `SetVisibility` request when the tags clear. Rebinds on `OnPossessedPawnChanged` and on `UModularAbilityExtensionComponent::OnAbilitySystemInitialized` (client possess can precede ASC init). No pawn / no ASC counts as "no tags"; empty `HiddenByTags` subscribes nothing. `HiddenByTags` is read once at construct — runtime edits do not rebind |

## Dialogs

`UModularUIMessaging` (`UCommonMessagingSubsystem`) pushes confirmation / error dialogs to the Modal layer. Classes come from config (`ConfirmationDialogClass`, `ErrorDialogClass` in `DefaultGame.ini`); without this subsystem the base implementation silently drops dialog requests. `UModularConfirmationScreen` builds buttons from the descriptor; its WBP must contain the `BindWidget` children and set the cancel action explicitly.

## Lists, tabs, buttons

- `UModularListView` + `UModularWidgetFactory` / `UModularWidgetFactory_Class` — pick the entry widget per data item class (walks the class hierarchy); used for mixed lists (sessions, experiences).
- `UModularTabListWidgetBase` / `UModularTabButtonBase` / `IModularTabButtonInterface`, `UModularButtonBase`, `UModularBoundActionButton`.
- `UEnhancedInputActionWidget` — `UCommonActionWidget` showing the icon for an `UInputAction` under current mappings.
- `UModularGameplayUIUtils::GetKeyBrush(PC, Key, OutBrush)` — key icon brush for the current input device with an explicit PlayerController (works for dynamically created children whose owning player is missing). For the key itself use `UModularExperienceUtils::GetKeyValueByTag`.

## Indicator system

World-anchored screen widgets (pickups, objectives, nameplates).

```cpp
UIndicatorManagerComponent* Mgr = UIndicatorManagerComponent::GetComponent(Controller); // Controller component
UIndicatorDescriptor* Ind = NewObject<UIndicatorDescriptor>();
Ind->SetSceneComponent(Comp);
Ind->SetComponentSocketName(Socket);
Ind->SetIndicatorClass(WidgetClass);   // soft class
Ind->SetDataObject(Data);
Mgr->AddIndicator(Ind);                // RemoveIndicator / Ind->UnregisterIndicator() to remove
```

- A `UIndicatorLayer` widget (place one in the HUD) renders all indicators through `SActorCanvas`.
- Widgets should implement `IIndicatorWidgetInterface` (`BindIndicator` / `UnbindIndicator`).
- Descriptor options: `SetProjectionMode` (`EActorCanvasProjectionMode`), H/V alignment, `SetClampToScreen`, `SetAutoRemoveWhenIndicatorComponentIsNull`, `SetDesiredVisibility`, `SetPriority`.
- `UIndicatorLibrary::GetIndicatorManagerComponent(Controller)` for Blueprint/script.

## Frontend flow

`UModularGameplayUIStateComponent` (GameState component, injected by the frontend Experience) runs a ControlFlow after Experience load: wait for user init → Press Start (`PressStartScreenClass`) → join requested session → main screen (`MainScreenClass`). It keeps the loading screen up until the flow finishes. The frontend only works in Standalone PIE.

## Other

- `GameFeatureAction_SplitscreenConfig` — vote-counted splitscreen disable.
- AngelScript widgets cannot use `BindWidgetAnimOptional`; optional animations go through `Transient` + meta passthrough (`ue-angelscript`).
