#include "MoverMode/WallClimb/ChaosWallClimbMode.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Utilities/ChaosMoverQueryUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "MoverMode/WallClimb/ChaosWallClimbExitCheck.h"
#include "MoverMode/WallClimb/ChaosWallClimbJumpCheck.h"
#include "MoverMode/WallClimb/WallClimbQueryUtils.h"
#include "MoverMode/WallClimb/YanWallClimbState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosWallClimbMode)

UChaosWallClimbMode::UChaosWallClimbMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 附着于墙面时不再是自由下落，移除继承自 falling 的下落标记，避免动画与技能据此误判
	GameplayTags.RemoveTag(Mover_IsFalling);

	// 在继承自 falling 的 LandingCheck 之外，追加主动脱墙与爬墙跳两个退出判定
	Transitions.Add(CreateDefaultSubobject<UChaosWallClimbExitCheck>(TEXT("WallClimbExitCheck")));
	Transitions.Add(CreateDefaultSubobject<UChaosWallClimbJumpCheck>(TEXT("WallClimbJumpCheck")));
}

void UChaosWallClimbMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	using namespace UE::YanMover::WallClimb;

	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosWallClimbMode 缺少 Simulation"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs*            CharacterInputs  = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultSimInputs || !CharacterInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosWallClimbMode 需要 FChaosMoverSimulationDefaultInputs 与 FCharacterDefaultInputs"));
		return;
	}

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float   DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector UpDir        = DefaultSimInputs->UpDir;
	const FVector CurrentPos   = StartingSyncState->GetLocation_WorldSpace();

	const FYanWallClimbState* ClimbState   = StartState.SyncState.SyncStateCollection.FindDataByType<FYanWallClimbState>();
	const bool                bStateCurrent = ClimbState && ClimbState->IsCurrentFor(TimeStep.ServerFrame);

	// 探测方向取上一帧墙面法线的反向：视角可以转离墙面而角色仍附着，
	// 以视角或角色朝向为探测方向会在转头瞬间误判墙面丢失。首帧无既有法线时退化用视角方向。
	FVector ProbeDir = bStateCurrent ? FVector::VectorPlaneProject(-ClimbState->WallNormal, UpDir).GetSafeNormal() : FVector::ZeroVector;
	if (ProbeDir.IsNearlyZero())
	{
		ProbeDir = FVector::VectorPlaneProject(CharacterInputs->ControlRotation.Vector(), UpDir).GetSafeNormal();
	}

	const FWallSweepParams SweepParams{
		.ResponseParams      = DefaultSimInputs->CollisionResponseParams,
		.QueryParams         = DefaultSimInputs->CollisionQueryParams,
		.Location            = CurrentPos,
		.ProbeDir            = ProbeDir,
		.UpDir               = UpDir,
		.World               = DefaultSimInputs->World,
		.ProbeDistance       = WallProbeDistance,
		.ProbeRadius         = WallProbeRadius,
		.PawnCollisionRadius = DefaultSimInputs->PawnCollisionRadius,
		.CollisionChannel    = DefaultSimInputs->CollisionChannel
	};

	FWallCheckResult WallResult;
	const bool       bFoundWall = WallSweep_Internal(SweepParams, WallResult)
	                              && IsClimbableAngle(WallResult.Normal, UpDir, MinWallAngle, MaxWallAngle);

	// 本帧探墙结果经黑板传给同帧 SimulationTick 落入 SyncState（帧内传递，重模拟会重新求值）
	if (UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard_Mutable())
	{
		SimBlackboard->Set(Blackboard::WallClimbNormal, bFoundWall ? WallResult.Normal : FVector::ZeroVector);
	}

	if (!bFoundWall)
	{
		// 墙面丢失，本帧退化为下落行为，由 UChaosWallClimbExitCheck 同帧切出本模式
		Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
		return;
	}

	const FVector WallNormal  = WallResult.Normal;
	const FVector InwardHoriz = FVector::VectorPlaneProject(-WallNormal, UpDir).GetSafeNormal();
	// 墙面切平面内的横向与上方向，倾斜墙面上 WallUp 即沿墙面向上
	const FVector WallRight = FVector::CrossProduct(UpDir, WallNormal).GetSafeNormal();
	const FVector WallUp    = FVector::CrossProduct(WallNormal, WallRight).GetSafeNormal();

	const float ClimbedTimeMs = bStateCurrent ? ClimbState->ClimbedTimeMs : 0.f;
	const bool  bIsExhausted  = MaxClimbDurationMs > 0.f && ClimbedTimeMs >= MaxClimbDurationMs;

	// 速度投影到墙面切平面，进入时的动量由此自然继承
	FVector TangentVel = ProjectOntoWallPlane(StartingSyncState->GetVelocity_WorldSpace(), WallNormal);

	// 视角背离墙面时保留既有惯性，仅屏蔽新的输入意图
	const bool bCanControl = IsFacingWall(CharacterInputs->ControlRotation.Vector(), WallNormal, UpDir, ViewFacingTolerance);
	if (bCanControl)
	{
		// 输入的朝墙分量映射为沿墙攀爬，横向分量映射为沿墙横移
		const FVector MoveInput    = CharacterInputs->GetMoveInput_WorldSpace();
		float         InwardAmount = FVector::DotProduct(MoveInput, InwardHoriz);
		const float   RightAmount  = FVector::DotProduct(MoveInput, WallRight);

		// 下滑期失去爬升能力，只保留下移与横移
		if (bIsExhausted)
		{
			InwardAmount = FMath::Min(InwardAmount, 0.f);
		}

		const FVector ClimbDir = WallUp * InwardAmount + WallRight * RightAmount;
		if (!ClimbDir.IsNearlyZero())
		{
			// 只在未达上限的方向上加速，不钳制已有速度，使继承的高速动量交由摩擦耗散
			const FVector AccelDir        = ClimbDir.GetSafeNormal();
			const float   SpeedAlongAccel = FVector::DotProduct(TangentVel, AccelDir);
			if (SpeedAlongAccel < MaxClimbSpeed)
			{
				const float InputScale = FMath::Min(ClimbDir.Size(), 1.f);
				const float DeltaSpeed = FMath::Min(ClimbAcceleration * InputScale * DeltaSeconds, MaxClimbSpeed - SpeedAlongAccel);
				TangentVel += AccelDir * DeltaSpeed;
			}
		}
	}

	// 爬墙摩擦：dV = ClimbFriction * |V| * dt，只减速不反向
	const float TangentSpeed = TangentVel.Size();
	if (TangentSpeed > 0.f)
	{
		const float NewSpeed = FMath::Max(TangentSpeed - ClimbFriction * TangentSpeed * DeltaSeconds, 0.f);
		TangentVel           = TangentVel.GetSafeNormal() * NewSpeed;
	}

	if (bIsExhausted)
	{
		// 父类积分会扣除物理引擎重力，故下滑重力需在此显式施加
		TangentVel += ProjectOntoWallPlane(DefaultSimInputs->Gravity, WallNormal) * SlideGravityScale * DeltaSeconds;

		const float DownSpeed = -FVector::DotProduct(TangentVel, WallUp);
		if (DownSpeed > MaxSlideSpeed)
		{
			TangentVel += WallUp * (DownSpeed - MaxSlideSpeed);
		}
	}

	OutProposedMove.LinearVelocity = TangentVel - WallNormal * WallStickSpeed;
	OutProposedMove.MixMode        = EMoveMixMode::OverrideVelocity;
	// 朝向由本模式接管转向墙面，不接受输入的朝向意图
	OutProposedMove.bHasDirIntent = false;

	if (!InwardHoriz.IsNearlyZero())
	{
		OutProposedMove.AngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(
			StartingSyncState->GetOrientation_WorldSpace(), InwardHoriz.ToOrientationRotator(), DeltaSeconds, TurnToWallRate);
	}

	UpdateFloorSweep_Internal(OutProposedMove, DefaultSimInputs, DeltaSeconds, CurrentPos);
}

void UChaosWallClimbMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	using namespace UE::YanMover::WallClimb;

	// 空中积分与重力补偿沿用父类
	Super::SimulationTick_Implementation(Params, OutputState);

	const FYanWallClimbState* StartClimbState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FYanWallClimbState>();
	FYanWallClimbState&       OutClimbState   = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FYanWallClimbState>();

	// 帧号不连续说明中间有帧不在本模式，属新一轮爬墙，计时从零开始
	const float PriorClimbedTimeMs = (StartClimbState && StartClimbState->IsCurrentFor(Params.TimeStep.ServerFrame)) ? StartClimbState->ClimbedTimeMs : 0.f;

	OutClimbState.ClimbedTimeMs         = PriorClimbedTimeMs + Params.TimeStep.StepMs;
	OutClimbState.LastUpdateServerFrame = Params.TimeStep.ServerFrame;

	FVector FrameWallNormal = FVector::ZeroVector;
	if (const UMoverBlackboard* SimBlackboard = Simulation ? Simulation->GetBlackboard() : nullptr)
	{
		SimBlackboard->TryGet(Blackboard::WallClimbNormal, FrameWallNormal);
	}
	OutClimbState.WallNormal = FrameWallNormal;
}

void UChaosWallClimbMode::UpdateFloorSweep_Internal(const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs* DefaultSimInputs, const float DeltaSeconds, const FVector& CurrentPos) const
{
	UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard_Mutable();
	if (!SimBlackboard)
	{
		return;
	}

	const UE::ChaosMover::Utils::FFloorSweepParams SweepParams{
		.ResponseParams     = DefaultSimInputs->CollisionResponseParams,
		.QueryParams        = DefaultSimInputs->CollisionQueryParams,
		.Location           = CurrentPos,
		.DeltaPos           = ProposedMove.LinearVelocity * DeltaSeconds,
		.UpDir              = DefaultSimInputs->UpDir,
		.World              = DefaultSimInputs->World,
		.QueryDistance      = 1.2f * GetTargetHeight(),
		.QueryRadius        = FMath::Min(GetGroundQueryRadius(), FMath::Max(DefaultSimInputs->PawnCollisionRadius - 5.0f, 0.0f)),
		.MaxWalkSlopeCosine = GetMaxWalkSlopeCosine(),
		.TargetHeight       = GetTargetHeight(),
		.CollisionChannel   = DefaultSimInputs->CollisionChannel
	};

	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, FloorResult, WaterResult);

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);
}
