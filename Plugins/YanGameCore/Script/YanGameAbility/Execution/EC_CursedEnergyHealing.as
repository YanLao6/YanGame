/**
 * UEC_CursedEnergyHealing — 咒力换血的执行计算（ExecutionCalculation）。
 *
 * 供一个 Infinite + Periodic 的 GameplayEffect 挂载：每次执行读取目标当前咒力，
 * 按 1:1 消耗咒力并恢复等量生命值。咒力低于 MinimumCursedEnergy 时不产出任何修改，
 * 治疗随之停止；单次结算量以「当前咒力」封顶，避免咒力见底时凭空回血。
 *
 * 每秒一次的节奏由 GE 资产的 Period 决定，本计算只负责单次结算。
 *
 * 蓝图需设置：
 *   - CursedEnergyAttribute     → UYanBaseCombatSet.CursedEnergy    （读取当前咒力）
 *   - CursedEnergyCostAttribute → UYanBaseCombatSet.CursedEnergyCost（meta：换算为 -CursedEnergy）
 *   - HealingAttribute          → UYanHealthSet.Healing             （meta：换算为 +Health）
 */
class UEC_CursedEnergyHealing : UGameplayEffectExecutionCalculation
{
	// 读取当前咒力的属性
	UPROPERTY(EditDefaultsOnly, Category = "CursedEnergyHealing")
	FGameplayAttribute CursedEnergyAttribute;

	// 咒力消耗 meta，输出后由属性集换算为 -CursedEnergy
	UPROPERTY(EditDefaultsOnly, Category = "CursedEnergyHealing")
	FGameplayAttribute CursedEnergyCostAttribute;

	// 治疗 meta，输出后由属性集换算为 +Health
	UPROPERTY(EditDefaultsOnly, Category = "CursedEnergyHealing")
	FGameplayAttribute HealingAttribute;

	// 咒力低于此值时停止治疗；默认 0，即耗尽才停
	UPROPERTY(EditDefaultsOnly, Category = "CursedEnergyHealing")
	float MinimumCursedEnergy = 0.0f;

	// 单次执行消耗的咒力，按 1:1 恢复等量生命值
	UPROPERTY(EditDefaultsOnly, Category = "CursedEnergyHealing", Meta = (ClampMin = "0.0"))
	float CursedEnergyPerExecution = 1.0f;

	//~Begin UGameplayEffectExecutionCalculation Interface
	UFUNCTION(BlueprintOverride)
	void Execute(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
	{
		UAbilitySystemComponent TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
		if (TargetASC == nullptr)
		{
			return;
		}

		bool bFound = false;
		const float CurrentCursedEnergy = TargetASC.GetGameplayAttributeValue(CursedEnergyAttribute, bFound);
		if (!bFound || CurrentCursedEnergy < MinimumCursedEnergy)
		{
			// 咒力不足阈值，本次不产出任何修改
			return;
		}

		// 单次结算量以当前咒力封顶，保证 1:1 且不透支
		const float Amount = Math::Min(CursedEnergyPerExecution, CurrentCursedEnergy);
		if (Amount <= 0.0f)
		{
			return;
		}

		OutExecutionOutput.AddOutputModifier(MakeModifier(CursedEnergyCostAttribute, Amount));
		OutExecutionOutput.AddOutputModifier(MakeModifier(HealingAttribute, Amount));
	}
	//~End UGameplayEffectExecutionCalculation Interface

	// 构造一条 Additive 的已评估修改数据
	private FGameplayModifierEvaluatedData MakeModifier(const FGameplayAttribute& Attribute, float Magnitude) const
	{
		FGameplayModifierEvaluatedData EvaluatedData;
		EvaluatedData.SetAttribute(Attribute);
		EvaluatedData.SetModifierOp(EGameplayModOp::Additive);
		EvaluatedData.SetMagnitude(Magnitude);
		EvaluatedData.SetIsValid(true);
		return EvaluatedData;
	}
}
