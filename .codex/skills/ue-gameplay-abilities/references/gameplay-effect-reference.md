# GameplayEffect Reference

Everything a `UGameplayEffect` can express, organised the way the editor's details panel is laid out,
followed by the C++ extension points. Target engine: UE 5.8. Headers: `GameplayEffect.h`,
`GameplayEffectTypes.h`, `GameplayEffectComponent.h`, `GameplayEffectComponents/*.h`.
Runtime behaviour quoted below was read from `GameplayEffect.cpp`, `GameplayEffectAggregator.cpp`,
`AbilitySystemComponent.cpp` and `GameplayEffectComponents/*.cpp`.

---

## Mental model

A GE is a **data-only Blueprint asset** (`UCLASS(Blueprintable)` only so children can template a
parent; the class comment says GEs "should NOT contain blueprint graphs"). There is no event graph to
script. All behaviour comes from four places:

1. **Fields on the GE** — duration, period, modifiers, executions, cues, stacking.
2. **GE Components** (`GEComponents`, shown as "Components") — instanced subobjects that own tags,
   immunity, granted abilities, chained effects, application gates.
3. **Pluggable classes referenced by the GE** — `UGameplayModMagnitudeCalculation` (MMC),
   `UGameplayEffectExecutionCalculation`, `UGameplayEffectCustomApplicationRequirement`,
   `UGameplayEffectUIData`. These are where logic lives.
4. **Per-application data on the spec** — level, SetByCaller values, dynamic tags, duration and
   stack-count overrides, set by whoever applies the GE.

Terminology used by the engine (and below):

| Term | Meaning |
|---|---|
| Apply | Generic: the GE passed all gates. Covers both Add and Execute |
| Add | A Duration/Infinite GE enters the ASC's active container |
| Execute | Permanent write to **base** values. Only `Instant` GEs and each **period tick**, and only on the server |
| Inhibit | An added GE stays in the container but is switched off: modifiers, granted tags, ability blocks, granted abilities and active cues are removed, and periodic ticks are skipped. The duration timer keeps running |

Client-predicted `Instant` GEs are added locally as if infinite until the server confirms, so on the
predicting client they touch the current value only and executions do not run there.

### What Blueprint-only authoring can and cannot do

| Can be done purely in the GE asset | Needs a class (C++ or script) |
|---|---|
| Instant / timed / infinite effects; periodic DoT/HoT; max duration cap | Formulas over several attributes → MMC or Execution |
| Any of the 6 modifier ops on any attribute, with curve-table scaling by level | Outputs to more than one attribute from one formula → Execution |
| Magnitude from another attribute of source or target, snapshot or live | Custom apply gate beyond tags/chance → `UGameplayEffectCustomApplicationRequirement` |
| Per-modifier tag conditions ("+20% vs undead" style conditional mods) | UI payload beyond plain text → `UGameplayEffectUIData` subclass |
| SetByCaller magnitudes and durations filled at apply time | New component behaviour → `UGameplayEffectComponent` subclass (native only) |
| Granting tags, blocking/cancelling abilities, granting abilities | Reacting to the effect in gameplay code → ASC delegates or `PostGameplayEffectExecute` |
| Immunity, removing other effects, chance to apply, tag gates | |
| Chaining effects on apply / normal end / premature end; overflow effects | |
| Stacking per source or per target, with refresh/extend/decay rules | |
| Cues with level- or attribute-normalised magnitude | |
| Evaluation channels (layered formulas) once enabled in project settings | |
| Scaling every effect's duration via the ASC system attributes | |

MMC, Execution and CustomApplicationRequirement are `Blueprintable` with `BlueprintNativeEvent`
entry points, so they can be Blueprint or AngelScript classes. A pure-Blueprint Execution is of
limited use: `FGameplayEffectCustomExecutionParameters` exposes no Blueprint helpers to read captures
or emit output modifiers. In this project write Executions in AngelScript or C++.

---

## Details panel map

| Category (panel order) | Fields |
|---|---|
| Status | `EditorStatusText` — live validation result ("All Ok" / first warning or error) |
| Duration | `DurationPolicy`, `DurationMagnitude`, `MaxDurationMagnitude` |
| Duration → Period | `Period`, `bExecutePeriodicEffectOnApplication`, `PeriodicInhibitionPolicy` |
| GameplayEffect | `GEComponents` ("Components"), `Modifiers`, `Executions` |
| GameplayCues | `bRequireModifierSuccessToTriggerCues`, `bSuppressStackingCues`, `GameplayCues` |
| Stacking | `StackingType`, `StackLimitCount`, `StackDurationRefreshPolicy`, `StackPeriodResetPolicy`, `StackExpirationPolicy`, `bFactorInStackCount` |
| Stacking → Overflow | `OverflowEffects`, `bDenyOverflowApplication`, `bClearStackOnOverflow` |

Fields under `Deprecated` (`InheritableOwnedTagsContainer`, `ConditionalGameplayEffects`,
`ChanceToApplyToTarget_DEPRECATED`, `GrantedAbilities`, `UIData`, …) are cleared by `PreSave` on
any asset at version `Modular53` or later. Writing them reads back fine and saves "successfully" but
lands empty on disk. Use the matching component.

Defaults from the constructor: `Instant`, `bExecutePeriodicEffectOnApplication = true`,
`PeriodicInhibitionPolicy = NeverReset`, `StackingType = None`, `StackLimitCount = 0`,
`RefreshOnSuccessfulApplication`, `ResetOnSuccessfulApplication`,
`bRequireModifierSuccessToTriggerCues = true`, `bFactorInStackCount = true`.

---

## Duration

| `EGameplayEffectDurationType` | Behaviour |
|---|---|
| `Instant` | Executes once against the **base** value; never enters the active container; fires `PostGameplayEffectExecute` |
| `HasDuration` | Added; modifiers aggregate onto the **current** value until `DurationMagnitude` seconds elapse |
| `Infinite` | Added until removed explicitly (by handle, query, tag requirements, another GE's removal component) |

`DurationMagnitude` is a full `FGameplayEffectModifierMagnitude` (see [Magnitude](#modifier-magnitude)),
so a duration can be a constant, a curve by level, derived from an attribute, computed by an MMC, or
SetByCaller.

Duration is resolved **at application time** on the target (it may depend on captured target
attributes) in this order:

1. Evaluate `DurationMagnitude` (SetByCaller is read from the spec).
2. Apply duration modifiers: the source ASC's `OutgoingDuration` (snapshot at spec creation) and the
   target ASC's `IncomingDuration` (live). These are system attributes on
   `UAbilitySystemComponent`; they appear in a modifier's attribute picker as
   `AbilitySystemComponent.OutgoingDuration` / `IncomingDuration`. An infinite GE with
   `MultiplyAdditive 0.7` on `IncomingDuration` shortens every timed effect applied to that actor by 30%.
3. Add carried-over time if stacking with `ExtendDuration`.
4. A result `<= 0` is clamped to 0.1 s with an error log (a timed GE cannot be modded into instant/infinite).
5. Clamp to `MaxDurationMagnitude` when it evaluates `> 0`. The newest spec's max wins.

A spec-level override (`SetDuration` in Blueprint / `FGameplayEffectSpec::SetDuration`) replaces step 1.

---

## Period

`Period` is a plain `FScalableFloat` (constant or curve by level) — **not** a magnitude, so it cannot
be SetByCaller or attribute-driven. `0` means not periodic. Shown for `HasDuration` and `Infinite`.

| Field | Behaviour |
|---|---|
| `Period` | Each tick runs the effect as an **Execute**: modifiers write the base value, executions run, `PostGameplayEffectExecute` fires, `OnPeriodicGameplayEffectExecuteDelegateOnSelf` broadcasts. Server only |
| `bExecutePeriodicEffectOnApplication` | First tick on the next frame after application instead of after one full period |
| `PeriodicInhibitionPolicy` | When the GE becomes un-inhibited: `NeverReset` (timer kept running), `ResetPeriod` (next tick a full period later), `ExecuteAndResetPeriod` (tick now, then reset) |

A periodic GE's modifiers never sit in the aggregator — they are applied only as executions. An
`Infinite` GE with a period is the standard "regenerate forever" pattern. Ticks are skipped while
inhibited; the timer itself is not paused.

---

## Modifiers

`Modifiers` is an array of `FGameplayModifierInfo`, one row per attribute change:

| Field | Meaning |
|---|---|
| `Attribute` | Target attribute. Attributes tagged `meta=(HideFromModifiers)` are hidden from the picker (editor filter only). ASC system attributes are listed too |
| `ModifierOp` | One of the six ops below |
| `ModifierMagnitude` | How the number is produced ([Magnitude](#modifier-magnitude)) |
| `EvaluationChannelSettings` | Which channel the mod lands in ([Evaluation channels](#evaluation-channels)) |
| `SourceTags` / `TargetTags` | `FGameplayTagRequirements` gate for this single row: `Must Have Tags`, `Must Not Have Tags`, `Query Must Match` |

A modifier on an attribute whose `UAttributeSet` the target does not own is silently skipped.

### Ops and the aggregation formula

Per channel, qualifying mods are combined as

```
((Base + ΣAddBase) * MulAdd / DivAdd * ΠMulCompound) + ΣAddFinal
```

| Op (display name) | Bias | Combination rule | Two mods of 1.5 → |
|---|---|---|---|
| `AddBase` ("Add (Base)") | 0 | Sum | +3 |
| `MultiplyAdditive` ("Multiply (Additive)") | 1 | `1 + Σ(m − 1)` | ×2.0 |
| `DivideAdditive` ("Divide (Additive)") | 1 | `1 + Σ(m − 1)`; a sum of 0 is replaced by 1 with a warning | ÷2.0 |
| `MultiplyCompound` ("Multiply (Compound)") | 0 | Product | ×2.25 |
| `AddFinal` ("Add (Final)") | 0 | Sum, after all multiplication | +3 |
| `Override` | — | The **first qualifying** override in the aggregator's list wins and short-circuits the channel | — |

So "+50% damage" is `MultiplyAdditive 1.5`, and two such buffs give +100%, not +125%. Use
`MultiplyCompound` when buffs should compound. The override list is in add order but removals
swap-remove, so with several active overrides on one attribute the winner is not reliably the newest
or the oldest. Keep at most one active override per attribute and channel (e.g. gate them with
immunity or `RemoveOther`).

`EGameplayModOp` is a namespaced enum (`TEnumAsByte<EGameplayModOp::Type>`). `Additive`,
`Multiplicitive` and `Division` are hidden back-compat aliases of `AddBase`, `MultiplyAdditive`,
`DivideAdditive`. `Override` is value 3 for serialisation reasons and `MultiplyCompound` is 4.

### Instant vs timed: what the same row does

| | Instant / periodic tick | HasDuration / Infinite (non-periodic) |
|---|---|---|
| Writes | Base value, permanently | Current value, while active |
| Row tag requirements | Checked once against captured source/target **actor** tags at execution | Re-checked on every aggregator evaluation against the tags of **whoever evaluates** (see below) |
| Non-snapshot captures | Read once | Linked: the magnitude recalculates when the backing attribute changes |
| Attribute set hook | `PreGameplayEffectExecute` / `PostGameplayEffectExecute` | `PreAttributeChange` / `PostAttributeChange` |

Row tag requirements on timed GEs are **conditional modifiers**, not on/off switches for the
attribute itself. When the ASC recomputes the attribute's current value it evaluates with **empty**
source/target tags (`OnAttributeAggregatorDirty`), so a row with `Must Have Tags` never shows up in
the attribute's current value and a row with only `Must Not Have Tags` always does. The tags matter
when another GE, MMC or Execution **captures** that attribute: the capture evaluates with that spec's
source/target tags. Example: an infinite GE adds `+20% AttackPower` with target `Must Have
Enemy.Type.Undead`; the damage execution capturing `AttackPower` sees it only when hitting undead.
To switch a whole buff on/off by the owner's tags, use `OngoingTagRequirements`
([components](#gameplayeffect-components)) instead.

### Evaluation channels

Ten channels (`Channel0`–`Channel9`). Each channel runs the formula above and feeds its result in as
the next channel's base, in ascending order — e.g. channel 0 for gear, channel 1 for "% of total" buffs.
Disabled by default: enable `bAllowGameplayModEvaluationChannels` in
`UGameplayAbilitiesDeveloperSettings` and give channels names via `GameplayModEvaluationChannelAliases`;
otherwise the field is hidden and everything lands in channel 0.

---

## Modifier magnitude

`FGameplayEffectModifierMagnitude` is used by modifier rows, `DurationMagnitude`,
`MaxDurationMagnitude` and execution scoped modifiers. `MagnitudeCalculationType` picks one of four:

### ScalableFloat

`FScalableFloat`: `Value × Curve(Level)`. `Curve` is a `UCurveTable` row; `RegistryType` instead
pulls the curve from a Data Registry, using the row name as item name. With no curve the value is
constant. Level is the spec level (usually the ability level).

### AttributeBased

`(Coefficient × (PreMultiplyAdditiveValue + AttributeValue)) + PostMultiplyAdditiveValue`

| Field | Meaning |
|---|---|
| `Coefficient`, `PreMultiplyAdditiveValue`, `PostMultiplyAdditiveValue` | Each is an `FScalableFloat`, so they scale with level |
| `BackingAttribute` | `AttributeToCapture` + `AttributeSource` (`Source` / `Target`) + `bSnapshot` |
| `AttributeCalculationType` | `AttributeMagnitude` (final), `AttributeBaseValue`, `AttributeBonusMagnitude` (final − base), `AttributeMagnitudeEvaluatedUpToChannel` (stop at `FinalChannel`) |
| `AttributeCurve` | If set, the attribute value is used as the X lookup into this curve row instead of being used directly (e.g. Strength → damage table) |
| `SourceTagFilter` / `TargetTagFilter` | Only count mods on the backing attribute that were applied with all these source/target tags |

Examples: "heal for 20% of target max health" = `Target.MaxHealth`, coefficient 0.2; "armour
ignores buffs" = `AttributeBaseValue`.

### CustomCalculationClass

Runs an MMC class (`CalculationClassMagnitude`), then
`(Coefficient × (PreMultiplyAdditiveValue + MMCResult)) + PostMultiplyAdditiveValue`, then optionally
looks the result up in `FinalLookupCurve`. The coefficient fields let one MMC serve many GEs.

### SetByCaller

`DataTag` (picker filtered to `SetByCaller.*`) — or legacy `DataName` — is filled on the spec by
whoever applies the GE (`AssignTagSetByCallerMagnitude` in Blueprint). A missing value is logged
and evaluates to 0 — the GE still applies. With stacking, the newest application's values replace the old ones.

### Capture timing and snapshots

| Capture | Captured at | `bSnapshot = true` | `bSnapshot = false` |
|---|---|---|---|
| Source attribute | Spec creation (`MakeOutgoingSpec`) | Value frozen at creation | Linked to the source's live aggregator; for timed GEs the modifier updates when the source's attribute changes |
| Target attribute | Application on the target | Frozen at application | Linked to the target's live aggregator |

Rule of thumb: snapshot source attributes for damage ("damage as it was when I fired"); leave buffs
that should follow the caster's stats un-snapshotted.

---

## Executions

`Executions` is an array of `FGameplayEffectExecutionDefinition`. They run **only when the GE
executes** — `Instant` GEs and each period tick, on the server. On a non-periodic `HasDuration` or
`Infinite` GE they never run and there is no validation warning.

| Field | Meaning |
|---|---|
| `CalculationClass` | `UGameplayEffectExecutionCalculation` subclass. The GE stores the **class**, so the execution's own `EditDefaultsOnly` properties never appear on the GE; configure them on a Blueprint subclass of the execution |
| `PassedInTags` | Handed to the execution as-is for conditional logic. `bRequiresPassedInTags` on the execution class marks them as expected |
| `CalculationModifiers` | Scoped modifiers (`FGameplayEffectExecutionScopedModifierInfo`) folded into the calculation only — never added to an aggregator. Each has op, magnitude, channel and source/target tag requirements |
| `ConditionalGameplayEffects` | `FConditionalGameplayEffect` list applied to the same target when the execution succeeds (`bTriggerConditionalGameplayEffects` in its output). With no `CalculationClass` they always apply. Each entry has `EffectClass` and `RequiredSourceTags`, and is created at the parent spec's level and context |

The `CalculationModifiers` row is hidden unless the execution class declares something to modify
(`GameplayEffectExecutionDefinitionDetails`):

- **Captured attribute backed**: attributes in the execution's `RelevantAttributesToCapture` appear
  in the dropdown. Lyra's approach — the GE then adds e.g. "+10 BaseDamage" for this effect only.
  `InvalidScopedModifierAttributes` hides specific captures from the list.
- **Transient**: tags in `ValidTransientAggregatorIdentifiers` (editor-only) act as data-driven
  "temporary variables" with no backing attribute.

If both lists are empty, the panel also clears entries already authored.

Execution output: emitted mods are applied like instant modifiers (base value, through
`PostGameplayEffectExecute`). Unless the execution sets `bHandledStackCountManually`, each emitted mod
is scaled by the stack count. `bHandledGameplayCuesManually` suppresses the GE's own execute cues.

---

## GameplayEffect components

`GEComponents` holds instanced `UGameplayEffectComponent` subobjects. The engine ships 11 plus the
UI data base class. One instance of each class per GE (validated). Components are `Const`: one
instance is shared by all applications, so they cannot store per-application state.

| Component (editor display name) | Fields | Behaviour and timing | Invalid with |
|---|---|---|---|
| `UTargetTagsGameplayEffectComponent` ("Grant Tags to Target Actor") | `InheritableGrantedTagsContainer` (Add / Remove from inherited) | Tags owned by the target while the GE is active and not inhibited. Count-based: two GEs granting the same tag need both removed | Instant |
| `UAssetTagsGameplayEffectComponent` ("Tags This Effect Has (Asset Tags)") | `InheritableAssetTags` | Tags describing the GE itself; never given to actors. What queries, immunity and `RemoveActiveEffectsWithTags` match against | — |
| `UBlockAbilityTagsGameplayEffectComponent` ("Block Abilities with Tags") | `InheritableBlockedAbilityTagsContainer` | Abilities whose asset tags match cannot activate while the GE is active | Instant |
| `UCancelAbilityTagsGameplayEffectComponent` ("Cancel Abilities with Tags") | `ComponentMode` (`OnApplication` / `OnExecution`), `Cancel Abilities With Tags`, `Cancel Abilities Without Tags` | Cancels running abilities once on apply, or on every execution (each period tick) | `OnExecution` on a non-periodic timed GE |
| `UTargetTagRequirementsGameplayEffectComponent` ("Require Tags to Apply/Continue This Effect") | `ApplicationTagRequirements`, `OngoingTagRequirements`, `RemovalTagRequirements` | Application: pass/fail gate. Ongoing: inhibits/uninhibits as target tags change. Removal: blocks application if already met, and removes the active GE when met later | `Ongoing` on Instant |
| `UImmunityGameplayEffectComponent` ("Immunity to Other Effects") | `ImmunityQueries` (array of `FGameplayEffectQuery`) | While active, incoming specs matching any query are rejected; `OnImmunityBlockGameplayEffectDelegate` fires | Instant |
| `URemoveOtherGameplayEffectComponent` ("Remove Other Effects") | `RemoveGameplayEffectQueries` | On every application (including stacks), removes active GEs matching any query | Periodic (use `RemovalTagRequirements` instead) |
| `UChanceToApplyGameplayEffectComponent` ("Chance To Apply This Effect") | `ChanceToApplyToTarget` (`FScalableFloat`, 0–1, by level) | Random gate at application | — |
| `UCustomCanApplyGameplayEffectComponent` ("Custom Can Apply This Effect") | `ApplicationRequirements` (array of requirement classes) | Every class's `CanApplyGameplayEffect(GE, Spec, TargetASC)` must return true | — |
| `UAbilitiesGameplayEffectComponent` ("Grant Gameplay Abilities") | `GrantAbilityConfigs`: `Ability`, `Level` (`FScalableFloat`), `InputID`, `RemovalPolicy` | Server only. Granted while active; removed on inhibit, re-granted on uninhibit. `RemovalPolicy`: `CancelAbilityImmediately`, `RemoveAbilityOnEnd`, `DoNothing` | Instant; duplicate ability classes |
| `UAdditionalEffectsGameplayEffectComponent` ("Apply Additional Effects") | `OnApplicationGameplayEffects` (conditional), `bOnApplicationCopyDataFromOriginalSpec`, `OnCompleteAlways`, `OnCompleteNormal`, `OnCompletePrematurely` | OnApplication: applied on every application/stack (a periodic GE applies them once, not per tick). OnComplete: applied when the active GE is removed — naturally, prematurely, or both | OnComplete on Instant |
| `UGameplayEffectUIData_TextOnly` ("UI Data (Text Only)") | `Description` (multi-line `FText`) | Pure data for UI; read with `FindComponent<UGameplayEffectUIData_TextOnly>()`. Subclass `UGameplayEffectUIData` for icons etc. | — |

`FConditionalGameplayEffect` entries in `OnApplicationGameplayEffects` also carry `RemovalPolicy`
(`GrantedEffectControlsOwnLifetime` default, or `RemoveGrantedEffectOnEnd` to remove the child when
this GE ends, with `StackCountToRemove`; invalid if either side is Instant).
`bOnApplicationCopyDataFromOriginalSpec` copies SetByCaller values into those child specs; it
defaults to false for backward compatibility, while OnComplete specs always copy.

### `FGameplayEffectQuery` (used by immunity and removal)

All non-empty fields must match:

| Field | Matches against |
|---|---|
| `OwningTagQuery` | Tags the GE grants **plus** its asset tags (kept from pre-5.3 behaviour) |
| `EffectTagQuery` | The GE's asset tags |
| `SourceTagQuery` (shown as SourceSpecTagQuery) | Tags added to the source spec |
| `SourceAggregateTagQuery` | All source tags (actor + spec) |
| `ModifyingAttribute` | GEs that modify this attribute |
| `EffectSource` | GEs whose context source object is this object |
| `EffectDefinition` | GEs of this class |

Asset-tag matching includes the spec's dynamic asset tags (`AddAssetTag` at apply time), so one GE
asset can be made immune-able or removable per application.

---

## Gameplay cues on a GE

| Field | Meaning |
|---|---|
| `GameplayCues` | Array of `FGameplayEffectCue`: `GameplayCueTags`, `MinLevel`/`MaxLevel` (normalise the spec level to 0–1 for the cue), `MagnitudeAttribute` (use this modified attribute's magnitude instead of the level) |
| `bRequireModifierSuccessToTriggerCues` | Default true: execute cues fire only if at least one modifier/execution output actually applied. GEs with no modifiers or executions always fire |
| `bSuppressStackingCues` | Only the first application of a stacking GE fires OnActive cues |

Instant GE → `Executed` cue event. Timed GE → `OnActive` / `WhileActive` on add, `Removed` on removal
or inhibit. Periodic tick → `Executed` each tick.

---

## Stacking

Stacking applies only to `HasDuration` and `Infinite` GEs; Instant + stacking gets an editor warning
and never stacks.

| `EGameplayEffectStackingType` | Panel name | Stack group |
|---|---|---|
| `None` | No Stacking | Every application is a separate active GE |
| `AggregateBySource` | Stack Per Source | One group per instigator ASC on this target |
| `AggregateByTarget` | Stack Per Target | One group on the target, shared by all instigators |

| Field | Behaviour |
|---|---|
| `StackLimitCount` | 0 or −1 = unlimited |
| `StackDurationRefreshPolicy` | `RefreshOnSuccessfulApplication`, `NeverRefresh`, `ExtendDuration` (remaining + new duration, then clamped by max duration) |
| `StackPeriodResetPolicy` | `ResetOnSuccessfulApplication` (progress to next tick discarded), `NeverReset` |
| `StackExpirationPolicy` | `ClearEntireStack`; `RemoveSingleStackAndRefreshDuration` (decay one stack per duration); `RefreshDuration` (effectively infinite; handle decrements yourself via stack change callbacks) |
| `bFactorInStackCount` | Default true. Modifier magnitude is scaled by stack count using the op's bias: additive ops `bias + (m − bias) × N`, `MultiplyCompound` `m^N`, `Override` unaffected. Turn off when the magnitude already depends on the stack count (e.g. MMC reading it) |
| `OverflowEffects` | Applied whenever an application would exceed the limit, whether or not it succeeds. With unlimited stacks they fire on every added stack |
| `bDenyOverflowApplication` | At the limit, further applications fail: no duration refresh, no context/spec update |
| `bClearStackOnOverflow` | With deny: overflowing removes the whole stack (e.g. "5 stacks → detonate") |

Behaviour that is easy to miss:

- **A new stack replaces the active spec** with the newest one — level, SetByCaller values, context
  and captured attributes. Stacks do not remember per-application values.
- `ExtendDuration` combined with a non-`ClearEntireStack` expiration policy refreshes with the total
  extended duration (editor warning).
- With the default `AbilitySystem.ActiveGameplayEffectOverflowBehavior`, an application that would
  push past the limit fills up to the limit and drops the surplus instead of being refused.
- Granted abilities are only granted on the first application of a stack.

---

## Inheritance between GE Blueprints

Child GE Blueprints inherit every field and component. Tag fields on components use
`FInheritedTagContainer`: the child edits **Add to Inherited** / **Remove from Inherited** and the
read-only `CombinedTags` shows the result. "Remove" only edits the inherited list; it cannot strip a
tag from an actor. A child cannot delete a component its parent has — the fix-up step in
`PostCDOCompiled` recreates it — so put optional components in the children, not the base.

---

## Per-application overrides (Blueprint spec functions)

`UAbilitySystemBlueprintLibrary` functions on `FGameplayEffectSpecHandle`, used between
`MakeOutgoingGameplayEffectSpec` and `ApplyGameplayEffectSpecTo*`:

| Function | Overrides |
|---|---|
| `AssignTagSetByCallerMagnitude` / `AssignSetByCallerMagnitude` | SetByCaller values (tag / legacy name) |
| `SetDuration` | Duration for this application (replaces `DurationMagnitude`) |
| `SetStackCount` / `SetStackCountToMax` | Stacks added by this application |
| `AddGrantedTag(s)` | Extra tags granted to the target by this application only |
| `AddAssetTag(s)` | Extra dynamic asset tags (visible to queries and immunity) |
| `AddLinkedGameplayEffect` / `AddLinkedGameplayEffectSpec` | Extra GEs applied together with this one |

The level comes from `MakeOutgoingGameplayEffectSpec(Class, Level)`; every `FScalableFloat` in the GE
is evaluated at it.

---

## C++ extension points

### CustomCalculationClass (MMC)

The examples below capture attributes from a combat set declared like `UMyHealthSet`:
`ATTRIBUTE_ACCESSORS_BASIC(UMyCombatSet, AttackPower)` generates `GetAttackPowerAttribute()`, and
likewise for `Armor`.

```cpp
// MyDamageMagnitudeCalc.h
#pragma once

#include "GameplayModMagnitudeCalculation.h"
#include "MyDamageMagnitudeCalc.generated.h"

UCLASS()
class MYGAME_API UMyDamageMagnitudeCalc : public UGameplayModMagnitudeCalculation
{
    GENERATED_BODY()

public:
    UMyDamageMagnitudeCalc();

    virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
    FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
};
```

```cpp
// MyDamageMagnitudeCalc.cpp
#include "MyDamageMagnitudeCalc.h"
#include "MyCombatSet.h"

UMyDamageMagnitudeCalc::UMyDamageMagnitudeCalc()
{
    // Capture AttackPower from the source, evaluated at execution time rather than snapshotted.
    AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
        UMyCombatSet::GetAttackPowerAttribute(),
        EGameplayEffectAttributeCaptureSource::Source,
        false);

    RelevantAttributesToCapture.Add(AttackPowerDef);
}

float UMyDamageMagnitudeCalc::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
    FAggregatorEvaluateParameters EvalParams;
    EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    float AttackPower = 0.f;
    GetCapturedAttributeMagnitude(AttackPowerDef, Spec, EvalParams, AttackPower);

    return AttackPower * 1.5f;
}
```

`RelevantAttributesToCapture` comes from `UGameplayEffectCalculation` and must be filled in the
constructor (or class defaults for a Blueprint MMC): captures are resolved when the spec is created,
not when the calculation runs. Blueprint/script MMCs read values with `GetCapturedAttributeMagnitude`,
`GetSetByCallerMagnitudeByTag`, `GetSourceAggregatedTags` / `GetTargetAggregatedTags` and friends.
`GetExternalModifierDependencyMulticast` lets an MMC request recalculation on non-attribute changes.

### Execution calculations (`UGameplayEffectExecutionCalculation`)

```cpp
// MyDamageExecution.h
#pragma once

#include "GameplayEffectExecutionCalculation.h"
#include "MyDamageExecution.generated.h"

UCLASS()
class MYGAME_API UMyDamageExecution : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    UMyDamageExecution();

    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

private:
    FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
    FGameplayEffectAttributeCaptureDefinition ArmorDef;
};
```

```cpp
// MyDamageExecution.cpp
#include "MyDamageExecution.h"
#include "MyCombatSet.h"
#include "MyHealthSet.h"

UMyDamageExecution::UMyDamageExecution()
{
    AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
        UMyCombatSet::GetAttackPowerAttribute(),
        EGameplayEffectAttributeCaptureSource::Source,
        true);

    ArmorDef = FGameplayEffectAttributeCaptureDefinition(
        UMyCombatSet::GetArmorAttribute(),
        EGameplayEffectAttributeCaptureSource::Target,
        false);

    RelevantAttributesToCapture.Add(AttackPowerDef);
    RelevantAttributesToCapture.Add(ArmorDef);
}

void UMyDamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    FAggregatorEvaluateParameters EvalParams;
    EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    // Scoped CalculationModifiers authored on the GE are folded into these reads.
    float AttackPower = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AttackPowerDef, EvalParams, AttackPower);

    float Armor = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(ArmorDef, EvalParams, Armor);

    const float Damage = FMath::Max(1.f, (AttackPower * 2.f) - Armor);

    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        UMyHealthSet::GetHealthAttribute(),
        EGameplayModOp::AddBase,
        -Damage));
}
```

`FGameplayModifierEvaluatedData(Attribute, ModOp, Magnitude)` takes an optional fourth
`FActiveGameplayEffectHandle`. Transient aggregators declared in `ValidTransientAggregatorIdentifiers`
are read with `AttemptCalculateTransientAggregatorMagnitude(Tag, EvalParams, OutValue)`.

### SetByCaller from C++

```cpp
const FGameplayEffectSpecHandle SpecHandle =
    MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
SpecHandle.Data->SetSetByCallerMagnitude(
    FGameplayTag::RequestGameplayTag(TEXT("SetByCaller.Damage")), 75.f);
```

There is also an `FName` overload, `SetSetByCallerMagnitude(FName DataName, float Magnitude)`.

### Custom components

Subclass `UGameplayEffectComponent` (native only) and override the hooks that match the moment you
need. Store no per-application state on the component; bind data into delegates on the active GE's
`EventSet` instead.

| Hook | Called |
|---|---|
| `CanGameplayEffectApply` | Before application; any component returning false blocks it |
| `OnActiveGameplayEffectAdded` | Timed GE added (also on clients through replication); return false to start inhibited |
| `OnGameplayEffectExecuted` | Instant execution or periodic tick (server) |
| `OnGameplayEffectApplied` | Initial application or a new stack; not periodic, not replicated. Prefer this one |
| `OnGameplayEffectChanged` | Asset loaded or recompiled; push cached data (tags) onto the owning GE |

A native GE class that creates components in its constructor must add them to `GEComponents`
(`ensure` otherwise) and use fixed subobject names so Blueprint children inherit them.

---

## Stacking from code

```cpp
const int32 StackCount = ASC->GetCurrentStackCount(ActiveHandle);

const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(
    FGameplayTagContainer(PoisonTag));
const int32 TotalStacks = ASC->GetAggregatedStackCount(Query);

ASC->UpdateActiveGameplayEffectSetByCallerMagnitude(
    ActiveHandle,
    FGameplayTag::RequestGameplayTag(TEXT("SetByCaller.StackDamage")),
    NewDamageValue);
```

---

## Querying and removing active effects

```cpp
FGameplayEffectQuery Query;
Query.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(
    FGameplayTagContainer(FGameplayTag::RequestGameplayTag(TEXT("Effect.Type.Buff"))));

const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(Query);
const TArray<float> Remaining = ASC->GetActiveEffectsTimeRemaining(Query);

ASC->RemoveActiveEffects(Query);
ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(SpeedBuffTag));
ASC->RemoveActiveEffectsWithSourceTags(FGameplayTagContainer(PoisonTag));
```

`FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags` and `MakeQuery_MatchAllOwningTags` are the
convenience constructors for tag-only queries.

---

## Effect lifecycle delegates

| Delegate on the ASC | Fires |
|---|---|
| `OnGameplayEffectAppliedDelegateToSelf` | Any effect applied to this ASC |
| `OnGameplayEffectAppliedDelegateToTarget` | Any effect this ASC applied to someone else |
| `OnActiveGameplayEffectAddedDelegateToSelf` | A duration or infinite effect entered the active container |
| `OnPeriodicGameplayEffectExecuteDelegateOnSelf` | A periodic effect ticked on this ASC |
| `OnImmunityBlockGameplayEffectDelegate` | An incoming effect was blocked by immunity |

The first four are `FOnGameplayEffectAppliedDelegate`, a three-parameter multicast delegate:
`(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)`.
The immunity delegate passes `(const FGameplayEffectSpec& BlockedSpec, const FActiveGameplayEffect* ImmunityEffect)`.

Removal is per-handle rather than global:

```cpp
// Declaration:
void HandleEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo);

// Binding, after the effect was applied:
if (FOnActiveGameplayEffectRemoved_Info* RemovalDelegate =
        ASC->OnGameplayEffectRemoved_InfoDelegate(ActiveHandle))
{
    RemovalDelegate->AddUObject(this, &AMyCharacter::HandleEffectRemoved);
}
```

`FGameplayEffectRemovalInfo` carries `bPrematureRemoval`, `bPredictionRejected`, `StackCount`,
`EffectContext`, `OwningASC` and a `const FActiveGameplayEffect* ActiveEffect`.

---

## Console variables that change GE behaviour

| CVar | Default | Effect |
|---|---|---|
| `AbilitySystem.ActiveGameplayEffectOverflowBehavior` | 3 | 0 = legacy: overflow handled only when already at the limit, surplus applications refused |
| `AbilitySystem.AllowPredictiveStackingGEs` | true | Clients may predict adding stacks (never removing) |
| `AbilitySystem.Fix.UseModTagReqsOnAllGE` | true | Modifier row tag requirements also apply to Instant/periodic GEs |
| `AbilitySystem.GameplayEffects.MaxVersion` | Current | Caps the asset upgrade version |
