/**
 * ADamageZoneActor — 重叠即施加 GameplayEffect 的测试危险区。
 *
 * 一个带碰撞盒（Trigger）的 Actor：玩家进入重叠时，向其 AbilitySystemComponent
 * 施加一个可配置的 GameplayEffect。改动哪个属性、改动多少全由 GE 决定，
 * 因此既能扣血（Health），也能改动咒力等其它 AttributeSet——只需替换 DamageEffect。
 *
 * 之所以走 GE 而非直接改属性：UYanHealthSet 的 Health 与 Damage 均标记
 * HideFromModifiers，唯一合法的扣血路径是让 GE 的 Execution 输出到 Damage(meta)，
 * 再由属性集换算为 -Health。直接写属性会被拦截，也绕过了 clamp 与死亡广播。
 *
 * 蓝图需设置：
 *   - DamageEffect            → 目标要承受的 GameplayEffect（如项目内的伤害 GE）
 *   - DamageSetByCallerTag    → 可选；GE 用 SetByCaller 取量时填其标签
 *   - DamageMagnitude         → 仅当上面标签有效时，注入的量
 */
class ADamageZoneActor : AActor
{
	/** 触发用碰撞盒，作为根组件；玩家与之重叠即触发 */
	UPROPERTY(DefaultComponent, RootComponent)
	UBoxComponent TriggerBox;
	default TriggerBox.BoxExtent = FVector(100.0, 100.0, 100.0);

	/** 重叠时施加到目标的 GameplayEffect（决定改动哪个属性、改多少） */
	UPROPERTY(EditAnywhere, Category = "DamageZone")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** 施加时的 GameplayEffect Level */
	UPROPERTY(EditAnywhere, Category = "DamageZone")
	float EffectLevel = 1.0f;

	/** GE 通过 SetByCaller 取量时对应的标签；无效则不注入，量由 GE 自身决定 */
	UPROPERTY(EditAnywhere, Category = "DamageZone")
	FGameplayTag DamageSetByCallerTag;

	/** 仅当 DamageSetByCallerTag 有效时，注入到 SetByCaller 的量 */
	UPROPERTY(EditAnywhere, Category = "DamageZone")
	float DamageMagnitude = 10.0f;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		// 配置为可重叠的 Trigger，并订阅进入重叠的回调
		TriggerBox.SetCollisionProfileName(n"OverlapAllDynamic");
		TriggerBox.SetGenerateOverlapEvents(true);
		TriggerBox.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
	}

	UFUNCTION()
	private void HandleBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor,
		UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in Hit)
	{
		if (OtherActor == nullptr || OtherActor == this)
		{
			return;
		}

		UAbilitySystemComponent TargetASC = AbilitySystem::GetAbilitySystemComponent(OtherActor);
		if (TargetASC == nullptr)
		{
			// 重叠对象不具备 ASC（非战斗单位），无需处理
			return;
		}

		if (DamageEffect.Get() == nullptr)
		{
			Log("ADamageZoneActor: DamageEffect 未设置，未施加任何效果");
			return;
		}

		FGameplayEffectContextHandle Context = TargetASC.MakeEffectContext();
		Context.AddInstigator(this, this);

		FGameplayEffectSpecHandle Spec = TargetASC.MakeOutgoingSpec(DamageEffect, EffectLevel, Context);
		if (!Spec.IsValid())
		{
			return;
		}

		// GE 用 SetByCaller 取量时，把可配置的数值注入进去
		if (DamageSetByCallerTag.IsValid())
		{
			Spec = AbilitySystem::AssignTagSetByCallerMagnitude(Spec, DamageSetByCallerTag, DamageMagnitude);
		}

		TargetASC.ApplyGameplayEffectSpecToSelf(Spec);
	}
}
