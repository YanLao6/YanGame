# 第三方代码与资源

本项目包含以下非本项目原创的代码、插件与资源。第三方部分不受根目录
[LICENSE](LICENSE) 中的项目原创许可约束，使用时须遵守各自的许可条款，
并保留原始版权、许可及署名声明。目录名、类名被修改不改变第三方部分的许可。

本清单依据当前工作区中的插件描述、版权头、许可证、字体 `.tps` 声明及上游资料整理。
「待核对」表示当前证据不足以确认授权范围，不表示已取得使用或再分发许可。
二进制资产的完整来源、购买记录及实际发布包中的许可文件未逐项审计。

## Unreal Engine 官方组件

以下插件源自 Unreal Engine 及 Lyra Starter Game，源码中保留 Epic 版权声明。
引擎代码适用 [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal)；
Lyra 代码与资源应同时核对取得该版本时适用的许可。
Epic 内容的许可入口见 [Epic Content License Agreement](https://www.unrealengine.com/eula/content)。
本清单不转授 Epic 的代码、内容或商标权利。

| 路径 | 来源 |
|---|---|
| `Plugins/AsyncMixin` | Lyra Starter Game |
| `Plugins/CommonGame` | Lyra Starter Game |
| `Plugins/CommonUser` | Lyra Starter Game |
| `Plugins/CommonLoadingScreen` | Lyra Starter Game |
| `Plugins/UIExtension` | Lyra Starter Game |
| `Plugins/PocketWorlds` | Lyra Starter Game |
| `Plugins/GameSubtitles` | Lyra Starter Game |
| `Plugins/EquipManager` | 基于 Lyra 装备系统改写（本项目有修改） |
| `Plugins/GameSettings` | Lyra Starter Game |
| `Plugins/ModularTeams` | 基于 Lyra 队伍系统 |

### 项目模块中的派生代码

第三方部分也可能位于项目自有模块中，不能仅按目录判断许可。
以下实现保留 Epic 版权头，涉及 Lyra 的设置、界面及射击辅助功能；项目修改不替代原有许可。

| 路径或符号 | 内容 |
|---|---|
| `Plugins/YanGameCore/Source/YanGameSetting/` 中保留 Epic 版权头的文件 | 设置注册、输入设置、画面设置、HDR 与安全区域编辑等 |
| `Plugins/YanGameCore/Source/YanGameUI/` 中的 `YanSettingsScreen`、`YanSettingsListEntrySetting_KeyboardInput`、`YanTouchRegion`、`YanSimulatedInputWidget`、`YanJoystickWidget`、`YanLobbyBackground`、`YanMaterialProgressBar` | 设置界面、触屏输入、前端背景与进度显示 |
| `Plugins/GameFeatures/YanShooter/Source/YanShooterRuntime/Private/Input/` | `AimAssistInputModifier` 与 `AimAssistTargetManagerComponent` 射击辅助实现 |

### 美术资源

| 路径 | 说明 |
|---|---|
| `Plugins/YanShooterContent/Content/UI/Foundation/Platform/Input/` | 手柄与键鼠按键图标，源自 Lyra。其中 PlayStation、Xbox、Nintendo Switch 的按键符号涉及各自厂商的权利，本项目不对其主张或转授相关权利 |
| `Plugins/YanShooterContent/Content/UI/` 下部分界面素材 | 源自 Lyra，如 `Keybind-Frame-*`、`M_UI_HollowCircularProgressBar_Base`；字体另见下文 |
| `Plugins/YanShooterContent/Content/Characters/Heroes/Mannequin/` | Lyra 的 Manny、Quinn 角色及关联资源 |
| `Plugins/YanShooterContent/Content/` 与 `Plugins/GameFeatures/YanShooterMaps/Content/` 中的其余资源 | 动画、音效、武器、环境与地图资源的来源及修改范围尚未逐个核实，不能将整个目录视为项目原创内容 |

## 第三方插件（仓库内）

| 路径 | 上游 | 许可与本地依据 |
|---|---|---|
| `Plugins/ModularGameplayAbilities` | [chronicler-software/ModularGameplayAbilities](https://github.com/chronicler-software/ModularGameplayAbilities) | 上游 README 声明 MIT；本项目有修改。部分文件保留 Epic 版权头，应分别核对其原有许可 |
| `Plugins/ModularGameplayActors` | [chronicler-software/ModularGameplayActors](https://github.com/chronicler-software/ModularGameplayActors) | 含 Epic 版权代码；上游未见明确的整体许可，新增或修改部分的授权待确认 |
| `Plugins/ModalCamera` | [chronicler-software/ModalCamera](https://github.com/chronicler-software/ModalCamera) | 插件作者为 Chronicler，源码同时包含 Chronicler 与 Epic 版权头；未见明确的整体许可，不能将整个插件视作 Epic 官方代码 |
| `Plugins/PoolManager` | [JanSeliv/PoolManager](https://github.com/JanSeliv/PoolManager) | MIT，见 [本地 LICENSE](Plugins/PoolManager/LICENSE)；版权人为 Yevhenii Selivanov |
| `Plugins/SettingsWidgetConstructor` | [JanSeliv/SettingsWidgetConstructor](https://github.com/JanSeliv/SettingsWidgetConstructor) | MIT，见 [本地 LICENSE](Plugins/SettingsWidgetConstructor/LICENSE)；版权人为 Yevhenii Selivanov |
| `Plugins/DataAssetsLoader` | [JanSeliv/DataAssetsLoader](https://github.com/JanSeliv/DataAssetsLoader) | [本地 README](Plugins/DataAssetsLoader/README.md) 声明 MIT，但当前插件目录未找到配套 LICENSE；需补齐对应版本的许可与版权声明 |
| `Plugins/Monolith` | [tumourlove/monolith](https://github.com/tumourlove/monolith) | MIT，见 [本地 LICENSE](Plugins/Monolith/LICENSE) 与 [ATTRIBUTION](Plugins/Monolith/ATTRIBUTION.md)；版权人为 tumourlove |

公开发布于 GitHub 不等于授予一般的修改、使用或再分发许可。
未明确授权的部分须核对对应版本的许可或取得权利人的授权，不能以公开可下载代替授权依据。
参见 [GitHub 关于仓库许可的说明](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)。

## 第三方插件（上游引用）

项目保留的 [.gitmodules](.gitmodules) 记录了以下上游引用。
当前 Perforce 工作区中这些目录已有源码；是否随某次交付提供源码，应以实际交付内容为准。
引入方式不改变各组件的许可。

| 路径 | 上游 | 许可 |
|---|---|---|
| `Plugins/OpenUnrealUtilities` | [JonasReich/OpenUnrealUtilities](https://github.com/JonasReich/OpenUnrealUtilities) | MIT，见 [本地 LICENSE](Plugins/OpenUnrealUtilities/LICENSE.md) 与 [CREDITS](Plugins/OpenUnrealUtilities/CREDITS.md) |
| `Plugins/OUUTags` | [JonasReich/OUUTags](https://github.com/JonasReich/OUUTags) | MIT，见 [本地 LICENSE](Plugins/OUUTags/LICENSE.md) 与 [CREDITS](Plugins/OUUTags/CREDITS.md) |
| `Plugins/UE-Blueprint-to-Markdown-Doc` | [wuzhineihan/UE-Blueprint-to-Markdown-Doc](https://github.com/wuzhineihan/UE-Blueprint-to-Markdown-Doc) | 当前本地目录未找到明确许可证；内含 `Plugins/UE-Blueprint-to-Markdown-Doc/Plugins/BP2AI`，描述文件与源码注明 A-Maze Games，须核对原作者授权，不能仅据 fork 地址判断权利归属 |
| `Plugins/GameplayMessageRouter` | [IncantaUnreal/GameplayMessageRouter](https://github.com/IncantaUnreal/GameplayMessageRouter)（Lyra 同名插件的 fork） | 源自 Unreal Engine，受 UE EULA 约束 |
| `Plugins/SubsystemBrowserPlugin` | [aquanox/SubsystemBrowserPlugin](https://github.com/aquanox/SubsystemBrowserPlugin) | MIT，见 [本地 LICENSE](Plugins/SubsystemBrowserPlugin/LICENSE) |

## 商业插件与来源待核对的资源

| 路径 | 来源与许可状态 |
|---|---|
| `Plugins/Marketplace/FPSAnimadd2a4002e2f9V6/` | KINEMATION 的 FPS Animation Ultimate，插件名 `FPSAnimationPack`，本地版本 `2025.2.0`；源码注明保留所有权利。产品资料见 [作者文档](https://kinemation.gitbook.io/fps-animation-pack-unreal)。具体许可以取得该版本时的购买记录及条款为准，当前未核对购买或源码再分发授权 |
| `Content/UltraVolumetrics/` | 当前存在蓝图、材质、贴图及演示地图；仅凭目录名无法确认作者、取得渠道或许可，待核对 |

通过 Marketplace 或 Fab 取得的内容不因放入项目而转为项目原创内容。
应按取得时适用的条款确认团队共享、成品发布及源资产提供范围；
许可入口见 [Fab Standard License](https://www.fab.com/eula) 与
[Epic Content License Agreement](https://www.unrealengine.com/eula/content)。

## 字体

| 路径 | 来源、许可与本地依据 |
|---|---|
| `Plugins/YanShooterContent/Content/UI/Foundation/Fonts/NotoSans/` 中的 Noto Sans 字体 | Google / Noto Project，SIL Open Font License 1.1；本地 `Noto.tps` 指向 [上游 LICENSE](https://github.com/googlefonts/noto-fonts/blob/main/LICENSE)。含 Latin、SC、TC、JP、KR、Arabic 等字体资产 |
| 同目录下的 `NotoColorEmoji.uasset` | Noto Color Emoji 字体；本地 `NotoColorEmoji.tps` 指向 [上游 LICENSE](https://github.com/googlefonts/noto-emoji/blob/main/LICENSE)，当前上游声明 SIL Open Font License 1.1；应核对所导入版本的许可，不将该结论扩展到上游所有图片或代码 |
| `Plugins/YanShooterContent/Content/UI/Foundation/Fonts/Orbitron/` | Orbitron，SIL Open Font License 1.1，见 [上游 OFL](https://github.com/google/fonts/blob/main/ofl/orbitron/OFL.txt)。包含 Black、Bold、Medium 的原始 TTF 与 UE 字体资产；本地 `Raw/Orbitron.tps` 记录从 Lyra 引入的许可信息 |

分发字体时须保留对应版本的版权声明和 OFL 许可，并遵守保留字体名称等条件。
`.tps` 用于记录来源；当前工作区未找到其引用的 Lyra
`Content/Legal/ThirdParty/Licenses/Orbitron_LICENSE.txt`，应核对字体交付中的完整声明。

## 引擎与外部依赖

本项目依赖 [Hazelight 的 Unreal Engine AngelScript 分支](https://github.com/Hazelight/UnrealEngine-Angelscript)，
引擎及 AngelScript 集成位于引擎目录，不在本项目的 `Plugins/` 清单中。
该依赖不由项目根目录 `LICENSE` 重新授权，应查阅所使用引擎版本随附的许可。

`YanGame.uproject` 中启用的引擎插件与工具不等于本项目包含它们的源码。
引擎自带第三方软件的许可入口包括 `Engine/Source/ThirdParty/Licenses/` 与
`Engine/Extras/ThirdPartyNotUE/`；其他外部插件应查阅各自安装目录及许可。

## 维护与交付核对

- 新增或迁移第三方内容时记录真实路径、上游、所用版本、修改范围及许可依据。
- 随交付内容保留对应的 LICENSE、NOTICE、版权头与必要署名；本清单不代替完整许可文本。
- 待核对项包括未明确授权的 Chronicler 插件、BP2AI、商业资源的取得记录，以及缺失的 DataAssetsLoader 与字体许可附件。
- 发布前按实际提供的源码、源资产与打包成品分别核对许可；禁用插件不会自动消除其源码或资源随交付提供的问题。
