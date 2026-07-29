/**
 * FYanBurntTechnique — 一条熔断记录，保存重建该技能所需的全部信息。
 *
 * ClearAbility 之后 Spec 即不复存在，故在移除前对其快照。
 * InputTags 来自原 Spec 的 DynamicAbilityTags，
 * UModularAbilitySystemComponent 靠它反查技能来路由输入，
 * 修复时必须原样写回，否则按键将失去响应。
 */
struct FYanBurntTechnique
{
	UPROPERTY()
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY()
	FGameplayTag TechniqueTag;

	UPROPERTY()
	FGameplayTagContainer InputTags;

	UPROPERTY()
	int32 Level = 1;
}

/**
 * UYanTechniqueComponent — 术式熔断状态的权威来源。
 *
 * 术式不另立清单：可用术式直接遍历 ASC 得到，
 * 熔断记录在移除技能的那一刻从 Spec 快照而来，修复时按快照重新授予。
 *
 * "修复熔断的术式"是一条受同一套机制管理的普通术式：
 * 它在 ASC 中即代表持有修复机会，被熔断即代表机会已用掉。
 * 因此它始终留在系统内，无需为它保留技能类引用，仅靠 RestoreAbilityTag 识别。
 */
class UYanTechniqueComponent : UActorComponent
{
	default bReplicates = true;

	/** "修复熔断的术式"的标识，用于把它与可施放术式区分开 */
	UPROPERTY(EditAnywhere, Category = "Technique", Meta = (Categories = "Ability.CursedTechnique"))
	FGameplayTag RestoreAbilityTag;

	/** 已熔断的术式快照 */
	UPROPERTY(Replicated)
	TArray<FYanBurntTechnique> BurntTechniques;

	// ASC 依赖 PlayerState 复制，BeginPlay 时未必就绪，故惰性获取
	private UAbilitySystemComponent CachedAbilitySystem;

	// 等待下一帧执行的熔断范围
	private FGameplayTagContainer PendingBurnoutTags;

	UFUNCTION(BlueprintPure, Category = "Technique")
	UAbilitySystemComponent GetAbilitySystem()
	{
		if (CachedAbilitySystem == nullptr)
		{
			CachedAbilitySystem = AbilitySystem::GetAbilitySystemComponent(GetOwner());
		}
		return CachedAbilitySystem;
	}

	/**
	 * 收集当前可施放的术式。修复凭证不是可施放术式，故排除在外。
	 * 句柄与名称按下标一一对应。
	 */
	UFUNCTION(BlueprintCallable, Category = "Technique")
	void CollectAvailableTechniques(TArray<FGameplayAbilitySpecHandle>& OutHandles, TArray<FText>& OutNames)
	{
		UAbilitySystemComponent ASC = GetAbilitySystem();
		if (ASC == nullptr)
		{
			return;
		}

		TArray<FGameplayAbilitySpecHandle> AllHandles;
		ASC.GetAllAbilities(AllHandles);

		for (FGameplayAbilitySpecHandle Handle : AllHandles)
		{
			FGameplayAbilitySpec Spec;
			if (!ASC.FindAbilitySpecFromHandle(Handle, Spec))
			{
				continue;
			}

			UYanTechniqueAbility TechniqueAbility = Cast<UYanTechniqueAbility>(Spec.Ability);
			if (TechniqueAbility == nullptr || TechniqueAbility.TechniqueTag.MatchesTag(RestoreAbilityTag))
			{
				continue;
			}

			OutHandles.Add(Handle);
			OutNames.Add(TechniqueAbility.DisplayName);
		}
	}

	/**
	 * 收集可修复的术式。修复凭证自身虽然也会进入熔断记录，但不可被修复，故排除。
	 * OutBurntIndices 用于回传给 ServerCommitRestore。
	 */
	UFUNCTION(BlueprintCallable, Category = "Technique")
	void CollectRestorableTechniques(TArray<int32>& OutBurntIndices, TArray<FText>& OutNames) const
	{
		for (int32 Index = 0; Index < BurntTechniques.Num(); ++Index)
		{
			if (BurntTechniques[Index].TechniqueTag.MatchesTag(RestoreAbilityTag))
			{
				continue;
			}

			UYanTechniqueAbility TechniqueCDO = Cast<UYanTechniqueAbility>(
				BurntTechniques[Index].Ability.Get().GetDefaultObject());

			OutBurntIndices.Add(Index);
			OutNames.Add(TechniqueCDO != nullptr ? TechniqueCDO.DisplayName : FText());
		}
	}

	/** 是否持有一次可用的修复机会，即修复凭证当前在 ASC 中。 */
	UFUNCTION(BlueprintPure, Category = "Technique")
	bool HasRestoreOpportunity() const
	{
		UAbilitySystemComponent ASC = AbilitySystem::GetAbilitySystemComponent(GetOwner());
		if (ASC == nullptr)
		{
			return false;
		}

		TArray<FGameplayAbilitySpecHandle> AllHandles;
		ASC.GetAllAbilities(AllHandles);

		for (FGameplayAbilitySpecHandle Handle : AllHandles)
		{
			FGameplayAbilitySpec Spec;
			if (!ASC.FindAbilitySpecFromHandle(Handle, Spec))
			{
				continue;
			}

			UYanTechniqueAbility TechniqueAbility = Cast<UYanTechniqueAbility>(Spec.Ability);
			if (TechniqueAbility != nullptr && TechniqueAbility.TechniqueTag.MatchesTag(RestoreAbilityTag))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * 在下一帧熔断标识匹配 BurnoutTags 的术式，并在确有术式被熔断时授予一次修复机会。
	 * 传入父级标识可一次命中一整组，如 Ability.CursedTechnique.Gojo 覆盖五条全部术式。
	 *
	 * 延迟一帧是必要的：调用方通常是领域展开技能的 EndAbility 流程，
	 * 而熔断范围往往覆盖领域展开自身，就地 ClearAbility 会在技能结束途中
	 * 移除正在结束的 Spec。改由组件承接则不受技能实例存亡影响。
	 */
	UFUNCTION(BlueprintCallable, Category = "Technique")
	void ScheduleBurnout(FGameplayTagContainer BurnoutTags)
	{
		if (!GetOwner().HasAuthority())
		{
			return;
		}

		PendingBurnoutTags = BurnoutTags;
		System::SetTimerForNextTick(this, "ExecutePendingBurnout");
	}

	UFUNCTION()
	private void ExecutePendingBurnout()
	{
		UAbilitySystemComponent ASC = GetAbilitySystem();
		if (ASC == nullptr)
		{
			return;
		}

		TArray<FGameplayAbilitySpecHandle> AllHandles;
		ASC.GetAllAbilities(AllHandles);

		bool bAnyBurnt = false;
		for (FGameplayAbilitySpecHandle Handle : AllHandles)
		{
			FGameplayAbilitySpec Spec;
			if (!ASC.FindAbilitySpecFromHandle(Handle, Spec))
			{
				continue;
			}

			// 修复凭证的存亡由修复流程掌管，不受领域熔断波及
			UYanTechniqueAbility TechniqueAbility = Cast<UYanTechniqueAbility>(Spec.Ability);
			if (TechniqueAbility == nullptr
				|| TechniqueAbility.TechniqueTag.MatchesTag(RestoreAbilityTag)
				|| !TechniqueAbility.TechniqueTag.MatchesAny(PendingBurnoutTags))
			{
				continue;
			}

			BurnoutSpec(Spec);
			bAnyBurnt = true;
		}

		PendingBurnoutTags = FGameplayTagContainer();

		// 熔断即给自己一次修复机会，否则修复链条无从开始
		if (bAnyBurnt)
		{
			GrantRestoreOpportunity();
		}
	}

	/**
	 * 提交一次修复：客户端在轮盘上选定后调用，由服务器落实。
	 * 修复成功后授予所有对手一次修复机会，并收回本次已用掉的机会。
	 */
	UFUNCTION(Server, Category = "Technique")
	void ServerCommitRestore(int32 BurntIndex)
	{
		if (!HasRestoreOpportunity() || !BurntTechniques.IsValidIndex(BurntIndex))
		{
			return;
		}

		// 凭证不可作为修复目标，否则一次修复能换回两次机会
		if (BurntTechniques[BurntIndex].TechniqueTag.MatchesTag(RestoreAbilityTag))
		{
			return;
		}

		RestoreFromRecord(BurntIndex);
		GrantRestoreOpportunityToOpponents();
		ConsumeRestoreOpportunity();
	}

	/** 授予一次修复机会：把修复凭证从熔断记录中取回。 */
	UFUNCTION(BlueprintCallable, Category = "Technique")
	void GrantRestoreOpportunity()
	{
		if (!GetOwner().HasAuthority() || HasRestoreOpportunity())
		{
			return;
		}

		for (int32 Index = 0; Index < BurntTechniques.Num(); ++Index)
		{
			if (BurntTechniques[Index].TechniqueTag.MatchesTag(RestoreAbilityTag))
			{
				RestoreFromRecord(Index);
				return;
			}
		}
	}

	// 用掉机会即熔断凭证，它随后停在熔断记录里等待下一次授予
	private void ConsumeRestoreOpportunity()
	{
		UAbilitySystemComponent ASC = GetAbilitySystem();
		if (ASC == nullptr)
		{
			return;
		}

		TArray<FGameplayAbilitySpecHandle> AllHandles;
		ASC.GetAllAbilities(AllHandles);

		for (FGameplayAbilitySpecHandle Handle : AllHandles)
		{
			FGameplayAbilitySpec Spec;
			if (!ASC.FindAbilitySpecFromHandle(Handle, Spec))
			{
				continue;
			}

			UYanTechniqueAbility TechniqueAbility = Cast<UYanTechniqueAbility>(Spec.Ability);
			if (TechniqueAbility != nullptr && TechniqueAbility.TechniqueTag.MatchesTag(RestoreAbilityTag))
			{
				BurnoutSpec(Spec);
				return;
			}
		}
	}

	// 先快照后移除：ClearAbility 之后 Spec 即不可用
	private void BurnoutSpec(FGameplayAbilitySpec Spec)
	{
		UYanTechniqueAbility TechniqueAbility = Cast<UYanTechniqueAbility>(Spec.Ability);
		if (TechniqueAbility == nullptr)
		{
			return;
		}

		FYanBurntTechnique Record;
		Record.Ability = Spec.Ability.Class;
		Record.TechniqueTag = TechniqueAbility.TechniqueTag;
		Record.Level = Spec.Level;
		Record.InputTags = Spec.DynamicAbilityTags;
		BurntTechniques.Add(Record);

		GetAbilitySystem().ClearAbility(Spec.Handle);
	}

	private void RestoreFromRecord(int32 BurntIndex)
	{
		UAbilitySystemComponent ASC = GetAbilitySystem();
		if (ASC == nullptr || !BurntTechniques.IsValidIndex(BurntIndex))
		{
			return;
		}

		const FYanBurntTechnique Record = BurntTechniques[BurntIndex];
		FGameplayAbilitySpec NewSpec(Record.Ability, Record.Level);
		NewSpec.DynamicAbilityTags.AppendTags(Record.InputTags);
		ASC.GiveAbility(NewSpec);

		BurntTechniques.RemoveAt(BurntIndex);
	}

	// 对手获得的是"修复机会"本身，具体修复哪个术式由其自己的熔断记录决定，
	// 因此不需要在角色之间映射术式，五条与宿傩各自列出自己的熔断项即可。
	private void GrantRestoreOpportunityToOpponents()
	{
		TArray<AActor> Pawns;
		Gameplay::GetAllActorsOfClass(APawn, Pawns);

		for (AActor PawnActor : Pawns)
		{
			if (PawnActor == GetOwner() || UYanTechniqueComponent::Get(PawnActor) == nullptr)
			{
				continue;
			}

			UYanTechniqueComponent::Get(PawnActor).GrantRestoreOpportunity();
		}
	}
}
