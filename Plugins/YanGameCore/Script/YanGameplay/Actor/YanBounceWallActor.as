/**
 * AYanBounceWallActor — 弹射壁：重叠即把玩家朝场内弹回，并进入冷却。
 *
 * 定位是回场手段而非攻击，弹射强度恒定、不可格挡、不随负荷放大——
 * 玩家要能预判自己会被送到哪，才谈得上「利用它回场」。
 *
 * 冷却挂在玩家 ASC 上而非本 Actor 上，多面弹射壁因而共享同一次机会。
 * 冷却若各自记在 Actor 上，玩家可在两面壁之间往复弹射，限制随即失效。
 *
 * 弹射方向取本 Actor 前方的水平分量并按 UpwardAngleDegrees 抬起，摆放时把 Actor 转向场内。
 * 不采信重叠事件的命中法线：OnComponentBeginOverlap 仅在 bFromSweep 为真时才带有效 Hit，
 * 高速飞入时拿不到。
 *
 * 触发体沿弹射方向须足够厚。物理步长 0.0333s 下，满负荷击退速度（约 2400 cm/s）单帧位移
 * 已达 80 cm，薄壳会在最需要弹射的时刻被穿过去。
 *
 * 摆放须完全落在出界体积（ADeathActor）内侧：两者重叠时玩家会在同一帧既被判出界又被弹回，
 * 出界优先，弹射壁形同虚设。
 *
 * 只在服务器判定。重叠回调两端都会触发，客户端自行施加会在服务器确认前就把人弹走。
 *
 * 蓝图需设置：
 *   BounceLaunchEffect   → 以 UMoverLaunchGameplayEffect 为父类的 GE，且须改三项默认值：
 *                          SpeedMultiplierAtMaxLoad = 1  负荷缩放会让被打得越惨的人弹得越远
 *                          BlockFacingConeDegrees   = 0  面朝壁的玩家不应因格挡姿态而弹不动
 *                          bOverrideVelocity        = true  覆盖撞入速度，弹射强度才恒定
 *   BounceCooldownEffect → Duration 型 GE，经 Target Tags 组件授予 CooldownTag
 *   CooldownTag          → 与上面 GE 授予的标签一致
 */
class AYanBounceWallActor : AActor
{
	/** 弹射触发体，作为根组件；X 轴为弹射方向，其厚度决定对高速穿透的容忍度 */
	UPROPERTY(DefaultComponent, RootComponent)
	UBoxComponent TriggerBox;
	default TriggerBox.BoxExtent = FVector(300.0, 1500.0, 1000.0);

	/** 弹射速度，恒定值，不随撞入速度变化 */
	UPROPERTY(EditAnywhere, Category = "BounceWall", Meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float BounceSpeed = 2200.0f;

	/** 弹射方向自水平面抬起的角度；纯水平弹回会贴着场地边缘再次坠落 */
	UPROPERTY(EditAnywhere, Category = "BounceWall", Meta = (ClampMin = "0", ClampMax = "89"))
	float UpwardAngleDegrees = 20.0f;

	/** 弹射后禁止落地的时长，避免速度尚未发挥即被地面摩擦接管 */
	UPROPERTY(EditAnywhere, Category = "BounceWall", Meta = (ClampMin = "0", ForceUnits = "s"))
	float NoLandingDuration = 0.25f;

	/** 弹射用击飞 GE，须使用 UMoverLaunchExecutionCalculation（即 UMoverLaunchGameplayEffect 或其子类） */
	UPROPERTY(EditAnywhere, Category = "BounceWall")
	TSubclassOf<UGameplayEffect> BounceLaunchEffect;

	/** 冷却 GE：Duration 型，须经 Target Tags 组件授予 CooldownTag */
	UPROPERTY(EditAnywhere, Category = "BounceWall")
	TSubclassOf<UGameplayEffect> BounceCooldownEffect;

	/** 冷却期间玩家持有的标签，须与 BounceCooldownEffect 授予的一致 */
	UPROPERTY(EditAnywhere, Category = "BounceWall")
	FGameplayTag CooldownTag;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		TriggerBox.SetCollisionProfileName(n"OverlapAllDynamic");
		TriggerBox.SetGenerateOverlapEvents(true);
		TriggerBox.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
	}

	UFUNCTION()
	private void HandleBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor,
		UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in Hit)
	{
		if (!HasAuthority())
		{
			return;
		}

		// 只有玩家有回场需求；碎块与投射物飞进来无需惊动 GAS
		APawn OtherPawn = Cast<APawn>(OtherActor);
		if (OtherPawn == nullptr)
		{
			return;
		}

		UAbilitySystemComponent TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(OtherPawn);
		if (TargetASC == nullptr || BounceLaunchEffect.Get() == nullptr)
		{
			return;
		}

		// 冷却中撞入不做任何事，玩家贴着壁继续坠落——比弱化版弹射更容易读懂
		if (IsOnCooldown(TargetASC))
		{
			return;
		}

		const FVector BounceVelocity = ComputeBounceVelocity();
		if (BounceVelocity.IsNearlyZero())
		{
			return;
		}

		// 施加方即玩家自身：本 Actor 没有 ASC，且「玩家借弹射壁把自己弹回场」的归属本就在其自身
		MoverGameplay::ApplyLaunchEffectToTarget(TargetASC, TargetASC, BounceLaunchEffect, BounceVelocity);

		if (NoLandingDuration > 0.0f)
		{
			UMoverComponent MoverComp = UMoverComponent::Get(OtherPawn);
			if (MoverComp != nullptr)
			{
				MoverAuthority::ApplyNoLanding(MoverComp, NoLandingDuration);
			}
		}

		StartCooldown(TargetASC);
	}

	// 方向取本 Actor 前方的水平分量并抬起，速率恒为 BounceSpeed
	private FVector ComputeBounceVelocity() const
	{
		const FVector InwardDir = GetActorForwardVector().GetSafeNormal2D();
		if (InwardDir.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}

		const float UpwardRadians = Math::DegreesToRadians(UpwardAngleDegrees);

		return (InwardDir * Math::Cos(UpwardRadians) + FVector::UpVector * Math::Sin(UpwardRadians)) * BounceSpeed;
	}

	private bool IsOnCooldown(UAbilitySystemComponent TargetASC) const
	{
		if (!CooldownTag.IsValid())
		{
			return false;
		}

		FGameplayTagContainer OwnedTags;
		TargetASC.GetOwnedGameplayTags(OwnedTags);

		return OwnedTags.HasTag(CooldownTag);
	}

	private void StartCooldown(UAbilitySystemComponent TargetASC)
	{
		if (BounceCooldownEffect.Get() == nullptr)
		{
			Log("AYanBounceWallActor: BounceCooldownEffect 未设置，弹射不受冷却限制");
			return;
		}

		FGameplayEffectContextHandle Context = TargetASC.MakeEffectContext();
		Context.AddInstigator(this, this);

		FGameplayEffectSpecHandle Spec = TargetASC.MakeOutgoingSpec(BounceCooldownEffect, 1.0f, Context);
		if (Spec.IsValid())
		{
			TargetASC.ApplyGameplayEffectSpecToSelf(Spec);
		}
	}
}
