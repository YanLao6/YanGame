---
type: guide
tags: [interaction, pickup, inventory, indicator, gas, lyra-port, equipmanager, modulargameplayui]
requires: []
---
# 交互 / 拾取 / 指示器框架

从 Lyra 移植到本项目的一套「靠近地上物 → 屏幕指示器提示 → 按键 → 物品进背包」的完整框架。跨 `EquipManager` 与 `ModularGameplayUI` 两个插件，纯 C++ 提供机制，具体玩法在编辑器 / AngelScript 层组装。

## 能力（提供什么）

- **交互扫描**：常驻能力周期性球形扫描角色周围，发现可交互物。
- **数据驱动授予**：可交互物自带「交互能力」，靠近时动态授予玩家，离开不再触发。
- **拾取入包**：触发拾取能力后把地上物的物品清单写入玩家背包组件。
- **屏幕指示器**：把可交互物投影为屏幕空间 UI（可钳制到屏幕边缘并显示箭头）。

## 架构（两插件三层）

```
ModularGameplayUI (块 B)                EquipManager (块 A)
────────────────────────                ──────────────────────
IndicatorSystem/            ◄────────── Interaction/Abilities/GameplayAbility_Interact
  UIndicatorManagerComponent            (对接指示器)
  UIndicatorDescriptor                  Interaction/Tasks/  (扫描/检测)
  UIndicatorLayer (SActorCanvas)        Interaction/  (接口/选项/Statics)
                                        Inventory/IPickupable  (拾取动作)
                                        Inventory/WorldCollectable  (地上物基类)
                                        Interaction/Actor/GameplayAbility_Pickup
```

依赖方向单向：`EquipManager → ModularGameplayUI`（Build.cs 模块依赖 + .uplugin 插件依赖各一处）。`ModularGameplayUI` 不反向依赖，无循环。

## 关键类


| 类                                                        | 模块              | 职责                                                                                                                             |
| --------------------------------------------------------- | ----------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `UGameplayAbility_Interact`                               | EquipManager      | 常驻交互能力（OnSpawn）。启动扫描 Task，`UpdateInteractions` 刷新指示器，`TriggerInteraction` 触发拾取。**Abstract，需 BP 子类** |
| `UAbilityTask_GrantNearbyInteraction`                     | EquipManager      | 按 Interaction 通道球形扫描，把可交互物携带的能力动态授予玩家                                                                    |
| `UAbilityTask_WaitForInteractableTargets_SingleLineTrace` | EquipManager      | 单线检测持续更新可交互选项，变化时广播                                                                                           |
| `IInteractableTarget` / `IPickupable`                     | EquipManager      | 可交互 / 可拾取接口。**纯 C++ virtual + CannotImplementInterfaceInBlueprint**                                                    |
| `UPickupableStatics::AddPickupToInventory`                | EquipManager      | 「把地上东西变成 inventory」的实际动作；`BlueprintAuthorityOnly`                                                                 |
| `AWorldCollectable`                                       | EquipManager      | 地上可拾取物基类。自带碰撞体默认响应 Interaction 通道                                                                            |
| `UGameplayAbility_Pickup`                                 | EquipManager      | 拾取能力。`ServerOnly`，从 `TriggerEventData->Target` 取物、调 `AddPickupToInventory`                                            |
| `UIndicatorManagerComponent`                              | ModularGameplayUI | Controller 上管理活动指示器                                                                                                      |
| `UIndicatorDescriptor` / `UIndicatorLayer`                | ModularGameplayUI | 单个指示器数据 / 屏幕投影画布                                                                                                    |

## 数据流（拾取全链路）

```
GA_Interact (OnSpawn 常驻)
  ├─ GrantNearbyInteraction  每帧扫描 → 可交互物.GatherInteractionOptions()
  │     └─ 返回带 InteractionAbilityToGrant 的选项 → 动态 GiveAbility 给玩家
  ├─ WaitForInteractableTargets → UpdateInteractions() → 生成 UIndicatorDescriptor
  └─ 玩家按键 → TriggerInteraction()
        └─ TriggerAbilityFromGameplayEvent → GA_Pickup.ActivateAbility(TriggerEventData)
              └─ GetFirstPickupableFromActor(Target) → AddPickupToInventory(InvComp, pickup)
                    └─ InventoryManagerComponent.AddItemDefinition()  【入包】
```

## 关键约束 / 边界

- **接口只能 C++ 实现**：`IInteractableTarget` / `IPickupable` 标了 `CannotImplementInterfaceInBlueprint` 且方法非 `UFUNCTION`，蓝图和 AngelScript 都无法实现。地上物的基类必须是 C++（`AWorldCollectable`）。
- **专用碰撞通道**：交互扫描使用 `ECC_GameTraceChannel1`，在 `Config/DefaultEngine.ini` 命名为 `Interaction`（默认 `Ignore`）。常量定义见 `Interaction/InteractionChannels.h` 的 `EquipManager_TraceChannel_Interaction`。可交互物须对该通道 `Overlap`（`AWorldCollectable` 已默认设置）。
- **拾取在服务器**：`AddPickupToInventory` 为 `BlueprintAuthorityOnly`，`GA_Pickup` 用 `ServerOnly` + `IsNetAuthority()` 守卫。
- **组件挂载位置**：`InventoryManagerComponent`、`IndicatorManagerComponent` 都是 `UControllerComponent`，挂在 PlayerController 上（跨 Pawn 死亡存活）。
- **C++ 提供积木，蓝图编排**：`GA_Interact` 的 `UpdateInteractions` / `TriggerInteraction` 由 BP 子类用检测 Task 和输入事件串联。

## 编辑器装配清单（让框架实跑）

1. 编译 `AWorldCollectable`、`UGameplayAbility_Pickup`、块 A/B 全部类。
2. PlayerController 挂 `UInventoryManagerComponent` + `UIndicatorManagerComponent`。
3. `BP_GA_Interact`（继承 `UGameplayAbility_Interact`）加入 pawn `ABS_DefaultAbilitySet`；BP 内：`WaitForInteractableTargets_SingleLineTrace` → `InteractableObjectsChanged` → `UpdateInteractions`；交互输入键 → `TriggerInteraction`。
4. 拾取能力直接用 `UGameplayAbility_Pickup`（或其 BP 子类）。
5. HUD 放 `UIndicatorLayer`（设 `ArrowBrush`）+ 实现 `IIndicatorWidgetInterface` 的提示 Widget。
6. `BP_Collectable`（继承 `AWorldCollectable`）：`Option.InteractionAbilityToGrant = UGameplayAbility_Pickup`、`StaticInventory.Templates += {ItemDef, 数量}`、加可见 Mesh，摆入场景。
7. PIE 验证：走近 → 指示器浮现 → 按键 → 背包 +1。

## 扩展点

- **持械角色**：走完整 Inventory → QuickBar → Equipment 三层；需补回 `EquipmentDefinition` 中被注释的 `AbilitySetsToGrant`（用 `UModularAbilitySet` 替代 `ULyraAbilitySet`）。

## 相关配置

- `Config/DefaultEngine.ini` → `[/Script/Engine.CollisionProfile]`：`ECC_GameTraceChannel1` = `Interaction`。
- `EquipManager.Build.cs`：依赖 `UMG`、`GameplayTasks`、`AngelscriptGAS`、`ModularGameplayUI` 等。
- `EquipManager.uplugin`：插件依赖 `ModularGameplayUI`。
- `ModularGameplayUI.Build.cs`：依赖 `AsyncMixin`（SActorCanvas）。
