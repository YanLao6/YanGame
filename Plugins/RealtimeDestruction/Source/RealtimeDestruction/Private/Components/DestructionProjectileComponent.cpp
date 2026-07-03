// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "Components/DestructionProjectileComponent.h"

#include "DebugConsoleVariables.h"
#include "Components/RealtimeDestructibleMeshComponent.h"
#include "Components/DestructionNetworkComponent.h"
#include "Engine/GameInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "NetworkLogMacros.h"
#include "Data/ImpactProfileDataAsset.h"
#include "Debug/DestructionDebugger.h"
#include "Subsystems/DestructionGameInstanceSubsystem.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "HAL/PlatformTime.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Misc/MessageDialog.h"
#include "Engine/OverlapResult.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#endif


UDestructionProjectileComponent::UDestructionProjectileComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

#if WITH_EDITOR
void UDestructionProjectileComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		                     ? PropertyChangedEvent.Property->GetFName()
		                     : NAME_None;

	// 当ToolShape属性更改时
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDestructionProjectileComponent, ToolShape))
	{
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
				"PropertyEditor");
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}
}
#endif

void UDestructionProjectileComponent::BeginPlay()
{
	Super::BeginPlay();

	// 如果在蓝图中将根组件的碰撞事件连接到OnProjectileHit，需将bAutoBind设为false
	if (bAutoBindHit)
	{
		// 如果所有者的根组件是PrimitiveComponent，则绑定OnHit事件
		AActor* Owner = GetOwner();
		if (!Owner)
		{
			UE_LOG(LogTemp, Warning, TEXT("DestructionProjectileComponent: Owner为空"));
			return;
		}

		UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
		if (RootPrimitive)
		{
			// 绑定Hit事件
			RootPrimitive->OnComponentHit.AddDynamic(this, &UDestructionProjectileComponent::ProcessProjectileHit);

			// 要使Hit事件触发，"Simulation Generates Hit Events"必须为true
			if (!RootPrimitive->GetBodyInstance()->bNotifyRigidBodyCollision)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DestructionProjectileComponent: 根组件的'Simulation Generates Hit Events'已禁用。正在启用。"));
				RootPrimitive->SetNotifyRigidBodyCollision(true);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("DestructionProjectileComponent: 根组件不是PrimitiveComponent。Hit事件将无法工作。"));
		}
	}

	if (!ToolMeshPtr.IsValid())
	{
		if (!EnsureToolMesh())
		{
			UE_LOG(LogTemp, Warning, TEXT("DestructionProjectileComponent: Tool mesh无效。"));
		}
	}


	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UDestructionGameInstanceSubsystem* Subsystem = GI->GetSubsystem<UDestructionGameInstanceSubsystem>())
		{
			CachedDecalDataAsset = Subsystem->FindDataAssetByConfigID(DecalConfigID);
		}
	}
}

void UDestructionProjectileComponent::ProcessProjectileHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!OtherActor)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 忽略与自身的碰撞
	if (OtherActor == Owner)
	{
		return;
	}

	// 忽略Owner是Pawn/Character的情况 (防止角色死亡)
	if (Owner->IsA<APawn>())
	{
		return;
	}

	// 在被击中的Actor中查找破坏组件
	URealtimeDestructibleMeshComponent* DestructComp =
		OtherActor->FindComponentByClass<URealtimeDestructibleMeshComponent>();

	bool bSuccess = false;
	if (DestructComp)
	{
		// 撞击可破坏对象
		int32 ChunkNum = DestructComp->GetChunkNum();
		if (ChunkNum == 0)
		{
			bSuccess = BooleanSourceMesh(DestructComp, Hit);
		}
		else
		{
			bSuccess = ProcessDestructionRequestForChunk(DestructComp, Hit);
		}
	}
	else
	{
		// 撞击不可破坏对象
		OnNonDestructibleHit.Broadcast(Hit);

		if (bDestroyOnNonDestructibleHit && bDestroyOnHit)
		{
			GetOwner()->Destroy();
		}
	}

	if (bSuccess && bDestroyOnHit)
	{
		GetOwner()->Destroy();
	}
}

bool UDestructionProjectileComponent::ProcessDestructionRequestForChunk(
	URealtimeDestructibleMeshComponent* DestructComp,
	const FHitResult& Hit)
{
	if (!DestructComp)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// ===== 从DataAsset加载Tool Shape (必须在创建网格之前!) =====
	// ===== 绝不能将此代码块下移 =====
	FName SurfaceTypeForShape = DestructComp->SurfaceType;
	bool bHasDecalConfig = false;
	FImpactProfileConfig OverrideDecalConfig;

	if (CachedDecalDataAsset)
	{
		bHasDecalConfig = CachedDecalDataAsset->GetConfigRandom(SurfaceTypeForShape, OverrideDecalConfig);
		if (bHasDecalConfig)
		{
			bool bShapeChanged = (CylinderRadius != OverrideDecalConfig.CylinderRadius ||
				CylinderHeight != OverrideDecalConfig.CylinderHeight ||
				SphereRadius != OverrideDecalConfig.SphereRadius ||
				ToolShape != OverrideDecalConfig.ToolShape);

			SurfaceMargin = OverrideDecalConfig.CylinderRadius;
			CylinderRadius = OverrideDecalConfig.CylinderRadius;
			CylinderHeight = OverrideDecalConfig.CylinderHeight;
			SphereRadius = OverrideDecalConfig.SphereRadius;
			ToolShape = OverrideDecalConfig.ToolShape;

			if (bShapeChanged && ToolMeshPtr.IsValid())
			{
				ToolMeshPtr.Reset();
				if (!EnsureToolMesh())
				{
					UE_LOG(LogTemp, Warning, TEXT("DestructionProjectileComponent: Tool mesh无效。"));
				}
			}
		}
	}

	float ToolRadius = ToolShape == EDestructionToolShape::Cylinder ? CylinderRadius : SphereRadius;
	// 为重叠区域增加缓冲
	float OverlappedRadius = ToolRadius * 1.2f;

	// 忽略自身
	TArray<AActor*> ActorToIgnore;
	ActorToIgnore.Add(Owner);

	// 用于防止重复处理的目标Set
	TSet<int32> Targets;

	// 必须包含直接命中的区块
	int32 HitChunkIndex = DestructComp->GetChunkIndex(Hit.GetComponent());
	if (HitChunkIndex != INDEX_NONE)
	{
		Targets.Add(HitChunkIndex);
	}

	AActor* TargetActor = DestructComp->GetOwner();

	// 获取相邻区块索引
	TArray<int32> NearbyChunkIndices;
	NearbyChunkIndices.Reserve(32);
	DestructComp->FindChunksInRadius(Hit.ImpactPoint, OverlappedRadius, NearbyChunkIndices, false);

	const FBox ToolBounds = FBox::BuildAABB(Hit.ImpactPoint, FVector(OverlappedRadius));
	for (const int32 ChunkIndex : NearbyChunkIndices)
	{
		if (Targets.Contains(ChunkIndex))
		{
			continue;
		}

		UDynamicMeshComponent* Chunk = DestructComp->GetChunkMeshComponent(ChunkIndex);
		if (!Chunk)
		{
			continue;
		}

		if (!Chunk->IsVisible() || Chunk->GetOwner() != TargetActor)
		{
			continue;
		}

		const FBox ChunkBox = Chunk->Bounds.GetBox();
		if (ToolBounds.Intersect(ChunkBox))
		{
			DrawDebugAffetedChunks(ToolBounds, FColor::Black);
			Targets.Add(ChunkIndex);
		}
	}

	FVector Direction = GetToolDirection(Hit, Owner);
	FVector ToolStart = Hit.ImpactPoint;
	FVector ToolEnd = ToolStart + (Direction * CylinderHeight);

	TArray<int32> LineAlongChunkIndices;
	LineAlongChunkIndices.Reserve(DestructComp->GetChunkNum());
	DestructComp->FindChunksAlongLine(ToolStart, ToolEnd, ToolRadius, LineAlongChunkIndices, false);
	for (int32 ChunkIndex : LineAlongChunkIndices)
	{
		if (auto ChunkComp = DestructComp->GetChunkMeshComponent(ChunkIndex))
		{
			Targets.Add(ChunkIndex);
		}
	}

	APawn* InstigatorPawn = Owner->GetInstigator();
	APlayerController* PC = InstigatorPawn ? Cast<APlayerController>(InstigatorPawn->GetController()) : nullptr;
	UDestructionNetworkComponent* NetworkComp = PC ? PC->FindComponentByClass<UDestructionNetworkComponent>() : nullptr;
	for (int32 TargetIndex : Targets)
	{
		FRealtimeDestructionRequest Request;
		Request.ImpactPoint = Hit.ImpactPoint;
		Request.ImpactNormal = Hit.ImpactNormal;
		Request.ChunkIndex = TargetIndex;
		Request.ToolForwardVector = GetToolDirection(Hit, Owner);

		Request.ToolMeshPtr = ToolMeshPtr;
		Request.ToolShape = ToolShape;

		int32 HitMaterialID = DestructComp->GetMaterialIDFromFaceIndex(Hit.FaceIndex);
		if (HitMaterialID != 1)
		{
			// 仅在非内部且是直接命中的区块上生成贴花
			if (TargetIndex == HitChunkIndex)
			{
				Request.bSpawnDecal = true;
			}
			else
			{
				Request.bSpawnDecal = false;
			}
		}
		else
		{
			Request.bSpawnDecal = false;
		}

		FName SurfaceType = DestructComp->SurfaceType;
		Request.SurfaceType = SurfaceType;
		Request.DecalConfigID = DecalConfigID; // 用于网络传输

		if (bHasDecalConfig)
		{
			Request.DecalSize = OverrideDecalConfig.DecalSize;
			Request.DecalLocationOffset = OverrideDecalConfig.LocationOffset;
			Request.DecalRotationOffset = OverrideDecalConfig.RotationOffset;
			Request.DecalMaterial = OverrideDecalConfig.DecalMaterial;
			Request.bRandomRotation = OverrideDecalConfig.bRandomDecalRotation;
		}

		SetShapeParameters(Request);

		if (NetworkComp)
		{
			// NetworkComp处理所有网络情况（服务器/客户端/单机）
			NetworkComp->RequestDestruction(DestructComp, Request);
		}
		else
		{
			// 如果没有NetworkComp，则在本地直接处理（单机或配置错误）
			DestructComp->RequestDestruction(Request);
		}

		// 调试
		{
			DrawDebugToolShape(Request.ToolOriginWorld, Request.ToolForwardVector, FColor::Cyan);
			if (bShowAffetedChunks)
			{
				FBox ChunkBox = DestructComp->GetChunkMeshComponent(TargetIndex)->Bounds.GetBox();
				DrawDebugAffetedChunks(ChunkBox, FColor::Red);
			}
		}
	}

	// 广播事件
	OnDestructionRequested.Broadcast(Hit.ImpactPoint, Hit.ImpactNormal);

	return true;
}

void UDestructionProjectileComponent::ProcessSphereDestructionRequestForChunk(
	URealtimeDestructibleMeshComponent* DestructComp, const FVector& ExplosionCenter)
{
	if (!DestructComp)
	{
		return;
	}
	AActor* Owner = GetOwner();

	if (!Owner)
	{
		return;
	}

	// 从DataAsset加载Tool Shape
	FName SurfaceTypeForShape = DestructComp->SurfaceType;
	bool bHasDecalConfig = false;
	FImpactProfileConfig OverrideDecalConfig;


	if (CachedDecalDataAsset)
	{
		bHasDecalConfig = CachedDecalDataAsset->GetConfigRandom(SurfaceTypeForShape, OverrideDecalConfig);
		if (bHasDecalConfig)
		{
			bool bShapeChanged = (SphereRadius != OverrideDecalConfig.SphereRadius);

			SphereRadius = OverrideDecalConfig.SphereRadius;

			if (bShapeChanged)
			{
				ToolMeshPtr.Reset();
				EnsureToolMesh();
			}
		}
	}

	// 查找受影响的区块
	TArray<int32> AffectedChunks;
	AffectedChunks.Reserve(32);

	DestructComp->FindChunksInRadius(ExplosionCenter, SphereRadius * 1.2f, AffectedChunks, false);

	if (AffectedChunks.Num() == 0)
	{
		return;
	}

	if (!ToolMeshPtr.IsValid())
	{
		if (!EnsureToolMesh())
		{
			return;
		}
	}

	// 网络组件
	APawn* InstigatorPawn = Owner->GetInstigator();
	APlayerController* PC = InstigatorPawn ? Cast<APlayerController>(InstigatorPawn->GetController()) : nullptr;
	UDestructionNetworkComponent* NetworkComp = PC ? PC->FindComponentByClass<UDestructionNetworkComponent>() : nullptr;

	// 球体扫描
	TArray<FHitResult> HitResults;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bReturnFaceIndex = true;

	// 计算朝向目标Actor的方向
	AActor* TargetActor = DestructComp->GetOwner();
	FVector DirToTarget = (TargetActor->GetActorLocation() - ExplosionCenter).GetSafeNormal();


	bool bFoundAny = GetWorld()->SweepMultiByChannel(
		HitResults, // 结果数组
		ExplosionCenter,
		ExplosionCenter + DirToTarget,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(SphereRadius * 1.1f),
		QueryParams
	);

	FVector DecalImpactPoint = FVector::ZeroVector;
	FVector DecalImpactNormal = FVector::ZeroVector;
	FVector DirectionToActor = FVector::ZeroVector;

	bool bHitFound = false;
	float ClosestDistance = FLT_MAX;

	if (bFoundAny)
	{
		// 在所有命中的碰撞体中，找到与传入的DestructComp匹配的那个
		for (const FHitResult& Hit : HitResults)
		{
			// 比较最近距离
			if (Hit.GetActor() == TargetActor)
			{
				float Dist = FVector::DistSquared(ExplosionCenter, Hit.ImpactPoint);
				if (Dist < ClosestDistance)
				{
					ClosestDistance = Dist;
					DecalImpactPoint = Hit.ImpactPoint;
					DecalImpactNormal = Hit.ImpactNormal;
					DirectionToActor = (DecalImpactPoint - ExplosionCenter).GetSafeNormal();
					bHitFound = true;
				}
			}
		}
	}

	if (!bHitFound)
	{
		return;
	}


	// 区块循环 (布尔减法)
	bool bFirstChunk = true;

	DrawDebugSphere(GetWorld(), ExplosionCenter, SphereRadius, 24, FColor::Red, true, -1.0f, 0, 3.0f);
	DrawDebugPoint(GetWorld(), ExplosionCenter, 20.0f, FColor::Yellow, true, -1.0f);
	DrawDebugLine(GetWorld(), ExplosionCenter, DecalImpactPoint, FColor::Green, true, -1.0f);
	for (int32 ChunkIndex : AffectedChunks)
	{
		UDynamicMeshComponent* ChunkComp = DestructComp->GetChunkMeshComponent(ChunkIndex);
		if (!ChunkComp || !ChunkComp->IsVisible()) continue;

		// ======= 区块整体销毁逻辑 =======
		FBoxSphereBounds ChunkBounds = ChunkComp->Bounds;
		float DistToCenter = FVector::Dist(ExplosionCenter, ChunkBounds.Origin);
		float ChunkRadius = ChunkBounds.SphereRadius;
		bool bFullyContained = (DistToCenter + ChunkRadius) <= SphereRadius;

		if (bFullyContained)
		{
			// 不进行布尔运算，直接隐藏/删除
			continue;
		}

		// ======= 区块部分销毁逻辑 =======
		FRealtimeDestructionRequest Request;
		Request.ToolOriginWorld = ExplosionCenter;

		// 贴花数据使用上面计算好的
		Request.ImpactPoint = DecalImpactPoint;
		Request.ImpactNormal = DecalImpactNormal;
		Request.ToolForwardVector = DirectionToActor;

		Request.ChunkIndex = ChunkIndex;
		Request.ToolMeshPtr = ToolMeshPtr;
		Request.ToolShape = EDestructionToolShape::Sphere;
		Request.Depth = SphereRadius;

		// 只在第一个区块上生成贴花
		Request.bSpawnDecal = bFirstChunk;
		bFirstChunk = false;

		Request.SurfaceType = DestructComp->SurfaceType;
		Request.DecalConfigID = DecalConfigID;
		Request.ShapeParams.Radius = SphereRadius;
		Request.ShapeParams.StepsPhi = SphereStepsPhi;
		Request.ShapeParams.StepsTheta = SphereStepsTheta;

		if (bHasDecalConfig)
		{
			Request.DecalSize = OverrideDecalConfig.DecalSize;
			Request.DecalLocationOffset = OverrideDecalConfig.LocationOffset;
			Request.DecalRotationOffset = OverrideDecalConfig.RotationOffset;
			Request.DecalMaterial = OverrideDecalConfig.DecalMaterial;
			Request.bRandomRotation = OverrideDecalConfig.bRandomDecalRotation;
		}

		if (NetworkComp)
		{
			NetworkComp->RequestDestruction(DestructComp, Request);
		}
		else
		{
			DestructComp->RequestDestruction(Request);
		}
	}
}

bool UDestructionProjectileComponent::EnsureToolMesh()
{
	if (ToolMeshPtr.IsValid())
	{
		return true;
	}

	UDynamicMesh* TempMesh = NewObject<UDynamicMesh>(this);

	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	PrimitiveOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::SingleGroup;

	switch (ToolShape)
	{
	case EDestructionToolShape::Sphere:
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(
				TempMesh,
				PrimitiveOptions,
				FTransform::Identity,
				SphereRadius,
				SphereStepsPhi,
				SphereStepsTheta,
				EGeometryScriptPrimitiveOriginMode::Center
			);
			break;
		}
	case EDestructionToolShape::Cylinder:
		{
			SurfaceMargin = CylinderRadius;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
				TempMesh,
				PrimitiveOptions,
				FTransform::Identity,
				CylinderRadius,
				CylinderHeight + SurfaceMargin,
				RadialSteps,
				HeightSubdivisions,
				bCapped,
				EGeometryScriptPrimitiveOriginMode::Base
			);
		}
		break;
	}


	ToolMeshPtr = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();
	TempMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Source)
	{
		*ToolMeshPtr = Source;
	});

	// 创建材质 (用于填充孔洞)
	const int32 InternalMaterialID = 1;

	// 启用Attributes
	if (!ToolMeshPtr->HasAttributes())
	{
		ToolMeshPtr->EnableAttributes();
	}

	// 启用MaterialID属性
	if (!ToolMeshPtr->Attributes()->HasMaterialID())
	{
		ToolMeshPtr->Attributes()->EnableMaterialID();
	}

	// 为所有三角形设置Material ID
	UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDAttr = ToolMeshPtr->Attributes()->GetMaterialID();
	for (int32 TriId : ToolMeshPtr->TriangleIndicesItr())
	{
		MaterialIDAttr->SetValue(TriId, InternalMaterialID);
	}
	return true;
}

void UDestructionProjectileComponent::SetShapeParameters(FRealtimeDestructionRequest& OutRequest)
{
	const float PenetrationOffset = 0.5f; // 可调参数
	const float TotalHeight = CylinderHeight + SurfaceMargin;
	switch (OutRequest.ToolShape)
	{
	case EDestructionToolShape::Cylinder:
		{
			/*
			 * 创建Cylinder时以Base为原点 - 圆柱体底部在(0,0,0)，并沿+z轴生成
			 */
			OutRequest.Depth = CylinderHeight;
			OutRequest.ToolOriginWorld = OutRequest.ImpactPoint - (OutRequest.ToolForwardVector * SurfaceMargin);
			break;
		}
	case EDestructionToolShape::Sphere:
		{
			OutRequest.Depth = SphereRadius;
			if (OutRequest.ToolOriginWorld.IsZero())
			{
				OutRequest.ToolOriginWorld = OutRequest.ImpactPoint + (OutRequest.ToolForwardVector * PenetrationOffset);
			}
		}
		break;
	default:
		{
			OutRequest.Depth = CylinderHeight;
			OutRequest.ToolOriginWorld = OutRequest.ImpactPoint - (OutRequest.ToolForwardVector * SurfaceMargin);
		}
		break;
	}

	// 根据Shape类型填充参数 (用于网络传输)
	switch (ToolShape)
	{
	case EDestructionToolShape::Cylinder:
		OutRequest.ShapeParams.Radius = CylinderRadius;
		OutRequest.ShapeParams.Height = CylinderHeight;
		OutRequest.ShapeParams.RadiusSteps = RadialSteps;
		OutRequest.ShapeParams.HeightSubdivisions = HeightSubdivisions;
		OutRequest.ShapeParams.bCapped = bCapped;
		OutRequest.ShapeParams.SurfaceMargin = SurfaceMargin;
		break;

	case EDestructionToolShape::Sphere:
		OutRequest.ShapeParams.Radius = SphereRadius;
		OutRequest.ShapeParams.StepsPhi = SphereStepsPhi;
		OutRequest.ShapeParams.StepsTheta = SphereStepsTheta;
		break;

	default:
		OutRequest.ShapeParams.Radius = CylinderRadius;
		OutRequest.ShapeParams.Height = CylinderHeight;
		OutRequest.ShapeParams.RadiusSteps = RadialSteps;
		OutRequest.ShapeParams.HeightSubdivisions = HeightSubdivisions;
		OutRequest.ShapeParams.bCapped = bCapped;
		OutRequest.ShapeParams.SurfaceMargin = SurfaceMargin;
		break;
	}
}

void UDestructionProjectileComponent::DrawDebugToolShape(const FVector& Center, const FVector& Direction,
                                                         const FColor& Color) const
{
	if (!GetWorld() || !bShowToolShape)
	{
		return;
	}

	switch (ToolShape)
	{
	case EDestructionToolShape::Cylinder:
		{
			DrawDebugCylinderInternal(Center, Direction, Color);
			break;
		}
	case EDestructionToolShape::Sphere:
		{
			DrawDebugSphereInternal(Center, Color);
			break;
		}
	}
}

void UDestructionProjectileComponent::DrawDebugAffetedChunks(const FBox& ChunkBox, const FColor& Color) const
{
	if (!bShowAffetedChunks || !GetWorld())
	{
		return;
	}

	DrawDebugBox(GetWorld(), ChunkBox.GetCenter(),
	             ChunkBox.GetExtent() + FVector(0.5f), Color, false,
	             2.0f, 0, 2.5f);
}

void UDestructionProjectileComponent::DrawDebugCylinderInternal(const FVector& Center, const FVector& Direction,
                                                                const FColor& Color) const
{
	float TotalHeight = CylinderHeight + SurfaceMargin;
	FVector Start = Center;
	FVector End = Center + (Direction * TotalHeight);
	DrawDebugCylinder(GetWorld(), Start, End, CylinderRadius, 16, Color, false, 5.0f, 0, 1.5f);
	DrawDebugPoint(GetWorld(), Start + (Direction * SurfaceMargin), 10.0f, FColor::Red, false, 5.0f);
}

void UDestructionProjectileComponent::DrawDebugSphereInternal(const FVector& Center, const FColor& Color) const
{
	DrawDebugSphere(GetWorld(), Center, SphereRadius, 16, Color, false, 5.0f, 0, 1.5f);
}

FVector UDestructionProjectileComponent::GetToolDirection(const FHitResult& Hit, AActor* Owner) const
{
	FVector Direction = (Hit.TraceEnd - Hit.TraceStart);

	if (Direction.IsNearlyZero() && Owner)
	{
		Direction = Owner->GetVelocity();
	}

	if (Direction.IsNearlyZero() && Owner)
	{
		Direction = Owner->GetActorForwardVector();
	}

	if (Direction.IsNearlyZero() && Owner)
	{
		Direction = GetForwardVector();
	}

	if (Direction.IsNearlyZero())
	{
		Direction = Hit.ImpactNormal;
	}

	return Direction.GetSafeNormal();
}

void UDestructionProjectileComponent::RequestDestructionManual(const FHitResult& HitResult)
{
	if (!HitResult.GetActor())
	{
		return;
	}

	URealtimeDestructibleMeshComponent* DestructComp =
		HitResult.GetActor()->FindComponentByClass<URealtimeDestructibleMeshComponent>();

	if (DestructComp)
	{
		// 撞击可破坏对象
		int32 ChunkNum = DestructComp->GetChunkNum();
		if (ChunkNum == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: 没有区块，正在创建区块"), *DestructComp->GetName());
		}
		else
		{
			ProcessDestructionRequestForChunk(DestructComp, HitResult);
		}
	}
	else
	{
		// 撞击不可破坏对象
		OnNonDestructibleHit.Broadcast(HitResult);

		if (bDestroyOnNonDestructibleHit && bDestroyOnHit)
		{
			GetOwner()->Destroy();
		}
	}
}

void UDestructionProjectileComponent::RequestDestructionAtLocation(const FVector& Center)
{
	AActor* Owner = GetOwner();

	if (!Owner)
	{
		return;
	}

	// 保存当前的ToolShape (RequestDestructionAtLocation只支持球体)
	EDestructionToolShape PreviousShape = ToolShape;
	ToolShape = EDestructionToolShape::Sphere;
	bool bNeedRecreate = (PreviousShape != ToolShape);

	// 如果需要，重置网格
	if (bNeedRecreate && ToolMeshPtr.IsValid())
	{
		ToolMeshPtr.Reset();
	}

	// 创建ToolMesh (如果不存在或已重置)
	if (!ToolMeshPtr.IsValid())
	{
		if (!EnsureToolMesh())
		{
			UE_LOG(LogTemp, Warning, TEXT("RequestDestructionAtLocation: Tool mesh无效。"));
			return;
		}
	}

	if (CachedDecalDataAsset)
	{
		FImpactProfileConfig OverrideConfig;
		// 没有SurfaceType，使用Default
		if (CachedDecalDataAsset->GetConfigRandom(FName("Default"), OverrideConfig))
		{
			bool bRadiusChanged = (SphereRadius != OverrideConfig.SphereRadius);

			SphereRadius = OverrideConfig.SphereRadius;

			if (bRadiusChanged && ToolMeshPtr.IsValid())
			{
				ToolMeshPtr.Reset();
			}
		}
	}
	// 使用SphereOverlap查找周围的DestructibleMesh
	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner); // 排除自身

	float OverlapRadius = SphereRadius;
	bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		Center,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(OverlapRadius),
		QueryParams
	);

	if (!bHasOverlap)
	{
		return;
	}

	// 从重叠的Actor中查找DestructibleMeshComponent
	TSet<URealtimeDestructibleMeshComponent*> ProcessedComps; // 防止重复处理
	UE_LOG(LogTemp, Warning, TEXT("OverlapResults数量: %d"), OverlapResults.Num());

	// 收集区块
	for (const FOverlapResult& Result : OverlapResults)
	{
		AActor* HitActor = Result.GetActor();

		if (!HitActor)
		{
			continue;
		}

		URealtimeDestructibleMeshComponent* DestructComp =
			HitActor->FindComponentByClass<URealtimeDestructibleMeshComponent>();

		if (!DestructComp || ProcessedComps.Contains(DestructComp))
		{
			continue;
		}

		ProcessedComps.Add(DestructComp);
		ProcessSphereDestructionRequestForChunk(DestructComp, Center);
	}

	// 广播事件
	OnDestructionRequested.Broadcast(Center, FVector::UpVector);

	// 投射物的销毁由调用者处理
}

void UDestructionProjectileComponent::GetCalculateDecalSize(FName SurfaceType, FVector& LocationOffset,
                                                            FRotator& RotatorOffset,
                                                            FVector& SizeOffset) const
{
	if (CachedDecalDataAsset)
	{
		// 如果SurfaceType为空，则使用"Default"
		FName ActualSurfaceType = SurfaceType.IsNone() ? FName("Default") : SurfaceType;

		FImpactProfileConfig Config;
		if (CachedDecalDataAsset->GetConfig(ActualSurfaceType, 0, Config))
		{
			LocationOffset = Config.LocationOffset;
			RotatorOffset = Config.RotationOffset;
			SizeOffset = Config.DecalSize;
			return;
		}
	}
	if (bUseDecalSizeOverride)
	{
		LocationOffset = DecalLocationOffset;
		RotatorOffset = DecalRotationOffset;
		SizeOffset = DecalSizeOverride;
		return;
	}

	float BaseSize = 0.0f;
	LocationOffset = FVector::ZeroVector;
	RotatorOffset = FRotator::ZeroRotator;
	switch (ToolShape)
	{
	case EDestructionToolShape::Cylinder:
		BaseSize = CylinderRadius;
		break;

	case EDestructionToolShape::Sphere:
		BaseSize = SphereRadius;
		break;

	default:
		break;
	}

	float FinalSize = BaseSize * DecalSizeMultiplier;
	SizeOffset = FVector(FinalSize, FinalSize, FinalSize);
}

bool UDestructionProjectileComponent::RequestDestructionFromProjectile(UPrimitiveComponent* HitComp, AActor* OtherActor,
                                                                        UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit,
	bool bDestroyProjectile)
{
	if (!OtherActor)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// 忽略与自身的碰撞
	if (OtherActor == Owner)
	{
		return false;
	}

	// 忽略Owner是Pawn/Character的情况 (防止角色死亡)
	if (Owner->IsA<APawn>())
	{
		return false;
	}

	// 在被击中的Actor中查找破坏组件
	URealtimeDestructibleMeshComponent* DestructComp =
		OtherActor->FindComponentByClass<URealtimeDestructibleMeshComponent>();

	bool bSuccess = ProcessDestructionRequestForChunk(DestructComp, Hit);

	if ((bSuccess && bDestroyProjectile) || !DestructComp)
	{
		Owner->Destroy();
	}

	return bSuccess;
}

bool UDestructionProjectileComponent::RequestDestructionFromHitScan(URealtimeDestructibleMeshComponent* DestructComp,
                                                                    const FHitResult& Hit, bool bDestroyProjectile)
{
	bool bSuccess = ProcessDestructionRequestForChunk(DestructComp, Hit);

	if (bSuccess && bDestroyProjectile)
	{
		if (AActor* Owner = GetOwner())
		{
			if (!Owner->IsA<APawn>())
			{
				Owner->Destroy();
			}
		}
	}

	return bSuccess;
}

void UDestructionProjectileComponent::UpdateCachedDecalDataAssetIfNeeded()
{
	// 仅当ConfigID更改时才更新
	if (CachedConfigID != DecalConfigID)
	{
		CachedConfigID = DecalConfigID;

		if (UGameInstance* GI = GetWorld()->GetGameInstance())
		{
			if (UDestructionGameInstanceSubsystem* Subsystem = GI->GetSubsystem<UDestructionGameInstanceSubsystem>())
			{
				CachedDecalDataAsset = Subsystem->FindDataAssetByConfigID(DecalConfigID);
			}
		}
	}
}

bool UDestructionProjectileComponent::BooleanSourceMesh(
	URealtimeDestructibleMeshComponent* DestructComp,
	const FHitResult& Hit,
	bool bDestroyProjectile)
{
	if (!DestructComp)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	FName SurfaceTypeForShape = DestructComp->SurfaceType;
	bool bHasDecalConfig = false;
	FImpactProfileConfig OverrideDecalConfig;

	if (CachedDecalDataAsset)
	{
		bHasDecalConfig = CachedDecalDataAsset->GetConfigRandom(SurfaceTypeForShape, OverrideDecalConfig);
		if (bHasDecalConfig)
		{
			bool bShapeChanged = (CylinderRadius != OverrideDecalConfig.CylinderRadius ||
				CylinderHeight != OverrideDecalConfig.CylinderHeight ||
				SphereRadius != OverrideDecalConfig.SphereRadius ||
				ToolShape != OverrideDecalConfig.ToolShape);

			SurfaceMargin = OverrideDecalConfig.CylinderRadius;
			CylinderRadius = OverrideDecalConfig.CylinderRadius;
			CylinderHeight = OverrideDecalConfig.CylinderHeight;
			SphereRadius = OverrideDecalConfig.SphereRadius;
			ToolShape = OverrideDecalConfig.ToolShape;

			if (bShapeChanged && ToolMeshPtr.IsValid())
			{
				ToolMeshPtr.Reset();
				if (!EnsureToolMesh())
				{
					UE_LOG(LogTemp, Warning, TEXT("DestructionProjectileComponent: Tool mesh无效。"));
				}
			}
		}
	}


	APawn* InstigatorPawn = Owner->GetInstigator();
	APlayerController* PC = InstigatorPawn ? Cast<APlayerController>(InstigatorPawn->GetController()) : nullptr;
	UDestructionNetworkComponent* NetworkComp = PC ? PC->FindComponentByClass<UDestructionNetworkComponent>() : nullptr;

	FRealtimeDestructionRequest Request;
	Request.ImpactPoint = Hit.ImpactPoint;
	Request.ImpactNormal = Hit.ImpactNormal;
	Request.ChunkIndex = 1;
	Request.ToolForwardVector = GetToolDirection(Hit, Owner);

	Request.ToolMeshPtr = ToolMeshPtr;
	Request.ToolShape = ToolShape;

	Request.bSpawnDecal = false;

	FName SurfaceType = DestructComp->SurfaceType;
	Request.SurfaceType = SurfaceType;
	Request.DecalConfigID = DecalConfigID; // 用于网络传输

	if (bHasDecalConfig)
	{
		Request.DecalSize = OverrideDecalConfig.DecalSize;
		Request.DecalLocationOffset = OverrideDecalConfig.LocationOffset;
		Request.DecalRotationOffset = OverrideDecalConfig.RotationOffset;
		Request.DecalMaterial = OverrideDecalConfig.DecalMaterial;
		Request.bRandomRotation = OverrideDecalConfig.bRandomDecalRotation;
	}

	SetShapeParameters(Request);

	if (NetworkComp)
	{
		// NetworkComp处理所有网络情况
		NetworkComp->RequestDestruction(DestructComp, Request);
	}
	else
	{
		// 如果没有NetworkComp，则在本地直接处理
		DestructComp->RequestDestruction(Request);
	}


	// 广播事件
	OnDestructionRequested.Broadcast(Hit.ImpactPoint, Hit.ImpactNormal);

	return true;
}