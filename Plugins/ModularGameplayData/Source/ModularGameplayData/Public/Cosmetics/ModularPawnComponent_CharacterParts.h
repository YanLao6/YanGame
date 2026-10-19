// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PawnComponent.h"
#include "Cosmetics/ModularCharacterPartTypes.h"
#include "Cosmetics/ModularCosmeticAnimationTypes.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "ModularPawnComponent_CharacterParts.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class UModularPawnComponent_CharacterParts;

namespace EEndPlayReason { enum Type : int; }

struct FGameplayTag;
struct FModularCharacterPartList;

class UChildActorComponent;
class USceneComponent;
class USkeletalMeshComponent;
struct FNetDeltaSerializeInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularSpawnedCharacterPartsChanged, UModularPawnComponent_CharacterParts*, ComponentWithChangedParts);

/** 一条已应用的部件记录。 */
USTRUCT()
struct FModularAppliedCharacterPartEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FModularAppliedCharacterPartEntry() {}

	FString GetDebugString() const;

private:
	friend FModularCharacterPartList;
	friend UModularPawnComponent_CharacterParts;

	UPROPERTY()
	FModularCharacterPart Part;

	// 返回给调用方的句柄，仅服务端有效
	UPROPERTY(NotReplicated)
	int32 PartHandle = INDEX_NONE;

	// 实际生成的部件组件，专用服务器上不生成
	UPROPERTY(NotReplicated)
	TObjectPtr<UChildActorComponent> SpawnedComponent = nullptr;
};

/** 已应用部件的复制列表。 */
USTRUCT(BlueprintType)
struct FModularCharacterPartList : public FFastArraySerializer
{
	GENERATED_BODY()

	FModularCharacterPartList()
		: OwnerComponent(nullptr)
	{}

	//~Begin FFastArraySerializer Interface
	UE_API void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	UE_API void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	UE_API void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End FFastArraySerializer Interface

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FModularAppliedCharacterPartEntry, FModularCharacterPartList>(Entries, DeltaParms, *this);
	}

	UE_API FModularCharacterPartHandle AddEntry(FModularCharacterPart NewPart);
	UE_API void RemoveEntry(FModularCharacterPartHandle Handle);
	UE_API void ClearAllEntries(bool bBroadcastChangeDelegate);

	/** 汇总所有部件 Actor 通过 IGameplayTagAssetInterface 暴露的标签。 */
	UE_API FGameplayTagContainer CollectCombinedTags() const;

	void SetOwnerComponent(UModularPawnComponent_CharacterParts* InOwnerComponent) { OwnerComponent = InOwnerComponent; }

private:
	friend UModularPawnComponent_CharacterParts;

	bool SpawnActorForEntry(FModularAppliedCharacterPartEntry& Entry);
	bool DestroyActorForEntry(FModularAppliedCharacterPartEntry& Entry);

	UPROPERTY()
	TArray<FModularAppliedCharacterPartEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UModularPawnComponent_CharacterParts> OwnerComponent;

	// 句柄自增计数器
	int32 PartHandleCounter = 0;
};

template <>
struct TStructOpsTypeTraits<FModularCharacterPartList> : public TStructOpsTypeTraitsBase2<FModularCharacterPartList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * 在所有客户端上生成挂载于 Pawn 的外观部件。
 * 部件列表由服务端权威修改并复制，客户端据此增删实际 Actor；
 * 身体网格随部件标签变化自动切换，使配件与体型规则解耦。
 */
UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent))
class UModularPawnComponent_CharacterParts : public UPawnComponent
{
	GENERATED_BODY()

public:
	UE_API UModularPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void OnRegister() override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

	/** 添加部件，仅可在服务端调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Cosmetics)
	UE_API FModularCharacterPartHandle AddCharacterPart(const FModularCharacterPart& NewPart);

	/** 按句柄移除先前添加的部件，仅可在服务端调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Cosmetics)
	UE_API void RemoveCharacterPart(FModularCharacterPartHandle Handle);

	/** 移除全部部件，仅可在服务端调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Cosmetics)
	UE_API void RemoveAllCharacterParts();

	/** 取当前已生成的部件 Actor。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, BlueprintCosmetic, Category = Cosmetics)
	UE_API TArray<AActor*> GetCharacterPartActors() const;

	/**
	 * 部件所依附的骨骼网格：属主为 ACharacter 时取其 Mesh，否则取属主上的首个骨骼网格组件。
	 *
	 * 属主不必是 ACharacter。持有多具网格的属主须把主网格声明在前——
	 * 附加网格（例如真第一人称专用的那具）应排在其后，否则部件会挂到附加网格上。
	 */
	UE_API USkeletalMeshComponent* GetParentMeshComponent() const;

	/** 取部件的挂载目标：优先骨骼网格，其次属主根组件。 */
	UE_API USceneComponent* GetSceneComponentToAttachTo() const;

	/** 取所有部件标签的并集，可按前缀过滤。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, BlueprintCosmetic, Category = Cosmetics)
	UE_API FGameplayTagContainer GetCombinedTags(FGameplayTag RequiredPrefix) const;

	/** 部件增删后重算身体网格并广播。 */
	UE_API void BroadcastChanged();

	/** 部件列表变化时广播，供队伍配色等表现逻辑订阅。 */
	UPROPERTY(BlueprintAssignable, Category = Cosmetics, BlueprintCallable)
	FModularSpawnedCharacterPartsChanged OnCharacterPartsChanged;

private:
	UPROPERTY(Replicated, Transient)
	FModularCharacterPartList CharacterPartList;

	// 依据部件标签挑选身体网格的规则
	UPROPERTY(EditAnywhere, Category = Cosmetics)
	FModularAnimBodyStyleSelectionSet BodyMeshes;
};

#undef UE_API
