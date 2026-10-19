// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Actor/ModularExperiencePawn.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "ModularTeamAgentInterface.h"
#include "YanPawn.generated.h"

#define UE_API YANGAMEPLAY_API

class UAnimMontage;
class UModularAbilityExtensionComponent;
class UMoverComponent;
class USkeletalMeshComponent;

/**
 * 项目基础 Pawn，集成 Mover 运动系统。
 * 移动输入到 Mover 的桥接由 UYanHeroInputComponent 实现（IMoverInputProducerInterface），
 * Pawn 只负责按 MoverComponentClass 在初始化早期创建 Mover 组件，不感知其配置。
 *
 * 队伍归属由控制器（最终是 PlayerState）驱动，Pawn 只缓存并复制结果，供未被控制时仍可查询。
 *
 * ASC 由 PlayerState 持有。本类实现 IAbilitySystemInterface 与 IGameplayTagAssetInterface，把查询
 * 转发到 Ability 扩展组件所绑定的 ASC，使按 Actor 解析 ASC 的通用代码传入 Pawn 时也能取到正确结果。
 */
UCLASS(MinimalAPI)
class AYanPawn : public AModularExperiencePawn, public IAbilitySystemInterface, public IModularTeamAgentInterface
{
	GENERATED_BODY()

public:
	UE_API explicit AYanPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin AActor Interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void Reset() override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	//~Begin APawn Interface
	UE_API virtual void PossessedBy(AController* NewController) override;
	UE_API virtual void UnPossessed() override;
	UE_API virtual void PawnClientRestart() override;
	UE_API virtual void NotifyRestarted() override;
	//~End APawn Interface

	//~Begin IAbilitySystemInterface Interface
	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End IAbilitySystemInterface Interface

	//~Begin IGameplayTagAssetInterface Interface
	UE_API virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	UE_API virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	UE_API virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	UE_API virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	//~End IGameplayTagAssetInterface Interface

	/**
	 * 脚本层初始化入口，时机等同 BeginPlay。
	 *
	 * 派生蓝图重写 Event BeginPlay 会覆盖父类实现，使脚本层挂在 BeginPlay 上的初始化
	 * 被静默截断。本事件不在该覆盖链上，故用它承载不容缺失的初始化。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Yan|Pawn")
	UE_API void OnPawnInitialized();

	/**
	 * 控制权就绪，此时可安全读取 Controller 与 Owner。
	 *
	 * 服务端的全部 Pawn 与拥有端的玩家 Pawn 各触发一次，供脚本层刷新依赖归属的表现，
	 * 例如按视角切换两具网格的可见性。与 OnPawnInitialized 同样不受子蓝图事件覆盖影响。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Yan|Pawn")
	UE_API void OnPawnRestarted();

	//~Begin IModularTeamAgentInterface Interface
	UE_API virtual void                                SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId                      GetGenericTeamId() const override;
	UE_API virtual FOnModularTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End IModularTeamAgentInterface Interface

	/**
	 * 查询任意 Actor 是否处于蹲伏姿态。
	 *
	 * 姿态由 Mover 的 stance 修改器维护。做成接受 Actor 的静态查询，
	 * 是为了让持有 Actor 引用的蓝图无需先转型即可读取，
	 * 对应 ACharacter 的 bIsCrouched 在迁入资产中的用法。
	 * 目标未使用 Mover 时恒为 false。
	 *
	 * 注意：派生的 AYanDefaultPawn 已在脚本层提供同义的成员方法 IsCrouching()，
	 * 此处不再定义同名成员，以免与其构成重载冲突。
	 */
	UFUNCTION(BlueprintPure, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API bool IsActorCrouching(const AActor* Actor);

	/** 查询任意 Actor 是否处于滞空状态，含跳跃上升段。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API bool IsActorFalling(const AActor* Actor);

	/**
	 * 令目标进入蹲伏。
	 *
	 * 姿态经 Mover 的 stance 修改器生效，与 ACharacter 的同名方法语义一致。
	 * 目标未使用 Mover 时无效果。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API void CrouchActor(AActor* Actor);

	/** 令目标解除蹲伏。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API void UnCrouchActor(AActor* Actor);

	/** 目标当前能否蹲伏。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API bool CanActorCrouch(AActor* Actor);

	/**
	 * 游戏逻辑所用的骨骼网格：插槽定位、命中判定与蒙太奇均以此为准。
	 *
	 * 基类返回首个骨骼网格组件。持有多具网格的派生类（例如第一人称与第三人称各一具全身骨骼）
	 * 必须重写为权威的那一具，否则取到哪一具取决于组件注册顺序。
	 * 对应 ACharacter::Mesh 在迁入资产中的用法。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Yan|Pawn")
	UE_API USkeletalMeshComponent* GetGameplayMesh();

	/** 取任意 Actor 的游戏逻辑骨骼网格，供持有 Actor 引用的蓝图免转型调用。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API USkeletalMeshComponent* GetActorGameplayMesh(AActor* Actor);

	/**
	 * 停止目标身上正在播放的蒙太奇。
	 *
	 * 对应 ACharacter 的同名方法：解析目标的骨骼网格与动画实例后停止播放。
	 * 传入的蒙太奇为空时停止当前正在播放的那个。
	 * 对多具网格的角色逐具停止——只停权威那具会让另一具继续演完。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Pawn", meta = (DefaultToSelf = "Actor"))
	static UE_API void StopActorAnimMontage(AActor* Actor, UAnimMontage* AnimMontage);

	/** Ability 扩展组件，Pawn 与 ASC 的对接入口；恒存在，不随 Experience 变化。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Pawn")
	UModularAbilityExtensionComponent* GetAbilityExtensionComponent() const { return AbilityExtensionComponent; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCapsuleComponent> Capsule;

	/**
	 * Mover 组件类，于 PreInitializeComponents 按类运行时创建。
	 *
	 * 只存类引用而不在蓝图里放组件模板：模板会持有 MovementModes 等内联子对象的独立拷贝，
	 * 组件蓝图 Class Defaults 的后续修改无法传递到模板与各级子蓝图的覆写记录。
	 * 运行时创建的组件以类 CDO 为 archetype，组件蓝图因而是唯一数据源。
	 * 两端在同一阶段以固定名字创建，后端与网络物理子组件的 NetAddressable 路径才能对上。
	 * 为空则不创建，供不使用 Mover 的派生类使用。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Mover")
	TSubclassOf<UMoverComponent> MoverComponentClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UModularAbilityExtensionComponent> AbilityExtensionComponent;

	/** 失去控制器后应归属的队伍，默认脱离队伍。 */
	virtual FGenericTeamId DetermineNewTeamAfterPossessionEnds(FGenericTeamId OldTeamID) const
	{
		return FGenericTeamId::NoTeam;
	}

	/**
	 * 死亡收尾：让出 ASC 的 Avatar 身份、脱离控制器并安排销毁。
	 *
	 * 必须早于重生 Pawn 的 Possess 完成，否则新 Pawn 绑定同一 ASC 时会撞上仍在场的旧 Avatar。
	 */
	UE_API virtual void UninitAndDestroy();

	/** 死亡流程结束时的蓝图回调，在销毁前触发。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Yan|Pawn", meta = (DisplayName = "On Death Finished"))
	UE_API void K2_OnDeathFinished();

private:
	// 在拥有端接管 GameplayCamera 取景；组件由派生类创建，故按类型查找
	void ActivateGameplayCamera();

	UFUNCTION()
	void OnDeathFinished(AActor* OwningActor);

	// 让出 ASC 的 Avatar 身份；已被下一个 Pawn 接管时不做处理，可重复调用
	void ReleaseAbilitySystemAvatar();

	// 延后一帧销毁，让死亡阶段的通知在本帧走完
	void DestroyDueToDeath();

	UFUNCTION()
	void OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	UFUNCTION()
	void OnRep_MyTeamID(FGenericTeamId OldTeamID);

	UPROPERTY()
	FOnModularTeamIndexChangedDelegate OnTeamChangedDelegate;

	UPROPERTY(ReplicatedUsing = OnRep_MyTeamID)
	FGenericTeamId MyTeamID;
};

#undef UE_API
