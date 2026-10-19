# ModularGameplayExperiences (project plugin)

`Plugins/ModularGameplayExperiences` — the project's Lyra-style Experience framework: a data asset decides which GameFeatures, actions and default PawnData a match uses, and the GameMode delegates spawning to GameState components. The generic pattern is in [experience-system.md](experience-system.md); this file is the contract of the concrete implementation.

Depends on: `ModularGameplayData` (PawnData, AssetManager), `ModularGameplayActors`, `CommonGame` / `CommonUser` / `CommonLoadingScreen`, `GameplayAbilities`, `GameplayMessageRouter`. Depended on by `ModularGameplayAbilities`, `ModularGameplayUI`, `ModularTeams`.
Project-side hero assignment on top of it: `yangame-systems/references/experience-pawndata.md`.

## Assets

| Type | Role |
|---|---|
| `UModularExperienceDefinition` (PrimaryAssetType `ModularExperienceDefinition`) | `GameFeaturesToEnable`, `DefaultPawnData` (soft, bundle `Equipped`), `ActionSets`, inline `Actions` |
| `UModularExperienceActionSet` | Reusable `Actions` + `GameFeaturesToEnable`; prefer this over duplicating actions across Experiences |
| `UModularUserFacingExperienceDefinition` | Frontend tile: `MapID`, `ExperienceID`, `ExtraArgs`, texts, icon, `LoadingScreenWidget`, `bIsDefaultExperience`, `bShowInFrontEnd`, `MaxPlayerCount`; `CreateHostingRequest()` builds the `UCommonSession_HostSessionRequest` |
| `AModularWorldSettings` | Per-map `DefaultGameplayExperience`, `bEnableGameplayExperience`, extra `Actions` / `ActionSets` (test convenience; their bundles and GameFeatures are loaded too) |

Both Experience types plus `Map` must be `CookRule=AlwaysCook` in every GameFeatureData that scans them — they are discovered only by directory scan, so Cook otherwise prunes them and the packaged frontend list is empty.

## Experience resolution (server, `AModularExperienceGameMode`)

`InitGame` defers one frame, then picks the first valid source, highest first:

Matchmaking assignment → URL option `?Experience=` → DeveloperSettings (PIE) → command line `Experience=` → `AModularWorldSettings::DefaultGameplayExperience` → dedicated-server flow → default asset.

The id is replicated through `UModularExperienceComponent::CurrentExperience` on the GameState; both ends then load bundles (`Equipped` + Client/Server), activate GameFeatures, and execute actions.

## Runtime classes

| Class | Lives on | Responsibility |
|---|---|---|
| `UModularExperienceComponent` | GameState | Load / activate / deactivate; `ShouldShowLoadingScreen` while loading |
| `AModularExperienceGameMode` | — | Resolves Experience and PawnData, delegates player start / restart to the spawning component; `RequestPlayerRestartNextFrame`, `SetPendingSpawnTransform`, `OnGameModePlayerInitialized` |
| `AModularExperienceGameState` / `AModularExperiencePlayerState` / `AModularExperiencePawn` / `AModularExperienceCharacter` | — | Bases that cooperate with the load flow |
| `UModularPlayerSpawningComponent` | GameState | `ChoosePlayerStart`, `ControllerCanRestart`, `FinishRestartPlayer`; subclass and override `OnChoosePlayerStart` |
| `UModularBotCreationComponent` | GameState | Spawns `NumBotsToCreate` bots after load (URL `NumBots` overrides); `SpawnOneBot` / `RemoveOneBot` |
| `UModularPawnComponent` | Pawn | Init-state chain, replicated `PawnData`, applies Pawn-scope Fragments |
| `UModularInputComponent` | Pawn | Default IMC + PawnData `InputConfig` native bindings (Move / Look / Crouch); `AddAdditionalInputConfig` |
| `UAsyncAction_ExperienceReady` | — | Waits for GameState + load; always fires at least one frame later |
| `UModularExperienceUtils::GetKeyValueByTag(PC, InputTag)` | — | Key currently bound to an InputTag (Ability table before Native table, current device preferred) — use this for UI key hints |
| `UModularSystemStatics` | — | `GetTypedSoftObjectReferenceFromPrimaryAssetId`, `GetPrimaryAssetIdFromUserFacingExperienceName`, `PlayNextGame` (seamless restart of the current level), `Set*ParameterValueOnAllMeshComponents`, `FindComponentsByClass` |
| `UModularGameInstance` | — | `UCommonGameInstance` base |

## Load callbacks

```cpp
UModularExperienceComponent* EC = GameState->FindComponentByClass<UModularExperienceComponent>();
EC->CallOrRegister_OnExperienceLoaded_HighPriority(FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
// also: CallOrRegister_OnExperienceLoaded / _LowPriority; IsExperienceLoaded()
```

- Fires immediately if already loaded.
- **Within one priority the broadcast is in reverse registration order**: a GameState component later in an Experience's `ComponentList` runs first. Never rely on ComponentList order for cross-component dependencies; subscribe to the result event instead (e.g. PlayerState `OnTeamIndexChanged`).

## PawnData flow

`GetPawnDataForController`: PlayerState PawnData → Experience `DefaultPawnData` → `UModularAssetManager::GetDefaultPawnData()`. `PawnData->PawnClass` decides the spawned class; spawning is deferred so PawnData is injected before `FinishSpawning`.

`AModularExperiencePlayerState::SetPawnData` is **idempotent first-write** (writing the same data again returns silently). It applies PlayerState- and Controller-scope Fragments; `UModularPawnComponent` applies Pawn-scope ones. `ChangePawnData` revokes and re-applies. Hooks for subclasses: `PreRevokePawnData` / `PostApplyPawnData`. See [modular-gameplay-data.md](modular-gameplay-data.md) for Fragments.

## Constraints

- **One spawning component per Experience.** The GameMode uses `FindComponentByClass<UModularPlayerSpawningComponent>()`; a second one is silently ignored. Custom spawning components should log an error when they are not the first.
- A pure spectator never goes through `RestartPlayer`; place and sync it manually (see memory on native GameMode spectators).
- Init-state tags (`InitState.Spawned → DataAvailable → DataInitialized → GameplayReady`) and `InputTag.Move/Look.Mouse/Look.Stick/Crouch`, `GameplayEvent.Reset/RequestReset` are native tags in `ModularGameplayTags.h`.
- `UModularExperienceManager` (engine subsystem) ref-counts GameFeature activation across PIE instances; do not deactivate plugins directly in editor code.

## Messages

`Messages/` holds message-bus payload helpers used with `GameplayMessageRouter`:

- `FModularVerbMessage` — Instigator / Target / Verb / ContextTags / Magnitude.
- `UVerbMessageHelpers` — `GetPlayerStateFromObject`, `GetPlayerControllerFromObject`, `GetPawnFromObject`, **`GetAbilitySystemComponentFromObject`** (preferred way to resolve the ASC from any object; it walks Controller/PlayerState), Verb ↔ `FGameplayCueParameters`.
- `UModularGameplayMessageProcessor` — server-side actor component that listens and rebroadcasts (combos, streaks). Usually one global instance, so filter by owner yourself; register handles via `AddListenerHandle`, they are cleared in `EndPlay`.
- `FModularNotificationMessage`, `FGameplayTagStackContainer` (replicated tag counts, FastArray).
