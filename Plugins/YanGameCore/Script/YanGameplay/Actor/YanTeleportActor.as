/**
 * AYanTeleportActor — 传送门：Pawn 踩入盒形触发区即被送到 TeleportingTarget 中随机一扇门的出口。
 *
 * 出口箭头、Niagara 门面与盒形触发区由蓝图的 SCS 提供，本类只按类型解析：
 * 出口箭头的位置与朝向逐实例摆放，把组件搬进脚本会让这些覆盖失效。
 *
 * 传送走本地预测：本地受控端把出口位姿与出口速度经 UYanWorldTriggerInputComponent 写进输入包，
 * sim 侧 UChaosCharacterTeleporterCheck 以 FYanTeleportEffect（AttemptTeleport + 出口速度）施加，
 * 服务器按同一输入包重模拟得到同一结果。传送门是静态 Actor，两端算出的目标一致。
 * 不能走项目里的 Blink——那是撞墙会被挡住的限时冲刺，不是跨地图搬运。
 *
 * 出口点须落在目的门的触发区之外，否则抵达即被送回；用出口速度把人推离触发区。
 * 控制朝向只在本地端设置：只有本地端能改自己的视角。
 *
 * GameplayCue 双路触发：本地受控端与传送同帧本地执行预测份，服务器再经目标 ASC 多播给旁观者；
 * 处理器靠 YanGameplayCue::ShouldHandlePredictableCue 丢弃本机已预测过的那份。
 * 远端模拟代理的复制位置可能跳过薄薄的触发盒，旁观者看到的门闪必须靠服务器多播送达。
 *
 * 两门之间只差一个偏航角：以玩家穿过本门的方向对齐目的门的出口方向，视角、身体朝向、
 * 入口偏移与入口速度都整体转过这个角，玩家在入口门坐标系里怎样，在出口门坐标系里就怎样。
 * 引擎 Teleport 按「当前朝向 → 目标朝向」的差旋转速度，故目标朝向须是转过该角的自身朝向而非出口方向。
 */
class AYanTeleportActor : AActor
{
	/** 候选目的门，触发时随机取一扇；为空则本门只作接收端 */
	UPROPERTY(EditAnywhere, Category = "Teleporter")
	TArray<AYanTeleportActor> TeleportingTarget;

	/** 传送后把玩家视角按两门偏航差转过去，正对本门进则正背目的门出；关闭则视角不动 */
	UPROPERTY(EditAnywhere, Category = "Teleporter")
	bool bForceDirection = true;

	/** 传送后以 LaunchMultiplier 的速率沿出口方向推出，关闭则保留按两门偏航差转向后的入口速度 */
	UPROPERTY(EditAnywhere, Category = "Teleporter")
	bool bLaunchOnTeleport = true;

	/** 出口速率（cm/s），仅 bLaunchOnTeleport 为真时生效 */
	UPROPERTY(EditAnywhere, Category = "Teleporter", Meta = (EditCondition = "bLaunchOnTeleport", ForceUnits = "cm/s"))
	float LaunchMultiplier = 1250.0f;

	/** 出口点叠加玩家在入口门面上的相对偏移，从门的哪一处进就从对应处出 */
	UPROPERTY(EditAnywhere, Category = "Teleporter")
	bool bKeepRelativeEntryUponExit = false;

	/**
	 * 本门作为目的门时叠加在出口点上的偏移（cm），以出口箭头为坐标系：X 沿出口方向、Y 向右、Z 向上。
	 * 用于把落点推离本门触发区，避免玩家落地后一移动就再次触发、在两门间来回穿梭。
	 */
	UPROPERTY(EditAnywhere, Category = "Teleporter", Meta = (ForceUnits = "cm"))
	FVector ExitOffset = FVector::ZeroVector;

	/** 只作接收端：触发区不参与重叠 */
	UPROPERTY(EditAnywhere, Category = "Teleporter")
	bool bReceiverOnly = false;

	/** 触发时在目标 Pawn 上执行的 GameplayCue */
	UPROPERTY(EditDefaultsOnly, Category = "Teleporter")
	FGameplayTag ActivateCueTag = FGameplayTag::RequestGameplayTag(n"GameplayCue.World.Teleporter.Activate");

	// 以下为 Niagara 门面参数，构造时整体写入
	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	float TeleporterHeight = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	float TeleporterWidth = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	float TeleporterEdgeWidth = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	int TeleporterNumPlanes = 500;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	float TeleporterSpread = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	float TeleporterPhase = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	bool bTeleporterInwardMotion = true;

	UPROPERTY(EditAnywhere, Category = "Teleporter Size")
	float TeleporterBrightness = 1.0f;

	/**
	 * 最近一次选定的目的门，供 GameplayCue 点亮出口门面。
	 * 各端在重叠时各自选取；配置多扇目的门时，远端看到被点亮的门可能与实际传送所用的不同。
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Teleporter")
	AYanTeleportActor TeleporterDestination;

	/** 门面动画的触发计数：Niagara 以该值的变化作为动画起点，由 GameplayCue 经 NotifyTeleporterEntry 推进 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Teleporter")
	int TeleporterEntered = 0;

	// 相对入口偏移的竖直上限须留出这段余量，避免把人送到门面顶部之外
	private const float ExitHeightMargin = 100.0f;

	// 计数回绕上限，只是避免整数溢出，Niagara 只关心值有没有变
	private const int TeleporterEnteredWrap = 1000000;

	//~Begin AActor Interface
	UFUNCTION(BlueprintOverride)
	void ConstructionScript()
	{
		UNiagaraComponent Niagara = UNiagaraComponent::Get(this);
		if (Niagara != nullptr)
		{
			Niagara.SetVariableFloat(n"EdgeWidth", TeleporterEdgeWidth);
			Niagara.SetVariableFloat(n"PortalWidth", TeleporterWidth);
			Niagara.SetVariableFloat(n"PortalHeight", TeleporterHeight);
			Niagara.SetVariableInt(n"NumPlanes", TeleporterNumPlanes);
			Niagara.SetVariableFloat(n"Spread", TeleporterSpread);
			Niagara.SetVariableFloat(n"Phase", TeleporterPhase);
			// 粒子系统自带值为 1，留着首次触发不会播放
			Niagara.SetVariableInt(n"SystemEntered", 0);
			Niagara.SetVariableFloat(n"Brightness", TeleporterBrightness);
			Niagara.SetVariableBool(n"Inward", bTeleporterInwardMotion);
		}

		UBoxComponent Box = UBoxComponent::Get(this);
		if (Box != nullptr && bReceiverOnly)
		{
			Box.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UBoxComponent Box = UBoxComponent::Get(this);
		if (Box != nullptr && !bReceiverOnly)
		{
			Box.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
		}
	}
	//~End AActor Interface

	/** 出口点（世界空间），取出口箭头位置并叠加 ExitOffset */
	FVector GetExitLocation() const
	{
		UArrowComponent ExitDirection = UArrowComponent::Get(this);
		if (ExitDirection != nullptr)
		{
			return ExitDirection.GetWorldLocation() + ExitDirection.GetComponentQuat().RotateVector(ExitOffset);
		}
		return GetActorLocation() + GetActorQuat().RotateVector(ExitOffset);
	}

	/** 出口方向（世界空间），取出口箭头朝向 */
	FVector GetExitForward() const
	{
		UArrowComponent ExitDirection = UArrowComponent::Get(this);
		return ExitDirection != nullptr ? ExitDirection.GetForwardVector() : GetActorForwardVector();
	}

	/**
	 * 推进门面动画：计数加一写进 Niagara 的 SystemEntered，UseDirection 区分入口（正向）与出口（反向）。
	 * 由 GameplayCue 在各端调用，入口门与出口门各一次。
	 */
	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void NotifyTeleporterEntry(bool bEnter)
	{
		TeleporterEntered = (TeleporterEntered + 1) % TeleporterEnteredWrap;

		UNiagaraComponent Niagara = UNiagaraComponent::Get(this);
		if (Niagara != nullptr)
		{
			Niagara.SetVariableInt(n"SystemEntered", TeleporterEntered);
			Niagara.SetVariableFloat(n"UseDirection", bEnter ? 1.0f : -0.8f);
		}
	}

	UFUNCTION()
	private void HandleBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor, UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in Hit)
	{
		APawn OtherPawn = Cast<APawn>(OtherActor);
		if (OtherPawn == nullptr)
		{
			return;
		}

		UMoverComponent MoverComp = UMoverComponent::Get(OtherPawn);
		if (MoverComp == nullptr)
		{
			return;
		}

		AYanTeleportActor Destination = PickDestination();
		if (Destination == nullptr)
		{
			return;
		}
		TeleporterDestination = Destination;

		// 意图只能由本地受控端写进输入包；远端玩家的传送由其自己的输入包送达，服务器不替它执行
		if (OtherPawn.IsLocallyControlled())
		{
			const FVector ExitForward = Destination.GetExitForward();
			const float   DeltaYaw    = Math::FindDeltaAngleDegrees(ComputeEntryDirection(OtherPawn).Rotation().Yaw, ExitForward.Rotation().Yaw);

			FVector ExitLocation = Destination.GetExitLocation();
			if (bKeepRelativeEntryUponExit)
			{
				ExitLocation += ComputeEntryRelativeOffset(OtherPawn).RotateAngleAxis(DeltaYaw, FVector::UpVector);
			}

			FRotator ExitRotation = OtherPawn.GetActorRotation();
			ExitRotation.Yaw += DeltaYaw;

			UYanWorldTriggerInputComponent TriggerInput = UYanWorldTriggerInputComponent::FindOrAdd(OtherPawn);
			if (TriggerInput != nullptr)
			{
				TriggerInput.RequestTeleport(ExitLocation, ExitRotation, bLaunchOnTeleport, ExitForward * LaunchMultiplier);
			}

			if (bForceDirection && OtherPawn.Controller != nullptr)
			{
				FRotator ControlRotation = OtherPawn.Controller.GetControlRotation();
				ControlRotation.Yaw += DeltaYaw;
				OtherPawn.Controller.SetControlRotation(ControlRotation);
			}

			YanGameplayCue::ExecuteGameplayCuePredicted(OtherPawn, ActivateCueTag, MakeActivateCueParameters(OtherPawn));
		}

		if (HasAuthority())
		{
			ExecuteActivateCue(OtherPawn);
		}
	}

	private AYanTeleportActor PickDestination() const
	{
		TArray<AYanTeleportActor> ValidTargets;
		for (int Index = 0; Index < TeleportingTarget.Num(); ++Index)
		{
			if (TeleportingTarget[Index] != nullptr)
			{
				ValidTargets.Add(TeleportingTarget[Index]);
			}
		}

		return ValidTargets.Num() > 0 ? ValidTargets[Math::RandRange(0, ValidTargets.Num() - 1)] : nullptr;
	}

	// 玩家穿过本门的水平方向：触发区双面可进，以玩家所在的一侧决定穿门方向指向另一侧
	private FVector ComputeEntryDirection(APawn EntryPawn) const
	{
		const FVector PortalNormal = GetActorForwardVector();
		const bool bOnFrontSide = (EntryPawn.GetActorLocation() - GetActorLocation()).DotProduct(PortalNormal) > 0.0;
		return bOnFrontSide ? -PortalNormal : PortalNormal;
	}

	// 玩家网格投影到入口门面后相对门根的偏移（本门坐标系），竖直分量夹在 [0, 门高 - 余量]
	private FVector ComputeEntryRelativeOffset(APawn EntryPawn) const
	{
		USkeletalMeshComponent EntryMesh = USkeletalMeshComponent::Get(EntryPawn);
		const FVector EntryPosition = EntryMesh != nullptr ? EntryMesh.GetWorldLocation() : EntryPawn.GetActorLocation();

		const FVector PortalBase   = GetActorLocation();
		const FVector PortalNormal = GetActorForwardVector();
		FVector Offset = EntryPosition.PointPlaneProject(PortalBase, PortalNormal) - PortalBase;
		Offset.Z = Math::Clamp(Offset.Z, 0.0, TeleporterHeight - ExitHeightMargin);

		return Offset;
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
		CueParams.EffectCauser = this;
		CueParams.Instigator   = TargetPawn;
		return CueParams;
	}
}
