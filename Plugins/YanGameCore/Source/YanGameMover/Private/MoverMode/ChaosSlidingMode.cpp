#include "MoverMode/ChaosSlidingMode.h"

#include "Chaos/Character/CharacterGroundConstraintSettings.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterFallingCheck.h"
#include "ChaosMover/Utilities/ChaosGroundMovementUtils.h"
#include "ChaosMover/Utilities/ChaosMoverQueryUtils.h"
#include "Math/UnitConversion.h"
#include "MoverTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosSlidingMode)

UChaosSlidingMode::UChaosSlidingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	GameplayTags.AddTag(Mover_IsOnGround);

	RadialForceLimit = 2000.0f;
	SwingTorqueLimit  = 3000.0f;
	TwistTorqueLimit  = 1500.0f;

	// 滑出地面边缘时切换到下落，与 UChaosWalkingMode 的默认行为对齐
	Transitions.Add(CreateDefaultSubobject<UChaosCharacterFallingCheck>(TEXT("DefaultFallingCheck")));
}

void UChaosSlidingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const
{
	Super::UpdateConstraintSettings(ConstraintSettings);
	ConstraintSettings.FrictionForceLimit             = FUnitConversion::Convert(FrictionForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared);
	ConstraintSettings.DampingFactor                  = GroundDamping;
	ConstraintSettings.MotionTargetMassBias           = FractionalGroundReaction;
	ConstraintSettings.RadialForceMotionTargetScaling = FractionalRadialForceLimitScaling;
}

void UChaosSlidingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosSlidingMode 缺少 Simulation"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!DefaultSimInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosSlidingMode 需要 FChaosMoverSimulationDefaultInputs"));
		return;
	}

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	// 读取上一帧速度，做摩擦衰减：dV = SlideFriction * |V| * dt（只减速，不反向）
	FVector     PriorVelocity = StartingSyncState->GetVelocity_WorldSpace();
	const float PriorSpeed    = PriorVelocity.Size();
	if (PriorSpeed > 0.f)
	{
		const float NewSpeed = FMath::Max(PriorSpeed - SlideFriction * PriorSpeed * DeltaSeconds, 0.f);
		PriorVelocity        = PriorVelocity.GetSafeNormal() * NewSpeed;
	}

	// 坡度重力：取黑板最近地面结果，把重力投影到地面切平面，得到顺坡（切向）加速度
	UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard_Mutable();
	FFloorCheckResult LastFloorResult;
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		const FVector FloorNormal  = LastFloorResult.HitResult.ImpactNormal.GetSafeNormal();
		const FVector GravityAccel = DefaultSimInputs->Gravity;
		// 去除法向分量，仅保留沿坡面向下的切向加速度
		const FVector SlopeGravity = GravityAccel - FloorNormal * FVector::DotProduct(GravityAccel, FloorNormal);
		if (!SlopeGravity.IsNearlyZero(0.01f))
		{
			PriorVelocity += SlopeGravity * DeltaSeconds;
		}
	}

	OutProposedMove.bHasDirIntent          = false;
	OutProposedMove.LinearVelocity         = PriorVelocity;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;

	// 在落点做地面扫描，更新黑板地面结果，并约束移动避免冲入不可行走面
	if (SimBlackboard)
	{
		FVector           OutDeltaPos = FVector::ZeroVector;
		FFloorCheckResult FloorResult;
		FWaterCheckResult WaterResult;
		GetFloorAndCheckMovement(*StartingSyncState, OutProposedMove, *DefaultSimInputs, DeltaSeconds, FloorResult, WaterResult, OutDeltaPos);

		if (DeltaSeconds > 0.f)
		{
			OutProposedMove.LinearVelocity = OutDeltaPos / DeltaSeconds;
		}

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
		SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);
	}
}

void UChaosSlidingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosSlidingMode 缺少 Simulation"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!DefaultSimInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosSlidingMode 需要 FChaosMoverSimulationDefaultInputs"));
		return;
	}

	const FProposedMove           ProposedMove      = Params.ProposedMove;
	const FMoverDefaultSyncState*  StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	TStrongObjectPtr<const USharedChaosCharacterMovementSettings> SharedSettingsStrongPtr = SharedSettingsWeakPtr.Pin();
	if (!SharedSettingsStrongPtr)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosSlidingMode 无法读取共享设置（USharedChaosCharacterMovementSettings）"));
		return;
	}
	const USharedChaosCharacterMovementSettings* SharedSettings = SharedSettingsStrongPtr.Get();

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState = *StartingSyncState;

	const float   DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector UpDirection  = DefaultSimInputs->UpDir;

	FVector                 GroundNormal  = UpDirection;
	const UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard();
	FFloorCheckResult       FloorResult;
	if (SimBlackboard)
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
		GroundNormal = FloorResult.HitResult.ImpactNormal;
	}

	if (FloorResult.IsWalkableFloor())
	{
		const float InitialHeightAboveFloor = FloorResult.FloorDist - GetTargetHeight();

		// 把目标位置放到地面上的目标高度处
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace() - UpDirection * InitialHeightAboveFloor;

		// 贴地模式不靠物理引擎重力推进：此处手动补偿 Mover 重力，并扣除物理引擎将施加的默认重力，
		// 使重力表现与 Mover 设定一致而非物理默认重力
		const FVector ProjectedVelocity = StartingSyncState->GetVelocity_WorldSpace() + DefaultSimInputs->Gravity * DeltaSeconds;
		FVector       TargetVelocity    = ProjectedVelocity - DefaultSimInputs->PhysicsObjectGravity * FVector::UpVector * DeltaSeconds;

		// 若存在非竖直速度或方向意图，则采用提议移动的平面速度；否则仅随重力下落
		constexpr float ParallelCosThreshold = 0.999f;
		const bool      bNonVerticalVelocity = !FVector::Parallel(TargetVelocity.GetSafeNormal(), UpDirection, ParallelCosThreshold);
		const bool      bUseProposedMove     = bNonVerticalVelocity || ProposedMove.bHasDirIntent;
		bool            bNormalVelocityIntent = false;

		if (bUseProposedMove)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(GroundNormal);

			// 法向若存在速度意图则采用提议移动的法向速度，否则保留原竖直速度
			const FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				TargetVelocity += ProposedNormalVelocity - TargetVelocity.ProjectOnToNormal(GroundNormal);
				bNormalVelocityIntent = true;
			}

			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		// 地面自身的线速度（支持匀速移动平台）
		const FVector ProjectedGroundVelocity              = UChaosGroundMovementUtils::ComputeLocalGroundVelocity_Internal(StartingSyncState->GetLocation_WorldSpace(), FloorResult);
		const bool    bIsGroundMoving                      = ProjectedGroundVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER;
		const float   ProjectedRelativeNormalVelocity      = FloorResult.HitResult.ImpactNormal.Dot(TargetVelocity - ProjectedGroundVelocity);
		const float   ProjectedRelativeVerticalVelocity    = GroundNormal.Dot(TargetVelocity - ProjectedGroundVelocity);
		const float   VerticalVelocityLimit                = bNormalVelocityIntent ? 2.0f / DeltaSeconds : FMath::Abs(GroundNormal.Dot(DefaultSimInputs->Gravity) * DeltaSeconds);

		bool bIsLiftingOffSurface = false;
		if ((ProjectedRelativeNormalVelocity > VerticalVelocityLimit) && bIsGroundMoving && (ProjectedRelativeVerticalVelocity > VerticalVelocityLimit))
		{
			bIsLiftingOffSurface = true;
		}

		// 判断踏上/踏下：踏上需高度在 MaxStepHeight 内，踏下需补一段向下速度贴回地面
		const float EndHeightAboveFloor          = InitialHeightAboveFloor + ProjectedRelativeVerticalVelocity * DeltaSeconds;
		const bool  bIsSteppingDown              = InitialHeightAboveFloor > UE_KINDA_SMALL_NUMBER;
		const bool  bIsWithinReach               = EndHeightAboveFloor <= SharedSettings->MaxStepHeight;
		const bool  bIsSupported                 = bIsWithinReach && !bIsLiftingOffSurface;
		const bool  bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboveFloor > 0.0f) && !bIsLiftingOffSurface;
		if (bNeedsVerticalVelocityToTarget)
		{
			TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboveFloor / DeltaSeconds) * UpDirection;
		}

		// 目标朝向：无论是否贴地都施加（滑行下角速度通常为 0，朝向保持不变）
		const FRotator TargetOrientation = UMovementUtils::ApplyAngularVelocityToRotator(StartingSyncState->GetOrientation_WorldSpace(), ProposedMove.AngularVelocityDegrees, DeltaSeconds);

		OutputSyncState.SetTransforms_WorldSpace(TargetPosition,
			TargetOrientation,
			TargetVelocity,
			ProposedMove.AngularVelocityDegrees);
	}

	OutputState.MovementEndState.RemainingMs  = 0.0f;
	OutputState.MovementEndState.NextModeName = Params.StartState.SyncState.MovementMode;
	OutputSyncState.MoveDirectionIntent       = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
}

bool UChaosSlidingMode::CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const
{
	TStrongObjectPtr<const USharedChaosCharacterMovementSettings> SharedSettingsStrongPtr = SharedSettingsWeakPtr.Pin();
	const USharedChaosCharacterMovementSettings* SharedSettings = SharedSettingsStrongPtr.Get();
	if (!SharedSettings)
	{
		return false;
	}

	const float StepHeight = GetTargetHeight() - FloorResult.FloorDist;

	bool            bWalkable    = StepHeight <= SharedSettings->MaxStepHeight;
	constexpr float MinStepHeight = 2.0f;
	const bool      SteppingUp   = StepHeight > MinStepHeight;
	if (bWalkable && SteppingUp)
	{
		bWalkable = UGroundMovementUtils::CanStepUpOnHitSurface(FloorResult.HitResult);
	}

	return bWalkable;
}

void UChaosSlidingMode::GetFloorAndCheckMovement(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs& DefaultSimInputs, float DeltaSeconds, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const
{
	TStrongObjectPtr<const USharedChaosCharacterMovementSettings> SharedSettingsStrongPtr = SharedSettingsWeakPtr.Pin();
	const USharedChaosCharacterMovementSettings* SharedSettings = SharedSettingsStrongPtr.Get();
	if (!SharedSettings)
	{
		return;
	}

	const FVector DeltaPos = ProposedMove.LinearVelocity * DeltaSeconds;
	OutDeltaPos = DeltaPos;

	const float VelocityPadding = FMath::Max(DefaultSimInputs.UpDir.Dot(SyncState.GetVelocity_WorldSpace()) * DeltaSeconds, 0.0f);

	UE::ChaosMover::Utils::FFloorSweepParams SweepParams{
		.ResponseParams     = DefaultSimInputs.CollisionResponseParams,
		.QueryParams        = DefaultSimInputs.CollisionQueryParams,
		.Location           = SyncState.GetLocation_WorldSpace(),
		.DeltaPos           = DeltaPos,
		.UpDir              = DefaultSimInputs.UpDir,
		.World              = DefaultSimInputs.World,
		.QueryDistance      = GetTargetHeight() + SharedSettings->MaxStepHeight + VelocityPadding,
		.QueryRadius        = FMath::Min(GetGroundQueryRadius(), FMath::Max(DefaultSimInputs.PawnCollisionRadius - 5.0f, 0.0f)),
		.MaxWalkSlopeCosine = GetMaxWalkSlopeCosine(),
		.TargetHeight       = GetTargetHeight(),
		.CollisionChannel   = DefaultSimInputs.CollisionChannel
	};

	// 先在落点处扫描地面
	UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);

	if (!OutFloorResult.bBlockingHit)
	{
		// 落点无命中，沿用当前地面结果
		return;
	}

	const bool bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
	if (bWalkableFloor)
	{
		// 找到可行走地面
		return;
	}

	// 命中不可行走面，尝试在落点附近重新寻找可行走面
	const float StepBlockedHeight = GetTargetHeight() - DefaultSimInputs.PawnCollisionHalfHeight + DefaultSimInputs.PawnCollisionRadius;
	const float StepHeight        = GetTargetHeight() - OutFloorResult.FloorDist;

	if (StepHeight > StepBlockedHeight)
	{
		// 碰撞应阻止移动，仅在起点处寻找地面
		SweepParams.QueryRadius = 0.75f * GetGroundQueryRadius();
		SweepParams.DeltaPos    = FVector::ZeroVector;

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);
		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		return;
	}

	if (DeltaPos.SizeSquared() < UE_SMALL_NUMBER)
	{
		// 静止
		OutDeltaPos = FVector::ZeroVector;
		return;
	}

	// 尝试把移动限制在可行走面上
	FVector HorizSurfaceDir       = FVector::VectorPlaneProject(OutFloorResult.HitResult.ImpactNormal, DefaultSimInputs.UpDir);
	float   HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();
	bool    bFoundOutwardDir      = false;
	if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
	{
		HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
		bFoundOutwardDir = true;
	}
	else
	{
		// 平坦的不可行走面，改用 Normal 求水平方向
		HorizSurfaceDir       = FVector::VectorPlaneProject(OutFloorResult.HitResult.Normal, DefaultSimInputs.UpDir);
		HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();

		if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
		{
			HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
			bFoundOutwardDir = true;
		}
	}

	if (bFoundOutwardDir)
	{
		const float DP          = DeltaPos.Dot(HorizSurfaceDir);
		FVector     NewDeltaPos = DeltaPos;
		if (DP > 0.0f)
		{
			// 远离不可行走面：在运动落点做射线查询
			SweepParams.QueryRadius = 0.0f;
		}
		else
		{
			// 否则沿表面寻找可行走地面
			SweepParams.QueryRadius = 0.25f * GetGroundQueryRadius();
			NewDeltaPos             = DeltaPos - DP * HorizSurfaceDir;
		}
		SweepParams.DeltaPos = NewDeltaPos;

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);
		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);

		if (OutFloorResult.bWalkableFloor)
		{
			OutDeltaPos = NewDeltaPos;
		}
	}
	else
	{
		OutDeltaPos = FVector::ZeroVector;
	}
}
