// Copyright Chronicler.

#pragma once

#include "ModalCameraMode.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "Camera/CameraComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "GameFramework/Actor.h"

#include "ModalCameraComponent.generated.h"

template <class TClass>
class TSubclassOf;

DECLARE_DELEGATE_RetVal(TSubclassOf<UCameraMode>, FCameraModeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCustomCameraDelegate, const TArray<UCameraMode*>&, CustomCameraModes);


/**
 * UModalCameraComponent
 *
 * 支持在多个 CameraMode 之间 Blend 的相机组件。
 *
 * 优先级（高 → 低）：
 * 1. Debug Camera：Cheat Manager 打开时
 * 2. Custom Camera：多播委托 CustomCameraDelegate，可按层 Push
 * 3. Cinematic Camera：组件上设置的覆盖
 * 4. Ability Camera：Gameplay Ability 写入的覆盖（参见 ModularGameplayAbilities）
 * 5. Default Camera：组件默认 CameraMode
 *
 * @todo 改为依赖 ModularGameplayExperiences 接口，避免模板复制粘贴。
 */
UCLASS()
class MODALCAMERA_API UModalCameraComponent : public UCameraComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	/** 构造并初始化 CameraModeStack 相关状态。 */
	UModalCameraComponent(const FObjectInitializer& ObjectInitializer);

	/** 玩家操控 Pawn 时使用的默认 CameraMode Class。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	TSubclassOf<UCameraMode> DefaultCameraMode;

	/** 查找 Actor 上的本组件；不存在则 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Camera")
	static UModalCameraComponent* FindCameraComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UModalCameraComponent>() : nullptr); }

	/** 当前相机跟随的 TargetActor（默认即 Owner）。 */
	virtual AActor* GetTargetActor() const { return GetOwner(); }

	/** 由外部绑定：返回当前帧应使用的 UModalCameraMode 子类（可动态决策）。 */
	FCameraModeDelegate DetermineCameraModeDelegate;

	/** 自定义相机层：广播后写入 CustomCameraModeStack，供 DetermineCameraMode 消费。 */
	FCustomCameraDelegate CustomCameraDelegate;

	/** 对 FOV 叠加单帧偏移，应用后自动清零。 */
	void AddFieldOfViewOffset(const float FovOffset) { FieldOfViewOffset += FovOffset; }

	/** 按上述优先级规则解析当前应采用的 CameraMode Class。 */
	TSubclassOf<UCameraMode> DetermineCameraMode() const;

	/** 由激活的 GameplayAbility 设置 Ability 层相机覆盖。 */
	void SetAbilityCameraMode(TSubclassOf<UModalCameraMode> CameraMode, const FGameplayAbilitySpecHandle& OwningSpecHandle);

	/** 若 SpecHandle 匹配则清除 Ability 相机覆盖。 */
	void ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle);

	/** 设置 Cinematic 层相机覆盖。 */
	void SetCinematicCameraMode(TSubclassOf<UModalCameraMode> CameraMode);

	/** 清除 Cinematic 相机覆盖。 */
	void ClearCinematicCameraMode();

	/** 设置 Debug 层相机覆盖。 */
	void SetDebugCameraMode(TSubclassOf<UModalCameraMode> CameraMode);

	/** 清除 Debug 相机覆盖。 */
	void ClearDebugCameraMode();

	/**
	 * @ingroup UActorComponent
	 */
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 返回拥有本组件的 Pawn。
	 *
	 * 运行时通常有效；编辑器中可能为 nullptr。
	 *
	 * @todo 替换为 ModularGameplayExperiences 接口。
	 *
	 * @ingroup ModularGameplayExperiencesInterface
	 */
	template <class T>
	T* GetPawn() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "'T' template parameter to GetPawn must be derived from APawn");
		return Cast<T>(GetOwner());
	}

	template <class T>
	T* GetPawnChecked() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "'T' template parameter to GetPawnChecked must be derived from APawn");
		return CastChecked<T>(GetOwner());
	}

	/** 返回 PlayerState；客户端上复制未完成的玩家 Pawn 可能为 nullptr。 */
	template <class T>
	T* GetPlayerState() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APlayerState>::Value, "'T' template parameter to GetPlayerState must be derived from APlayerState");
		return GetPawnChecked<APawn>()->GetPlayerState<T>();
	}

	/** 返回 Controller；客户端上常常为 nullptr。 */
	template <class T>
	T* GetController() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AController>::Value, "'T' template parameter to GetController must be derived from AController");
		return GetPawnChecked<APawn>()->GetController<T>();
	}

	/** 在 HUD/Canvas 上绘制调试信息。 */
	virtual void DrawDebug(UCanvas* Canvas) const;

	/** 取得栈底条目的 Blend 权重与 CameraTypeTag（同 CameraModeStack::GetBlendInfo）。 */
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

	/**
	 * @ingroup IGameFrameworkInitStateInterface
	 */
	static const FName NAME_ActorFeatureName;

	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool  CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void  HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void  OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void  CheckDefaultInitialization() override;

protected:
	/** 用于 Blend 各 CameraMode 的栈对象。 */
	UPROPERTY()
	TObjectPtr<UCameraModeStack> CameraModeStack;

	/** 单帧 FOV 偏移，在 GetCameraView 中并入后清零。 */
	float FieldOfViewOffset;

	// 若委托已绑定则 Push 委托返回的 CameraMode
	virtual void UpdateCameraModes();

	/**
	 * @ingroup UCameraComponent
	 */
	/** Ability 层指定的 CameraMode。 */
	UPROPERTY()
	TSubclassOf<UCameraMode> AbilityCameraMode;

	/** 最后设置 Ability 相机的 GameplayAbilitySpecHandle。 */
	FGameplayAbilitySpecHandle AbilityCameraModeOwningSpecHandle;

	/** Cinematic 层 CameraMode Class。 */
	UPROPERTY()
	TSubclassOf<UCameraMode> CinematicCameraMode;

	/** CustomCameraDelegate 广播填充的 CameraMode 栈（用于多层 Push）。 */
	UPROPERTY()
	TArray<TObjectPtr<UCameraMode>> CustomCameraModeStack;

	/** Debug 层 CameraMode Class。 */
	UPROPERTY()
	TSubclassOf<UCameraMode> DebugCameraMode;

	virtual void OnRegister() override;
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;
};
