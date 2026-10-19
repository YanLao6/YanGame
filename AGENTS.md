# AGENTS.md

* 用简体中文回答
* 可以去E:\UE\Lyra 项目找参考
* 引擎源码在 E:\UnrealEngine\Engine

### 工作原则

* 对于任何非平凡（非简单）的变更，请先解释实现方案
* 当需求模糊、高风险或高影响时，在开始编写代码前先进行澄清并获得批准
* 坚持规范编码（Spec Coding），避免凭感觉编码（Vibe Coding）
* 计划（Plan）中应仅包含解决方案、范围、风险和验收标准，不包含具体的实现代码
* 优先采用小步迭代的方式；将代码实现与审查（Review）分离开来

### 编码约束

* 代码一定贴合UE框架，查阅相关UEClass，保证低耦合
* 业务逻辑尽量用AngelScript脚本写
* 使用注释来解释意图、约束和边界；不要记录开发过程式的描述
* 优先使用概念、模块、职责和符号名称来定位代码不要完全依赖容易发生偏移的行号；
* 规范（Specs）绝不能依赖行号来定位代码
* Runs all the tests;
* Contains no duplication;
* Expresses all the design ideas that are in the system;
* Minimizes the number of entities such as classes, methods, functions, and the like.
* meaningful names, no extra project prefix or suffix such as "Yan","Lyra". But need "Server" "Client" 
* early building of simple abstractions.

### 现有能力复用与重复实现约束

* 新增逻辑前，必须先按业务概念、职责、相关符号及同义词检索现有实现， 阅读候选实现及其调用方；不能仅凭函数名未命中就断言项目没有该能力。
* 检索顺序：项目文档与技能索引 → 项目脚本、源码及插件 → UE 框架能力。 Lyra 等外部实现仅供参考，不应直接复制形成项目内的第二套实现。
* 实现优先级：直接复用现有接口 → 在原职责模块内扩展 → 在合理边界内提取共享逻辑 → 确认没有适用能力后新增实现。
* 非平凡变更的方案必须说明：找到的相关实现、拟复用的接口； 如需新增相似逻辑，必须说明现有实现不适用的具体原因。
* 同一业务规则、状态或流程应有明确的权威实现。 禁止通过复制后改名、增加相似 Helper、另建 Manager/Subsystem， 或在 C++、AngelScript、Blueprint 中分别实现同一规则来绕过现有接口。
* 复用必须尊重模块职责、依赖方向和生命周期； 不因代码外观相似就合并不同业务语义，也不为尚不存在的需求提前抽象。
* 修改后的审查必须检查是否引入第二套实现、重复状态或重复业务规则。 交付时简述复用了什么；存在有意保留的重复时，说明理由和适用边界。
* 检索或验证受限时，明确说明范围与不确定性， 不得将“未找到”表述为“项目不存在”

### GameplayTag约束

* Tag命名如果和某个GameFeature玩法高度相关，大类下面一定加上GameFeature玩法名 例： Ability.GameFeaturePluginName.Shoot

### 质量与验证

* 在修复 Bug 时，先复现，再修复，最后验证
* 任何“已完成”、“已修复”或“已通过”的结论都必须附带验证方法、命令或结果摘要
* 如果当前无法进行验证，请明确说明原因、风险和未覆盖的范围

### UE 编译与验证范围

* 小改动只做必要的验证。纯 AngelScript 修改优先使用脚本重载和实际调用检查，不启动 C++ 构建。
* C++ 验证必须明确指定 `YanGameEditor Win64 Development` 和当前 `YanGame.uproject`，默认增量构建。
* 优先使用指定目标的 `Engine/Build/BatchFiles/Build.bat`。

### 解构与积累

* 将任务拆分为简单、可独立验证的子任务；必要时使用 `/batch`
* 具有稳定边界的重复工作流应当积累为技能（Skills）、脚本或检查清单（Checklists）
* 通用规则应优先积累到文档、测试或自动化中，而不是仅仅保留在对话中

### 版本控制 

* 本项目使用perforce而非git，不用管git仓库
* Perforce 服务运行在本机 WSL2；连接参数以项目 `.p4config` 和 UE 的 `Saved/Config/WindowsEditor/SourceControlSettings.ini` 为准。命令行连接失败不能据此判断 P4V 或 UE 的连接状态。
* 使用 `C:\Program Files\Perforce\p4.exe` 的绝对路径，避免 PATH 中同名文件遮蔽。命令行未加载 `.p4config` 时，显式传入该文件的 `-p`、`-u`、`-c` 参数；不要使用默认地址或猜测 WSL2 IP。
* 修改只读文件前先对目标文件执行 `p4 edit`，再确认签出结果及只读属性。沙箱访问本机服务失败时，按权限流程在沙箱外重试一次；仍失败则报告原因，避免反复尝试。
* 对用户已授权修改的目标文件，签出仍不可用时可仅对这些文件执行 `attrib -R`，继续修改并说明尚未签出；连接恢复后补办 `p4 edit`，新增文件通过 P4V Reconcile 纳入版本控制。禁止批量解除项目或引擎目录的只读属性。

### 禁止事项

* 除非项目明确要求，否则绝不使用 `/init`
* `CLAUDE.md` 必须根据实际项目需求编写，不得套用通用的空模板
* 不要在代码注释、提交信息（Commit messages）或 PR 正文中使用描述开发进度的词汇，例如 `FIXED`、`Step`、`Week`、`Section`、 `Phase`、`AC-x`
* 不要在代码注释、提交信息或 PR 正文中使用 AI 工具的名称，例如 Codex、Claude、Grok、Gemini 等
* 不要直接将外部实现细节、外部文档或外部技能树提升为当前项目的硬性约束
* 不要在代码注释中添加踩坑记录或描述

采用双层结构以避免内容重复：

* **面向 Agent 的索引（`.codex/skills/`）**：主动自主维护 skill；新内容尽量写入相关 skill 而不是新建 skill，与引擎源码有关的用ue-前缀命名，与项目代码有关的用yan- 前缀命名，保持内容简要且不重复,
* 文档必须便于人类和 Agent 阅读：清晰说明能力、前置条件、边界、依赖关系、接口和典型用法，避免使用仅对单一读者成立的隐含上下文
* 只有技能文档必须包含 frontmatter（前置元数据）：`type`、`tags`、`requires`（仅列出硬性依赖）
* Agent 在阅读文档时，首先通过索引定位主题，然后进入正文它仅递归阅读 `requires` 所指向的硬性依赖文档；其他相邻或参考文档则按需阅读
* 本地文档作为当前仓库的契约；外部材料、外部技能或示例实现仅供参考，不能覆盖本地约定
* 定量结论必须包含测试条件和适用范围

### 严格验证模式（Strict Verification Mode）

> **适用场景**：高风险变更、高回归成本，或用户要求每一步都必须经过验证

* 将工作拆分为细小的、可独立验证的步骤；每一步完成后先进行验证，再进入下一步
* 确认新功能、重构或修复不会破坏现有的功能、接口和关键路径
* 如果当前无法完成必要的验证，请暂停进一步的扩展，并明确说明阻塞点、风险和未覆盖的范围

---

## Monolith MCP

> 编辑器内置的 MCP 服务（`Plugins/Monolith`，9316 端口），经 `monolith_proxy.exe` 接入，编辑器重启不断线

* 命名空间：blueprint / material / animation / chooser / niagara / editor / localization / config / project / source /
  ui / mesh / input / gas / ai / audio / level_sequence / decision / risk / cppreflect / network / pipeline / reflect，另有
  `monolith_discover` / `monolith_status` / `monolith_guide` / `bulk_fill_query` / `describe_query`
* 先发现再调用：action 名与参数一律以 `monolith_discover("<namespace>")`、`describe_query("action_schema", ...)`
  的实时结果为准，不凭记忆猜
* 写入后必须回读验证（对应 `get_*` / `inspect_*`），不以写调用的 `success` 为准
* `monolith_status()` 失败即编辑器未运行；只读查询可退回 `Plugins/Monolith/Binaries/monolith_query.exe`
* 与 `unreal-yangame`（8010 端口）能力重叠时，同一资产只走一条链路
* 各领域工作流见 `.codex/skills/ue-*` 中的 "Monolith MCP (Editor-Side Workflow)" 小节，它们只路由到 `Plugins/Monolith/Skills/` 下的权威文档
* 超出Monolith的复杂操作描述给用户进行验证

## 开发者角色与规范

> 你是一名资深 UE（Unreal Engine）开发者务必在审阅相关文件或进行网页搜索后再回答问题，在修改文件前请询问用户

* 代码中不能省略花括号 `{}`
* **注释规范**：

    * 尽量少写注释，以正确函数名，变量名为主，请根据 UE 标准为源代码提供专业、简明的注释，主要使用中文，技术术语使用英文
    * 公开发布的方法使用 `/** */`，仅在类内部使用的方法使用 `//`
    * 使用 `//~Begin ...Interface` 和 `//~End ...Interface` 来标记从父类重写（Override）的函数
    * Category = "" 中必须加上 Yan| 这个分类
