// Fill out your copyright notice in the Description page of Project Settings.


#include "Hero/YanPawn.h"

#include "AbilitySystemComponent.h"
#include "ActorComponent/ModularAbilityExtensionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Hero/YanHealthComponent.h"
#include "ModularTeams.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPawn)


AYanPawn::AYanPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	// YanPawnCapsule 只响应 Weapon_Capsule 与 Weapon_Multi，把 Weapon 通道让给骨骼网格做逐骨骼判定
	Capsule->SetCollisionProfileName(TEXT("YanPawnCapsule"));
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetSimulatePhysics(true);
	Capsule->SetEnableGravity(true);
	SetRootComponent(Capsule);

	// ASC 对接是 Pawn 的固有能力，不交给 Experience 注入，避免加载时序影响依赖方
	AbilityExtensionComponent = CreateDefaultSubobject<UModularAbilityExtensionComponent>(TEXT("AbilityExtensionComponent"));

	// Mover 用 NetworkPrediction 自行同步位置，禁用引擎默认的 ReplicateMovement 避免冲突
	AActor::SetReplicateMovement(false);
}

void AYanPawn::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// 早于 InitializeComponents，使 Mover 及其后端与原生组件走同一条初始化路径，BeginPlay 时已可用。
	// 已有 Mover（例如仍带组件模板的旧蓝图）时不重复创建。
	if (MoverComponentClass && FindComponentByClass<UMoverComponent>() == nullptr)
	{
		UMoverComponent* MoverComponent = NewObject<UMoverComponent>(this, MoverComponentClass, TEXT("Mover"));

		// 两端各自创建且不复制，名字固定才能按路径互相引用；未标记时服务器发出的引用在客户端全部为空
		MoverComponent->SetNetAddressable();
		MoverComponent->RegisterComponent();
	}
}

void AYanPawn::BeginPlay()
{
	Super::BeginPlay();

	// 生命组件由派生蓝图挂载，构造期取不到，故在此按类型查找
	if (UYanHealthComponent* HealthComponent = FindComponentByClass<UYanHealthComponent>())
	{
		HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
	}

	OnPawnInitialized();
}

void AYanPawn::Reset()
{
	// 不走 APawn::Reset 的立即 Destroy：先让蓝图 OnReset 做清理（如清空背包），
	// 再按死亡收尾让出 ASC Avatar，避免与即将生成的新 Pawn 争用同一 ASC
	K2_OnReset();

	UninitAndDestroy();
}

UAbilitySystemComponent* AYanPawn::GetAbilitySystemComponent() const
{
	return AbilityExtensionComponent ? AbilityExtensionComponent->GetModularAbilitySystemComponent() : nullptr;
}

void AYanPawn::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->GetOwnedGameplayTags(TagContainer);
	}
}

bool AYanPawn::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		return AbilitySystem->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool AYanPawn::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		return AbilitySystem->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AYanPawn::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		return AbilitySystem->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AYanPawn::IsActorCrouching(const AActor* Actor)
{
	const UCharacterMoverComponent* MoverComp = Actor ? Actor->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
	return MoverComp && MoverComp->IsCrouching();
}

bool AYanPawn::IsActorFalling(const AActor* Actor)
{
	const UCharacterMoverComponent* MoverComp = Actor ? Actor->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
	return MoverComp && MoverComp->IsFalling();
}

void AYanPawn::CrouchActor(AActor* Actor)
{
	if (UCharacterMoverComponent* MoverComp = Actor ? Actor->FindComponentByClass<UCharacterMoverComponent>() : nullptr)
	{
		MoverComp->Crouch();
	}
}

void AYanPawn::UnCrouchActor(AActor* Actor)
{
	if (UCharacterMoverComponent* MoverComp = Actor ? Actor->FindComponentByClass<UCharacterMoverComponent>() : nullptr)
	{
		MoverComp->UnCrouch();
	}
}

bool AYanPawn::CanActorCrouch(AActor* Actor)
{
	UCharacterMoverComponent* MoverComp = Actor ? Actor->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
	return MoverComp && MoverComp->CanCrouch();
}

USkeletalMeshComponent* AYanPawn::GetGameplayMesh_Implementation()
{
	return FindComponentByClass<USkeletalMeshComponent>();
}

USkeletalMeshComponent* AYanPawn::GetActorGameplayMesh(AActor* Actor)
{
	if (AYanPawn* YanPawn = Cast<AYanPawn>(Actor))
	{
		return YanPawn->GetGameplayMesh();
	}

	return Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void AYanPawn::StopActorAnimMontage(AActor* Actor, UAnimMontage* AnimMontage)
{
	if (!Actor)
	{
		return;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);

	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!AnimInstance)
		{
			continue;
		}

		// 未指定蒙太奇时停止当前正在播放的那个，与 ACharacter 的同名方法一致
		UAnimMontage* MontageToStop = AnimMontage ? AnimMontage : AnimInstance->GetCurrentActiveMontage();
		if (MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop))
		{
			AnimInstance->Montage_Stop(MontageToStop->BlendOut.GetBlendTime(), MontageToStop);
		}
	}
}

void AYanPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, MyTeamID);
}

void AYanPawn::PossessedBy(AController* NewController)
{
	const FGenericTeamId OldTeamID = MyTeamID;

	Super::PossessedBy(NewController);

	// 取当前队伍并监听后续变化
	if (IModularTeamAgentInterface* ControllerAsTeamProvider = Cast<IModularTeamAgentInterface>(NewController))
	{
		MyTeamID = ControllerAsTeamProvider->GetGenericTeamId();
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnControllerChangedTeam);
	}
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void AYanPawn::UnPossessed()
{
	AController* const OldController = GetController();

	const FGenericTeamId OldTeamID = MyTeamID;
	if (IModularTeamAgentInterface* ControllerAsTeamProvider = Cast<IModularTeamAgentInterface>(OldController))
	{
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().RemoveAll(this);
	}

	Super::UnPossessed();

	// ASC 挂在 PlayerState 上，会随控制器转到下一个 Pawn；本 Pawn 必须在此让出 Avatar，
	// 否则重生路径（ServerRestartPlayer 先 UnPossess 再 RestartPlayer）会让新旧两个 Pawn 抢同一个 ASC。
	// 前提是 Pawn 不复用：脱离控制器后只会被销毁，不会再被 Possess 回来。
	ReleaseAbilitySystemAvatar();

	// ASC 的 ActorInfo 缓存了控制器，脱离后需刷新，否则技能仍按旧控制器解析输入与预测
	if (AbilityExtensionComponent)
	{
		AbilityExtensionComponent->HandleControllerChanged();
	}

	MyTeamID = DetermineNewTeamAfterPossessionEnds(OldTeamID);
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void AYanPawn::OnDeathFinished(AActor* OwningActor)
{
	// 让出 Avatar 必须同步完成：重生 Pawn 的 Possess 与本 Pawn 的销毁同在下一帧，先后不可控
	ReleaseAbilitySystemAvatar();

	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::DestroyDueToDeath);
}

void AYanPawn::ReleaseAbilitySystemAvatar()
{
	if (AbilityExtensionComponent == nullptr)
	{
		return;
	}

	// 仅在自己仍是 Avatar 时解绑；已被下一个 Pawn 接管时不可越权清理
	if (const UModularAbilitySystemComponent* AbilitySystem = AbilityExtensionComponent->GetModularAbilitySystemComponent();
	    AbilitySystem != nullptr && AbilitySystem->GetAvatarActor() == this)
	{
		AbilityExtensionComponent->UninitializeAbilitySystem();
	}
}

void AYanPawn::DestroyDueToDeath()
{
	K2_OnDeathFinished();

	UninitAndDestroy();
}

void AYanPawn::UninitAndDestroy()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		DetachFromControllerPendingDestroy();
		SetLifeSpan(0.1f);
	}

	ReleaseAbilitySystemAvatar();

	SetActorHiddenInGame(true);
}

void AYanPawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	ActivateGameplayCamera();
}

void AYanPawn::NotifyRestarted()
{
	Super::NotifyRestarted();

	OnPawnRestarted();
}

void AYanPawn::ActivateGameplayCamera()
{
	APlayerController* PlayerController = GetController<APlayerController>();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	// 相机求值属本地视图行为：组件不自动激活，由拥有端在此接管取景，
	// 使 CameraAsset 内的 Rig 参与求值，视场角等镜头参数方能生效
	if (UGameplayCameraComponent* GameplayCamera = FindComponentByClass<UGameplayCameraComponent>())
	{
		GameplayCamera->ActivateCameraForPlayerController(PlayerController);
	}
}

void AYanPawn::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (GetController() != nullptr)
	{
		UE_LOG(LogModularTeams, Error, TEXT("You can't set the team ID on a possessed pawn (%s); it's driven by the associated controller"), *GetPathNameSafe(this));
		return;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogModularTeams, Error, TEXT("You can't set the team ID on a pawn (%s) except on the authority"), *GetPathNameSafe(this));
		return;
	}

	const FGenericTeamId OldTeamID = MyTeamID;
	MyTeamID = NewTeamID;
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

FGenericTeamId AYanPawn::GetGenericTeamId() const
{
	return MyTeamID;
}

FOnModularTeamIndexChangedDelegate* AYanPawn::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void AYanPawn::OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	const FGenericTeamId MyOldTeamID = MyTeamID;
	MyTeamID = IntegerToGenericTeamId(NewTeam);
	ConditionalBroadcastTeamChanged(this, MyOldTeamID, MyTeamID);
}

void AYanPawn::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}
