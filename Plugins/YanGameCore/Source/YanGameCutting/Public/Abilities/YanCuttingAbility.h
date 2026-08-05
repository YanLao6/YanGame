#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "YanCuttingAbility.generated.h"

#define UE_API YANGAMECUTTING_API

class AYanCuttableActor;

USTRUCT(BlueprintType)
struct FYanTargetActorHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> TargetActor;
	
	UPROPERTY(BlueprintReadOnly)
	FHitResult HitInfo;
};

/**
 * Drag-to-cut ability.
 *
 * On press: records the mouse position and traces to find all cuttable actors.
 * On release: calculates the cut normal from the drag vector and cuts all recorded actors.
 *
 * Cut-normal derivation:
 *   DragWorld = DragX * CameraRight + (-DragY) * CameraUp
 *   CutNormal = normalize(DragWorld × CameraForward)
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew)
class UYanCuttingAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UYanCuttingAbility();

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

	/** Maximum line-trace distance to find a cuttable target (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting", meta = (ClampMin = "10"))
	float MaxCutDistance = 800.f;

	/** Minimum drag length in pixels to determine cut direction; below this a horizontal cut is assumed. */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting", meta = (ClampMin = "1"))
	float MinDragPixels = 10.f;

private:
	UE_API void ScanForTargets(const FGameplayAbilityActorInfo* ActorInfo);
	UE_API void ExecuteCut(const FGameplayAbilityActorInfo* ActorInfo);

	UE_API FVector ComputeCutNormal(FVector2D DragScreen, FVector CameraRight, FVector CameraUp, FVector CameraForward) const;

	FVector2D DragStartScreenPos = FVector2D::ZeroVector;
	
	UPROPERTY(Transient)
	TArray<FYanTargetActorHandle> RecordedTargets;
};

#undef UE_API
