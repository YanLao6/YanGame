#include "MoverLibrary/MoverQueryLibrary.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "LayeredMoveBase.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverQueryLibrary)

ULayeredMoveLogic* UMoverQueryLibrary::FindRegisteredMoveLogic(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> MoveLogicClass)
{
	if (!MoverComp || !MoveLogicClass)
	{
		return nullptr;
	}
	// Logic 对象以 MoverComponent 为 Outer 创建，遍历其子对象即可找到
	TArray<UObject*> Children;
	GetObjectsWithOuter(MoverComp, Children, /*EGtObjectsFlags*/EGetObjectsFlags::None);
	for (UObject* Child : Children)
	{
		if (ULayeredMoveLogic* Logic = Cast<ULayeredMoveLogic>(Child))
		{
			if (Logic->IsA(MoveLogicClass))
			{
				return Logic;
			}
		}
	}
	return nullptr;
}

FVector UMoverQueryLibrary::GetLastFloorNormal(UMoverComponent* MoverComp)
{
	if (!MoverComp)
	{
		return FVector::UpVector;
	}
	const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard();
	if (!Blackboard)
	{
		return MoverComp->GetUpDirection();
	}
	FFloorCheckResult FloorResult;
	if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
	{
		return FloorResult.HitResult.ImpactNormal;
	}
	return MoverComp->GetUpDirection();
}

bool UMoverQueryLibrary::HasWalkableFloor(UMoverComponent* MoverComp)
{
	if (!MoverComp)
	{
		return false;
	}
	const UMoverBlackboard* Blackboard = MoverComp->GetSimBlackboard();
	if (!Blackboard)
	{
		return false;
	}
	FFloorCheckResult FloorResult;
	return Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor();
}

bool UMoverQueryLibrary::HasWalkableFloorFromBlackboard(UMoverBlackboard* SimBlackboard)
{
	if (!SimBlackboard)
	{
		return false;
	}
	UMoverComponent* MoverComp = SimBlackboard->GetTypedOuter<UMoverComponent>();
	return HasWalkableFloor(MoverComp);
}

FVector UMoverQueryLibrary::GetGravityFromBlackboard(UMoverBlackboard* SimBlackboard)
{
	if (!SimBlackboard)
	{
		return FVector(0.f, 0.f, -980.f);
	}
	// SimBlackboard 由 MoverComponent 持有，Outer 即为 MoverComponent
	if (UMoverComponent* MoverComp = SimBlackboard->GetTypedOuter<UMoverComponent>())
	{
		if (!HasWalkableFloor(MoverComp))
		{
			return MoverComp->GetGravityAcceleration();
		}
	}
	return FVector(0.f, 0.f, -980.f);
}

bool UMoverQueryLibrary::GetOwnerViewLocationAndForward(UMoverComponent* MoverComp, FVector& OutLocation, FVector& OutForward)
{
	if (!MoverComp)
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(MoverComp->GetOwner());
	if (!Pawn)
	{
		return false;
	}
	// 当前游戏线程视点；各端可能处于不同采样时刻，不依赖客户端相机缓存。
	OutLocation                                = Pawn->GetPawnViewLocation();
	// 复用 Mover 最近使用的 ControlRotation（含完整 Pitch）；缺输入时退回身体朝向。
	const FCharacterDefaultInputs* CharInputs  = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : Pawn->GetActorRotation();
	OutForward                                 = AimRotation.Vector();
	return true;
}
