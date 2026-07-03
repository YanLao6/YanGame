// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VerbMessageHelpers.generated.h"

struct FGameplayCueParameters;
struct FModularVerbMessage;

class APlayerController;
class APlayerState;
class APawn;
class UObject;
class UAbilitySystemComponent;
struct FFrame;

/**
 * FModularVerbMessage 与 GameplayCueParameters 等类型的 Blueprint 转换辅助。
 */
UCLASS()
class MODULARGAMEPLAYEXPERIENCES_API UVerbMessageHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** 从 PlayerController / PlayerState / Pawn 解析 APlayerState。 */
	UFUNCTION(BlueprintCallable)
	static APlayerState* GetPlayerStateFromObject(UObject* Object);

	/** 从 PlayerController / PlayerState / Pawn 解析 APlayerController。 */
	UFUNCTION(BlueprintCallable)
	static APlayerController* GetPlayerControllerFromObject(UObject* Object);

	/** 从 PlayerController / PlayerState / Pawn 解析 APawn。 */
	UFUNCTION(BlueprintCallable)
	static APawn* GetPlayerPawnFromObject(UObject* Object);

	/** 从 PlayerController / PlayerState / Pawn / Actor 解析其 UAbilitySystemComponent。 */
	UFUNCTION(BlueprintCallable)
	static UAbilitySystemComponent* GetAbilitySystemComponentFromObject(UObject* Object);
	
	/** VerbMessage -> FGameplayCueParameters（部分字段映射）。 */
	UFUNCTION(BlueprintCallable)
	static FGameplayCueParameters VerbMessageToCueParameters(const FModularVerbMessage& Message);

	/** FGameplayCueParameters -> VerbMessage（部分字段映射）。 */
	UFUNCTION(BlueprintCallable)
	static FModularVerbMessage CueParametersToVerbMessage(const FGameplayCueParameters& Params);
};
