#pragma once

#include "Containers/ArrayView.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"

#include "YanContextEffectsInterface.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UPhysicalMaterial;
class USceneComponent;
class UYanContextEffectsLibrary;

/**
 * 一次上下文特效播放请求。
 *
 * 调用方（动画通知、GameplayCue 等）只声明「发生了什么」与「在哪里发生」，
 * 具体播放哪些音效与 Niagara 由响应方结合自身上下文查表决定。
 */
USTRUCT(BlueprintType)
struct FYanContextEffectRequest
{
	GENERATED_BODY()

	/** 事件标签，查表主键 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextEffects")
	FGameplayTag EffectTag;

	/**
	 * 特效附着的组件。留空表示调用方不掌握目标的身体布局，由响应方决定挂在哪里。
	 * 动画通知天然知道（触发通知的骨骼网格体），GameplayCue 通常留空。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	TObjectPtr<USceneComponent> AttachToComponent = nullptr;

	/** 附着插槽；NAME_None 表示附着到组件原点 */
	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	FName AttachPoint;

	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/**
	 * 调用方附加的上下文，与响应方自身上下文合并后参与匹配。
	 * 命中表面的语义由调用方译成标签后并入此处，契约本身不承载引擎物理类型。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	FGameplayTagContainer Contexts;

	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	FVector VFXScale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	float AudioVolume = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "ContextEffects")
	float AudioPitch = 1.0f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UYanContextEffectsInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 上下文特效的响应契约。
 *
 * 事件发起方不持有响应方引用：它遍历目标 Actor 及其组件，凡实现本接口者都会收到请求。
 * 这让同一段动画能复用在响应方式完全不同的 Actor 上，而动画资产无需改动。
 */
class IYanContextEffectsInterface : public IInterface
{
	GENERATED_BODY()

public:
	/** 响应一次上下文特效请求 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ContextEffects")
	UE_API void PlayContextEffect(const FYanContextEffectRequest& Request);
};

/**
 * 上下文特效的发起入口与共享生成逻辑。
 *
 * 这些函数不依赖任何世界状态，因此动画编辑器的预览世界也能直接调用——
 * 预览与运行时共用同一份生成实现，避免「编辑器里对、进游戏不对」。
 */
UCLASS(MinimalAPI)
class UYanContextEffectsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 把请求派发给目标 Actor 上所有实现了上下文特效接口的对象（Actor 本身与其全部组件）。
	 * 专用服务器上直接返回：上下文特效是纯表现层，不参与复制。
	 */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	static UE_API void DispatchContextEffect(AActor* Target, const FYanContextEffectRequest& Request);

	/**
	 * 把物理材质自带的标签并入上下文。
	 * 表面语义的唯一来源是材质资产上的标签，与伤害等系统共用同一份真值。
	 */
	static UE_API void AppendSurfaceContexts(const UPhysicalMaterial* PhysicalMaterial, FGameplayTagContainer& OutContexts);

	/** 在给定的特效库集合中查表并就地生成音效与 Niagara */
	static UE_API void PlayEffectsFromLibraries(const FYanContextEffectRequest& Request,
		const FGameplayTagContainer& ResolvedContexts,
		TArrayView<const TObjectPtr<UYanContextEffectsLibrary>> Libraries);
};

#undef UE_API
