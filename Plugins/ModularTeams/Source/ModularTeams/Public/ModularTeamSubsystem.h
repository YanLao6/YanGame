// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "ModularTeamSubsystem.generated.h"

#define UE_API MODULARTEAMS_API

class AActor;
class APlayerState;
class AModularTeamInfoBase;
class AModularTeamPrivateInfo;
class AModularTeamPublicInfo;
class FSubsystemCollectionBase;
class UModularTeamDisplayAsset;
struct FGameplayTag;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModularTeamDisplayAssetChangedDelegate, const UModularTeamDisplayAsset*, DisplayAsset);

/** 单支队伍在子系统中的登记项。 */
USTRUCT()
struct FModularTeamTrackingInfo
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<AModularTeamPublicInfo> PublicInfo = nullptr;

	UPROPERTY()
	TObjectPtr<AModularTeamPrivateInfo> PrivateInfo = nullptr;

	UPROPERTY()
	TObjectPtr<UModularTeamDisplayAsset> DisplayAsset = nullptr;

	UPROPERTY()
	FOnModularTeamDisplayAssetChangedDelegate OnTeamDisplayAssetChanged;

public:
	void SetTeamInfo(AModularTeamInfoBase* Info);
	void RemoveTeamInfo(AModularTeamInfoBase* Info);
};

/** 两个对象的队伍关系。 */
UENUM(BlueprintType)
enum class EModularTeamComparison : uint8
{
	/** 同队。 */
	OnSameTeam,

	/** 敌对队伍。 */
	DifferentTeams,

	/** 至少一方无效或不属于任何队伍。 */
	InvalidArgument
};

/**
 * 队伍查询子系统。
 *
 * 汇总当前世界中已注册的队伍信息，为 Pawn、Controller、PlayerState 等对象提供队伍归属查询、
 * 敌我判定与队伍展示资产访问。
 */
UCLASS(MinimalAPI)
class UModularTeamSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UE_API UModularTeamSubsystem();

	//~Begin USubsystem Interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End USubsystem Interface

	/** 注册队伍信息 Actor；队伍编号无效时返回 false。 */
	UE_API bool RegisterTeamInfo(AModularTeamInfoBase* TeamInfo);

	/** 注销队伍信息 Actor；未曾注册时返回 false。 */
	UE_API bool UnregisterTeamInfo(AModularTeamInfoBase* TeamInfo);

	/** 变更 Actor 的队伍归属，仅可在 Authority 上调用。 */
	UE_API bool ChangeTeamForActor(AActor* ActorToChange, int32 NewTeamId);

	/** 返回对象所属队伍编号，不属于任何队伍时返回 INDEX_NONE。 */
	UE_API int32 FindTeamFromObject(const UObject* TestObject) const;

	/**
	 * 返回与该 Actor 关联、且实现了队伍代理接口的 PlayerState。
	 *
	 * 不做具体 PlayerState 类型假设：队伍归属仅通过 IModularTeamAgentInterface 表达，
	 * 因此本插件不需要认识任何项目侧的 PlayerState 派生类。
	 */
	UE_API const APlayerState* FindPlayerStateFromActor(const AActor* PossibleTeamActor) const;

	/** 蓝图版队伍查询。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Teams, meta = (Keywords = "Get"))
	UE_API void FindTeamFromActor(const UObject* TestActor, bool& bIsPartOfTeam, int32& TeamId) const;

	/** 比较两个对象的队伍关系并输出各自队伍编号。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Teams, meta = (ExpandEnumAsExecs = ReturnValue))
	UE_API EModularTeamComparison CompareTeams(const UObject* A, const UObject* B, int32& TeamIdA, int32& TeamIdB) const;

	UE_API EModularTeamComparison CompareTeams(const UObject* A, const UObject* B) const;

	/** 结合友军伤害规则判断施加者能否伤害目标。 */
	UE_API bool CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowDamageToSelf = true) const;

	/** 为队伍的 Tag 增加层数（StackCount 小于 1 时无效）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Teams)
	UE_API void AddTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount);

	/** 为队伍的 Tag 减少层数（StackCount 小于 1 时无效）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Teams)
	UE_API void RemoveTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount);

	/** 返回队伍的 Tag 层数（公开与私有信息之和，不存在则为 0）。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API int32 GetTeamTagStackCount(int32 TeamId, FGameplayTag Tag) const;

	/** 队伍是否持有至少一层指定 Tag。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API bool TeamHasTag(int32 TeamId, FGameplayTag Tag) const;

	/** 指定队伍是否存在。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API bool DoesTeamExist(int32 TeamId) const;

	/**
	 * 取得队伍的展示资产。
	 *
	 * 需要一并指定观察者队伍，以支持“本地玩家恒为蓝方”这类相对配色规则。
	 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API UModularTeamDisplayAsset* GetTeamDisplayAsset(int32 TeamId, int32 ViewerTeamId);

	/** 以某个队伍代理对象作为观察者，取得队伍展示资产。 */
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API UModularTeamDisplayAsset* GetEffectiveTeamDisplayAsset(int32 TeamId, UObject* ViewerTeamAgent);

	/** 返回全部队伍编号，按升序排列。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Teams)
	UE_API TArray<int32> GetTeamIDs() const;

	/** 队伍展示资产被编辑时调用，通知所有配色观察者刷新。 */
	UE_API void NotifyTeamDisplayAssetModified(UModularTeamDisplayAsset* ModifiedAsset);

	/** 取得指定队伍的展示资产变更委托。 */
	UE_API FOnModularTeamDisplayAssetChangedDelegate& GetTeamDisplayAssetChangedDelegate(int32 TeamId);

private:
	UPROPERTY()
	TMap<int32, FModularTeamTrackingInfo> TeamMap;

	FDelegateHandle CheatManagerRegistrationHandle;
};

#undef UE_API
