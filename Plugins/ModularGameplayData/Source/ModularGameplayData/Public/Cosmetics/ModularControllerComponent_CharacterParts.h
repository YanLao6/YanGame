// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "Cosmetics/ModularCharacterPartTypes.h"

#include "ModularControllerComponent_CharacterParts.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class APawn;
class UModularPawnComponent_CharacterParts;

/** 部件的来源，决定它在作弊屏蔽时如何处理。 */
enum class EModularCharacterPartSource : uint8
{
	// 正常游戏逻辑下发的部件
	Natural,

	// 因作弊屏蔽而暂时移除的正常部件，作弊关闭后会恢复
	NaturalSuppressedViaCheat,

	// 由编辑器外观调试设置下发
	AppliedViaDeveloperSettingsCheat,

	// 由控制台作弊指令下发
	AppliedViaCheatManager
};

/** 控制器上登记的一条部件请求。 */
USTRUCT()
struct FModularControllerCharacterPartEntry
{
	GENERATED_BODY()

	FModularControllerCharacterPartEntry() {}

	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties))
	FModularCharacterPart Part;

	// 已下发到 Pawn 时持有的句柄
	FModularCharacterPartHandle Handle;

	EModularCharacterPartSource Source = EModularCharacterPartSource::Natural;
};

/**
 * 记录该控制器希望其 Pawn 具备的外观部件。
 * 部件登记在控制器上而非 Pawn 上，因此换 Pawn（重生、切换角色）后外观会自动跟随。
 */
UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent))
class UModularControllerComponent_CharacterParts : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API UModularControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

	/** 添加部件，仅可在服务端调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Cosmetics)
	UE_API void AddCharacterPart(const FModularCharacterPart& NewPart);

	/** 移除等价的部件，仅可在服务端调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Cosmetics)
	UE_API void RemoveCharacterPart(const FModularCharacterPart& PartToRemove);

	/** 移除全部部件，仅可在服务端调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Cosmetics)
	UE_API void RemoveAllCharacterParts();

	/** 重新应用编辑器外观调试设置，仅在带 CheatManager 的构建中有效。 */
	UE_API void ApplyDeveloperSettings();

protected:
	UPROPERTY(EditAnywhere, Category = Cosmetics)
	TArray<FModularControllerCharacterPartEntry> CharacterParts;

private:
	UModularPawnComponent_CharacterParts* GetPawnCustomizer() const;

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void AddCharacterPartInternal(const FModularCharacterPart& NewPart, EModularCharacterPartSource Source);

	void AddCheatPart(const FModularCharacterPart& NewPart, bool bSuppressNaturalParts);
	void ClearCheatParts();

	void SetSuppressionOnNaturalParts(bool bSuppressed);

	friend class UModularCosmeticCheats;
};

#undef UE_API
