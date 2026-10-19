#include "ContextEffects/YanContextEffectsLibrary.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "YanGameEffects.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanContextEffectsLibrary)

namespace YanContextEffects
{
	/**
	 * 条目上下文是否被运行时上下文满足。
	 * 空上下文是通配；非空时要求条目的每个标签都精确存在于运行时上下文中。
	 */
	static bool IsContextQualified(const FGameplayTagContainer& EntryContext, const FGameplayTagContainer& RuntimeContexts)
	{
		return EntryContext.IsEmpty() || RuntimeContexts.HasAllExact(EntryContext);
	}
}

void UYanContextEffectsLibrary::GatherEffects(const FGameplayTag EffectTag, const FGameplayTagContainer& Contexts,
	TArray<USoundBase*>& OutSounds, TArray<UNiagaraSystem*>& OutNiagaraSystems) const
{
	if (!EffectTag.IsValid() || !bLoaded)
	{
		return;
	}

	// 一趟筛出合格条目并记下最高特化度；BestMatch 只是在输出时多一道整数比较，
	// 不重复任何标签匹配
	TArray<const FYanLoadedContextEffect*, TInlineAllocator<8>> Matches;
	int32 BestSpecificity = 0;

	for (const FYanLoadedContextEffect& Loaded : LoadedEffects)
	{
		if (!EffectTag.MatchesTagExact(Loaded.EffectTag) || !YanContextEffects::IsContextQualified(Loaded.Context, Contexts))
		{
			continue;
		}

		BestSpecificity = FMath::Max(BestSpecificity, Loaded.Context.Num());
		Matches.Add(&Loaded);
	}

	const bool bBestOnly = MatchType == EYanContextEffectsMatchType::BestMatch;
	for (const FYanLoadedContextEffect* Match : Matches)
	{
		if (bBestOnly && Match->Context.Num() != BestSpecificity)
		{
			continue;
		}

		OutSounds.Append(Match->Sounds);
		OutNiagaraSystems.Append(Match->NiagaraSystems);
	}
}

TArray<FSoftObjectPath> UYanContextEffectsLibrary::CollectEffectPaths() const
{
	TArray<FSoftObjectPath> Paths;
	for (const FYanContextEffectEntry& Entry : ContextEffects)
	{
		if (!Entry.EffectTag.IsValid())
		{
			continue;
		}

		for (const FSoftObjectPath& Path : Entry.Effects)
		{
			if (!Path.IsNull())
			{
				Paths.AddUnique(Path);
			}
		}
	}

	return Paths;
}

void UYanContextEffectsLibrary::RequestLoad()
{
	if (bLoaded || LoadHandle.IsValid())
	{
		return;
	}

	const TArray<FSoftObjectPath> Paths = CollectEffectPaths();
	if (Paths.IsEmpty())
	{
		bLoaded = true;
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	LoadHandle = Streamable.RequestAsyncLoad(Paths, FStreamableDelegate::CreateWeakLambda(this, [this]()
	{
		ResolveLoadedEffects();
	}));
}

#if WITH_EDITOR
void UYanContextEffectsLibrary::LoadSynchronousForPreview()
{
	if (bLoaded)
	{
		return;
	}

	const TArray<FSoftObjectPath> Paths = CollectEffectPaths();
	if (!Paths.IsEmpty())
	{
		LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(Paths);
	}

	ResolveLoadedEffects();
}
#endif

void UYanContextEffectsLibrary::ResolveLoadedEffects()
{
	LoadedEffects.Reset(ContextEffects.Num());

	for (const FYanContextEffectEntry& Entry : ContextEffects)
	{
		if (!Entry.EffectTag.IsValid())
		{
			continue;
		}

		FYanLoadedContextEffect Loaded;
		Loaded.EffectTag = Entry.EffectTag;
		Loaded.Context = Entry.Context;

		for (const FSoftObjectPath& Path : Entry.Effects)
		{
			UObject* Object = Path.ResolveObject();
			if (!Object)
			{
				continue;
			}

			if (USoundBase* Sound = Cast<USoundBase>(Object))
			{
				Loaded.Sounds.Add(Sound);
			}
			else if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(Object))
			{
				Loaded.NiagaraSystems.Add(NiagaraSystem);
			}
		}

		LoadedEffects.Add(MoveTemp(Loaded));
	}

	bLoaded = true;

	UE_LOG(LogYanContextEffects, Verbose, TEXT("特效库 %s 加载完成，共 %d 条条目"), *GetName(), LoadedEffects.Num());
}
