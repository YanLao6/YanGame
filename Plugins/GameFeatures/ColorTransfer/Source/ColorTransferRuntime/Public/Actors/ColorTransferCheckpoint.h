#pragma once

#include "Player/ModularPlayerStart.h"
#include "GameplayAbilities/ModularAbilitySet.h"
#include "UIExtensionSystem.h"
#include "TimerManager.h"

#include "ColorTransferCheckpoint.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UBoxComponent;
class UPrimitiveComponent;
class UModularAbilitySystemComponent;
class ULocalPlayer;
class UUserWidget;
class APlayerController;
struct FHitResult;

USTRUCT()
struct FColorTransferCheckpointAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UModularAbilitySet> AbilitySet;

	UPROPERTY()
	FModularAbilitySet_GrantedHandles GrantedHandles;
};

/** 玩家进入触发范围后，将此点设为自己的复活点。 */
UCLASS(MinimalAPI, Blueprintable)
class AColorTransferCheckpoint : public AModularPlayerStart
{
	GENERATED_BODY()

public:
	UE_API AColorTransferCheckpoint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** 本地玩家进入范围后，在 HUD.Slot.Tutorial 显示 10 秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yan|ColorTransfer|Tutorial")
	TSubclassOf<UUserWidget> TutorialWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	TMap<int32, TObjectPtr<UModularAbilitySet>> TeamAbilitySets;

	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

private:
	struct FTutorialWidgetState
	{
		FUIExtensionHandle ExtensionHandle;
		FTimerHandle RemovalTimer;
	};

	TMap<TWeakObjectPtr<ULocalPlayer>, FTutorialWidgetState> TutorialWidgets;
	void ShowTutorialWidget(APlayerController* PlayerController);
	void RemoveTutorialWidget(TWeakObjectPtr<ULocalPlayer> LocalPlayer);

	// 仅回收此检查点的授予，不影响其他来源的技能。
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<UModularAbilitySystemComponent>, FColorTransferCheckpointAbilityGrant> AbilityGrants;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UBoxComponent> ActivationVolume;

	UFUNCTION()
	void HandleActivationVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	                                        AActor*              OtherActor,
	                                        UPrimitiveComponent* OtherComp,
	                                        int32                OtherBodyIndex,
	                                        bool                 bFromSweep,
	                                        const FHitResult&    SweepResult);
};

#undef UE_API
