// 版权声明请在「项目设置 → 描述」中填写。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ModularCharacterMovementComponent.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

/**
 * 角色脚下地面信息（按需更新）。
 */
USTRUCT(BlueprintType)
struct FModularCharacterGroundInfo
{
	GENERATED_BODY()

	FModularCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};



/**
 * 带地面探测与 GAS Tag 联动（如 MovementStopped）的 CharacterMovement。
 */
UCLASS(MinimalAPI,Config=Game)
class UModularCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	/** 构造。 */
	UE_API UModularCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void SimulateMovement(float DeltaTime) override;

	UE_API virtual bool CanAttemptJump() const override;

	/**
	 * 获取地面信息；若缓存过期会在此函数内更新。
	 *
	 * @return 缓存的 FModularCharacterGroundInfo 引用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular|CharacterMovement")
	UE_API const FModularCharacterGroundInfo& GetGroundInfo();

	/** 设置复制的加速度向量（用于在 SimulateMovement 中保持 Acceleration）。 */
	UE_API void SetReplicatedAcceleration(const FVector& InAcceleration);

	//~UMovementComponent 接口
	UE_API virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	UE_API virtual float GetMaxSpeed() const override;
	//~UMovementComponent 接口结束

protected:

	UE_API virtual void InitializeComponent() override;

protected:

	// 缓存地面信息；勿直接读，请通过 GetGroundInfo()。
	FModularCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;
};

#undef UE_API
