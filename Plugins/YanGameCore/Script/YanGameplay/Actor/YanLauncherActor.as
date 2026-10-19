/**
 * 发射器的两种施力方式，由子蓝图在 CDO 上选定。
 */
enum EYanLauncherMode
{
	/** 沿 Pawn 朝向水平推出并抬起，速度按 Multiplier 放大；可改用 Target 小部件反算落点 */
	Push,
	/** 只把 Pawn 竖直顶起，UpOverride 即竖直速度 */
	Up
}

/**
 * AYanLauncherActor — 发射板：Pawn 踩入球形触发区即被沿 Pawn 朝向弹出。
 *
 * 网格、点光与球形触发区由子蓝图的 SCS 提供，本类只按类型解析，不自带组件：
 * 关卡里已有大量实例对这些组件做了逐实例覆盖，把组件搬进脚本会让覆盖全部失效。
 *
 * 施力走本地预测：本地受控端把速度经 UYanWorldTriggerInputComponent 写进输入包，
 * sim 侧 UChaosCharacterLaunchPadCheck 以 Additive 速度效果施加并切入 Falling，
 * 服务器按同一输入包重模拟得到同一结果。重叠回调在 game thread，直接向 Mover 入队
 * 的效果不可回滚，预测出的弹射会被服务器状态抹掉，故不能在此直接施力。
 * 两种模式都是叠加而非覆盖——Up 模式落入时下坠速度会抵消一部分抬升，
 * 需要在关卡里按落入速度调 UpOverride。
 *
 * GameplayCue 双路触发：本地受控端与位移同帧本地执行预测份，服务器再经目标 ASC 多播给旁观者；
 * 处理器靠 YanGameplayCue::ShouldHandlePredictableCue 丢弃本机已预测过的那份。
 * 由 AI 控制的 Pawn 不触发：Bot 走默认 Mover 后端，没有本地输入包。
 */
class AYanLauncherActor : AActor
{
	/** 施力方式，Push/Up 子蓝图各自在 CDO 上固定 */
	UPROPERTY(EditDefaultsOnly, Category = "Launcher")
	EYanLauncherMode LaunchMode = EYanLauncherMode::Push;

	/** Push：竖直分量，与水平朝向一同乘以 Multiplier；Up：竖直速度本身（cm/s） */
	UPROPERTY(EditAnywhere, Category = "Launcher")
	float UpOverride = 0.3f;

	/** Push 模式的速度倍率，作用于 (Forward.X, Forward.Y, UpOverride) */
	UPROPERTY(EditAnywhere, Category = "Launcher", Meta = (EditCondition = "LaunchMode == EYanLauncherMode::Push", EditConditionHides))
	float Multiplier = 1500.0f;

	/** Push 模式改用 Target 小部件反算发射速度，忽略 UpOverride 与 Multiplier */
	UPROPERTY(EditAnywhere, Category = "Launcher", Meta = (EditCondition = "LaunchMode == EYanLauncherMode::Push", EditConditionHides))
	bool bUseTarget = false;

	/** 期望落点（本地空间），只取其水平距离与高度；构造时被 XYBounds/ZBounds 夹住 */
	UPROPERTY(EditAnywhere, Category = "Launcher", Meta = (MakeEditWidget, EditCondition = "bUseTarget && LaunchMode == EYanLauncherMode::Push", EditConditionHides))
	FVector Target = FVector(150.0, 0.0, 0.0);

	/** 落入速度低于此值（下坠更快）且 bDoNotActivateOnFallEntry 为真时不触发 */
	UPROPERTY(EditAnywhere, Category = "Launcher", Meta = (ForceUnits = "cm/s"))
	float MinimumVerticalVelocityAllowed = -450.0f;

	/** 高速坠入时不触发，避免把下落中的玩家再度弹起 */
	UPROPERTY(EditAnywhere, Category = "Launcher")
	bool bDoNotActivateOnFallEntry = false;

	/** 触发时在目标 Pawn 上执行的 GameplayCue */
	UPROPERTY(EditDefaultsOnly, Category = "Launcher")
	FGameplayTag ActivateCueTag = FGameplayTag::RequestGameplayTag(n"GameplayCue.World.Launcher.Activate");

	UPROPERTY(EditAnywhere, Category = "Launcher|Visual")
	FLinearColor LightColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Launcher|Visual")
	float LightIntensity = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Launcher|Visual")
	float LightAttenuation = 250.0f;

	/** 写入网格材质的 Intensity 参数 */
	UPROPERTY(EditAnywhere, Category = "Launcher|Visual")
	float GlowIntensity = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Launcher|Visual")
	float SphereRadius = 100.0f;

	// Target 的可编辑范围，超出后落点反算会失真
	private const float XYBounds = 5500.0f;
	private const float ZBounds  = 500.0f;

	//~Begin AActor Interface
	UFUNCTION(BlueprintOverride)
	void ConstructionScript()
	{
		UStaticMeshComponent LauncherMesh = UStaticMeshComponent::Get(this);
		if (LauncherMesh != nullptr)
		{
			UMaterialInstanceDynamic Material = LauncherMesh.CreateDynamicMaterialInstance(0, LauncherMesh.GetMaterial(0));
			if (Material != nullptr)
			{
				Material.SetScalarParameterValue(n"Intensity", GlowIntensity);
				Material.SetVectorParameterValue(n"Color", LightColor);
			}
		}

		UPointLightComponent PointLight = UPointLightComponent::Get(this);
		if (PointLight != nullptr)
		{
			PointLight.SetLightColor(LightColor);
			PointLight.SetIntensity(LightIntensity);
			PointLight.SetAttenuationRadius(LightAttenuation);
		}

		USphereComponent Sphere = USphereComponent::Get(this);
		if (Sphere != nullptr)
		{
			Sphere.SetSphereRadius(SphereRadius);
		}

		// 落点只在 X-Z 平面内取值，Y 恒为 0
		Target.X = Math::Clamp(Target.X, -XYBounds, XYBounds);
		Target.Y = 0.0;
		Target.Z = Math::Clamp(Target.Z, -ZBounds, ZBounds);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		USphereComponent Sphere = USphereComponent::Get(this);
		if (Sphere != nullptr)
		{
			Sphere.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
		}
	}
	//~End AActor Interface

	UFUNCTION()
	private void HandleBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor,
		UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in Hit)
	{
		APawn OtherPawn = Cast<APawn>(OtherActor);
		if (OtherPawn == nullptr || Cast<AAIController>(OtherPawn.Controller) != nullptr)
		{
			return;
		}

		UMoverComponent MoverComp = UMoverComponent::Get(OtherPawn);
		if (MoverComp == nullptr)
		{
			return;
		}

		if (bDoNotActivateOnFallEntry && MoverComp.GetVelocity().Z < MinimumVerticalVelocityAllowed)
		{
			return;
		}

		// 意图只能由本地受控端写进输入包；远端玩家的弹射由其自己的输入包送达，服务器不替它施力
		if (OtherPawn.IsLocallyControlled())
		{
			UYanWorldTriggerInputComponent TriggerInput = UYanWorldTriggerInputComponent::FindOrAdd(OtherPawn);
			if (TriggerInput != nullptr)
			{
				TriggerInput.RequestLaunch(ComputeLaunchVelocity(OtherPawn.GetActorForwardVector()));
			}

			YanGameplayCue::ExecuteGameplayCuePredicted(OtherPawn, ActivateCueTag, MakeActivateCueParameters(OtherPawn));
		}

		if (HasAuthority())
		{
			ExecuteActivateCue(OtherPawn);
		}
	}

	private FVector ComputeLaunchVelocity(FVector PawnForward) const
	{
		if (LaunchMode == EYanLauncherMode::Up)
		{
			return FVector(PawnForward.X, PawnForward.Y, UpOverride);
		}

		if (!bUseTarget)
		{
			return FVector(PawnForward.X, PawnForward.Y, UpOverride) * Multiplier;
		}

		// 由落点反算：水平速度与距离成正比，竖直速度按高度取值并限幅
		const float HorizontalSpeed = Target.Size2D() * 2.1f;
		const float VerticalSpeed   = Math::Clamp(Target.Z * 2.0f, 250.0f, 850.0f);
		const FVector LaunchVelocity(PawnForward.X * HorizontalSpeed, PawnForward.Y * HorizontalSpeed, VerticalSpeed);

		return LaunchVelocity.GetClampedToMaxSize(12000.0f);
	}

	// 服务器多播给旁观者的那份；本地受控端的预测份在重叠回调里直接执行
	private void ExecuteActivateCue(APawn TargetPawn)
	{
		UAbilitySystemComponent TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(TargetPawn);
		if (TargetASC == nullptr || !ActivateCueTag.IsValid())
		{
			return;
		}

		TargetASC.ExecuteGameplayCueWithParameters(ActivateCueTag, MakeActivateCueParameters(TargetPawn));
	}

	private FGameplayCueParameters MakeActivateCueParameters(APawn TargetPawn)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location     = GetActorLocation();
		CueParams.EffectCauser = this;
		CueParams.Instigator   = TargetPawn;
		return CueParams;
	}
}
