# OpenUnrealUtilities 插件参考

**版本：** 1.5.0  
**作者：** Jonas Reich  
**定位：** 全面的 UE 工具库，覆盖 C++ 模板工具、Blueprint 函数库、编辑器扩展和调试框架。

---

## 模块结构

| 模块 | 类型 | 用途 |
|------|------|------|
| `OUURuntime` | Runtime | 核心 C++ 工具（动画、摄像机、调度器、对象池等） |
| `OUUBlueprintRuntime` | Runtime | Blueprint 可调用的公开 API |
| `OUUUMG` | Runtime | UMG 窗口系统扩展 |
| `OUUEditor` | Editor | 编辑器工具（蓝图/材质/动画操作、资产验证） |
| `OUUDeveloper` | DeveloperTool | 开发期地图窗口、世界统计覆盖层 |
| `OUUTests` / `OUUTestUtilities` | DeveloperTool | 单元测试框架和帮助工具 |

---

## OUUBlueprintRuntime — Blueprint 公开 API

### 剪贴板（ApplicationCore）

- `UClipboardLibrary::ClipboardCopy(Text)` — 将文本复制到操作系统剪贴板
- `UClipboardLibrary::ClipboardPaste()` — 从操作系统剪贴板读取文本

### 核心对象操作（Core）

- `UOUUCoreBlueprintLibrary::GetClassDefaultObject(Class)` — 获取指定类的 CDO
- `UOUUCoreBlueprintLibrary::GetClassDefaultObjectFromObject(Object)` — 从对象实例获取其类的 CDO
- `UOUUCoreBlueprintLibrary::TryGetWorldFromObject(Object)` — 尝试从任意对象获取 UWorld
- `UOUUCoreBlueprintLibrary::ModifyObject(Object)` — 标记对象为已修改（触发序列化脏标记）
- `UOUUCoreBlueprintLibrary::Conv_TopLevelAssetPathToString(Path)` — TopLevelAssetPath 转字符串
- `UOUUCoreBlueprintLibrary::Conv_StringToTopLevelAssetPath(Str)` — 字符串转 TopLevelAssetPath
- `UOUUCoreBlueprintLibrary::Conv_ClassToTopLevelAssetPath(Class)` — Class 转 TopLevelAssetPath

### 配置文件读取（Config）

- `UOUUConfigBlueprintLibrary::GetConfigString(File, Section, Key)` — 读取字符串配置
- `UOUUConfigBlueprintLibrary::GetConfigInt(...)` — 读取整数配置
- `UOUUConfigBlueprintLibrary::GetConfigFloat(...)` — 读取浮点数配置
- `UOUUConfigBlueprintLibrary::GetConfigBool(...)` — 读取布尔值配置
- `UOUUConfigBlueprintLibrary::GetConfigArray(...)` — 读取数组配置
- `UOUUConfigBlueprintLibrary::GetConfigColor/Vector/Rotator(...)` — 读取复合类型配置
- `UOUUConfigBlueprintLibrary::GetConfigIniPath(File)` — 获取配置文件的磁盘路径
- `EGlobalIniFile` — 枚举：`Engine` / `Game` / `Input` / `Editor` 等标准 ini 文件

### 数据表操作（DataTable）

- `UOUUDataTableLibrary::AddRowToDataTable(Table, RowName, RowData)` — 运行时向数据表添加行
- `UOUUDataTableLibrary::RemoveRowFromDataTable(Table, RowName)` — 运行时从数据表移除行

### 字符串与 FName 比较（Lexical）

- `UOUULexicalOperatorLibrary::Less_StringString / Greater_StringString / ...` — 字符串字典序比较（<、>、<=、>=）
- `UOUULexicalOperatorLibrary::Less_NameName / Greater_NameName / ...` — FName 字典序比较

### 引擎编译类型检测（Engine Globals）

- `UEngineGlobalsLibrary::IsEditorBuild()` — 当前是否为 Editor 编译
- `UEngineGlobalsLibrary::IsDebugBuild()` — 当前是否为 Debug 编译
- `UEngineGlobalsLibrary::IsDevelopmentBuild()` — 当前是否为 Development 编译
- `UEngineGlobalsLibrary::IsTestBuild()` — 当前是否为 Test 编译
- `UEngineGlobalsLibrary::IsShippingBuild()` — 当前是否为 Shipping 编译
- `UEngineGlobalsLibrary::IsEditor()` — 是否运行在编辑器中
- `UEngineGlobalsLibrary::IsClient() / IsServer()` — 当前网络角色
- `UEngineGlobalsLibrary::IsInGameThread()` — 当前是否在游戏线程
- `UEngineGlobalsLibrary::IsRunningCommandlet()` — 是否在命令行工具模式下运行

### 世界类型（World）

- `UWorldBlueprintLibrary::GetWorldType(WorldContext)` — 获取 World 类型
- `EBlueprintWorldType` — 枚举：`None` / `Game` / `Editor` / `PIE` / `Preview` / `Inactive`

### 数学扩展（Math）

- `UOUUBlueprintMathLibrary::Add_RotatorRotator(A, B)` — 旋转器加法
- `UOUUBlueprintMathLibrary::Subtract_RotatorRotator(A, B)` — 旋转器减法
- `UOUUBlueprintMathLibrary::GetTransformForwardVector(T)` — 从 Transform 获取前向向量
- `UOUUBlueprintMathLibrary::GetTransformRightVector(T)` — 从 Transform 获取右向向量
- `UOUUBlueprintMathLibrary::GetTransformUpVector(T)` — 从 Transform 获取上向向量

### 文件读写（File I/O）

- `UOUUBlueprintFileHelperLibrary::LoadFileToString(FilePath)` — 从磁盘文件加载字符串
- `UOUUBlueprintFileHelperLibrary::SaveStringToFile(FilePath, Content)` — 将字符串写入磁盘文件

### 项目设置查询（General Project Settings）

- `UGeneralProjectSettingsLibrary::GetProjectName()` — 获取项目名称
- `UGeneralProjectSettingsLibrary::GetProjectVersion()` — 获取项目版本号
- `UGeneralProjectSettingsLibrary::GetProjectCompanyName()` — 获取公司名称
- `UGeneralProjectSettingsLibrary::GetProjectDescription()` — 获取项目描述
- `UGeneralProjectSettingsLibrary::GetProjectHomepageUrl()` — 获取主页 URL
- `UGeneralProjectSettingsLibrary::GetProjectCopyrightNotice()` — 获取版权声明
- `UGeneralProjectSettingsLibrary::GetProjectSupportContact()` — 获取支持联系方式

### 消息对话框（Message Dialog）

- `UMessageDialogLibrary::OpenMessageDialog(Message, Title)` — 打开模态消息对话框
- `UMessageDialogLibrary::OpenMessageDialogWithDefaultValue(...)` — 打开带默认按钮的对话框
- `UMessageDialogLibrary::ShowMessageDialogueNotification(Message)` — 非模态通知（不阻塞）

### 属性路径操作（Property Path）

- `UOUUPropertyPathHelpersLibrary::GetPropertyValueAsString(Object, Path)` — 按属性路径读取值为字符串
- `UOUUPropertyPathHelpersLibrary::SetPropertyValueFromString(Object, Path, Value)` — 按属性路径从字符串写入值

### 平台进程（Platform Process）

- `UGenericPlatformProcessLibrary::GetCurrentWorkingDirectory()` — 获取当前工作目录
- `UGenericPlatformProcessLibrary::ExecutablePath()` — 获取可执行文件完整路径
- `UGenericPlatformProcessLibrary::ExecutableName()` — 获取可执行文件名（不含路径）
- `UGenericPlatformProcessLibrary::ExecProcess(URL, Params, bLaunchDetached)` — 启动子进程
- `UGenericPlatformProcessLibrary::ExecElevatedProcess(URL, Params)` — 以管理员权限启动子进程
- `UGenericPlatformProcessLibrary::LaunchFileInDefaultExternalApplication_Open(FilePath)` — 用默认程序打开文件
- `UGenericPlatformProcessLibrary::LaunchFileInDefaultExternalApplication_Edit(FilePath)` — 用默认程序编辑文件
- `UGenericPlatformProcessLibrary::ExploreFolder(FolderPath)` — 在文件资源管理器中定位文件夹

### Slate 通知（Slate Notification）

- `USlateNotificationLibrary::AddSlateNotification(Info)` — 在编辑器右下角显示通知，返回句柄
- `USlateNotificationLibrary::SetSlateNotificationText(Handle, Text)` — 更新通知文本
- `USlateNotificationLibrary::SetSlateNotificationHyperlink(Handle, Text, URL)` — 设置通知超链接
- `USlateNotificationLibrary::SetSlateNotificationExpireDuration(Handle, Seconds)` — 设置通知持续时间
- `USlateNotificationLibrary::SetSlateNotificationFadeInDuration(Handle, Seconds)` — 设置淡入时间
- `USlateNotificationLibrary::SetSlateNotificationFadeOutDuration(Handle, Seconds)` — 设置淡出时间
- `USlateNotificationLibrary::GetSlateNotificationCompletionState(Handle)` — 获取通知完成状态
- `USlateNotificationLibrary::SetSlateNotificationCompletionState(Handle, State)` — 设置通知完成状态（Success/Fail/Pending）
- `USlateNotificationLibrary::ExpireSlateNotificationAndFadeout(Handle)` — 立即过期并淡出
- `USlateNotificationLibrary::FadeoutSlateNotification(Handle)` — 执行淡出动画
- `USlateNotificationLibrary::PulseSlateNotification(Handle)` — 触发脉冲高亮效果
- `USlateNotificationLibrary::ReleaseSlateNotificationHandle(Handle)` — 释放通知句柄
- `FSlateNotificationInfo` — 通知配置结构（Text、ExpireDuration、FadeInDuration 等）
- `FSlateNotificationHandle` — 通知句柄（用于后续操作）
- `ESlateNotificationState` — 枚举：`Pending` / `Success` / `Fail`

---

## OUURuntime — 核心 C++ 功能

### 摄像机投影（Camera）

- `UOUUSceneProjectionLibrary::ProjectWorldToScreen(...)` — 世界坐标投影到屏幕坐标
- `UOUUSceneProjectionLibrary::DeprojectScreenToWorld(...)` — 屏幕坐标反投影到世界坐标
- `UOUUSceneProjectionLibrary::GetViewProjectionData(...)` — 获取视图投影矩阵数据
- `UOUUGameViewportLibrary::UpdateSplitscreenInfo(...)` — 更新分屏玩家信息
- `UTextureRenderTargetLibrary::GetAverageColor(RenderTarget)` — 采样渲染目标的平均颜色

### 片尾字幕（Credits）

- `UOUUCreditsLibrary::CreditsFromMarkdownString(Markdown)` — 从 Markdown 字符串解析片尾数据
- `FOUUCredits` — 片尾数据根结构
- `FOUUCreditsBlock` — 单个字幕块（标题 + 描述 + 人员列表）
- `FOUUCreditsEntry` — 单条人员信息（姓名 + 职位）
- `UOUUCreditsWidget` — 片尾滚动 UMG Widget

### 流程控制（Flow Control）

- `ExclusiveLock` / `SharedLock` — RAII 风格的排他锁与共享锁
- `FRecursionGuard` — 防止函数递归调用的作用域保护器
- `UOUURequest` / `UOUURequestQueue` — 请求对象与请求队列管理

### 游戏权利管理（Game Entitlements）

- `UOUUGameEntitlementsSubsystem` (Engine Subsystem)
  - `IsEntitled(EntitlementTag)` — 检查当前版本是否具备指定权利（功能开关）
  - `GetActiveEntitlements()` — 获取所有当前有效权利的标签容器
  - `GetActiveVersion()` — 获取当前激活的版本标识
- `FOUUGameEntitlementModule` — 权利模块定义（包含版本与权利标签映射）
- `UOUUGameEntitlementsSettings` — 权利配置（在项目设置中填写）

### Gameplay Ability 扩展（GameplayAbilities）

- `UOUUGameplayAbility` — 自定义 GameplayAbility，提供调试器友元访问
- `UOUUAbilitySystemComponent` — 自定义 ASC，扩展事件历史记录
  - `HandleGameplayEvent(Tag, EventData)` — 处理游戏事件并记录历史

### Gameplay 调试器（GameplayDebugger）

- `FGameplayDebuggerCategory_OUUBase` — 基础 OUU 调试器类别
- `FGameplayDebuggerCategory_ViewModes` — 调试视图模式切换
- `FGameplayDebuggerExtension_ActorSelect` — 交互式 Actor 选择扩展
- `FGameplayDebugger_TreeView` — 树形数据显示调试器
- `FGameplayDebuggerCategory_Animation` — 动画调试类别
- `FGameplayDebuggerCategory_SequentialFrameScheduler` — 帧调度器调试类别
- `FGameplayDebuggerCategory_GameEntitlements` — 权利系统调试类别

### 本地化工具（Localization）

- `UOUUTextLibrary::FormatListText(Items, Separator)` — 将字符串列表格式化为自然语言文本
- `UOUUTextLibrary::JoinBy(Items, Separator)` — 用指定分隔符连接文本数组
- `UOUUTextLibrary::ExportStringTableToCSV(StringTable, Path)` — 导出字符串表为 CSV
- `UOUUTextLibrary::LoadLocalizedTextsFromCSV(Path, Culture)` — 从 CSV 加载本地化文本
- `UOUUTextLibrary::RegisterPluginStringTable(PluginName)` — 注册插件字符串表到引擎
- `ScopedCultureOverride` — RAII 作用域内临时切换 Culture

### 消息日志（Logging）

- `UMessageLogBlueprintLibrary::AddTextMessageLogMessage(Category, Severity, Text)` — 添加纯文本到消息日志
- `UMessageLogBlueprintLibrary::AddTokenizedMessageLogMessage(Category, Severity, Tokens)` — 添加令牌化消息（可点击跳转）
- `UMessageLogBlueprintLibrary::OpenMessageLog(Category)` — 打开消息日志窗口并聚焦指定类别
- `UMessageLogBlueprintLibrary::NotifyMessageLog(Category)` — 通知但不打开消息日志
- `UMessageLogBlueprintLibrary::NewMessageLogPage(Category, Label)` — 创建新的日志分页
- `UMessageLogBlueprintLibrary::CreateAssetNameMessageLogToken(AssetPath)` — 创建可点击的资产名称令牌
- `UMessageLogBlueprintLibrary::CreateObjectMessageLogToken(Object)` — 创建可点击的对象令牌
- `UMessageLogBlueprintLibrary::CreateTextMessageLogToken(Text)` — 创建文本令牌
- `UMessageLogBlueprintLibrary::CreateURLMessageLogToken(URL, Label)` — 创建可点击的 URL 令牌
- `EMessageLogName` — 枚举：常用消息日志类别名称
- `EMessageLogSeverity` — 枚举：`Info` / `Warning` / `Error` / `CriticalError`

### 数学工具（Math）

- `UOUUMathLibrary::AngleBetweenVectors(A, B)` — 计算两向量夹角（度）
- `UOUUMathLibrary::SignedAngleBetweenVectors(A, B, Normal)` — 计算有向夹角
- `UOUUMathLibrary::ClampToRange(Value, Range)` — 限制值到指定范围（模板）
- `UOUUMathLibrary::LinearValueToNormalizedLogScale(Value, Min, Max)` — 线性值转归一化对数刻度
- `SpiralIdUtilities` — 二维螺旋坐标到整数 ID 的互转工具

### 在线功能（Online）

- `UOUUSteamUtils::WriteSteamAppIdToDisk()` — 将 Steam AppId 写入磁盘文件（开发调试用）
- `UOUUSteamUtils::GetSteamAppIdFilename()` — 获取 Steam AppId 文件名

### Actor 对象池（Pooling）

- `UOUUActorPool` (World Subsystem) — 管理可复用 Actor 的对象池
- `IOUUPoolableActor` — 可池化 Actor 接口
  - `CanBePooled()` — 当前是否可回池
  - `OnAddedToPool()` — 回池时的回调
  - `OnRemovedFromPool()` — 出池时的回调
  - `GetMaxPoolSize()` — 该类型的最大池容量
- `FOUUActorPoolSpawnRequest` — Actor 生成请求结构
- `FOUUActorPoolSpawnRequestHandle` — 生成请求句柄
- `EOUUActorPoolSpawnRequestStatus` — 枚举：请求状态（Pending / Active / Completed 等）

### 语义版本（SemVer）

- `USemanticVersionBlueprintLibrary::TryParseSemVerString(Str, OutVer)` — 解析 SemVer 字符串（如 `1.2.3-alpha.1+build.42`）
- `USemanticVersionBlueprintLibrary::IncrementSemVerMajorVersion(Ver)` — 递增主版本号
- `USemanticVersionBlueprintLibrary::IncrementSemVerMinorVersion(Ver)` — 递增次版本号
- `USemanticVersionBlueprintLibrary::IncrementSemVerPatchVersion(Ver)` — 递增补丁版本号
- `USemanticVersionBlueprintLibrary::Equal_SemVerSemVer(A, B)` — 版本相等比较
- `USemanticVersionBlueprintLibrary::Less_SemVerSemVer(A, B)` — 版本 A < B
- `USemanticVersionBlueprintLibrary::Greater_SemVerSemVer(A, B)` — 版本 A > B
- `USemanticVersionBlueprintLibrary::Conv_SemVerString(Ver)` — 版本转字符串
- `FSemanticVersion` — 语义版本结构（Major、Minor、Patch、PreRelease、BuildMetadata）

### 顺序帧调度器（Sequential Frame Scheduler）

- `FSequentialFrameScheduler` — 将多个更新任务分散到多帧执行，避免单帧峰值
  - `Tick(DeltaTime)` — 每帧驱动调度器
  - `AddTask(Callback, IntervalFrames)` — 添加定期执行的任务
  - `AddNamedTask(Name, Callback, IntervalFrames)` — 添加具名任务
  - `TaskExists(Handle)` — 检查任务是否仍在调度
  - `MaxNumTasksToExecutePerFrame` — 每帧最多执行的任务数量上限
- `FSequentialFrameTaskHandle` — 任务句柄（用于后续查询或移除）

### C++ 模板工具库（Templates）

以下工具仅限 C++ 使用：

- `ArrayUtils` — 数组查找、过滤、变换等工具函数
- `BitmaskUtils` — 位掩码操作封装
- `CircularArrayAdaptor` / `CircularAggregator` — 环形缓冲区和环形聚合器
- `RWLockedVariable<T>` — 读写锁保护变量
- `ScopedAssign<T>` — 作用域内临时修改变量值，离开后自动还原
- `ScopedMultiRWLock` — 同时锁定多个读写锁
- `StringUtils` — 字符串分割、格式化等工具
- `StructSerializationHelpers` — 结构体序列化辅助
- `InterfaceUtils` — UInterface 相关的实用函数
- `ReverseIterator` / `IteratorUtils` — 反向迭代器和迭代器工具
- `SubclassWithInterfaces<Base, ...Interfaces>` — 同时满足多接口约束的类型模板

---

## OUUUMG — UMG 窗口系统

- `UOUUWindow` — Blueprint 可继承的独立窗口类
  - `OpenWindow()` — 打开新的系统窗口
  - `IsOpened()` — 检查窗口是否已打开
  - `CloseWindow()` — 关闭窗口
  - `GetWindowParameters()` — 获取当前窗口参数
  - `FOnOUUWindowClosed` — 窗口关闭时触发的委托
- `FOUUWindowParameters` — 窗口配置（标题、初始位置、尺寸、自动居中规则等）

---

## OUUEditor — 编辑器工具

### 通用编辑器操作

- `UOUUEditorLibrary::InvokeSessionFrontend()` — 打开 Session Frontend 窗口
- `UOUUEditorLibrary::RerunConstructionScripts(Actors)` — 对指定 Actor 重新运行构造脚本
- `UOUUEditorLibrary::FocusOnBlueprintContent(Class, Content)` — 在蓝图编辑器中聚焦到指定节点
- `UOUUEditorLibrary::GetCurrentlySelectedBlueprintNodeGuid()` — 获取当前选中蓝图节点的 GUID
- `UOUUEditorLibrary::CreateInstanceComponent(Actor, Class)` — 在 Actor 上创建实例化组件
- `UOUUEditorLibrary::DestroyInstanceComponent(Component)` — 销毁实例化组件

### 动画编辑器操作

- `UOUUAnimationEditorLibrary::ImplementsAnimationLayerInterface(Class)` — 检查动画类是否实现了动画层接口
- `UOUUAnimationEditorLibrary::GetAnimInstanceClassTargetSkeleton(Class)` — 获取动画类对应的目标骨骼
- `UOUUAnimationEditorLibrary::RemoveUnskinnedBonesFromMesh(Mesh)` — 移除网格体中无蒙皮权重的骨骼

### 材质编辑器操作

- `UOUUMaterialEditingLibrary::ConvertMaterialToMaterialAttributes(Material)` — 将材质的主节点转换为 MaterialAttributes 格式
- `UOUUMaterialEditingLibrary::InsertMaterialFunctionBeforeResult(Material, Function)` — 在材质结果节点前注入材质函数
- `UOUUMaterialEditingLibrary::OpenMaterialEditorAndJumpToExpression(Material, Expression)` — 打开材质编辑器并聚焦到指定表达式节点

### 进度对话框（Slow Editor Task）

- `USlowEditorTaskLibrary::StartSlowTask(Message, TotalWork)` — 启动编辑器进度任务
- `USlowEditorTaskLibrary::MakeSlowTaskDialog(Handle)` — 立即显示进度对话框
- `USlowEditorTaskLibrary::MakeSlowTaskDialogDelayed(Handle, Threshold)` — 超过时间阈值后显示对话框
- `USlowEditorTaskLibrary::EnterSlowTaskProgressFrame(Handle, Work, Message)` — 更新进度并推进一帧
- `USlowEditorTaskLibrary::EndSlowTask(Handle)` — 结束进度任务
- `FSlowEditorTaskHandle` — 进度任务句柄

### 资产验证框架（Asset Validation）

- `UOUUActorValidator` — Actor 层面的验证规则实现
- `UOUUBlueprintValidator` — 蓝图资产的验证规则实现
- `UOUUHardReferenceValidator` — 检查不合规的硬引用
- `UOUURestrictedNamesValidator` — 检查受限命名规范
- `UOUUTextValidator` — 检查文本本地化合规
- `UOUUAssetValidationSettings` — 验证规则的项目设置

### 其他编辑器工具

- `SOUUReferenceViewer` — 资产引用关系查看器 Slate 控件
- `UOUUContentBrowserExtensions` — 内容浏览器右键菜单扩展
- `UOUUMaterialAnalyzer` — 材质参数使用分析器
- `OUUValidateAssetListCommandlet` — 批量验证资产的命令行工具 
