---
name: yan-systems
description: Routes to the authoritative docs for YanGame's core systems. Use when a task touches the Mover input contract and network determinism, the UI layer stack and CommonUI, equipment/quick bar/inventory, Experience and PawnData hero assignment, the interaction/pickup/indicator framework, or OpenUnrealUtilities; also indexes where the Modular* plugin docs (Experiences, Data, Abilities, UI, Teams) and GameplayMessageRouter live. Read the matching reference before changing these systems to avoid already-recorded pitfalls.
type: index
tags: [architecture, gameplay, networking, ui, navigation]
requires: []
---

# YanGame Systems Index

Routing only; content lives in `references/`.

| Topic | Reference | Read when |
|---|---|---|
| Mover input contract | `references/mover-input-contract.md` | **Before adding any movement intent**; Ability↔Mover communication, network determinism, mode switching |
| UI layer framework | `references/ui-layer-framework.md` | New screens, back action, widget lifecycle and cleanup |
| Equipment and quick bar | `references/equipment-quickbar.md` | Inventory, quick bar, equip placement, or any new Server RPC (reference authority model) |
| Experience and PawnData | `references/experience-pawndata.md` | Hero assignment, hero swap, Experience injection, UI silently not appearing |
| Interaction / pickup | `references/interaction-pickup.md` | World pickups, interact ability, screen indicators |
| OpenUnrealUtilities | `references/ouu-utilities.md` | Before writing a general-purpose helper |

Other plugin docs live in the skills they belong to:

| Plugin | Doc |
|---|---|
| ChaosMover | `ue-mover/references/chaosmover.md` |
| OUUTags | `ue-gameplay-tags-messaging/references/ouutags.md` |
| GameplayMessageRouter (AngelScript fork) | `ue-gameplay-tags-messaging/references/gameplay-message-router.md` |
| ModularGameplayExperiences | `ue-game-features/references/modular-experiences.md` |
| ModularGameplayData | `ue-game-features/references/modular-gameplay-data.md` |
| ModularGameplayAbilities | `ue-gameplay-abilities/references/modular-gameplay-abilities.md` |
| ModularGameplayUI | `ue-ui-umg-slate/references/modular-gameplay-ui.md` |
| ModularTeams | `yan-teams` skill |

Modular stack dependency order (lower depends on nothing above it): `ModularGameplayData` → `ModularGameplayExperiences` → `ModularGameplayAbilities` / `ModularTeams` → `ModularGameplayUI`.

## Cross-system conventions

- **No array indices as RPC arguments.** Send an object reference or stable id (Tag / PrimaryAssetId); the index may point at another element after array changes and `IsValidIndex` cannot detect it. Quick bar placement is the positive example.
- **Unbind delegates and message listeners bound to long-lived objects.** `AS_QuickBarInputHost` is the template; Session widgets are the fixed counter-example.
- **AngelScript `UFUNCTION(Server)` is Reliable by default**, unlike C++ UHT; omitting `Reliable` is not a defect.
- **Fetch Controller components lazily**, never cache in `BeginPlay`; controller replication lags.

## Known defects

`reports/AngelScript代码审计.md` records 6 confirmed defects (4 fixed, 1 open, 1 partially fixed) and 3 ruled-out false positives; check it when touching related code. The open one: Session widgets do not unbind `UYanSessionManager` delegates, which needs a lifecycle refactor of four widgets.
