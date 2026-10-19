// Copyright (c) Yevhenii Selivanov

#pragma once

// UE
#include "K2Node.h"

#include "K2Node_TakeFromPoolBase.generated.h"

/**
 * TakeFromPool 系列蓝图节点的基类。
 */
UCLASS(Abstract)
class POOLMANAGEREDITOR_API UK2Node_TakeFromPoolBase : public UK2Node
{
	GENERATED_BODY()

public:
	/** 输入与输出引脚名的基础方法 */
	virtual FORCEINLINE FName GetReturnValuePinName() const { return TEXT("Output"); }
	virtual FORCEINLINE FName GetClassInputPinName() const { return TEXT("ObjectClass"); }
	virtual FORCEINLINE FName GetCompletedPinName() const { return TEXT("Completed"); }
	virtual FORCEINLINE FName GetFailedPinName() const { return TEXT("Failed"); }
	virtual FORCEINLINE FName GetPriorityPinName() const { return TEXT("Priority"); }

	/** 获取输出引脚参数的基础方法。 */
	virtual FCreatePinParams GetReturnValuePinParams() const { return FCreatePinParams(); }

	/** 必须由子类重写，返回原生类中对应函数的名称。 */
	virtual FName GetNativeFunctionName() const PURE_VIRTUAL(UK2Node_TakeFromPoolBase::NativeFunctionName, return NAME_None;);

	/** 连接子类额外创建的引脚的基础方法。 */
	virtual bool PostExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph& SourceGraph, class UK2Node_CallFunction& CallTakeFromPoolNode);

	/** 返回类输入引脚上选中的对象类；未设置时返回 nullptr */
	UClass* GetSelectedClass(const TArray<UEdGraphPin*>* InPinsToSearch = nullptr) const;

	/** 更新输出引脚类型，使其与选中的类保持一致 */
	void OnClassPinChanged() const;

	/** 生成校验节点并返回用于分支判断的 bool 输出引脚；基类实现对单个对象执行 IsValid 检查 */
	virtual UEdGraphPin* SpawnIsResultValidPin(FKismetCompilerContext& CompilerContext, UEdGraph& SourceGraph, UEdGraphPin* ResultVariablePin);

	//~Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual FString GetPinMetaData(FName InPinName, FName InKey) override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	//~End UEdGraphNode Interface

	//~Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual bool IsNodePure() const override { return false; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	//~End UK2Node Interface
};
