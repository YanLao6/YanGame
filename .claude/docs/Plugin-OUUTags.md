# OUUTags 插件参考

**版本：** 1.0.0  
**作者：** Jonas Reich & Contributors  
**定位：** 高级 GameplayTag 系统扩展，提供类型安全的字面标签、类型化标签、依赖管理和验证框架。

---

## 模块结构

| 模块 | 类型 | 用途 |
|------|------|------|
| `OUUTags` | Runtime | 核心运行时：字面标签、类型化标签、依赖接口、查询解析 |
| `OUUTagsEditor` | Editor | 编辑器验证子系统、属性面板自定义 |
| `OUUTagsTests` | DeveloperTool | 单元测试和规格测试 |

---

## OUUGameplayTagLibrary — 基础标签 Blueprint API

- `UOUUGameplayTagLibrary::GetParentTag(Tag)` — 获取标签的直接父标签
- `UOUUGameplayTagLibrary::GetChildTags(Tag, MaxDepth)` — 获取指定标签的所有子标签（支持深度限制）
- `UOUUGameplayTagLibrary::GetAllTagsInContainer(Container)` — 将 FGameplayTagContainer 转换为数组
- `UOUUGameplayTagLibrary::CreateTagContainerFromArray(Tags)` — 从数组创建 FGameplayTagContainer
- `UOUUGameplayTagLibrary::GetTagDepth(Tag)` — 获取标签层级深度（以 `.` 分隔符计数）
- `UOUUGameplayTagLibrary::GetTagUntilDepth(Tag, Depth)` — 截取标签到指定深度（返回前缀子标签）
- `UOUUGameplayTagLibrary::GetTagComponents(Tag)` — 将标签名分解为 FName 组件数组
- `UOUUGameplayTagLibrary::CreateTagFromComponents(Components)` — 从 FName 组件数组重新拼合标签

---

## 字面标签系统（Literal Gameplay Tags）

通过宏在 C++ 中声明编译期类型安全的标签层级，自动注册为原生 GameplayTag。

### 核心宏

```cpp
// 在 .h 中声明标签层级
OUU_DECLARE_GAMEPLAY_TAGS_START(MODULE_API, TagType, "RootName", "描述")
    OUU_GTAG(Foo, "叶子标签")
    OUU_GTAG_GROUP_START(Bar, "父标签")
        OUU_GTAG(Alpha, "Bar 的子标签 Alpha")
        OUU_GTAG(Beta, "Bar 的子标签 Beta")
    OUU_GTAG_GROUP_END(Bar)
OUU_DECLARE_GAMEPLAY_TAGS_END(TagType)

// 在 .cpp 中定义实例
OUU_DEFINE_GAMEPLAY_TAGS(TagType)
```

### 扩展已有标签层级

```cpp
// 允许其他模块向已有字面标签类型追加新标签
OUU_DECLARE_GAMEPLAY_TAGS_EXTENSION_START(MODULE_API, NewTagType, ExistingTagType)
    OUU_GTAG(Extra, "扩展的叶子标签")
OUU_DECLARE_GAMEPLAY_TAGS_END(NewTagType)
```

### 标签标志枚举（ELiteralGameplayTagFlags）

- `AutoRegister` — 自动在引擎中注册为原生标签（推荐默认开启）
- `AllowContentChildTags` — 允许编辑器中从内容添加子标签（即允许非原生子标签）
- `Inherited` — 从父标签继承标志
- `Explicit` — 显式指定，不继承
- `Default` — 推荐组合（`AutoRegister`）

### TLiteralGameplayTag 模板访问方式

```cpp
// 获取 FGameplayTag 值
FGameplayTag Tag = FMyTags::Bar::Alpha::GetTag();

// 获取完整标签名称字符串
FName Name = FMyTags::Bar::Alpha::GetName();
```

---

## 类型化标签系统（Typed Gameplay Tags）

为 GameplayTag 增加运行时类型约束，防止将错误分类的标签赋值给特定属性。

### 声明类型化标签

```cpp
// 通过 IMPLEMENT_TYPED_GAMEPLAY_TAG 宏定义
// 绑定到一个或多个字面标签根，只允许这些根的子标签通过类型检查
IMPLEMENT_TYPED_GAMEPLAY_TAG(FMyAbilityTag, FMyTags::Ability)
```

### TTypedGameplayTag 模板

- `TryConvert(Tag)` — 尝试将普通 FGameplayTag 转换为该类型（不带检查，返回可选值）
- `ConvertChecked(Tag)` — 带检查的强制转换（类型不符时触发断言）
- `GetAllRootTags()` — 获取该类型允许的所有根标签
- `GetAllLeafTags()` — 获取该类型下的所有叶子标签

### 类型化容器

```cpp
// 值类型容器（适合函数返回值）
TTypedGameplayTagContainerValue<FMyAbilityTag>

// 引用类型容器（适合 UPROPERTY 属性声明）
TTypedGameplayTagContainerReference<FMyAbilityTag>
```

两种容器均完整镜像 `FGameplayTagContainer` 的 API：

- `HasTag(Tag)` / `HasTagExact(Tag)` — 检查是否包含标签
- `HasAny(Container)` / `HasAll(Container)` — 集合匹配
- `AddTag(Tag)` / `RemoveTag(Tag)` — 添加/移除单个标签
- `AppendTags(Container)` — 追加多个标签
- `Filter(Container)` / `MatchesQuery(Query)` — 过滤与查询
- `Num()` / `IsEmpty()` / `IsValid()` — 状态检查
- `Reset()` — 清空容器
- `GetGameplayTagArray()` — 转为数组
- 支持范围 for 循环

---

## FTypedGameplayTagContainer — 运行时类型检查容器

可用于 `UPROPERTY`，在编辑器中通过属性面板进行类型过滤编辑。

### 创建

```cpp
// 通过静态工厂方法创建，绑定标签类型
FTypedGameplayTagContainer Container = FTypedGameplayTagContainer::Create<FMyAbilityTag>(InitialTags);
```

### 公开方法

- `GetTags()` — 获取底层 `FGameplayTagContainer`
- `GetTypedTagName()` — 获取绑定的类型化标签名称（用于运行时识别类型）
- `SetTags(Container)` — 设置所有标签（自动过滤不符合类型的标签）
- `AddTag(Tag)` / `RemoveTag(Tag)` — 添加/移除单个标签
- `AppendTags(Container)` — 批量追加
- `RemoveTags(Container)` — 批量移除
- `Reset()` — 清空

### Blueprint 操作库（UTypedGameplayTagContainerLibrary）

- `UTypedGameplayTagContainerLibrary::SetTypedContainerTags(Container, Tags)` — 设置容器所有标签
- `UTypedGameplayTagContainerLibrary::AddTagToTypedContainer(Container, Tag)` — 添加标签
- `UTypedGameplayTagContainerLibrary::AppendTagsToTypedContainer(Container, Tags)` — 追加标签
- `UTypedGameplayTagContainerLibrary::RemoveTagFromTypedContainer(Container, Tag)` — 移除单个标签
- `UTypedGameplayTagContainerLibrary::RemoveTagsFromTypedContainer(Container, Tags)` — 批量移除
- `UTypedGameplayTagContainerLibrary::ResetTypedContainer(Container)` — 清空容器

---

## IGameplayTagDependencyInterface — 标签依赖管理

允许 UObject 之间建立标签依赖链：子对象的标签自动包含父对象或依赖对象的标签，并在标签变化时广播事件。

### 核心数据结构

- `FGameplayTagDependencyChange` — 记录一次标签变化（新增标签集、移除标签集、当前全量标签）
- `FGameplayTagDependencyEvent` — 单播委托（单个监听者绑定）
- `FGameplayTagDependencyMulticastEvent` — 多播委托（多个监听者绑定）

### Interface 公开方法

- `AppendTags(OutContainer)` — 获取对象所有标签（含自身标签 + 所有依赖项的标签）
- `AppendOwnTags(OutContainer)` — 仅获取此对象自身持有的标签（子类覆写实现）
- `BroadcastTagsChanged(Change)` — 向所有监听者广播标签变化事件
- `BindEventToOnTagsChanged(Delegate)` — 注册标签变化回调
- `UnbindEventFromOnTagsChanged(Delegate)` — 取消注册标签变化回调
- `AddDependency(Object)` — 将 `Object` 添加为标签依赖来源（Object 的标签变化会影响本对象）
- `RemoveDependency(Object)` — 移除依赖关系
- `GetImmediateTagSources()` — 获取直接依赖来源的映射表（来源对象 → 贡献的标签集）
- `GetOriginalTagSources()` — 深度追踪原始标签来源（穿透依赖链，定位最初持有标签的对象）

---

## GameplayTag 查询解析器

- `FGameplayTagQueryParser::ParseQuery(QueryString)` — 将字符串格式的标签查询解析为 `FGameplayTagQuery`

支持的语法示例：

```
ANY(Tag.A, Tag.B)
ALL(Tag.C, Tag.D)
NOT(Tag.E)
ANY(ALL(Tag.F, Tag.G), Tag.H)
```

---

## UTypedGameplayTagSettings — 全局配置

- `UTypedGameplayTagSettings::GetAdditionalRootTags(TagType)` — 获取在项目设置中为该类型额外配置的根标签
- `UTypedGameplayTagSettings::ForEachAdditionalRootTag(TagType, Callback)` — 迭代额外根标签
- `UTypedGameplayTagSettings::AddNativeRootTags(TagType, Tags)` — 在代码中注册原生根标签
- `UTypedGameplayTagSettings::GetAllRootTags(TagType)` — 获取所有根标签（原生 + 项目设置中额外配置的）
- `UTypedGameplayTagSettings::GetAllLeafTags(TagType)` — 获取该类型下的所有叶子标签
- `FTypedGameplayTagSettingsEntry` — 配置条目（原生根标签列表、额外根标签列表、注释）

> 在 **Project Settings → OUU Tags → Typed Gameplay Tag Settings** 中配置。

---

## OUUTagsEditor — 编辑器验证框架

### UGameplayTagValidationSettings（全局验证规则）

在 **Project Settings → OUU Tags → Gameplay Tag Validation** 中配置。

| 属性 | 说明 |
|------|------|
| `MaxGlobalTagDepth` | 全局最大标签层级深度（默认 10） |
| `NativeTagAllowedChildDepth` | 原生标签允许的内容子标签深度（默认 3） |
| `bAllowContentRootTags` | 是否允许在内容层创建根标签 |
| `bDefaultAllowContentTagChildren` | 原生标签下是否默认允许内容子标签 |
| `bValidateTagsAfterStartup` | 编辑器启动后自动验证标签树 |
| `bValidateTagsAfterTagTreeChange` | 标签树发生变化后自动重新验证 |
| `bValidateTagsDuringCook` | Cook 阶段执行标签验证 |
| `WarnOnlyGameplayTags` | 只生成警告而不报错的标签集合（白名单）|

- `UGameplayTagValidationSettings::RefreshNativeTagOverrides()` — 强制刷新原生标签覆写配置
- `UGameplayTagValidationSettings::FindTagOverride(Tag)` — 查找某个标签的验证规则覆写
- `FGameplayTagValidationSettingsEntry` — 单条覆写规则（`bCanHaveContentChildren`、`AllowedChildDepth`）

### UGameplayTagValidatorSubsystem（Editor Subsystem）

- `UGameplayTagValidatorSubsystem::Get()` — 获取子系统实例
- `UGameplayTagValidatorSubsystem::ValidateGameplayTagTree()` — 手动触发完整标签树验证

> 子系统自动监听标签树变化事件，按配置规则触发验证。

### UGameplayTagValidator_Base（自定义验证器基类）

继承此类可实现自定义验证规则：

- `InitializeValidator()` — 验证器初始化（纯虚，必须覆写）
- `ValidateTag(Tag, Settings)` — 验证单个标签（纯虚，返回 `bool` 表示是否继续递归验证子标签）

### FTypedGameplayTagContainer_PropertyTypeCustomization

在编辑器细节面板中为 `FTypedGameplayTagContainer` 属性提供自定义 UI，自动按类型过滤可选标签。

---

## 日志

- `LogOUUTags` — 插件标准日志类别，用于所有 OUUTags 相关日志输出

---

## 使用示例

### 声明并使用字面标签

```cpp
// MyTags.h
OUU_DECLARE_GAMEPLAY_TAGS_START(MYGAME_API, FMyTags, "Game", "项目游戏标签")
    OUU_GTAG_GROUP_START(Ability, "技能分类标签")
        OUU_GTAG(Active, "主动技能")
        OUU_GTAG(Passive, "被动技能")
    OUU_GTAG_GROUP_END(Ability)
OUU_DECLARE_GAMEPLAY_TAGS_END(FMyTags)

// MyTags.cpp
OUU_DEFINE_GAMEPLAY_TAGS(FMyTags)

// 使用
FGameplayTag AbilityTag = FMyTags::Ability::Active::GetTag();
```

### 声明并使用类型化标签

```cpp
// 声明只接受 Game.Ability 子标签的类型
IMPLEMENT_TYPED_GAMEPLAY_TAG(FAbilityTag, FMyTags::Ability)

// 在组件属性上使用类型约束
UPROPERTY(EditAnywhere)
TTypedGameplayTagContainerReference<FAbilityTag> GrantedAbilities;

// 类型转换
if (auto TypedTag = FAbilityTag::TryConvert(SomeTag))
{
    // TypedTag 已通过类型检查
}
```
