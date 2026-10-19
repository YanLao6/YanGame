---
name: yan-teams
description: Team membership, friend/foe checks and team colours in YanGame via the ModularTeams plugin. Use when a task touches team assignment, IModularTeamAgentInterface, UModularTeamSubsystem, CompareTeams / CanCauseDamage (friendly fire), team display assets and colours, ObserveTeam / ObserveTeamColors, team tag stacks, lobby team selection (UYanTeamCreationComponent), or team cheats.
type: reference
tags: [gameplay, teams, networking, ui, gas]
requires: []
---

# Teams (ModularTeams)

`Plugins/ModularTeams` — team membership and team colour framework. Module `ModularTeams`, log `LogModularTeams`. Depends on `ModularGameplayExperiences` (team creation waits for Experience load), `GameplayAbilities` (damage rule), `Niagara` (colour application).

Consumers in the project: `AYanPlayerState`, `AYanPlayerController`, `AYanPawn`, `UYanTeamCreationComponent`, `UYanMinimapComponent`, `YanDamageExecution`, `WeaponStateComponent` (EquipManager), `YanLobbyTeamPanel.as`.

## Authority chain

```
AYanPlayerState  ── source of truth: replicated MyTeamID (FGenericTeamId) ──┐
AYanPlayerController ── forwards PlayerState's team, re-broadcasts change ──┤ all implement
AYanPawn ── follows its Controller's team, back to NoTeam on unpossess ─────┘ IModularTeamAgentInterface
```

- **Only the PlayerState stores the team.** `UModularTeamSubsystem::ChangeTeamForActor` resolves the PlayerState first and sets it there; Pawn and Controller merely forward. Calling `SetGenericTeamId` on a non-authority PlayerState logs an error and does nothing.
- Implementers return their `FOnModularTeamIndexChangedDelegate` from `GetOnTeamIndexChangedDelegate()` and broadcast through `ConditionalBroadcastTeamChanged(this, Old, New)` (fires only on an actual change). Call it from the setter **and** from the `OnRep`.
- `FGenericTeamId::NoTeam` ↔ `INDEX_NONE` in Blueprint/script: convert with `GenericTeamIdToInteger` / `IntegerToGenericTeamId`.
- The plugin knows no project PlayerState type; it only talks to the interface. Keep it that way.

## Team creation

`UModularTeamCreationComponent` (GameState component, injected by the Experience; project subclass `UYanTeamCreationComponent`):

- On Experience load (server): spawns one `AModularTeamPublicInfo` (+ `AModularTeamPrivateInfo`) per `TeamsToCreate` entry (`TeamId → UModularTeamDisplayAsset`), then assigns existing players; later joiners via `OnGameModePlayerInitialized`.
- Default choice: the team with the fewest players; pure spectators are stripped of a team.
- `UYanTeamCreationComponent::ServerChooseTeamForPlayer` uses the lobby roster saved by `UYanSessionManager` across travel; falls back to fewest-players when the roster team is not in `TeamsToCreate`, for bots, or for non-lobby starts.
- Info actors register with `UModularTeamSubsystem` on both ends after replication (`OnRep_TeamId`). Private info is meant for team members only, but that culling needs a replication graph — today it replicates to everyone.

Ordering trap: Experience load callbacks run in reverse registration order, so a component that needs teams (e.g. per-team hero assignment) must not assume team creation ran first. Subscribe to the PlayerState's team-changed delegate and act when the team becomes valid.

## Queries (`UModularTeamSubsystem`, world subsystem)

| Call | Notes |
|---|---|
| `FindTeamFromObject(Obj)` | Interface on the object → its **Instigator** (projectiles) → team info actor → PlayerState lookup; `INDEX_NONE` if none |
| `FindTeamFromActor(Obj, bIsPartOfTeam, TeamId)` | Blueprint form |
| `CompareTeams(A, B[, TeamIdA, TeamIdB])` | `OnSameTeam` / `DifferentTeams` / `InvalidArgument` |
| `CanCauseDamage(Instigator, Target, bAllowDamageToSelf = true)` | Self (same PlayerState) → true; different teams → true; target without team but **with an ASC** (training dummy) → true when the instigator has a team; otherwise false |
| `Add/RemoveTeamTagStack`, `GetTeamTagStackCount`, `TeamHasTag` | Authority-only writes; count = public + private info |
| `GetTeamDisplayAsset(TeamId, ViewerTeamId)`, `GetEffectiveTeamDisplayAsset(TeamId, ViewerAgent)` | Viewer-relative colours are supported by the API |
| `GetTeamIDs()`, `DoesTeamExist(TeamId)`, `FindPlayerStateFromActor` | |

Damage gating goes through `CanCauseDamage` (weapon hit filter, damage execution). Do not hand-roll team comparisons.

## Display and colours

- `UModularTeamDisplayAsset`: `ScalarParameters`, `ColorParameters`, `TextureParameters`, `TeamShortName`; `ApplyToMaterial`, `ApplyToMeshComponent` (creates MIDs for textures), `ApplyToNiagaraComponent`, `ApplyToActor`. Editing the asset in the editor notifies all colour observers live.
- `UModularTeamStatics` (script namespace `ModularTeam::`): `FindTeamFromObject(Agent, bIsPartOfTeam, TeamId, DisplayAsset)`, `GetTeamDisplayAsset(WorldContext, TeamId)`, `GetTeam{Scalar,Color,Texture}WithFallback(Asset, Name, Default)`. Project convention: colour key `TeamColor`.
- Async actions (Blueprint): `ObserveTeam(TeamAgent)` and `ObserveTeamColors(TeamAgent)` broadcast once immediately, then on every change; cancel when done. Cosmetic parts re-apply colours on `OnCharacterPartsChanged`.

## Cheats

`UModularTeamCheats` is attached to every CheatManager: `CycleTeam`, `SetTeam <id>`, `ListTeams`.

## Adding a new team-aware actor

1. Implement `IModularTeamAgentInterface`; if it belongs to a player, forward to the PlayerState/Controller rather than storing a team.
2. If it stores a team (non-player actor), replicate it, broadcast via `ConditionalBroadcastTeamChanged` in setter and `OnRep`.
3. For projectiles, set the **Instigator** — `FindTeamFromObject` uses it.
4. Query only through `UModularTeamSubsystem` / `ModularTeam::`.

## Related

- Experience callbacks, spawning, PlayerState PawnData: `ue-game-features/references/modular-experiences.md`.
- Lobby / session flow and per-team hero assignment: `yangame-systems`.
- Generic team id / AI perception affiliation: `ue-ai-navigation`.
