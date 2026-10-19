#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "ContextEffects/YanContextEffectsInterface.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPath.h"

#include "AnimNotify_YanContextEffects.generated.h"

#define UE_API YANGAMEEFFECTS_API

USTRUCT(BlueprintType)
struct FYanContextEffectNotifyAudioSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float PitchMultiplier = 1.0f;
};

/** 取命中表面所用的射线检测参数 */
USTRUCT(BlueprintType)
struct FYanContextEffectNotifyTraceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECollisionChannel::ECC_Visibility;

	/** 相对起点的终点偏移，脚步类效果通常向下 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	FVector EndTraceLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	bool bIgnoreActor = true;
};

USTRUCT(BlueprintType)
struct FYanContextEffectNotifyPreviewSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (AllowedClasses = "/Script/YanGameEffects.YanContextEffectsLibrary"))
	FSoftObjectPath PreviewContextEffectsLibrary;

	/** 预览时替代运行时上下文；想试不同地面就填对应的表面标签 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	FGameplayTagContainer PreviewContexts;
};

/**
 * 上下文特效动画通知。
 *
 * 只声明「这一帧发生了什么事件」，不引用任何音效或特效资源；具体播放什么由响应方
 * 结合其上下文查表决定。可选做一次射线检测取得脚下材质的标签，
 * 实现同一段动画在不同地面出不同声音。
 */
UCLASS(MinimalAPI, const, hidecategories = Object, CollapseCategories, Config = Game, meta = (DisplayName = "Play Yan Context Effects"))
class UAnimNotify_YanContextEffects : public UAnimNotify
{
	GENERATED_BODY()

public:
	//~Begin UAnimNotify Interface
	UE_API virtual FString GetNotifyName_Implementation() const override;
	UE_API virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	//~End UAnimNotify Interface

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (DisplayName = "Effect", ExposeOnSpawn = true))
	FGameplayTag Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FVector VFXScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FYanContextEffectNotifyAudioSettings AudioProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttachmentProperties", meta = (ExposeOnSpawn = true))
	bool bAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttachmentProperties", meta = (ExposeOnSpawn = true, EditCondition = "bAttached"))
	FName SocketName;

	/** 表面标签依赖本次检测的结果，不勾选则无法区分地面材质 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	bool bPerformTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true, EditCondition = "bPerformTrace"))
	FYanContextEffectNotifyTraceSettings TraceProperties;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Config, EditAnywhere, Category = "PreviewProperties")
	bool bPreviewInEditor = false;

	UPROPERTY(EditAnywhere, Category = "PreviewProperties", meta = (EditCondition = "bPreviewInEditor"))
	FYanContextEffectNotifyPreviewSettings PreviewProperties;
#endif

private:
#if WITH_EDITOR
	/** 动画编辑器预览：库来自通知自身的预览属性，生成仍走与运行时相同的实现 */
	void PreviewInEditor(const FYanContextEffectRequest& Request) const;
#endif
};

#undef UE_API
