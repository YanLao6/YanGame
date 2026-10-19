// Copyright (c) Yevhenii Selivanov

#pragma once

// Pool Manager
#include "K2Node_TakeFromPoolBase.h"

#include "K2Node_TakeFromPool.generated.h"

/**
 * TakeFromPool 蓝图节点。
 */
UCLASS()
class POOLMANAGEREDITOR_API UK2Node_TakeFromPool : public UK2Node_TakeFromPoolBase
{
	GENERATED_BODY()

public:
	static inline const FName TransformInputName = TEXT("Transform");

	//~Begin UK2Node_TakeFromPoolBase Interface
	virtual FORCEINLINE FName GetReturnValuePinName() const override { return TEXT("Object"); }
	virtual FName GetNativeFunctionName() const override;
	virtual bool PostExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph& SourceGraph, UK2Node_CallFunction& CallTakeFromPoolNode) override;
	//~End UK2Node_TakeFromPoolBase Interface

	/** 重写以为该节点额外创建默认引脚。 */
	virtual void AllocateDefaultPins() override;
};
