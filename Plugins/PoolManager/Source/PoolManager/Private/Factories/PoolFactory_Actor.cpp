// Copyright (c) Yevhenii Selivanov

#include "Factories/PoolFactory_Actor.h"

// Pool Manager
#include "Data/PoolObjectData.h"
#include "Data/PoolObjectState.h"
#include "Data/TakeFromPoolPayload.h"

// UE
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoolFactory_Actor)

// 重写以处理继承自 Actor 的类
const UClass* UPoolFactory_Actor::GetObjectClass_Implementation() const
{
	return AActor::StaticClass();
}

/*********************************************************************************************
 * 创建
 ********************************************************************************************* */

// 重写以改用引擎的 Spawn Actor 方式生成 Actor
UObject* UPoolFactory_Actor::SpawnNow_Implementation(const FSpawnRequest& Request)
{
	// 不调用 Super，改用 Spawn Actor 而非 NewObject

	UWorld* World = GetWorld();
	checkf(World, TEXT("ERROR: [%i] %hs:\n'World' is null!"), __LINE__, __FUNCTION__);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.OverrideLevel = World->PersistentLevel; // 新对象始终放在 Persistent 关卡上
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.bDeferConstruction = true; // 延迟构造，先把对象加入池中
	SpawnParameters.bNoFail = true; // 生成失败时不中断
#if WITH_EDITORONLY_DATA
	SpawnParameters.bCreateActorPackage = false; // 不把该运行时 Actor 烘焙进 World Partition 关卡
#endif

	return World->SpawnActor(Request.GetClassChecked<AActor>(), &Request.Transform, SpawnParameters);
}

// 重写以完成 Actor 的延迟生成
void UPoolFactory_Actor::OnPreRegistered(const FSpawnRequest& Request, const FPoolObjectData& ObjectData)
{
	Super::OnPreRegistered(Request, ObjectData);

	AActor& SpawnedActor = ObjectData.GetChecked<AActor>();
	SpawnedActor.FinishSpawning(Request.Transform);
}

/*********************************************************************************************
 * 销毁
 ********************************************************************************************* */

// 重写以改用引擎的 Destroy Actor 方式销毁给定 Actor
void UPoolFactory_Actor::Destroy_Implementation(UObject* Object)
{
	// 不调用 Super，改用 Destroy Actor 而非 ConditionalBeginDestroy

	AActor* Actor = CastChecked<AActor>(Object);
	checkf(IsValid(Actor), TEXT("ERROR: [%i] %hs:\n'IsValid(Actor)' is null!"), __LINE__, __FUNCTION__);
	Actor->Destroy();
}

/*********************************************************************************************
 * 池化
 ********************************************************************************************* */

// 重写以在对象从池中取出前设置 Actor 的 Transform
void UPoolFactory_Actor::OnTakeFromPool_Implementation(UObject* Object, const FTakeFromPoolPayload& Payload)
{
	// Transform 只设置一次：仅在从池中取出时设置，新生成的对象在 spawn 时已经设置过
	if (!Payload.bIsNewSpawned)
	{
		AActor* Actor = CastChecked<AActor>(Object);
		Actor->SetActorTransform(Payload.Transform);
	}

	// Super 最后调用，这样回调中依赖 Transform 的逻辑才是安全的
	Super::OnTakeFromPool_Implementation(Object, Payload);
}

// 重写以在对象归还到池前重置 Actor 的 Transform
void UPoolFactory_Actor::OnReturnToPool_Implementation(UObject* Object)
{
	Super::OnReturnToPool_Implementation(Object);

	// SetCollisionEnabled 不会被网络复制，客户端仍会与隐藏的 Actor 碰撞，因此把它移到极远处
	AActor* Actor = CastChecked<AActor>(Object);
	Actor->SetActorLocation(MaxPos);
}

// 重写以按新状态改变可见性、碰撞、Tick 等
void UPoolFactory_Actor::OnChangedStateInPool_Implementation(EPoolObjectState NewState, UObject* InObject)
{
	Super::OnChangedStateInPool_Implementation(NewState, InObject);

	AActor* Actor = CastChecked<AActor>(InObject);
	const bool bActivate = NewState == EPoolObjectState::Active;

	Actor->SetActorHiddenInGame(!bActivate);
	Actor->SetActorEnableCollision(bActivate);
	Actor->SetActorTickEnabled(bActivate);
}
