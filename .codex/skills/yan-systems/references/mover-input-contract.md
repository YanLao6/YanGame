# Mover Input Contract

`FYanCharacterInputs` is the **only** channel between Abilities and Mover. Field docs live in `Plugins/YanGameCore/Source/YanGameMover/Public/YanMoverDataModelTypes.h`; this file keeps only the rules.

## Hard constraints

- **Single writer.** Only a locally controlled `UMoverInputAbility` subclass writes, inside `ProduceMoverInput`. `ProduceInput` runs on the local end only; the server replays the replicated struct for remote pawns.
- **Single reader.** Sim-side modes (`GenerateMove`) and transitions (`Evaluate`) use replicated inputs and simulation sync state; they must not read camera, game-thread state or Actor references. Side-channel reads are not reproducible on server resim and are the main source of rollback jitter.
- **Mode switches follow a persistent flag.** Never use the one-shot `FCharacterDefaultInputs::SuggestedMovementMode` (not reproducible in resim). Enter and exit apply the same intent and geometric validity rules, so an invalid intent cannot re-enter within the same simulation step (grapple `EnterCheck`/`ExitCheck` is the template).

## Pulse vs persistent flag

Decide by how long the reader needs to see it, not by key feel.

| Form | When | Examples |
|---|---|---|
| Single-frame pulse | Reader finishes every decision on the trigger frame | `bWantsToAirDash`, `bWantsToBlink`, `bWantsToBreakFree` |
| Persistent flag | Reader keeps state across frames, or sampling can miss a tick | `bIsGrapplingActive`, `bWantsToSlide`, `bWantsToClinch` |

Anything consumed across threads or tick domains must be persistent. `bWantsToClinch`: the server must read it on the game thread to apply effects to the victim, and game ticks do not map 1:1 to sim ticks, so a pulse gets sampled away. Duplicate enqueues are filtered sim-side by tag.

## Upload a geometric value or derive it on both ends

One test: **is the value a pure function of replicated inputs plus static config?**

- Yes → do not upload; both ends derive it (air-dash direction comes from `FCharacterDefaultInputs`, speed from transition config).
- No → upload; the server trusts the originator's value. Anything from collision sweeps, another actor's interpolated state, or a one-shot event diverges if recomputed: `BlinkTargetPoint`, `GrappleAnchorPoint`, `ClinchAnchorPoint` (the victim's position is extrapolated on the originator, unlike the server's truth).

## Shared time base

Timings both sides must agree on cannot be timed locally. `ClinchFallStartSimTimeMs` is taken from `UMoverComponent::GetLastTimeStep` by the originator; that is the server sim timeline, which client prediction also uses. Per-side timers would grow a vertical gap of g·Δt·t because the two effects start one round trip apart.

## Adding a field

Update `NetSerialize`, `ShouldReconcile`, `Merge`, `Interpolate`. `ShouldReconcile` is the one usually forgotten (symptom: under- or over-correction). `FCharacterDefaultInputs` needs no change.

## Boundaries

- Grapple direction validity lives in `UChaosGrapplingExitCheck::IsHookOutsideForwardHemisphere`: a vertical dividing plane through the simulated player location follows only `FCharacterDefaultInputs::ControlRotation.Yaw`. Pitch, Roll and hook height do not tilt the plane. Hook position comes from `FYanCharacterInputs::GrappleHookLocation`, independently of the pulling meeting point. A horizontal angle of 90° or greater ends the grapple; directly above/below lies on the plane and also exits. The Ability reuses this check during game-thread finalization for cleanup, including target-only pulling and synchronized-start waiting.
- Movement intent only. Damage and hit resolution go through GAS TargetData.
- Projectiles are cosmetic plus a hit probe on the local end; the server neither spawns them nor treats their collision as movement truth.
- Client-authoritative hits are cheatable; server-side validation of uploaded geometry (range, facing, target existence) is a known, unimplemented hardening item.

## See also

- `MoverGrapplingAbility.as` (symmetric enter/exit), `MoverSlideAbility.as` with `CharacterSlideCheck`/`CharacterSlideExitCheck`
- `ue-mover` skill and its `references/chaosmover.md`
- `Docs/Spec_ChaosMover网络确定性改造.md` — design basis and acceptance criteria
