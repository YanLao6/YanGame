// Copyright Chronicler.

#pragma once

#include "GameFramework/PlayerStart.h"
#include "GameplayTagContainer.h"

#include "ModularPlayerStart.generated.h"

enum class EModularPlayerStartLocationOccupancy
{
	Empty,
	Partial,
	Full
};

/**
 * 模块化 PlayerStart：支持 GameplayTag 标记、占用检测与 Claim 计时释放。
 */
UCLASS(Config = Game)
class MODULARGAMEPLAYEXPERIENCES_API AModularPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	/** 构造。 */
	AModularPlayerStart(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 返回本出生点关联的 GameplayTag。 */
	const FGameplayTagContainer& GetGameplayTags() { return StartPointTags; }

	/** 按默认 Pawn 体积检测该点 Empty / Partial / Full。 */
	EModularPlayerStartLocationOccupancy GetLocationOccupancy(AController* const ControllerPawnToFit) const;

	/** 是否已被某 Controller Claim。 */
	bool IsClaimed() const;

	/** 若尚未 Claim，则为 OccupyingController 占用本点。 */
	bool TryClaim(AController* OccupyingController);

protected:
	/** 定时检测占用是否已空，以解除 Claim。 */
	void CheckUnclaimed();

	/** 当前占用的 Controller。 */
	UPROPERTY(Transient)
	TObjectPtr<AController> ClaimingController = nullptr;

	/** Claim 后周期性检测占用是否释放的间隔（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Player Start Claiming")
	float ExpirationCheckInterval = 1.f;

	/** 标识本 PlayerStart 的 GameplayTag（分队/模式筛选等）。 */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer StartPointTags;

	/** ExpirationCheck 用的循环 Timer。 */
	FTimerHandle ExpirationTimerHandle;
};
