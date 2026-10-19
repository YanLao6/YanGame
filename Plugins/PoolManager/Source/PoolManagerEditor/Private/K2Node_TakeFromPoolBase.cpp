// Copyright (c) Yevhenii Selivanov

#include "K2Node_TakeFromPoolBase.h"

// Pool Manager
#include "PoolManagerSubsystem.h"

// UE
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(K2Node_TakeFromPoolBase)

#define LOCTEXT_NAMESPACE "K2Node_TakeFromPoolBase"

// 连接子类额外创建的引脚的基础方法
bool UK2Node_TakeFromPoolBase::PostExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph& SourceGraph, class UK2Node_CallFunction& CallTakeFromPoolNode)
{
	bool bIsErrorFree = true;

	// 连接 Priority 输入
	UEdGraphPin* CallPriorityPin = CallTakeFromPoolNode.FindPin(GetPriorityPinName());
	UEdGraphPin* PriorityPin = FindPin(GetPriorityPinName());
	if (PriorityPin && CallPriorityPin)
	{
		if (PriorityPin->LinkedTo.Num() > 0)
		{
			bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*PriorityPin, *CallPriorityPin).CanSafeConnect();
		}
		else
		{
			// 复制字面量默认值
			CallPriorityPin->DefaultValue = PriorityPin->DefaultValue;
			CallPriorityPin->DefaultObject = PriorityPin->DefaultObject;
		}
	}
	else
	{
		bIsErrorFree = false;
	}

	return bIsErrorFree;
}

// 返回类输入引脚上选中的对象类；未设置时返回 nullptr
UClass* UK2Node_TakeFromPoolBase::GetSelectedClass(const TArray<UEdGraphPin*>* InPinsToSearch /*= nullptr*/) const
{
	const TArray<UEdGraphPin*>& PinsToSearch = InPinsToSearch ? *InPinsToSearch : Pins;

	const FName ClassPinName = GetClassInputPinName();
	const UEdGraphPin* ClassPin = nullptr;
	for (const UEdGraphPin* PinIt : PinsToSearch)
	{
		if (PinIt->PinName == ClassPinName)
		{
			ClassPin = PinIt;
			break;
		}
	}

	if (!ClassPin)
	{
		return nullptr;
	}

	if (ClassPin->DefaultObject && ClassPin->LinkedTo.IsEmpty())
	{
		return CastChecked<UClass>(ClassPin->DefaultObject);
	}

	if (!ClassPin->LinkedTo.IsEmpty())
	{
		const UEdGraphPin* ClassSource = ClassPin->LinkedTo[0];
		return ClassSource ? Cast<UClass>(ClassSource->PinType.PinSubCategoryObject.Get()) : nullptr;
	}

	return nullptr;
}

// 更新输出引脚类型，使其与选中的类保持一致
void UK2Node_TakeFromPoolBase::OnClassPinChanged() const
{
	UEdGraphPin* ReturnValuePin = FindPin(GetReturnValuePinName());
	if (!ReturnValuePin)
	{
		return;
	}

	TArray<UEdGraphPin*> ConnectionList = ReturnValuePin->LinkedTo;
	ReturnValuePin->BreakAllPinLinks(true);

	const UClass* SelectedClass = GetSelectedClass();
	ReturnValuePin->PinType.PinSubCategoryObject = SelectedClass
	                                                   ? const_cast<UClass*>(SelectedClass->GetAuthoritativeClass())
	                                                   : UObject::StaticClass();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (UEdGraphPin* Connection : ConnectionList)
	{
		K2Schema->TryCreateConnection(ReturnValuePin, Connection);
	}

	GetGraph()->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}

// 生成校验节点并返回用于分支判断的 bool 输出引脚；基类实现对单个对象执行 IsValid 检查
UEdGraphPin* UK2Node_TakeFromPoolBase::SpawnIsResultValidPin(FKismetCompilerContext& CompilerContext, UEdGraph& SourceGraph, UEdGraphPin* ResultVariablePin)
{
	UK2Node_CallFunction* IsValidNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, &SourceGraph);
	IsValidNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid), UKismetSystemLibrary::StaticClass());
	IsValidNode->AllocateDefaultPins();

	UEdGraphPin* ObjectInputPin = IsValidNode->FindPinChecked(TEXT("Object"));
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	Schema->TryCreateConnection(ResultVariablePin, ObjectInputPin);

	return IsValidNode->GetReturnValuePin();
}

void UK2Node_TakeFromPoolBase::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	UEdGraphPin* CompletedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Completed);
	CompletedPin->PinToolTip = LOCTEXT("CompletedTooltip", "Called when the object is taken from pool and valid").ToString();

	UEdGraphPin* FailedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, GetFailedPinName());
	FailedPin->PinToolTip = LOCTEXT("FailedTooltip", "Called when the pool returned null, e.g. the object class is invalid or spawn failed").ToString();

	UEdGraphPin* TargetPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UPoolManagerSubsystem::StaticClass(), UEdGraphSchema_K2::PN_Self);
	checkf(TargetPin, TEXT("ERROR: [%i] %hs:\n'TargetPin' is null!"), __LINE__, __FUNCTION__);
	TargetPin->PinFriendlyName = LOCTEXT("Target", "Target");
	TargetPin->bDefaultValueIsIgnored = true;

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Class, UObject::StaticClass(), GetClassInputPinName());

	UEnum* PriorityEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/PoolManager.ESpawnRequestPriority"));
	UEdGraphPin* PriorityPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, PriorityEnum, GetPriorityPinName());
	PriorityPin->DefaultValue = PriorityEnum->GetNameStringByValue(static_cast<int64>(ESpawnRequestPriority::Normal));

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), GetReturnValuePinName(), GetReturnValuePinParams());
}

void UK2Node_TakeFromPoolBase::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	if (ChangedPin && ChangedPin->PinName == GetClassInputPinName())
	{
		OnClassPinChanged();
	}
}

void UK2Node_TakeFromPoolBase::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin && Pin->PinName == GetClassInputPinName())
	{
		OnClassPinChanged();
	}
}

FString UK2Node_TakeFromPoolBase::GetPinMetaData(FName InPinName, FName InKey)
{
	if (InPinName == GetClassInputPinName() && InKey == TEXT("AllowAbstract"))
	{
		return TEXT("false");
	}

	return Super::GetPinMetaData(InPinName, InKey);
}

void UK2Node_TakeFromPoolBase::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

	// 保留旧的类选择所对应的输出引脚类型
	const UClass* SelectedClass = GetSelectedClass(&OldPins);
	if (SelectedClass)
	{
		UEdGraphPin* ReturnValuePin = FindPin(GetReturnValuePinName());
		if (ReturnValuePin)
		{
			ReturnValuePin->PinType.PinSubCategoryObject = const_cast<UClass*>(SelectedClass->GetAuthoritativeClass());
		}
	}

	// 旧版本兼容：把旧的 "Then" 引脚重命名为 "Completed"
	UEdGraphPin* OldThenPin = nullptr;
	const UEdGraphPin* OldCompletedPin = nullptr;

	for (UEdGraphPin* CurrentPin : OldPins)
	{
		if (CurrentPin->PinName == UEdGraphSchema_K2::PN_Then)
		{
			OldThenPin = CurrentPin;
		}
		else if (CurrentPin->PinName == UEdGraphSchema_K2::PN_Completed)
		{
			OldCompletedPin = CurrentPin;
		}
	}

	if (OldThenPin && !OldCompletedPin)
	{
		// 这是 Completed 尚被称作 Then 的旧节点，改名为 Completed 后即可走正常的重连流程
		OldThenPin->PinName = UEdGraphSchema_K2::PN_Completed;
	}
}

void UK2Node_TakeFromPoolBase::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(Schema);
	bool bIsErrorFree = true;

	// Sequence 节点，默认带两个输出引脚
	UK2Node_ExecutionSequence* SequenceNode = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	SequenceNode->AllocateDefaultPins();

	// 连接到输入执行引脚
	{
		UEdGraphPin* InputExePin = GetExecPin();
		UEdGraphPin* SequenceInputExePin = SequenceNode->GetExecPin();
		bIsErrorFree &= InputExePin && SequenceInputExePin && CompilerContext.MovePinLinksToIntermediate(*InputExePin, *SequenceInputExePin).CanSafeConnect();
	}

	// 创建 TakeFromPool 函数调用节点
	UK2Node_CallFunction* CallTakeFromPoolNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallTakeFromPoolNode->FunctionReference.SetExternalMember(GetNativeFunctionName(), UPoolManagerSubsystem::StaticClass());
	CallTakeFromPoolNode->AllocateDefaultPins();

	// 把该调用接到 Sequence 的第一个输出引脚
	{
		UEdGraphPin* CallFunctionInputExePin = CallTakeFromPoolNode->GetExecPin();
		UEdGraphPin* SequenceFirstExePin = SequenceNode->GetThenPinGivenIndex(0);
		bIsErrorFree &= SequenceFirstExePin && CallFunctionInputExePin && Schema->TryCreateConnection(CallFunctionInputExePin, SequenceFirstExePin);
	}

	// 创建局部变量
	UK2Node_TemporaryVariable* TempVarOutput = CompilerContext.SpawnInternalVariable(this, UEdGraphSchema_K2::PC_Object, NAME_None, UObject::StaticClass(), GetReturnValuePinParams().ContainerType);

	// 创建赋值节点
	UK2Node_AssignmentStatement* AssignNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	AssignNode->AllocateDefaultPins();

	UEdGraphPin* LoadedObjectVariablePin = TempVarOutput->GetVariablePin();

	// 把局部变量接到赋值节点
	{
		UEdGraphPin* AssignLHSPPin = AssignNode->GetVariablePin();
		bIsErrorFree &= AssignLHSPPin && LoadedObjectVariablePin && Schema->TryCreateConnection(AssignLHSPPin, LoadedObjectVariablePin);
	}

	// 把局部变量接到输出引脚
	{
		UEdGraphPin* OutputObjectPinPin = FindPin(GetReturnValuePinName());
		bIsErrorFree &= LoadedObjectVariablePin && OutputObjectPinPin && CompilerContext.MovePinLinksToIntermediate(*OutputObjectPinPin, *LoadedObjectVariablePin).CanSafeConnect();
	}

	// 用 self 连接 Pool Manager 输入
	UEdGraphPin* CallPoolManagerPin = CallTakeFromPoolNode->FindPin(UEdGraphSchema_K2::PN_Self);
	{
		UEdGraphPin* PoolManagerPin = FindPin(UEdGraphSchema_K2::PN_Self);
		ensure(CallPoolManagerPin);

		if (PoolManagerPin && CallPoolManagerPin)
		{
			if (PoolManagerPin->LinkedTo.Num() > 0)
			{
				bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*PoolManagerPin, *CallPoolManagerPin).CanSafeConnect();
			}
			else
			{
				// 复制字面量默认值
				CallPoolManagerPin->DefaultValue = PoolManagerPin->DefaultValue;
			}
		}
		else
		{
			bIsErrorFree = false;
		}
	}

	// 连接类输入
	UEdGraphPin* CallClassPin = CallTakeFromPoolNode->FindPin(GetClassInputPinName());
	{
		UEdGraphPin* ClassPin = FindPin(GetClassInputPinName());
		ensure(CallClassPin);

		if (ClassPin && CallClassPin)
		{
			if (ClassPin->LinkedTo.Num() > 0)
			{
				bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*ClassPin, *CallClassPin).CanSafeConnect();
			}
			else
			{
				// 即使未连线也要保证默认值被复制过去
				CallClassPin->DefaultValue = ClassPin->DefaultValue;
				CallClassPin->DefaultObject = ClassPin->DefaultObject;
			}
		}
		else
		{
			bIsErrorFree = false;
		}
	}

	// 创建 Completed 委托参数
	const FName DelegateCompletedParamName(GetCompletedPinName());
	UK2Node_CustomEvent* CompletedEventNode = CompilerContext.SpawnIntermediateNode<UK2Node_CustomEvent>(this, SourceGraph);
	CompletedEventNode->CustomFunctionName = *FString::Printf(TEXT("%s_%s"), *GetCompletedPinName().ToString(), *CompilerContext.GetGuid(this));
	CompletedEventNode->AllocateDefaultPins();
	{
		UFunction* TakeFromPoolFunction = CallTakeFromPoolNode->GetTargetFunction();
		FDelegateProperty* CompletedDelegateProperty = TakeFromPoolFunction ? FindFProperty<FDelegateProperty>(TakeFromPoolFunction, DelegateCompletedParamName) : nullptr;
		UFunction* CompletedSignature = CompletedDelegateProperty ? CompletedDelegateProperty->SignatureFunction : nullptr;
		ensure(CompletedSignature);
		for (TFieldIterator<FProperty> PropIt(CompletedSignature); PropIt && PropIt->PropertyFlags & CPF_Parm; ++PropIt)
		{
			const FProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				bIsErrorFree &= Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
				bIsErrorFree &= nullptr != CompletedEventNode->CreateUserDefinedPin(Param->GetFName(), PinType, EGPD_Output);
			}
		}
	}

	// 连接委托
	{
		UEdGraphPin* CallFunctionDelegatePin = CallTakeFromPoolNode->FindPin(DelegateCompletedParamName);
		ensure(CallFunctionDelegatePin);
		UEdGraphPin* EventDelegatePin = CompletedEventNode->FindPin(UK2Node_CustomEvent::DelegateOutputName);
		bIsErrorFree &= CallFunctionDelegatePin && EventDelegatePin && Schema->TryCreateConnection(CallFunctionDelegatePin, EventDelegatePin);
	}

	// 把事件返回的对象接到赋值节点
	{
		UEdGraphPin* LoadedAssetEventPin = CompletedEventNode->FindPin(GetReturnValuePinName());
		ensure(LoadedAssetEventPin);
		UEdGraphPin* AssignRHSPPin = AssignNode->GetValuePin();
		bIsErrorFree &= AssignRHSPPin && LoadedAssetEventPin && Schema->TryCreateConnection(LoadedAssetEventPin, AssignRHSPPin);
	}

	// 把赋值节点的执行输入接到事件的执行输出
	{
		UEdGraphPin* CompletedEventOutputPin = CompletedEventNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* AssignInputExePin = AssignNode->GetExecPin();
		bIsErrorFree &= AssignInputExePin && CompletedEventOutputPin && Schema->TryCreateConnection(AssignInputExePin, CompletedEventOutputPin);
	}

	// 按结果是否有效分支：有效走 Completed，否则走 Failed
	UEdGraphPin* IsValidPin = SpawnIsResultValidPin(CompilerContext, *SourceGraph, LoadedObjectVariablePin);
	if (IsValidPin)
	{
		UK2Node_IfThenElse* BranchNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		BranchNode->AllocateDefaultPins();

		UEdGraphPin* AssignOutputExePin = AssignNode->GetThenPin();
		bIsErrorFree &= AssignOutputExePin && Schema->TryCreateConnection(AssignOutputExePin, BranchNode->GetExecPin());
		bIsErrorFree &= Schema->TryCreateConnection(IsValidPin, BranchNode->GetConditionPin());

		UEdGraphPin* OutputCompletedPin = FindPin(UEdGraphSchema_K2::PN_Completed);
		bIsErrorFree &= OutputCompletedPin && CompilerContext.MovePinLinksToIntermediate(*OutputCompletedPin, *BranchNode->GetThenPin()).CanSafeConnect();

		UEdGraphPin* OutputFailedPin = FindPin(GetFailedPinName());
		bIsErrorFree &= OutputFailedPin && CompilerContext.MovePinLinksToIntermediate(*OutputFailedPin, *BranchNode->GetElsePin()).CanSafeConnect();
	}
	else
	{
		// 没有可用的校验方式，直接把赋值节点接到 Completed
		UEdGraphPin* OutputCompletedPin = FindPin(UEdGraphSchema_K2::PN_Completed);
		UEdGraphPin* AssignOutputExePin = AssignNode->GetThenPin();
		bIsErrorFree &= OutputCompletedPin && AssignOutputExePin && CompilerContext.MovePinLinksToIntermediate(*OutputCompletedPin, *AssignOutputExePin).CanSafeConnect();
	}

	bIsErrorFree &= PostExpandNode(CompilerContext, *SourceGraph, *CallTakeFromPoolNode);

	if (!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "K2Node_TakeFromPoolBase: Internal connection error. @@").ToString(), this);
	}

	BreakAllNodeLinks();
}

FText UK2Node_TakeFromPoolBase::GetTooltipText() const
{
	const FName FunctionName = GetNativeFunctionName();
	const UFunction* Function = UPoolManagerSubsystem::StaticClass()->FindFunctionByName(FunctionName);
	ensureMsgf(Function, TEXT("ASSERT: [%i] %hs:\n'Function' is not found: %s"), __LINE__, __FUNCTION__, *FunctionName.ToString());
	return Function ? Function->GetToolTipText() : Super::GetTooltipText();
}

FText UK2Node_TakeFromPoolBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FName FunctionName = GetNativeFunctionName();
	const UFunction* Function = UPoolManagerSubsystem::StaticClass()->FindFunctionByName(FunctionName);
	ensureMsgf(Function, TEXT("ASSERT: [%i] %hs:\n'Function' is not found: %s"), __LINE__, __FUNCTION__, *FunctionName.ToString());
	return Function ? Function->GetDisplayNameText() : Super::GetNodeTitle(TitleType);
}

bool UK2Node_TakeFromPoolBase::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	bool bIsCompatible = false;
	// 事件只能放在 ubergraph 和宏中（其他机制会阻止带 latent 的宏被放进函数），而本异步节点会展开出事件节点
	const EGraphType GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
	if (GraphType == GT_Ubergraph || GraphType == GT_Macro)
	{
		bIsCompatible = true;
	}
	return bIsCompatible && Super::IsCompatibleWithGraph(TargetGraph);
}

FName UK2Node_TakeFromPoolBase::GetCornerIcon() const
{
	return TEXT("Graph.Latent.LatentIcon");
}

void UK2Node_TakeFromPoolBase::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// action 以特定的 object-key 注册；其含义是当该 object-key 发生变化（或被移除）时，
	// 对应的 action 也应随之更新（或删除）……这里用节点自身的类作为 key，
	// 这样一旦该节点类型消失，对应 action 也会一并消失
	const UClass* ActionKey = GetClass();
	// 为避免无谓地实例化 UBlueprintNodeSpawner，先确认注册器确实在收集此类型的 action
	//（它可能只是在为某个特定资产重新生成 action，此时只接受与该资产对应的 action）
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_TakeFromPoolBase::GetMenuCategory() const
{
	return FText(LOCTEXT("UK2Node_TakeFromPoolBaseGetMenuCategory", "Pool Manager"));
}

#undef LOCTEXT_NAMESPACE
