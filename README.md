# YanGame

基于 [Unreal Engine Angelscript](https://angelscript.hazelight.se/)（Hazelight 维护的 UE 源码分支）的多人对战游戏项目。

移动系统建立在 **Mover / ChaosMover** 之上而非 CharacterMovementComponent，能力系统建立在 **GAS** 之上，两者通过一份显式的输入契约衔接。大量玩法逻辑用 AngelScript 编写，保存即热重载。

引擎下载：[Hazelight/UnrealEngine-Angelscript](https://github.com/Hazelight/UnrealEngine-Angelscript)

> 本项目为个人学习与实验性质，代码与文档以中文为主。下列系统中有自行实现的，也有在开源实现上扩展或直接搬运的，来源见 [THIRD_PARTY.md](THIRD_PARTY.md)。

## 实现、扩展或搬运的系统

### 1. Mover 与 GAS 的结合

`FYanCharacterInputs` 是 Ability 与 Mover 之间唯一的通信接口，三条硬约束：

- **写入方唯一**：只有本地受控端的 `UMoverInputAbility` 子类在 `ProduceMoverInput` 中写入
- **读取方只读本结构**：仿真内的 MovementMode 与 Transition 禁止旁路读取摄像机或 game-thread 状态，旁路读到的值在服务器重模拟时不可复现，是回滚抖动的主要来源
- **模式切换只由持续标志裁决**：不用一次性的 `SuggestedMovementMode`，进入与退出由同一持续标志的两个 Transition 对称裁决

GAS 技能实例本身登记为 `MoverComponent` 的输入源。登记与实例生命周期对齐（Avatar 就绪即登记、技能移除才注销），不随激活与结束开关——GAS 的激活期在 game thread 时间轴、`ProduceInput` 在仿真时间轴，两者频率与相位不一致，按激活期开关会丢失脉冲输入。

### 2. Mover 移动技能

滑铲、钩锁、冲刺、空中冲刺、击飞、爬墙、闪现、贴身抓取，均以 ChaosMover 的 MovementMode + Transition + LayeredMove 实现。移动效果走输入包而非从 game thread 下发——后者会被引擎标记为不可回滚，重模拟时无法重放，客户端预测出的位移会被服务器权威状态抹掉。

### 3. Experience 系统

Experience 按 class 注入：通过 GameFeatureAction 向 GameState、Controller、Pawn 注入组件、Widget、AbilitySet，粒度是"这一局的所有玩家"。

##### 4. PawnDataFragment 角色配置

PawnData 按玩家决议：技能、输入、组件、界面全部通过 Fragment 配置，粒度是"这一个玩家"。它弥补了单纯 Experience 只能按 class 添加组件的局限，使同一局内不同玩家可以扮演不同英雄。

### 5. UIExtension 与 CommonUI

基于 CommonUI 的四层 Widget 栈（Game / GameMenu / Menu / Modal），所有界面都进栈，不存在直接 `AddToViewport` 的界面。任意代码可按 GameplayTag 把 widget 压入对应层，无需持有布局引用。

### 6. Equipment 装备系统

背包与快捷栏组件挂在 PlayerController（随玩家存续，换 Pawn 不丢背包），请求组件挂在 Pawn（借客户端所有权发 Server RPC）。槽位是服务器权威数据，物品图标经 DataRegistry 异步取。

### 其他

Steam 联机会话、小地图、Lyra设置系统。

## 环境要求

| 项 | 版本 |
|---|---|
| 引擎 | [Hazelight/UnrealEngine-Angelscript](https://github.com/Hazelight/UnrealEngine-Angelscript)，分支 `angelscript-master`（基于 UE 5.8） |
| IDE | Visual Studio 2022（C++）+ VS Code 或 Rider（AngelScript） |
| 平台 | Windows |

**必须使用 AngelScript 分支引擎**，官方 UE 无法编译本项目——`Plugins/YanGameCore/Script/` 下的 `.as` 脚本承载了大量玩法逻辑。

AngelScript 编辑器支持：

- VS Code：[Unreal Angelscript 扩展](https://marketplace.visualstudio.com/items?itemName=Hazelight.unreal-angelscript)
- Rider：[rider-unreal-angelscript](https://github.com/Hazelight/rider-unreal-angelscript)

## 获取

本项目用 git submodule 引用了五个第三方插件，克隆时一并拉取：

```bash
git clone --recurse-submodules https://github.com/YanLao6/YanGame.git
```

若已克隆但没带 submodule：

```bash
git submodule update --init --recursive
```

| submodule | 用途 |
|---|---|
| `Plugins/OpenUnrealUtilities` | 通用工具库 |
| `Plugins/OUUTags` | GameplayTag 扩展与校验 |
| `Plugins/UE-Blueprint-to-Markdown-Doc` | 蓝图文档导出 |
| `Plugins/GameplayMessageRouter` | 松耦合消息总线 |
| `Plugins/SubsystemBrowserPlugin` | 编辑器子系统浏览器 |

## 构建

1. 克隆并编译 [UnrealEngine-Angelscript](https://github.com/Hazelight/UnrealEngine-Angelscript)（`angelscript-master` 分支）
2. 右键 `YanGame.uproject` → Generate Visual Studio project files，选择该引擎
3. 以 `Development Editor` 或 `DebugGame Editor` 配置编译 `YanGameEditor`
4. 打开 `YanGame.uproject`

改动 C++ 后的单文件快速查错：

```bash
Engine/Build/BatchFiles/Build.bat YanGameEditor Win64 DebugGame ^
  -Project="<路径>/YanGame.uproject" ^
  -SingleFile="<改动的 .cpp>" -NoLink -NoHotReload -NoXGE
```

AngelScript 无需编译，保存 `.as` 即触发热重载，报错输出在 `Saved/Logs/YanGame.log`。

## 克隆后的注意事项

仓库包含完整的代码、配置、蓝图与美术资产，但有两处必须留意，否则打开项目会报错：

**必须带 submodule 克隆。** 五个第三方插件以 submodule 引用，直接 `git clone` 得到的是五个空目录，项目将因缺少模块而无法编译。用上文的 `--recurse-submodules`，或在克隆后补 `git submodule update --init --recursive`。

**必须用 AngelScript 分支引擎。** 用官方 UE 打开会因缺少 Angelscript 插件而失败。

## 目录结构

```
Plugins/YanGameCore/              项目主插件
  Source/YanGameMover/            Mover 移动模式、Transition、LayeredMove（C++）
  Source/YanGameAbility/          GAS 基类与属性集（C++）
  Source/YanGameplay/             Pawn、Controller、PlayerState（C++）
  Source/YanGameUI/               UI 基类（C++）
  Source/HeroAssignmentSystem/    英雄分配（C++）
  Script/                         AngelScript 玩法逻辑
Plugins/EquipManager/             装备与背包
Source/                           项目 Target 与模块入口
Content/                          蓝图、数据资产、界面
.claude/skills/                   各系统设计文档（技能索引 + references）
```

## 文档

`.claude/skills/yangame-systems/references/` 下有各系统的设计文档（英文），说明能力边界、前置条件与既定约定：

| 文档 | 内容 |
|---|---|
| `mover-input-contract.md` | Mover 输入契约（新增移动意图前必读） |
| `experience-pawndata.md` | Experience 与 PawnData |
| `ui-layer-framework.md` | UI 分层框架 |
| `equipment-quickbar.md` | 装备与快捷栏 |
| `interaction-pickup.md` | 交互 / 拾取 / 指示器 |

## 许可

本项目采用 **PolyForm Noncommercial License 1.0.0**：个人学习、教学、研究等非商业用途无需额外授权，商业用途需另行取得书面许可。详见 [LICENSE](LICENSE)。

仓库中的第三方代码与资源不受此许可约束，逐项说明见 [THIRD_PARTY.md](THIRD_PARTY.md)。其中源自 Unreal Engine 与 Lyra 的部分受 Unreal Engine EULA 约束，使用者须自行持有有效的 UE 授权。
