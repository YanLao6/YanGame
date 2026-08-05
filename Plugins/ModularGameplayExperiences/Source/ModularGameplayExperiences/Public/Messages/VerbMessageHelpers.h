// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VerbMessageHelpers.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

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
UCLASS(MinimalAPI)
class UVerbMessageHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** 从 PlayerController / PlayerState / Pawn 解析 APlayerState。 */
	UFUNCTION(BlueprintCallable)
	static UE_API APlayerState* GetPlayerStateFromObject(UObject* Object);

	/** 从 PlayerController / PlayerState / Pawn 解析 APlayerController。 */
	UFUNCTION(BlueprintCallable)
	static UE_API APlayerController* GetPlayerControllerFromObject(UObject* Object);

	/** 从 PlayerController / PlayerState / Pawn 解析 APawn。 */
	UFUNCTION(BlueprintCallable)
	static UE_API APawn* GetPlayerPawnFromObject(UObject* Object);

	/** 从 PlayerController / PlayerState / Pawn / Actor 解析其 UAbilitySystemComponent。 */
	UFUNCTION(BlueprintCallable)
	static UE_API UAbilitySystemComponent* GetAbilitySystemComponentFromObject(UObject* Object);
	
	/** VerbMessage -> FGameplayCueParameters（部分字段映射）。 */
	UFUNCTION(BlueprintCallable)
	static UE_API FGameplayCueParameters VerbMessageToCueParameters(const FModularVerbMessage& Message);

	/** FGameplayCueParameters -> VerbMessage（部分字段映射）。 */
	UFUNCTION(BlueprintCallable)
	static UE_API FModularVerbMessage CueParametersToVerbMessage(const FGameplayCueParameters& Params);
};

#undef UE_API
