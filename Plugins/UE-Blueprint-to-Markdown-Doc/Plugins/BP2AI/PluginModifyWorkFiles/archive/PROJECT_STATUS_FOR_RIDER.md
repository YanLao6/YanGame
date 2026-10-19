# BP2AI 项目开发状态报告

**生成日期**: 2025-01-14  
**目标平台**: Unreal Engine 5.4  
**IDE 迁移**: VSCode → JetBrains Rider  
**当前阶段**: Task 1.3 - 等待编译测试

---

## 🎯 项目目标

### **核心需求**
将 Unreal Engine 蓝图(Blueprint)自动导出为 AI 可理解的结构化文档,支持人机协作开发。

### **导出内容 (6层数据结构)**
1. **Blueprint Metadata** - 类名、父类、路径、接口
2. **Components** - 组件层次结构与类型
3. **Variables** - 名称、类型、默认值、可见性
4. **Functions/Events** - 名称、参数、返回值、是否纯函数
5. **Graph Logic** - 各图表内部的节点逻辑 (复用 BP2AI)
6. **References** - 依赖的其他蓝图或资源

### **实现方案**
- **Graph Logic**: 复用现有的 BP2AI 插件核心功能
- **其他数据**: 直接读取 UBlueprint API
- **最终目标**: 批量导出整个项目的所有蓝图

---

## 📁 项目结构

### **工作目录**
```
c:\Users\Dau\Desktop\5.4BP2AI\BP2AI\
```

### **核心插件结构**
```
BP2AI/
├── BP2AI.uplugin                          # 插件描述文件
├── Source/BP2AI/
│   ├── BP2AI.Build.cs                     # 构建配置
│   ├── Public/
│   │   ├── BP2AI.h                        # 模块主头文件
│   │   ├── Exporters/
│   │   │   └── BP2AIBatchExporter.h       # ✅ 新增:批量导出器
│   │   └── ...
│   └── Private/
│       ├── BP2AI.cpp                       # 模块实现
│       ├── Exporters/
│       │   └── BP2AIBatchExporter.cpp     # ✅ 新增:批量导出器实现
│       ├── Test/
│       │   ├── CurrentPhaseTest.h         # 测试框架
│       │   └── CurrentPhaseTest.cpp       # ✅ 已修改:添加 Task 1.3 测试
│       ├── Trace/
│       │   └── ExecutionFlow/
│       │       └── ExecutionFlowGenerator.h/.cpp  # BP2AI 核心生成器
│       └── Widgets/
│           └── SMarkdownOutputWindow.cpp  # BP2AI UI 窗口
└── Resources/
    └── modern-ui.js                        # HTML 输出的 JS 支持
```

---

## 📋 任务进度追踪

### **已维护的文档 (必读!)**

| 文档 | 路径 | 用途 | 状态 |
|------|------|------|------|
| **任务路线图** | `TASK_ROADMAP.md` | 21个任务的清单和进度 | 🔄 持续更新 |
| **技术笔记** | `TECHNICAL_NOTES.md` | BP2AI 工作原理分析 | ✅ 完成 Task 1.2 |
| **测试指南** | `TASK_1.3_TEST_GUIDE.md` | Task 1.3 的详细测试步骤 | ✅ 已创建 |
| **对话记录** | `conversation.txt` | 需求讨论记录 | 📖 参考 |

### **当前进度**

```
阶段 1: 理解与准备
  ✅ Task 1.1 - 理解 BP2AI 架构
  ✅ Task 1.2 - 分析 BP2AI 入口点
  🔄 Task 1.3 - 编写自动化导出测试函数 (代码已完成,等待测试)

阶段 2: 导出 Graph Logic (0/3)
阶段 3: 导出其他元数据 (0/5)
阶段 4: 整合与格式化输出 (0/3)
阶段 5: 批量导出功能 (0/3)
阶段 6: Python API 集成 (0/2)
阶段 7: 测试与文档 (0/3)

总进度: 10% (2/21 任务完成)
```

---

## 🔍 核心技术发现

### **BP2AI 的工作原理**

#### **调用链**
```
用户点击工具栏按钮
  ↓
FBP2AIModule::GenerateExecFlowAction()
  ↓
创建 SMarkdownOutputWindow 窗口
  ↓
用户点击 "Refresh Flow"
  ↓
GetSelectedBlueprintNodes() - 获取编辑器选中的节点
  ↓
FExecutionFlowGenerator::GenerateDocumentForNodes()
  ↓
返回 Markdown/HTML 文本
```

#### **关键洞察**
```cpp
// BP2AI 的本质是一个"节点数组 → 文档"的转换器
FString MarkdownOutput = FExecutionFlowGenerator::GenerateDocumentForNodes(
    AllNodes,    // TArray<UEdGraphNode*> - 不需要用户选中!
    Settings,    // 生成配置
    Context      // 输出格式 (Markdown/HTML)
);
```

**重要**: BP2AI 不依赖 UI,只需要节点数组作为输入!

---

## ✅ 已完成的工作

### **Task 1.1: 理解架构**
- ✅ 分析了 BP2AI 的三个核心组件:
  - `FBlueprintDataExtractor` - 提取节点数据
  - `FMarkdownDataTracer` - 追踪执行流
  - `FExecutionFlowGenerator` - 生成文档

### **Task 1.2: 分析入口点**
- ✅ 找到了 BP2AI 如何获取选中节点:
  ```cpp
  // Source/BP2AI/Private/Widgets/SMarkdownOutputWindow.cpp:426
  TArray<UEdGraphNode*> SMarkdownOutputWindow::GetSelectedBlueprintNodes() const
  {
      // 通过 UAssetEditorSubsystem 获取当前蓝图编辑器
      // 调用 FBlueprintEditor::GetSelectedNodes()
      // 返回选中的节点数组
  }
  ```

- ✅ 理解了核心生成器的接口:
  ```cpp
  // Source/BP2AI/Private/Trace/ExecutionFlow/ExecutionFlowGenerator.h
  FString GenerateDocumentForNodes(
      const TArray<UEdGraphNode*>& SelectedNodes,
      const FGenerationSettings& Settings,
      FMarkdownGenerationContext& Context
  );
  ```

### **Task 1.3: 编写自动化导出器 (代码完成,待测试)**

#### **新增文件 1: `BP2AIBatchExporter.h`**
**路径**: `Source/BP2AI/Public/Exporters/BP2AIBatchExporter.h`

**功能**: 批量导出器的头文件

**核心方法**:
```cpp
class BP2AI_API FBP2AIBatchExporter
{
public:
    // 导出单个图表
    FString ExportSingleGraph(UEdGraph* Graph, bool bIncludeNestedFunctions = true);
    
    // 导出蓝图的所有图表 (EventGraph, Functions, Macros, Delegates)
    TMap<FString, FString> ExportAllGraphsFromBlueprint(UBlueprint* Blueprint, bool bIncludeNestedFunctions = true);
    
    // 静态测试函数
    static bool TestExportBlueprint(const FString& BlueprintPath);
};
```

#### **新增文件 2: `BP2AIBatchExporter.cpp`**
**路径**: `Source/BP2AI/Private/Exporters/BP2AIBatchExporter.cpp`

**实现逻辑**:
```cpp
FString FBP2AIBatchExporter::ExportSingleGraph(UEdGraph* Graph, bool bIncludeNestedFunctions)
{
    // 1. 获取图表的所有节点
    TArray<UEdGraphNode*> AllNodes = Graph->Nodes;
    
    // 2. 创建默认配置
    FGenerationSettings Settings;
    Settings.bTraceAllSelectedExec = true;
    Settings.bDefineUserGraphsSeparately = true;
    // ... 其他配置
    
    // 3. 创建 Markdown 输出上下文
    FMarkdownGenerationContext Context(
        FMarkdownGenerationContext::EOutputFormat::RawMarkdown
    );
    
    // 4. ✅ 核心:调用 BP2AI 的生成器
    FExecutionFlowGenerator Generator;
    FString MarkdownOutput = Generator.GenerateDocumentForNodes(
        AllNodes, 
        Settings, 
        Context
    );
    
    return MarkdownOutput;
}

TMap<FString, FString> FBP2AIBatchExporter::ExportAllGraphsFromBlueprint(
    UBlueprint* Blueprint, 
    bool bIncludeNestedFunctions)
{
    TMap<FString, FString> Results;
    
    // 遍历所有图表类型
    for (UEdGraph* Graph : Blueprint->UbergraphPages)      // 事件图表
        Results.Add("EventGraph_" + Graph->GetName(), ExportSingleGraph(Graph));
    
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)      // 函数图表
        Results.Add("Function_" + Graph->GetName(), ExportSingleGraph(Graph));
    
    for (UEdGraph* Graph : Blueprint->MacroGraphs)         // 宏图表
        Results.Add("Macro_" + Graph->GetName(), ExportSingleGraph(Graph));
    
    for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs) // 委托图表
        Results.Add("Delegate_" + Graph->GetName(), ExportSingleGraph(Graph));
    
    return Results;
}
```

**关键点**:
- ✅ 不依赖 UI - 完全程序化
- ✅ 支持所有图表类型
- ✅ 详细的日志输出
- ✅ 错误处理

#### **修改文件: `CurrentPhaseTest.cpp`**
**路径**: `Source/BP2AI/Private/Test/CurrentPhaseTest.cpp`

**改动**:
```cpp
#include "Exporters/BP2AIBatchExporter.h" // ✅ 新增引用

void ExecuteCurrentPhaseTest()
{
    // 测试蓝图路径 (用户需要根据实际情况修改)
    FString TestBlueprintPath = TEXT("/Game/Test/BP_TestExport");
    
    // 调用批量导出器的测试函数
    bool bSuccess = FBP2AIBatchExporter::TestExportBlueprint(TestBlueprintPath);
    
    // 输出详细的测试结果
    // ...
}
```

**测试方法**:
1. 在 UE 编辑器中打开 PropertyDumper Widget
2. 点击 "Run Test" 按钮
3. 查看 Output Log 中的详细输出

---

## 🧪 待测试的功能

### **测试目标**
验证我们可以**程序化地导出蓝图**,不依赖 UI 交互。

### **测试前准备**

#### **1. 在 UE 项目中创建测试蓝图**
```
路径: /Game/Test/BP_TestExport
类型: Actor Blueprint
内容: 
  - EventGraph: BeginPlay → Print String
  - 1个变量: Health (float)
  - 1个函数: TakeDamage (可选)
```

#### **2. 更新测试路径 (如果需要)**
编辑 `Source/BP2AI/Private/Test/CurrentPhaseTest.cpp` 第 25 行:
```cpp
FString TestBlueprintPath = TEXT("/Game/YourFolder/YourBlueprint");
```

### **测试步骤**
1. **编译插件** (在 Rider 中)
   - 打开 BP2AI.uproject 所在的解决方案
   - 找到 BP2AI 插件模块
   - Build → Rebuild Module "BP2AI"

2. **运行 UE 编辑器**
   - 打开你的 UE 项目
   - 确认 BP2AI 插件已启用

3. **执行测试**
   - 方法 1: 打开 PropertyDumper Widget → 点击 "Run Test"
   - 方法 2: 在 C++ 代码中调用 `BP2AITests::ExecuteCurrentPhaseTest()`

4. **查看结果**
   - 打开 Output Log (Window → Developer Tools → Output Log)
   - 搜索 "BP2AI" 查看详细日志

### **预期输出**
```
LogBP2AI: 🧪 BP2AI BATCH EXPORTER TEST - Task 1.3
LogBP2AI: 🧪 TEST: Attempting to load blueprint: /Game/Test/BP_TestExport
LogBP2AI: ✅ Blueprint loaded successfully: BP_TestExport
LogBP2AI: ========================================
LogBP2AI: BP2AIBatchExporter: Exporting blueprint 'BP_TestExport'
LogBP2AI: ========================================
LogBP2AI: 📊 Phase 1: Exporting Event Graphs
LogBP2AI: BP2AIBatchExporter: Starting export for graph 'EventGraph' (3 nodes)
LogBP2AI: ✅ Exported 'EventGraph': 456 characters, 23 lines
LogBP2AI: ✅ TEST PASSED: Successfully exported 1 graphs
```

---

## 🔧 Rider 开发环境设置

### **推荐配置**

#### **1. 打开项目**
```
File → Open → 选择 BP2AI.uproject 所在的解决方案文件
```

#### **2. 配置 UE 支持**
- 确保安装了 "Unreal Engine" 插件
- Settings → Build, Execution, Deployment → Toolset and Build
  - 选择正确的 UE 版本 (5.4)

#### **3. 代码导航快捷键**
- `Ctrl+Click` - 跳转到定义
- `Ctrl+Shift+F` - 全局搜索
- `Alt+Enter` - 快速修复/建议

#### **4. 日志查看**
- 编译日志: Build 窗口
- UE 运行日志: Output Log (在 UE 编辑器中)

### **关键文件定位**

需要频繁编辑的文件:
```
✅ Source/BP2AI/Private/Exporters/BP2AIBatchExporter.cpp
   → 实现批量导出逻辑

✅ Source/BP2AI/Private/Test/CurrentPhaseTest.cpp
   → 修改测试用例

📖 Source/BP2AI/Private/Trace/ExecutionFlow/ExecutionFlowGenerator.h
   → BP2AI 核心生成器 (只读,复用其接口)

📖 Source/BP2AI/Private/Widgets/SMarkdownOutputWindow.cpp
   → 理解 BP2AI 如何获取节点 (只读)
```

---

## 📚 UE API 参考

### **蓝图结构**
```cpp
UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Path/BP_Name"));

// 获取所有图表
TArray<UEdGraph*> EventGraphs = Blueprint->UbergraphPages;      // 事件图表
TArray<UEdGraph*> FunctionGraphs = Blueprint->FunctionGraphs;   // 函数图表
TArray<UEdGraph*> MacroGraphs = Blueprint->MacroGraphs;         // 宏图表
TArray<UEdGraph*> DelegateGraphs = Blueprint->DelegateSignatureGraphs; // 委托

// 获取图表节点
UEdGraph* Graph = EventGraphs[0];
TArray<UEdGraphNode*> AllNodes = Graph->Nodes;

// 获取变量 (TODO: Task 3.3 会用到)
TArray<FBPVariableDescription> Variables = Blueprint->NewVariables;

// 获取组件 (TODO: Task 3.2 会用到)
USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
```

### **关键类型**
```cpp
UBlueprint           // 蓝图资产
UEdGraph            // 图表 (EventGraph, 函数图表等)
UEdGraphNode        // 图表节点
UEdGraphPin         // 节点引脚 (输入/输出)
FBPVariableDescription  // 变量描述
USimpleConstructionScript  // 组件构造脚本
```

---

## 🎯 下一步任务 (待 Task 1.3 测试通过后)

### **Task 2.1: 研究如何获取蓝图的所有图表**
```cpp
// 需要验证的代码
UBlueprint* BP = ...;
UE_LOG(LogBP2AI, Log, TEXT("EventGraphs: %d"), BP->UbergraphPages.Num());
UE_LOG(LogBP2AI, Log, TEXT("Functions: %d"), BP->FunctionGraphs.Num());
// ... 打印所有图表类型的数量
```

### **Task 3.1: 导出 Blueprint Metadata**
```cpp
// 需要实现
struct FBlueprintMetadata
{
    FString ClassName;       // BP->GetName()
    FString ParentClass;     // BP->ParentClass->GetName()
    FString AssetPath;       // BP->GetPathName()
    TArray<FString> Interfaces; // BP->ImplementedInterfaces
};
```

---

## 🐛 已知问题和注意事项

### **编译依赖**
- ✅ 所有依赖已在 `BP2AI.Build.cs` 中配置
- ✅ 不需要添加额外的模块

### **线程安全**
- ⚠️ 所有 UObject 操作必须在游戏线程执行
- ⚠️ 不要在后台线程调用 UE API

### **资产加载**
- ✅ 使用 `LoadObject<UBlueprint>()` 加载蓝图
- ⚠️ 路径必须是完整的包路径 (如 `/Game/Folder/BP_Name`)
- ⚠️ 不要包含 `_C` 后缀 (那是类名,不是资产路径)

### **日志分类**
```cpp
UE_LOG(LogBP2AI, Log, TEXT("普通信息"));
UE_LOG(LogBP2AI, Warning, TEXT("警告信息"));
UE_LOG(LogBP2AI, Error, TEXT("错误信息"));
```

---

## 💡 与 Copilot 协作的提示

### **询问代码实现时**
```
"我需要实现 Task X.X 的功能,请参考 TASK_ROADMAP.md 中的详细描述。
当前已有的代码在 BP2AIBatchExporter.cpp 中,请帮我扩展..."
```

### **遇到编译错误时**
```
"编译错误: [错误信息]
当前文件: [文件路径]
请参考 TECHNICAL_NOTES.md 中的 API 参考部分..."
```

### **需要理解现有代码时**
```
"请解释 ExecutionFlowGenerator::GenerateDocumentForNodes() 的工作原理,
参考 TECHNICAL_NOTES.md 中的调用链分析..."
```

---

## 📞 关键联系点

### **重要文档**
- `TASK_ROADMAP.md` - 任务清单和进度
- `TECHNICAL_NOTES.md` - 技术原理分析
- `TASK_1.3_TEST_GUIDE.md` - 当前任务的测试指南

### **核心代码**
- `BP2AIBatchExporter.cpp` - 我们的新代码
- `ExecutionFlowGenerator.cpp` - BP2AI 核心 (只读)
- `CurrentPhaseTest.cpp` - 测试入口

### **日志关键字**
- `BP2AI` - 搜索所有相关日志
- `BP2AIBatchExporter` - 搜索导出器日志
- `TEST PASSED` / `TEST FAILED` - 搜索测试结果

---

## ✅ 检查清单

在开始开发前,确认:

- [ ] ✅ 已在 Rider 中打开项目
- [ ] ✅ 已读完 `TASK_ROADMAP.md`
- [ ] ✅ 已读完 `TECHNICAL_NOTES.md`
- [ ] ✅ 已理解 BP2AI 的工作原理
- [ ] ✅ 已创建测试蓝图 (或知道如何创建)
- [ ] ✅ 已定位到关键文件:
  - `BP2AIBatchExporter.cpp`
  - `CurrentPhaseTest.cpp`
  - `TASK_ROADMAP.md`

准备好了?开始编译和测试 Task 1.3!

---

**祝开发顺利! 🚀**

有任何问题,请参考相应的文档或直接询问 Copilot。
