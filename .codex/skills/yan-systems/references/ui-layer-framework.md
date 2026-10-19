# UI Layer Framework

Layered widget stacks on CommonUI. Every screen is pushed onto a stack; nothing calls `AddToViewport` directly. Code: `Plugins/YanGameCore/Script/UI/`.

## Layers

`UYanPrimaryLayout` (extends `UPrimaryGameLayout`) registers four `UCommonActivatableWidgetStack` to tags in `PreConstruct`:

| Tag | Bound variable | Holds |
|---|---|---|
| `UI.Layer.Game` | `GameLayer_Stack` | In-game HUD |
| `UI.Layer.GameMenu` | `GameMenu_Stack` | Pause menu |
| `UI.Layer.Menu` | `Menu_Stack` | Frontend, Press Start, main menu |
| `UI.Layer.Modal` | `Modal_Stack` | Confirm and error dialogs |

## Preconditions

- `B_YanUIPolicy.LayoutClass` points to a UMG blueprint derived from `UYanPrimaryLayout`.
- The four stack widgets keep the variable names above (`BindWidget` matches by name).
- The four layer tags are registered.

## Pushing

- **Code:** `UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, LayerTag, WidgetClass)` (e.g. main menu sub-pages).
- **GameFeatureAction:** `GameFeatureAction_AddWidgets` pushes `UYanHUDLayout` to `UI.Layer.Game`; lifetime follows the Experience. Its soft widget class needs the plugin directory in `PrimaryAssetTypesToScan`, else `.Get()` is null and the widget silently never shows (see `experience-pawndata.md`).

## Back action

CommonUI routes Back to the top widget that declares itself a back handler:

```as
default bIsBackHandler = true;
default bIsBackActionDisplayedInActionBar = true;

UFUNCTION(BlueprintOverride)
bool OnHandleBackAction()
{
    DeactivateWidget();
    return true;   // consumed
}
```

Screens that ESC must not close (main menu) set `bIsBackHandler = false`. `UYanHUDLayout` is not a back handler: `UModularGameplayLayout` registers `UI.Action.Escape` in `NativeOnInitialized` and pushes `EscapeMenuClass` to `UI.Layer.Menu`.

## Lifecycle conventions

- **Anything started is released in `Destruct`.** Looping timers and delegates bound to long-lived objects. Combat widgets (HealthBar, ImpulseBar, Minimap, Inventory, QuickBar, ItemWheel, TechniqueWheel) start a timer in `Construct` and call `ClearAndInvalidateTimerHandle` in `Destruct`.
- **Unbind delegates on GameInstance subsystems.** Widgets are created on push and destroyed after `DeactivateWidget`, while subsystems live as long as the process; otherwise each open leaves a dead binding. Bind in `Construct`, `Delegate.UnbindObject(this)` in `Destruct`. The Session group (`YanMainMenuWidget`, `YanHostSessionWidget`, `YanSearchSessionWidget`, `YanStartScreenWidget`) already does this.
- Delegates on the widget's own children (button `OnClicked`, ListView selection) need no unbinding.
- `PreConstruct` and `Construct` each run once at runtime, back to back (re-entry paths are `WITH_EDITOR` only). `PreConstruct` needs an `if (IsDesignTime) return;` guard.

## Attribute reads

UMG has no GAS attribute change delegate here; bars poll `GetGameplayAttributeValue` on a loop timer started in `Construct`, paced by each widget's `UpdateInterval`.

## See also

`YanPrimaryLayout.as`, `YanHUDLayout.as`, `YanGameMenuWidget.as`, `YanStartScreenWidget.as`, `UI/Session/`.
