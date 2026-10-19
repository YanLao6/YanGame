# ModularGameplayAbilities (project plugin)

`Plugins/ModularGameplayAbilities` — the project's GAS layer on top of the Modular stack (Lyra `LyraAbilitySystemComponent` / `LyraGameplayAbility` / `LyraAbilitySet` lineage). Ability and ASC bases derive from the **AngelscriptGAS** classes, so everything here is subclassable from script.

Depends on: `GameplayAbilities`, `AngelscriptGAS`, `ModularGameplayData`, `ModularGameplayExperiences`, `ModularGameplayActors`, `GameplayMessageRouter`, `ModalCamera`, `CommonGame`.

## Ownership chain

```
AModularAbilityPlayerState  ── owns ──>  UModularAbilitySystemComponent  (Owner = PlayerState)
AYanPawn ── default subobject ──>  UModularAbilityExtensionComponent  (sets Avatar = Pawn, binds ability input)
AModularAbilityPlayerController::PostProcessInput  ──>  ASC->ProcessAbilityInput()
```

- The ASC lives on the PlayerState and survives respawn. `UModularAbilityExtensionComponent::InitializeAbilitySystem(ASC, Owner)` runs during the init-state chain; subscribe with `OnAbilitySystemInitialized_RegisterAndCall` (fires immediately if already bound) / `OnAbilitySystemUninitialized_Register`.
- The extension component is an `AYanPawn` constructor subobject — **do not add it through an ActionSet `AddComponents`** (default flags do not de-duplicate; a second instance appears).
- Resolve the ASC from any object with `UVerbMessageHelpers::GetAbilitySystemComponentFromObject`. `AYanPawn` implements `IAbilitySystemInterface` forwarding to the extension component (null before init). `UModularAbilitySystemGlobals` cannot change ASC resolution — `GetAbilitySystemComponentFromActor` is static.
- `UModularAbilitySystemGlobals` (registered in `DefaultGame.ini`) only makes every GE use `FModularGameplayEffectContext`.

## UModularGameplayAbility

| Property | Meaning |
|---|---|
| `ActivationPolicy` | `OnInputTriggered` (try once on press), `WhileInputActive` (held), `OnSpawn` (auto-activate once the avatar is set) |
| `ActivationGroup` | `Independent`, `Exclusive_Replaceable` (cancelled by a newer exclusive), `Exclusive_Blocking` (blocks other exclusives); change at runtime with `ChangeActivationGroup` |
| `AdditionalCosts` | Instanced `UModularAbilityCost` (`CheckCost` / `ApplyCost`, `bOnlyApplyCostOnHit`); shipped: `UModularAbilityCost_PlayerTagStack` |
| `FailureTagToUserFacingMessages` / `FailureTagToAnimMontage` | Failure feedback, broadcast through `GameplayMessageRouter` |

Helpers: `GetModularAbilitySystemComponentFromActorInfo`, `GetControllerFromActorInfo`, `GetModularPawnFromActorInfo`, `GetHeroComponentFromActorInfo`, `SetCameraMode` / `ClearCameraMode` (ModalCamera, cleared in `EndAbility`).

Input flow: `InputTag` on the grant → `UModularAbilityExtensionComponent` binds the Pawn's `AbilityInputActions` to `Input_AbilityInputTagPressed/Released` → ASC queues specs → `ProcessAbilityInput` activates by policy. `CancelInputActivatedAbilities`, `IsInputTagActive`, `SetInputTagActive` exist on the ASC.

## Granting

All grant paths record handles and revoke only what they granted — never clear the whole ASC, Experience-level and hero-level grants coexist.

| Path | Scope | Asset / class |
|---|---|---|
| `UModularAbilitySet::GiveToAbilitySystem(ASC, &Handles, Source, Parts, GateTag)` | caller-owned | `GrantedGameplayAbilities` (Ability, Level, InputTag), `GrantedGameplayEffects`, `GrantedAttributes`; revoke with `FModularAbilitySet_GrantedHandles::TakeFromAbilitySystem` |
| `PawnDataFragment_AddAbilities` | one player, PlayerState scope, authority | soft `FModularAbilityGrant` / `FModularAttributeSetGrant` (+ init DataTable) / `FModularEffectGrant` + AbilitySets |
| `GameFeatureAction_AddAbilities` | every actor of `ActorClass` while the feature is active | same grant structs |
| `UModularGlobalAbilitySystem` (world subsystem) | every registered Modular ASC | `ApplyAbilityToAll`, `ApplyEffectToAll`, `Remove*FromAll`; ASCs register when they get an avatar |

Grant order is AttributeSet → Ability → Effect → AbilitySet, so effects find their attributes. `EModularAbilitySetGrantParts` splits a set: grant `AbilitiesOnly` early (e.g. when an item enters the quick bar, so SpecHandles reach clients first) and `PassivesOnly` when the equipment actually takes effect.

AngelScript can grant directly (`GiveAbility(Spec)` via mixin); to disable an ability, clear and re-give rather than blocking with tags.

## Ability gates

A gate is a tag under `Ability.Gate.*` written into the spec's `DynamicSpecSourceTags` (pass `GateTag` to `GiveToAbilitySystem`; written atomically with the grant). At `CanActivateAbility` every gate tag is evaluated by the providers registered on the ASC:

```cpp
class UMyComp : public UActorComponent, public IModularAbilityGateProvider
{
    //~Begin IModularAbilityGateProvider Interface
    virtual EModularAbilityGateResult QueryAbilityGate(FGameplayTag GateTag) const override;
    //~End IModularAbilityGateProvider Interface
};
ASC->RegisterAbilityGateProvider(this);   // and UnregisterAbilityGateProvider on teardown
```

- Return `NotHandled` for tags outside your namespace; the first non-`NotHandled` answer wins.
- **No provider answers → blocked** (`Ability.ActivateFail.Gate` is added to the failure tags).
- Pure query, no side effects: it runs on the client for prediction and on the server as the real gate, so read authoritative state. Shipped provider: `UQuickBarComponent` (EquipManager).
- A spec not yet found (activation in the grant frame) is not gated.

## Tag relationships

`UModularAbilityTagRelationshipMapping` — per `AbilityTag` (`Categories="Gameplay.Action"`): `AbilityTagsToBlock`, `AbilityTagsToCancel`, extra `ActivationRequiredTags` / `ActivationBlockedTags`. The ASC unions every mapping added via `AddTagRelationshipMapping` / removed via `RemoveTagRelationshipMapping`. Sources: `PawnDataFragment_TagRelationship` (per player) and `GameFeatureAction_TagRelationship` (per actor class, applied on both ends because client prediction needs it).

## Other pieces

| Piece | Use |
|---|---|
| `AddDynamicTagGameplayEffect(Tag)` / `RemoveDynamicTagGameplayEffect` | Grant a loose replicated tag through the `DynamicTagGameplayEffect` configured in `UModularAbilityData` |
| `UModularAbilityData` (path in `UModularGameplayAbilitiesConfig`) | Global defaults: SetByCaller damage / heal GEs, dynamic tag GE |
| `AModularAbilityPlayerState` stat tags | `AddStatTagStack` / `RemoveStatTagStack` / `GetStatTagStackCount` / `HasStatTag` (authority, replicated) — kills, ammo-like counters |
| `UModularAbilityUtils` | Find granted abilities by AssetTag (parent-inclusive): `FindAbilityHandleByAssetTag`, `FindAbilityHandlesByAssetTags`, class/instance AssetTag getters |
| `UAbilityVisualData` | Name + soft icon keyed by `AbilityTag` (bundle `AbilityVisual`), so UI never references ability classes |
| `UGameplayTagsAnimInstance` | `GameplayTagPropertyMap` mirrors ASC tags into AnimBP variables; call `InitializeWithAbilitySystem` |
| `UModularGamePhaseSubsystem` + `UModularGamePhaseAbility` | Tag-hierarchical match phases: starting a phase ends non-ancestor phases; `StartPhase`, `WhenPhaseStartsOrIsActive`, `WhenPhaseEnds`, `IsPhaseActive` |
| `UModularGameplayAbility_Reset` | Handles `GameplayEvent.RequestReset` |
| `GameFeatureAction_AddGameplayCuePath` | Adds cue directories while a feature is active |
| `GameFeatureAction_AddInputBinding` | Extra `UModularInputConfig` for local pawns (ability inputs → extension component, native → `UModularInputComponent`); pawn input component must be `UModularInputConfigComponent` |
| `UModularCharacterMovementComponent` | CMC variant for `ACharacter` pawns (the project's heroes use Mover instead) |

## Pitfalls

- Hooks such as `OnAvatarSet` / `OnGiveAbility` may run on the CDO; check `IsInstantiated()` before touching instance members (`ue-angelscript` / memory).
- Gameplay decisions based on movement state diverge between server and client; express them as Mover transitions instead of GAS checks (`ue-mover`).
- Executions/MMCs can be authored in AngelScript; execution parameters are filled on the GE asset.
