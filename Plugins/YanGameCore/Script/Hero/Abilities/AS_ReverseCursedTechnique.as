/**
 * UAS_ReverseCursedTechnique — 反转术式（持续回血）。
 *
 * 回血与咒力消耗由一个 Infinite + Periodic 的 GameplayEffect 承担，
 * 技能本身只负责施加与撤除它。把持续效果放在 GE 而非技能逻辑里，
 * 死亡、被打断、技能被清除时效果都会随之失效，不会出现停不下来的回血。
 *
 * 结束途径有二：轮盘再次选中本术式时广播的 Event.Technique.Toggle，
 * 以及咒力降至阈值以下。两者都走 EndAbility，撤除逻辑只有一处。
 *
 * 蓝图需设置：
 *   - ChannelEffectClass    → 消耗 CursedEnergyCost、补充 Healing 的周期性 GE
 *   - CursedEnergyAttribute → UYanBaseCombatSet.CursedEnergy
 */
class UAS_ReverseCursedTechnique : UYanTechniqueAbility
{
	/** 持续期间挂载的周期性 GameplayEffect */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse")
	TSubclassOf<UGameplayEffect> ChannelEffectClass;

	/** 用于监听耗尽的咒力属性 */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse")
	FGameplayAttribute CursedEnergyAttribute;

	/** 咒力低于此值时自动停止 */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse")
	float MinimumCursedEnergy = 1.0f;

	private FActiveGameplayEffectHandle ChannelEffectHandle;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility() || ChannelEffectClass.Get() == nullptr)
		{
			EndAbility();
			return;
		}

		ChannelEffectHandle = ApplyGameplayEffectToOwner(ChannelEffectClass);

		UAbilityTask_WaitGameplayEvent StopTask = AngelscriptAbilityTask::WaitGameplayEvent(
			this, FGameplayTag::RequestGameplayTag(n"Event.Technique.Toggle"), nullptr, true, true);
		StopTask.EventReceived.AddUFunction(this, n"HandleStopRequested");
		StopTask.ReadyForActivation();

		UAbilityTask_WaitAttributeChangeThreshold EnergyTask = AngelscriptAbilityTask::WaitForAttributeChangeThreshold(
			this, CursedEnergyAttribute, EWaitAttributeChangeComparison::LessThanOrEqualTo, MinimumCursedEnergy, true);
		EnergyTask.OnChange.AddUFunction(this, n"HandleCursedEnergyDepleted");
		EnergyTask.ReadyForActivation();
	}

	UFUNCTION(BlueprintOverride)
	void OnEndAbility(bool bWasCancelled)
	{
		RemoveGameplayEffectFromOwnerWithHandle(ChannelEffectHandle);
	}
	//~End UGameplayAbility Interface

	UFUNCTION()
	private void HandleStopRequested(FGameplayEventData Payload)
	{
		EndAbility();
	}

	UFUNCTION()
	private void HandleCursedEnergyDepleted(bool bMatchesComparison, float CurrentValue)
	{
		if (bMatchesComparison)
		{
			EndAbility();
		}
	}
}
