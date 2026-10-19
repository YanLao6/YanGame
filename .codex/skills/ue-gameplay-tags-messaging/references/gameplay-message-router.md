# GameplayMessageRouter (project plugin, AngelScript fork)

`Plugins/GameplayMessageRouter` is the Lyra message router **forked with AngelScript bindings**. It is the project's default tag-addressed message bus: C++, Blueprint and AngelScript all talk to the same `UGameplayMessageSubsystem`. Modules: `GameplayMessageRuntime` (runtime + script bindings), `GameplayMessageNodes` (editor-only K2 node). Log category `LogGameplayMessageSubsystem`.

Dependents: `ModularGameplayExperiences`, `ModularGameplayAbilities` (ability failure messages, `UModularGameplayMessageProcessor`), `EquipManager` (quick bar / inventory messages).

## Model

- **Channel** = `FGameplayTag`; **payload** = any `USTRUCT`. One channel should carry one struct type.
- **Process-local, synchronous.** `BroadcastMessage` calls every matching listener before returning. Nothing replicates: to reach other machines, replicate state or use an RPC and broadcast on each machine (see `ue-networking-replication`).
- **Global per GameInstance.** The subsystem is a `UGameInstanceSubsystem`, so every listener hears every broadcast on the channel, regardless of which player caused it. **Put an owner (Controller / PlayerState / component) in the payload and filter in the handler** — e.g. `FQuickBarSlotsChangedMessage.Owner`.
- **Match type** (`EGameplayMessageMatch`): `ExactMatch` (default) hears only the channel itself; `PartialMatch` also hears child channels (listener on `A.B` receives `A.B.C`). The broadcast walks from the channel up to its parents.
- **Type check:** the broadcast type must be the listener's type or a child of it, otherwise the subsystem logs `Struct type mismatch on channel ...` (Error) and skips that listener — it does not crash.

## AngelScript usage

Bindings live in `Source/GameplayMessageRuntime/Private/GameFramework/Bind_GameplayMessageSubsystem.cpp`; the plugin `README.md` has a minimal sample.

```angelscript
class UMyWidget : UUserWidget
{
    FGameplayMessageListenerHandle SlotsListener;

    UFUNCTION(BlueprintOverride)
    void Construct()
    {
        UGameplayMessageSubsystem Messaging = UGameplayMessageSubsystem::Get();
        SlotsListener = Messaging.RegisterListener(
            GameplayTags::EM_QuickBar_Message_SlotsChanged, // native tags (UE_DEFINE_GAMEPLAY_TAG[_STATIC]) appear in GameplayTags::
            this,
            n"HandleSlotsChanged",
            FQuickBarSlotsChangedMessage());                 // empty instance = type prototype
            // optional 5th arg: EGameplayMessageMatch::PartialMatch
    }

    UFUNCTION(BlueprintOverride)
    void Destruct()
    {
        SlotsListener.Unregister();
    }

    UFUNCTION()
    private void HandleSlotsChanged(FGameplayTag Channel, FQuickBarSlotsChangedMessage Payload)
    {
        // filter by Payload.Owner first
    }
}

// sender
FMyPayload Data;
UGameplayMessageSubsystem::Get().BroadcastMessage(GameplayTags::My_Channel, Data);
```

Bound script API (variadic `const ?&in` carries the struct):

| Signature | Notes |
|---|---|
| `UGameplayMessageSubsystem::Get()` | AngelScript's generated subsystem accessor; no argument |
| `bool UGameplayMessageSubsystem::HasInstance(const UObject WorldContextObject)` | |
| `FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, UObject Object, const FName& FunctionName, const ?&in EmptyStruct, EGameplayMessageMatch MatchType = ExactMatch)` | |
| `void BroadcastMessage(FGameplayTag Channel, const ?&in MaybeStruct)` | Non-USTRUCT argument logs a warning and does nothing |
| `void UnregisterListener(FGameplayMessageListenerHandle Handle)` / `Handle.Unregister()` | |

Rules enforced by the binding:

- The handler **must be a `UFUNCTION()`** (found via `FindFunction`); otherwise `FAngelscriptManager::Throw("Could not find function ...")`.
- Signature is **exactly** `void(FGameplayTag, <prototype struct>)`: two parameters, first CPP type `FGameplayTag`, second CPP type equal to the prototype's. Anything else throws with the expected vs. attempted signature.
- The listener holds a **weak** pointer to `Object`; when the object dies the next broadcast unregisters it. Still call `Unregister()` in `Destruct` / `EndPlay` / `OnRemoveAbility` (project convention: `AS_QuickBarInputHost.as`, `YanQuickBarWidget.as`).
- The thunk copies the sender's struct bytes into the script parameter. From script, **broadcast exactly the listener's struct type**, not a child struct.

Do **not** use `UAsyncAction_ListenForGameplayMessage` from script: `ListenForGameplayMessages` is `BlueprintInternalUseOnly` (skipped by `Bind_BlueprintCallable.cpp` unless it has `UsableInAngelscript`), and `GetPayload` is a wildcard `CustomThunk` that cannot be bound. That node is for Blueprint graphs only.

### Payload struct gotcha

A `TArray<TObjectPtr<T>>` member is seen as `TObjectPtr<T>` elements in script; it cannot share a ternary with a `nullptr` literal. Read the element into a `T` handle first.

### Verifying API availability

`angelscript_get_api` may list only the static functions of this class (it omitted `RegisterListener`/`BroadcastMessage`), and `Script/.vscode/as.debugdatabase` can be months stale. Neither disproves an API; only a hot-reload compile does (`ue-angelscript`). Script reports both "does not exist" and "signature mismatch" as `No matching signatures`; add a deliberately bogus function name as a control.

## C++ usage

```cpp
#include "GameFramework/GameplayMessageSubsystem.h"

UGameplayMessageSubsystem& Router = UGameplayMessageSubsystem::Get(this);
Router.BroadcastMessage(TAG_My_Channel, Payload);

// lambda form, or member form RegisterListener(Channel, this, &ThisClass::OnMsg)
FGameplayMessageListenerHandle Handle = Router.RegisterListener<FMyPayload>(TAG_My_Channel,
    [this](FGameplayTag Channel, const FMyPayload& Msg) { /*...*/ });
Handle.Unregister();
```

`FGameplayMessageListenerParams<T>` adds `MatchType` and `SetMessageReceivedCallback`. Blueprint uses `Broadcast Message` (`K2_BroadcastMessage`) and the `Listen For Gameplay Messages` async node.

## Before driving UI from a channel

Check that the channel is broadcast on the machine where the UI lives:

- `EM.QuickBar.Message.*` — reliable on server and clients; `QuickBarComponent` calls its `OnRep_*` manually in `SetActiveSlotIndex` / `AddItemToSlot` / `RemoveItemFromSlot`.
- `EM.Inventory.Message.StackChanged` — **client only**: `FInventoryList::AddEntry/RemoveEntry` only `MarkItemDirty`; the broadcasts live in `PostReplicatedAdd/Change` and `PreReplicatedRemove`. A listen server or standalone PIE never hears it; add a server-side broadcast before relying on it.
- DataRegistry icon readiness sends no message; message-driven item UI needs its own retry (`yangame-systems` → equipment-quickbar).

## Related

- Payload helpers for verb-style events (`FModularVerbMessage`, `UVerbMessageHelpers`) and the server-side combinator base `UModularGameplayMessageProcessor`: `ue-game-features/references/modular-experiences.md`.
- Engine-side alternative with threading and endpoints: [gameplay-messaging.md](gameplay-messaging.md) (`AsyncMessageSystem`).
