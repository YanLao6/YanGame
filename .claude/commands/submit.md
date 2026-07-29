---
description: 汇总 Perforce changelist 的变更，生成中文提交说明并写入 changelist，由用户自行在 P4V 中提交
argument-hint: "[changelist 号，省略则用 default]"
model: claude-haiku-4-5-20251001
allowed-tools: PowerShell, Write, Read
---

将当前 Perforce changelist 的变更整理成一份提交说明，写入 changelist 后结束。**不要提交，不要询问用户**，提交由用户自己在 P4V 中完成。

目标 changelist：`$1`（为空则用 default）。

后端脚本：`E:\UnrealEngine\Projecties\YanGame\.claude\scripts\p4-submit.ps1`
描述草稿：`E:\UnrealEngine\Projecties\YanGame\.claude\scripts\.p4-description.txt`

## 执行步骤

### 1. 收集变更

```
& 'E:\UnrealEngine\Projecties\YanGame\.claude\scripts\p4-submit.ps1' -Action Collect -Change '<目标或留空>'
```

输出 `EMPTY` 表示没有待提交的文件 —— 直接告知用户并结束，不要继续。

输出分三段：`BY ACTION`（动作统计）、`SOURCE`（源码逐条）、`ASSETS`（资产，量大时按目录聚合）。

### 2. 判断是否需要看 diff

`SOURCE` 段的文件不超过 8 个时，对其中的 `.h/.cpp/.cs/.as` 执行 `p4 diff -du <depot路径>` 了解实际改动，让说明写到点子上。

超过 8 个则跳过 diff，仅依据路径、目录和动作类型推断，不要逐个文件去读。

`p4` 必须用绝对路径 `C:\Program Files\Perforce\p4.exe` 调用 —— PATH 最前的 `System32\p4` 是 0 字节占位文件。

### 3. 写提交说明

用 Write 工具把说明写入描述草稿文件。格式：

```
一句话摘要，不超过 50 字

- 变更条目一
- 变更条目二
```

变更单一时只写摘要行，不要硬凑条目。

写作要求：

- 简体中文，类名、文件名、模块名、技术术语保留英文原形
- 写变更的**意图和影响**，不是罗列文件名 —— "修复装备切换时 QuickBar 索引越界" 而不是 "修改了 QuickBarComponent.cpp"
- 资产（`.uasset`/`.umap`）按用途归纳，如 "新增 4 个 UI 控件蓝图"，不要逐个列出
- 禁止使用进度类词汇：`FIXED`、`Step`、`Week`、`Section`、`Phase`、`AC-x`
- 禁止出现任何 AI 工具名称
- 不确定改动意图时，如实写观察到的事实，不要臆测

### 4. 写入 changelist

```
& 'E:\UnrealEngine\Projecties\YanGame\.claude\scripts\p4-submit.ps1' -Action Prepare -DescriptionFile 'E:\UnrealEngine\Projecties\YanGame\.claude\scripts\.p4-description.txt' -Change '<目标或留空>'
```

输出中的 `CHANGELIST: <n>` 即写入后的 changelist 编号。说明此时已经落在 changelist 里，用户在 P4V 中打开即可看到并编辑。

### 5. 报告并结束

输出两样东西，然后结束：

1. changelist 编号，以及"说明已写入，可在 P4V 中编辑并提交"
2. 说明正文全文，放在代码块里，方便用户需要时直接复制

若步骤 2 的 diff 显示这次改动**没有实质内容**（仅行尾符、空白字符或空行变化），在报告末尾直接指出这一事实，并说明丢弃的方式是 `p4 revert -c <n> //...` 加 `p4 change -d <n>`。不要代替用户决定，也不要执行这些命令。

## 边界

- 不要提交，不要调用 `-Action Submit`，提交是用户在 P4V 中的动作
- 不要用 AskUserQuestion 征求意见，直接给出最终说明
- 不要修改任何源码或资产文件，这个命令只负责整理说明
- 不要执行 `p4 revert`、`p4 obliterate` 或任何会丢弃工作的命令
- 不要新建或切换 changelist，只操作目标 changelist
