#pragma once

#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPath.h"

#include "YanContextEffectsLibrary.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UNiagaraSystem;
class USoundBase;
struct FStreamableHandle;

/** 同一事件命中多个条目时的取舍策略 */
UENUM(BlueprintType)
enum class EYanContextEffectsMatchType : uint8
{
	/** 播放全部合格条目 */
	All,
	/** 仅播放限定最具体的一档，即上下文标签数量最多的合格条目 */
	BestMatch
};

/**
 * 一条「事件 + 上下文 → 资源」的映射。
 */
USTRUCT(BlueprintType)
struct FYanContextEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContextEffects")
	FGameplayTag EffectTag;

	/** 生效所需的上下文；留空表示通配，任何上下文下都命中 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContextEffects")
	FGameplayTagContainer Context;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContextEffects", meta = (AllowedClasses = "/Script/Engine.SoundBase, /Script/Niagara.NiagaraSystem"))
	TArray<FSoftObjectPath> Effects;
};

/** 加载完成后的条目，持有资源硬引用 */
USTRUCT()
struct FYanLoadedContextEffect
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "ContextEffects")
	FGameplayTag EffectTag;

	UPROPERTY(VisibleAnywhere, Category = "ContextEffects")
	FGameplayTagContainer Context;

	UPROPERTY(VisibleAnywhere, Category = "ContextEffects")
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(VisibleAnywhere, Category = "ContextEffects")
	TArray<TObjectPtr<UNiagaraSystem>> NiagaraSystems;
};

/**
 * 上下文特效库资产。
 *
 * 纯数据查表：把事件标签与一组上下文标签翻译成待播放的音效与 Niagara 资源。
 * 资源以软引用保存，首次使用时异步加载；加载完成前的查询返回空。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanContextEffectsLibrary : public UObject
{
	GENERATED_BODY()

public:
	/** 命中多条时的取舍策略 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContextEffects")
	EYanContextEffectsMatchType MatchType = EYanContextEffectsMatchType::All;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContextEffects")
	TArray<FYanContextEffectEntry> ContextEffects;

	/**
	 * 收集事件标签精确匹配、且上下文合格的资源。
	 * 条目上下文为空视为通配；否则要求它是运行时上下文的子集。库未加载完成时不产出任何资源。
	 */
	UE_API void GatherEffects(const FGameplayTag EffectTag, const FGameplayTagContainer& Contexts,
		TArray<USoundBase*>& OutSounds, TArray<UNiagaraSystem*>& OutNiagaraSystems) const;

	/** 发起异步加载；已在加载中或已完成时无副作用 */
	UE_API void RequestLoad();

	bool IsLoaded() const
	{
		return bLoaded;
	}

#if WITH_EDITOR
	/** 动画编辑器预览专用：阻塞加载，避免预览时首次播放静默 */
	UE_API void LoadSynchronousForPreview();
#endif

private:
	/** 收集全部待加载软引用；无可加载资源时返回空数组 */
	TArray<FSoftObjectPath> CollectEffectPaths() const;

	/** 把已解析的资源按类型分桶写入 LoadedEffects */
	void ResolveLoadedEffects();

	UPROPERTY(Transient)
	TArray<FYanLoadedContextEffect> LoadedEffects;

	UPROPERTY(Transient)
	bool bLoaded = false;

	/** 持有期间维持资源存活，同时充当「加载在飞行中」的标记 */
	TSharedPtr<FStreamableHandle> LoadHandle;
};

#undef UE_API
