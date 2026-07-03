#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "YanCuttingAbility.generated.h"

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
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class YANGAMECUTTING_API UYanCuttingAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UYanCuttingAbility();

	//~Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

	/** Maximum line-trace distance to find a cuttable target (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting", meta = (ClampMin = "10"))
	float MaxCutDistance = 800.f;

	/** Minimum drag length in pixels to determine cut direction; below this a horizontal cut is assumed. */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting", meta = (ClampMin = "1"))
	float MinDragPixels = 10.f;

private:
	void ScanForTargets(const FGameplayAbilityActorInfo* ActorInfo);
	void ExecuteCut(const FGameplayAbilityActorInfo* ActorInfo);

	FVector ComputeCutNormal(FVector2D DragScreen, FVector CameraRight, FVector CameraUp, FVector CameraForward) const;

	FVector2D DragStartScreenPos = FVector2D::ZeroVector;
	
	UPROPERTY(Transient)
	TArray<FYanTargetActorHandle> RecordedTargets;
};
