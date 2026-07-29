#include "YanMoverAngelscriptLibrary.h"

#include "LayeredMoveBase.h"
#include "MovementMode.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/Pawn.h"
#include "Actor/YanHookProjectile.h"
#include "AbilitySystemComponent.h"
#include "GameFeature/GameFeatureAction_AddMoverAbilities.h"
#include "GE/MoverLaunchExecutionCalculation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMoverAngelscriptLibrary)

bool UYanMoverAngelscriptLibrary::GetDefaultSyncState(const FMoverDataCollection& Collection, FMoverDefaultSyncState& OutSyncState)
{
	if (const FMoverDefaultSyncState* Found = Collection.FindDataByType<FMoverDefaultSyncState>())
	{
		OutSyncState = *Found;
		return true;
	}

	return false;
}

bool UYanMoverAngelscriptLibrary::GetDefaultInputs(const FMoverDataCollection& Collection, FCharacterDefaultInputs& OutInputs)
{
	if (const FCharacterDefaultInputs* Found = Collection.FindDataByType<FCharacterDefaultInputs>())
	{
		OutInputs = *Found;
		return true;
	}

	return false;
}

bool UYanMoverAngelscriptLibrary::GetYanCharacterInputs(const FMoverDataCollection& Collection, FYanCharacterInputs& OutInputs)
{
	if (const FYanCharacterInputs* Found = Collection.FindDataByType<FYanCharacterInputs>())
	{
		OutInputs = *Found;
		return true;
	}

	return false;
}

void UYanMoverAngelscriptLibrary::InitTickEndForNewFrame(FMoverTickEndData& TickEndData)
{
	TickEndData.InitForNewFrame();
}


void UYanMoverAngelscriptLibrary::ResetMovementModeTickEndState(FMovementModeTickEndState& EndState)
{
	EndState.ResetToDefaults();
}

void UYanMoverAngelscriptLibrary::ResetMovementRecord(FMovementRecord& Record)
{
	Record.Reset();
}

FMovementSubstep UYanMoverAngelscriptLibrary::MakeMovementSubstep(FName MoveName, FVector MoveDelta, bool bIsRelevant)
{
	return FMovementSubstep(MoveName, MoveDelta, bIsRelevant);
}

void UYanMoverAngelscriptLibrary::AppendMovementSubstep(FMovementRecord& Record, FMovementSubstep Substep)
{
	Record.Append(Substep);
}

FVector UYanMoverAngelscriptLibrary::GetMovementRecordTotalMoveDelta(const FMovementRecord& Record)
{
	return Record.GetTotalMoveDelta();
}

FVector UYanMoverAngelscriptLibrary::GetMovementRecordRelevantMoveDelta(const FMovementRecord& Record)
{
	return Record.GetRelevantMoveDelta();
}

FVector UYanMoverAngelscriptLibrary::GetMovementRecordRelevantVelocity(const FMovementRecord& Record)
{
	return Record.GetRelevantVelocity();
}

TArray<FMovementSubstep> UYanMoverAngelscriptLibrary::GetMovementRecordSubsteps(const FMovementRecord& Record)
{
	return TArray<FMovementSubstep>(Record.GetSubsteps());
}

void UYanMoverAngelscriptLibrary::LockMovementRecordRelevancy(FMovementRecord& Record, bool bLockValue)
{
	Record.LockRelevancy(bLockValue);
}

void UYanMoverAngelscriptLibrary::UnlockMovementRecordRelevancy(FMovementRecord& Record)
{
	Record.UnlockRelevancy();
}

void UYanMoverAngelscriptLibrary::SetMovementRecordDeltaSeconds(FMovementRecord& Record, float DeltaSeconds)
{
	Record.SetDeltaSeconds(DeltaSeconds);
}

FString UYanMoverAngelscriptLibrary::MovementRecordToString(const FMovementRecord& Record)
{
	return Record.ToString();
}

FVector UYanMoverAngelscriptLibrary::GetSyncStateVelocity_WorldSpace(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetVelocity_WorldSpace();
}

FRotator UYanMoverAngelscriptLibrary::GetSyncStateOrientation_WorldSpace(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetOrientation_WorldSpace();
}

bool UYanMoverAngelscriptLibrary::HasWalkableFloor(UMoverComponent* MoverComp)
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

FVector UYanMoverAngelscriptLibrary::GetLastFloorNormal(UMoverComponent* MoverComp)
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

bool UYanMoverAngelscriptLibrary::HasWalkableFloorFromBlackboard(UMoverBlackboard* SimBlackboard)
{
	if (!SimBlackboard)
	{
		return false;
	}
	UMoverComponent* MoverComp = SimBlackboard->GetTypedOuter<UMoverComponent>();
	return HasWalkableFloor(MoverComp);
}

FVector UYanMoverAngelscriptLibrary::GetMoveInputWorldSpace(const FCharacterDefaultInputs& Inputs)
{
	return Inputs.GetMoveInput_WorldSpace();
}

void UYanMoverAngelscriptLibrary::QueueChaosCharacterApplyVelocityEffect(UMoverComponent* MoverComp, FVector WorldSpaceVelocity, bool bAdditive)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FChaosCharacterApplyVelocityEffect> Effect = MakeShared<FChaosCharacterApplyVelocityEffect>();
	Effect->VelocityOrImpulseToApply                      = WorldSpaceVelocity;
	Effect->Mode                                          = bAdditive ? EChaosMoverVelocityEffectMode::AdditiveVelocity : EChaosMoverVelocityEffectMode::OverrideVelocity;
	MoverComp->QueueInstantMovementEffect(Effect);
}

void UYanMoverAngelscriptLibrary::QueueApplyVelocityEffect(UMoverComponent* MoverComp, FName NewModeName)
{
	if (!MoverComp)
	{
		return;
	}
	// 零速度叠加 = 保持当前速度，ForceMovementMode 强制切换模式
	TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
	Effect->VelocityToApply                 = FVector::ZeroVector;
	Effect->bAdditiveVelocity               = true;
	Effect->ForceMovementMode               = NewModeName;
	MoverComp->QueueInstantMovementEffect(Effect);
}

FVector UYanMoverAngelscriptLibrary::GetGravityFromBlackboard(UMoverBlackboard* SimBlackboard)
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

FVector UYanMoverAngelscriptLibrary::GetSyncStateLocation_WorldSpace(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetLocation_WorldSpace();
}

FCharacterDefaultInputs UYanMoverAngelscriptLibrary::GetOrAddDefaultInputs(FMoverInputCmdContext& InputCmd)
{
	return InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
}

void UYanMoverAngelscriptLibrary::CommitDefaultInputs(FMoverInputCmdContext& InputCmd, const FCharacterDefaultInputs& Inputs)
{
	FCharacterDefaultInputs& Stored = InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	Stored                          = Inputs;
}

FYanCharacterInputs UYanMoverAngelscriptLibrary::GetOrAddYanInputs(FMoverInputCmdContext& InputCmd)
{
	return InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
}

void UYanMoverAngelscriptLibrary::CommitYanInputs(FMoverInputCmdContext& InputCmd, const FYanCharacterInputs& Inputs)
{
	FYanCharacterInputs& Stored = InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
	Stored                      = Inputs;
}

bool UYanMoverAngelscriptLibrary::GetOwnerViewLocationAndForward(UMoverComponent* MoverComp, FVector& OutLocation, FVector& OutForward)
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
	// 原点取 Pawn 视点（Actor 位置 + BaseEyeHeight），两端确定性一致，不依赖纯客户端的 PlayerCameraManager。
	OutLocation = Pawn->GetPawnViewLocation();
	// 方向取上传服务器的输入 ControlRotation（含完整 Pitch），两端一致；缺输入时退回 Actor 朝向。
	const FCharacterDefaultInputs* CharInputs  = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : Pawn->GetActorRotation();
	OutForward                                 = AimRotation.Vector();
	return true;
}

bool UYanMoverAngelscriptLibrary::LineTraceForGrapple(UMoverComponent* MoverComp, float MaxDistance, FVector& OutImpactPoint)
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
	// 取位口径与 GetOwnerViewLocationAndForward 一致：原点用 Pawn 视点、方向用输入 ControlRotation，
	// 两端确定性一致，便于服务器侧做命中校验，不依赖纯客户端的 PlayerCameraManager。
	const FVector                  Start       = Pawn->GetPawnViewLocation();
	const FCharacterDefaultInputs* CharInputs  = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : Pawn->GetActorRotation();
	const FVector                  Forward     = AimRotation.Vector();
	const FVector                  End         = Start + Forward * MaxDistance;

	FHitResult            HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleTrace), false);
	Params.AddIgnoredActor(Pawn);

	const bool bHit = MoverComp->GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
	if (bHit)
	{
		OutImpactPoint = HitResult.ImpactPoint;
	}
	return bHit;
}

void UYanMoverAngelscriptLibrary::QueueLaunchMove(UMoverComponent* MoverComp, FVector LaunchVelocity, FName ForceMovementMode, float DurationMs)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
	LaunchMove->LaunchVelocity                 = LaunchVelocity;
	LaunchMove->ForceMovementMode              = ForceMovementMode;
	LaunchMove->DurationMs                     = DurationMs;
	MoverComp->QueueLayeredMove(LaunchMove);
}

void UYanMoverAngelscriptLibrary::QueueLinearVelocityMove(UMoverComponent* MoverComp, FVector Velocity, float DurationMs, bool bOverrideVelocity)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FLayeredMove_LinearVelocity> LinearMove = MakeShared<FLayeredMove_LinearVelocity>();
	LinearMove->Velocity                               = Velocity;
	LinearMove->DurationMs                             = DurationMs;
	// Override 覆盖本帧提议速度（屏蔽方向输入与摩擦，直线冲刺）；否则默认叠加到正常移动上
	LinearMove->MixMode = bOverrideVelocity ? EMoveMixMode::OverrideVelocity : EMoveMixMode::AdditiveVelocity;
	MoverComp->QueueLayeredMove(LinearMove);
}

AYanHookProjectile* UYanMoverAngelscriptLibrary::SpawnHookProjectile(UMoverComponent*                MoverComp,
                                                                     TSubclassOf<AYanHookProjectile> ProjectileClass,
                                                                     FVector                         SpawnLocation,
                                                                     FRotator                        SpawnRotation)
{
	if (!MoverComp || !ProjectileClass)
	{
		return nullptr;
	}
	UWorld* World = MoverComp->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner                          = MoverComp->GetOwner();
	return World->SpawnActor<AYanHookProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, Params);
}

ULayeredMoveLogic* UYanMoverAngelscriptLibrary::FindRegisteredMoveLogic(UMoverComponent* MoverComp, TSubclassOf<ULayeredMoveLogic> MoveLogicClass)
{
	if (!MoverComp || !MoveLogicClass)
	{
		return nullptr;
	}
	// Logic 对象以 MoverComponent 为 Outer 创建，遍历其子对象即可找到
	TArray<UObject*> Children;
	GetObjectsWithOuter(MoverComp, Children, /*bIncludeNestedObjects=*/false);
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

void UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(UAbilitySystemComponent*     InstigatorASC,
                                                            UAbilitySystemComponent*     TargetASC,
                                                            TSubclassOf<UGameplayEffect> LaunchEffectClass,
                                                            FVector                      LaunchVelocity,
                                                            float                        DurationMs)
{
	if (!InstigatorASC || !TargetASC || !LaunchEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
	Context.AddInstigator(InstigatorASC->GetAvatarActor(), InstigatorASC->GetAvatarActor());

	FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(LaunchEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	// 将 FVector 拆成 3 个浮点 SetByCaller，规避 GE Spec 无法直接携带向量的限制
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityX, LaunchVelocity.X);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityY, LaunchVelocity.Y);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityZ, LaunchVelocity.Z);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_DurationMs, DurationMs);

	InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
}
