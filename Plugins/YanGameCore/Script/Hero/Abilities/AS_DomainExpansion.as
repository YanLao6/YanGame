/**
 * UAS_DomainExpansion — 领域展开，术式熔断的来源。
 *
 * 领域维持 Duration 后自行收束，收束时熔断 BurnoutOnEnd 列出的术式
 * （通常包含领域展开自身），并使玩家获得一次修复机会。
 *
 * 熔断委托给 UYanTechniqueComponent 延迟一帧执行：待熔断列表包含本技能自己时，
 * 就地移除会打断正在结束的 Spec。
 *
 * 领域的实际表现（范围、命中、演出）留给蓝图实现 OnDomainOpened / OnDomainClosed，
 * 本类只负责生命周期与熔断结算。
 */
class UAS_DomainExpansion : UYanTechniqueAbility
{
	/** 领域维持时长（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Domain")
	float Duration = 10.0f;

	/**
	 * 领域收束时熔断的术式标识，通常包含本技能自身。
	 * 填父级标识可一次命中一整组，如 Ability.CursedTechnique.Gojo 覆盖五条全部术式。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Domain", Meta = (Categories = "Ability.CursedTechnique"))
	FGameplayTagContainer BurnoutTagsOnEnd;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		OnDomainOpened();

		UAbilityTask_WaitDelay DurationTask = AngelscriptAbilityTask::WaitDelay(this, Duration);
		DurationTask.OnFinish.AddUFunction(this, n"HandleDurationElapsed");
		DurationTask.ReadyForActivation();
	}

	UFUNCTION(BlueprintOverride)
	void OnEndAbility(bool bWasCancelled)
	{
		OnDomainClosed(bWasCancelled);

		UYanTechniqueComponent TechniqueComp = UYanTechniqueComponent::Get(GetAvatarActorFromActorInfo());
		if (TechniqueComp != nullptr)
		{
			TechniqueComp.ScheduleBurnout(BurnoutTagsOnEnd);
		}
	}
	//~End UGameplayAbility Interface

	/** 蓝图实现：领域展开时的表现与判定。 */
	UFUNCTION(BlueprintEvent, Category = "Domain")
	void OnDomainOpened()
	{
	}

	/** 蓝图实现：领域收束时的收尾，被打断时 bWasCancelled 为真。 */
	UFUNCTION(BlueprintEvent, Category = "Domain")
	void OnDomainClosed(bool bWasCancelled)
	{
	}

	UFUNCTION()
	private void HandleDurationElapsed()
	{
		EndAbility();
	}
}
