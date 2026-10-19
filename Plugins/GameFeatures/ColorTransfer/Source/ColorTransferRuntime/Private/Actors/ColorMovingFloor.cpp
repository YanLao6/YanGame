#include "Actors/ColorMovingFloor.h"

#include "AlphaBlend.h"
#include "ChaosMover/Backends/ChaosMoverBackend.h"
#include "ChaosMover/PathedMovement/ChaosPathedMovementControllerComponent.h"
#include "ChaosMover/PathedMovement/ChaosPathedMovementMode.h"
#include "ChaosMover/PathedMovement/ChaosPointMovementPathPattern.h"
#include "Components/StaticMeshComponent.h"
#include "MoverComponent.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorMovingFloor)

namespace
{
	template <typename PropertyType>
	PropertyType* FindPathPropertyChecked(const UObject* Object, const TCHAR* PropertyName)
	{
		PropertyType* Property = FindFProperty<PropertyType>(Object->GetClass(), PropertyName);
		checkf(Property, TEXT("%s 缺少路径配置属性 %s"), *Object->GetClass()->GetName(), PropertyName);
		return Property;
	}
}

AColorMovingFloor::AColorMovingFloor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetEnableGravity(false);
	Mesh->BodyInstance.bSimulatePhysics               = true;
	Mesh->BodyInstance.bUpdateKinematicFromSimulation = true;
	RootComponent                                     = Mesh;

	PlatformMover                       = CreateDefaultSubobject<UMoverComponent>(TEXT("PlatformMover"));
	PlatformMover->BackendClass         = UChaosMoverBackendComponent::StaticClass();
	PlatformMover->StartingMovementMode = TEXT("PlatformPath");
	PlatformMover->SmoothingMode        = EMoverSmoothingMode::None;

	PathMode     = ObjectInitializer.CreateDefaultSubobject<UChaosPathedMovementMode>(PlatformMover, TEXT("PlatformPathMode"));
	PointPattern = ObjectInitializer.CreateDefaultSubobject<UChaosPointMovementPathPattern>(PathMode, TEXT("PlatformPointPattern"));
	PlatformMover->MovementModes.Add(PlatformMover->StartingMovementMode, PathMode);

	// 使用平台自己的事件调度设置，避免继承全局最大预测窗口作为启动延迟。
	NetworkPhysicsSettings                                     = CreateDefaultSubobject<UNetworkPhysicsSettingsComponent>(TEXT("NetworkPhysicsSettings"));
	MovementController                                         = CreateDefaultSubobject<UChaosPathedMovementControllerComponent>(TEXT("MovementController"));
	MovementController->InitialProperties.bWantsOneWayPlayback = true;
	PlatformMover->InputProducer                               = MovementController;

	bReplicates = true;
	SetReplicatingMovement(false);
	SetPhysicsReplicationMode(EPhysicsReplicationMode::Resimulation);
	PrimaryActorTick.bCanEverTick = false;
}

void AColorMovingFloor::PreInitializeComponents()
{
	ConfigureMovementPath();

	// Blueprint 可能覆盖组件默认值；物理后端初始化前必须具备有效的物理碰撞体。
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetEnableGravity(false);
	Mesh->BodyInstance.bUpdateKinematicFromSimulation = true;
	Mesh->SetSimulatePhysics(true);

	Super::PreInitializeComponents();
}

void AColorMovingFloor::ConfigureMovementPath()
{
	// 引擎未提供这些路径属性的公开 setter，反射配置仅发生在游戏线程初始化阶段。
	FindPathPropertyChecked<FBoolProperty>(PathMode, TEXT("bUseJointConstraint"))->SetPropertyValue_InContainer(PathMode, false);
	FindPathPropertyChecked<FFloatProperty>(PathMode, TEXT("OneWayPlaybackDuration"))->SetPropertyValue_InContainer(PathMode, FMath::Max(MoveDuration, 0.1f));
	const FEnumProperty* EasingProperty = FindPathPropertyChecked<FEnumProperty>(PathMode, TEXT("Easing"));
	EasingProperty->GetUnderlyingProperty()->SetIntPropertyValue(EasingProperty->ContainerPtrToValuePtr<void>(PathMode), static_cast<int64>(EAlphaBlendOption::HermiteCubic));

	const FArrayProperty*                                PatternsProperty = FindPathPropertyChecked<FArrayProperty>(PathMode, TEXT("PathPatterns"));
	TArray<TObjectPtr<UChaosPathedMovementPatternBase>>& Patterns         = *PatternsProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UChaosPathedMovementPatternBase>>>(PathMode);
	Patterns.Reset();
	Patterns.Add(PointPattern);

	const FArrayProperty*                 PointsProperty = FindPathPropertyChecked<FArrayProperty>(PointPattern, TEXT("PathPoints"));
	TArray<FChaosPointMovementPathPoint>& Points         = *PointsProperty->ContainerPtrToValuePtr<TArray<FChaosPointMovementPathPoint>>(PointPattern);
	Points.SetNum(2);
	Points[0]               = FChaosPointMovementPathPoint();
	Points[0].Basis         = EChaosPointMovementLocationBasis::PathOrigin;
	Points[0].RotationBasis = EChaosPointMovementLocationBasis::PathOrigin;
	Points[1]               = FChaosPointMovementPathPoint();
	Points[1].Basis         = EChaosPointMovementLocationBasis::World;
	Points[1].Location      = EndLocation;
	Points[1].RotationBasis = EChaosPointMovementLocationBasis::PathOrigin;
}

void AColorMovingFloor::BeginPlay()
{
	MovementController->OnPathedMovementStarted.AddDynamic(this, &ThisClass::OnPathStarted);
	Super::BeginPlay();
}

void AColorMovingFloor::AuthStartMoving()
{
	if (!HasAuthority() || IsMoving())
	{
		return;
	}

	// 联机时由控制器安排共同的未来物理帧，单机无需等待网络调度。
	const bool bIsScheduled = GetNetMode() != NM_Standalone;
	bStartRequestPending    = true;
	MovementController->RequestReversePlayback(bMovingToEnd, EChaosPathedMovementExecutionType::AuthorityOnly, bIsScheduled);
	MovementController->RequestStartPlayingPath(EChaosPathedMovementExecutionType::AuthorityOnly, bIsScheduled);
	bMovingToEnd = !bMovingToEnd;
}

bool AColorMovingFloor::IsMoving() const
{
	return bStartRequestPending || MovementController->IsPlayingPath();
}

void AColorMovingFloor::OnPathStarted()
{
	bStartRequestPending = false;
}
