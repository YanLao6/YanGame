# ChaosMover (5.8)

Physics-thread backend for Mover: movement simulates on the Chaos physics thread (PT) at a fixed async step, with network physics prediction and resim. Module `ChaosMover` (Runtime), `IsExperimentalVersion = true`: do not freeze its internals into project rules. Hard plugin dependency: `Mover`; also uses `Water`, `MotionWarping`.

**Required setting:** `Config/DefaultChaosMover.ini` must enable `[/Script/Engine.PhysicsSettings] PhysicsPrediction=(bEnablePhysicsPrediction=True)`; without it networked prediction fails.

## Layers

| Class | Thread | Role |
|---|---|---|
| `UChaosCharacterMoverComponent` (: `UCharacterMoverComponent`) | GT | Main entry; produces input, exposes `OnLandedDelegate`/`OnJumped`, `Launch`, trajectory prediction. Defaults `BackendClass` to `UChaosMoverBackendComponent`, and `bHandleJump = bHandleStanceChanges = false` (handled by PT transitions/modifiers) |
| `UChaosMoverBackendComponent` (`Within = MoverComponent`) | GT | `IMoverBackendLiaisonInterface`; creates particle, Character Ground Constraint and Actuation Joint, injects input, fetches output, handles rollback |
| `UChaosMoverSubsystem` (World) | GT | Collects backends, drives async callbacks after each physics tick |
| `UChaosMoverSimulation` (: `UMoverSimulation`) | PT | Runs `FMoverStateMachine` and the movement logic; receives networked sim actions |

**Thread red line:** code in `_Async` functions, `SimulationTick_Implementation`, `GenerateMove_Implementation`, `ExecuteMove_Async`, `Pre/PostSimulationTick_Async` must never touch `AActor`/`UActorComponent`/any GT-only object. Cache what is needed in `OnModeRegistered`/`OnRegistered` (GT) or pass via `FChaosMoverSimulationDefaultInputs` (e.g. `PrimaryVisualComponentRelativeTransform`).

## Default character modes

Registered by `UChaosCharacterMoverComponent`, `StartingMovementMode = Falling`: `Walking` (`UChaosWalkingMode`, drives the ground constraint), `Falling` (`UChaosFallingMode`: `AirControlPercentage`, `TerminalVerticalSpeed`, `bCancelVerticalSpeedOnLanding`), `Flying`. Optional: `Swimming` (with `ChaosCharacterWaterCheck`), `UChaosSimpleWalkingMode`, `UChaosSmoothWalkingMode` (ports `USmoothWalkingMode`; spring state in `FChaosSmoothWalkingState`).

Transitions (PT, `UChaosMovementModeTransition`, `Evaluate_Implementation` + `Trigger_Implementation`): `JumpCheck`, `FallingCheck` (relative speed over `ChaosMover.FallingCheckRelativeSpeedLimit`), `LandingCheck`, `LaunchCheck`, `CrouchCheck`, `WaterCheck`. `UChaosStanceModifier` handles crouch; the GT batches stance changes via `PendingBatchFinalStance` so resim jitter does not emit false `OnStanceChanged`. `UChaosCharacterApplyVelocityEffect` (`Impulse`/`AdditiveVelocity`/`OverrideVelocity`) backs `Launch()`.

Input blocks (`ChaosCharacterInputs.h`, all `FMoverDataStructBase`): `FChaosMoverLaunchInputs`, `FChaosMoverCrouchInputs`, `FChaosMovementSettingsOverrides`/`...Remover`, `FChaosMoverRequestedMoveInputs`.

## Extending (preferred order)

- **New mode:** configure `UChaosCompositeMovementMode` = `MoveSource` (`UChaosMoverSourceBase`, proposes `FProposedMove`, `IsFinished()` switches to `NextModeName`) + `MoveExecutor` (`UChaosMoveExecutorBase`, `ExecuteMove_Async` applies it). Built-in executors: `UChaosCharacterWalkingExecutor`, `UChaosCharacterAirExecutor`. An executor implementing `IChaosCharacterMovementModeInterface` / `IChaosCharacterConstraintMovementModeInterface` (`GetTargetHeight`, `ShouldEnableConstraint`, `UpdateConstraintSettings`) is exposed to the sim by `CollectSimulationInterfaces` automatically. Subclass `UChaosCharacterMovementMode` only when composition cannot do it.
- **Per-tick hooks:** attach `IChaosPreSimulationTickInterface` / `IChaosPostSimulationTickInterface` to a mode or executor; set `bApplyFallbackVelocity = true` in `FChaosMoverPostSimContext` if Post does not apply particle velocity itself.
- **Collision ignoring:** `IgnoredCollisionMode` on the mode base.
- **Cross-thread/network data:** only `FMoverDataStructBase` subclasses (`Clone`/`NetSerialize`/`ShouldReconcile`/`Interpolate`/`Merge`); no raw pointers or GT references.

## Pathed movement (platforms, props)

`UChaosPathedMovementMode` + `UChaosPathedMovementControllerComponent` move non-character objects along `PathPatterns` (ordered, accumulated per frame; point/spline/elliptical). `bUseJointConstraint = true` pulls via a joint (soft physical response); `false` is rigid kinematic. Set duration before play with `SetOneWayTripDuration_BeginPlayOnly`, at runtime use the controller's `SetMovementTimeDilation`. `UChaosPathedMovementReachedEndTransition` fires at the end.

## Networking

Two paths for LayeredMove / InstantMovementEffect, switched by CVar (default `true`):

| Path | CVar `ChaosMover.Networking.` | Detect at | Traits |
|---|---|---|---|
| Sim Actions | `NetworkMovesWithSimActions` / `NetworkInstantMovementEffectsWithSimActions` | **PostSim**, via `GetLastInputCmd()` | Server-authoritative; one frame later than PreSim |
| Embedded input cmd | same, `false` | PreSim input | Client-authoritative, co-op friendly |

With sim actions, Blueprint must detect conditions in PostSim through `GetLastInputCmd()`, not PreSim input (`UChaosMoverBlueprintLibrary::IsNetworking*WithSimActions()` reports the active path).

Server-only `UChaosMoverBlueprintLibrary` API (silent no-op without authority or backend): `Queue Layered Move / Instant Movement Effect (Authority)` replicate immediately with no client prediction; `Schedule ... (Authority)` apply on a future frame (`UNetworkPhysicsSettingsComponent::EventSchedulingMinDelaySeconds`) so all ends apply on the same physics frame, avoiding corrections (forced knockback, teleport).

Tolerance: override `IsNearlyEqualTo` on `FInstantMovementEffect` / `FLayeredMoveBase` subtypes so small client deviation does not trigger resim. Rollback: network-received moves/effects always roll back; GT-local ones (`bShouldRollBack = false`) do not, because resim does not rerun GT logic.

## API and debugging

- GT: `Launch(VelocityOrImpulse, Mode)` (the current mode needs a Launch transition), `OverrideMovementSettings` / `CancelMovementSettingsOverrides(ModeName)`, `TryGetFloorCheckHitResult`, `TryGetLastWaterResult`, `GetPredictedTrajectory`, `SetPredictedTrajectoryParams`.
- PT sim: `Queue Layered Move/Instant Movement Effect/Movement Modifier`, `FindMovementModeByName`, `HasGameplayTag`, `FindMovementModifierByType<T>()`, `AttemptTeleport` (`CanTeleport` first), `GetLocalSimInput()`.
- CVars (`ChaosMover.`): `ForceSingleThreadedGT/PT` (default true), `DebugDraw.GroundQueries/OverlapQueries`, `Perf.SkipGenerateMoveIfOverridden`, `Networking.DisableResimDuplicateEventChecking`, `Networking.*IDHistorySize` (30), `Networking.FrameDifferenceLeniencyForEventTimeComparison` (1), `EnablePreSimGroundCheck`/`EnableServerLaunchOverride` (false).
- Commands: `ChaosMover.Debug.TeleportClientOnly`, `ChaosMover.Debug.TeleportTo X Y Z Yaw Pitch Roll` (`=` keeps a component). Chaos Visual Debugger receives `TraceMoverData` output via `GetDebugSimData()`.

## Determinism red lines

PT logic must be resim-reproducible: no randomness, frame-rate or GT-state dependence. See the project input rules in `yangame-systems/references/mover-input-contract.md`.
