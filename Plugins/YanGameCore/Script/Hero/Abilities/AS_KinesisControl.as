/**
 * UAS_KinesisControl — 念力控制会话。
 *
 * 按住侧键进入会话、松开结束。会话期间玩家的两路操作合成一次推拉：
 *   转动视角  → 屏幕平面方向（上下左右）
 *   左右键    → 前后方向，左键推离、右键拉近，两键并按或都不按则无前后分量
 *
 * 按下瞬间锁定准心所指的可控物，此刻指向最明确；
 * 之后转动视角只表达手势，不改目标。
 * 须按满 EngageThreshold 会话才成立，不足则视为误触，松手不施力。
 *
 * 目标须挂有 UYanKinesisTargetComponent，选取在屏幕空间进行：准心压住哪个指示器就选中谁。
 * 玩家看到的指示器与此处的选取出自同一次投影，所见即所得。
 *
 * 会话中瞄准另一个目标按 DestinationKey 可指定目的地，被控目标松手后径直朝它飞去，
 * 此时手势不再参与方向。目的地以 Actor 引用上行，由服务器按实时位置求方向，
 * 不采信客户端算好的向量：两端的目标位置本就存在延迟差。
 *
 * 手势取自 ControlRotation 的首尾差分而非原始鼠标输入：该量已计入玩家的
 * 灵敏度与 Y 轴反转设置，无需在此重复处理符号与缩放，也不随帧率抖动。
 * Yaw 增量即屏幕横向、Pitch 增量即屏幕纵向，两者均为正右／正上。
 *
 * 左右键按住状态以 ModifierPollInterval 轮询记录时刻，松手时回看
 * ModifierLookbackSeconds 之内是否按下过。玩家松开侧键与松开左右键难以严格同时，
 * 只查当前状态会漏掉先松修饰键的那次操作，故以时间窗判定而非即时状态。
 *
 * 手势编码与阈值判定交由 UYanKinesisAbility::EncodeAimScreenDirection，
 * 与其解码端成对维护；本类只负责采集与转发，不持有编码约定。
 *
 * 会话状态仅在本地端维护：手势与按键源于本地输入，专用服务器上无从得知，
 * 施力所需的一切随激活事件一并复制过去。
 *
 * 蓝图默认值需设置：
 *   PushKey / PullKey     → 承担前后控制的两个鼠标键
 *   DestinationKey        → 指定目的地的按键
 *   KinesisEventTag       → 推拉技能 AbilityTriggers 中配置的事件标签
 *   ActivationOwnedTags   → 须含 State.Kinesis.Controlling，
 *                           攻击类技能以该标签作 ActivationBlockedTags，
 *                           会话期间左右键才不会同时触发普通攻击
 */
class UAS_KinesisControl : UYanGameplayAbility
{
	/** 将目标推离施法者的修饰键，通常为鼠标左键 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	FKey PushKey;

	/** 将目标拉向施法者的修饰键，通常为鼠标右键 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	FKey PullKey;

	/** 指定目的地的按键，会话中瞄准另一个目标按下即锁定，通常为 E */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	FKey DestinationKey;

	/** 修饰键状态的轮询间隔（秒），须小于 ModifierLookbackSeconds 才不会漏采 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0.001"))
	float ModifierPollInterval = 0.02f;

	/** 松手时向前回看的时长（秒），此窗口内按下过即计入前后意图 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0"))
	float ModifierLookbackSeconds = 0.1f;

	/** 激活推拉技能的事件标签，须与 UYanKinesisAbility 蓝图子类的 AbilityTriggers 一致 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (Categories = "Event.Kinesis"))
	FGameplayTag KinesisEventTag = FGameplayTag::RequestGameplayTag(n"Event.Kinesis.Release");

	/** 构成有效手势的最小视角变化量（度），低于此值视为玩家未划动鼠标 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0"))
	float MinGestureDegrees = 3.0f;

	/** 会话成立所需的按住时长（秒），不足此值松手视为误触 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0"))
	float EngageThreshold = 0.2f;

	/** 锁定目标的距离上限（cm），须与推拉技能的 MaxDistance 一致，否则本地选中的目标会被服务器驳回 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0", ForceUnits = "cm"))
	float TargetMaxDistance = 1000.0f;

	/**
	 * 可选中的屏幕半径，以视口高度为基准的比例：0.1 即准心周围视口高度十分之一的范围内可选中。
	 * 与推拉技能的角度参数无须对齐——服务器只做宽松复核，不复现本地的屏幕选取。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", Meta = (ClampMin = "0", ClampMax = "1"))
	float TargetScreenRadiusRatio = 0.1f;

	// 与 UYanKinesisAbility 的解码端硬编码一致，不开放配置以免两端漂移
	private FGameplayTag PushModifierTag = FGameplayTag::RequestGameplayTag(n"Event.Kinesis.Modifier.Push");
	private FGameplayTag PullModifierTag = FGameplayTag::RequestGameplayTag(n"Event.Kinesis.Modifier.Pull");

	private FRotator GestureStartRotation;
	private bool bTrackingGesture = false;

	private FTimerHandle ModifierPollTimer;
	private float LastPushDownTime = -1.0f;
	private float LastPullDownTime = -1.0f;

	private FTimerHandle EngageTimer;
	private bool bEngaged = false;
	private AActor ControlledTarget;

	private AActor LockedDestination;
	private bool bDestinationKeyDown = false;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		LastPushDownTime    = -1.0f;
		LastPullDownTime    = -1.0f;
		bEngaged            = false;
		ControlledTarget    = nullptr;
		LockedDestination   = nullptr;
		bDestinationKeyDown = false;

		APlayerController OwningController = GetOwningPlayerController();
		bTrackingGesture = IsLocallyControlled() && OwningController != nullptr;

		if (!bTrackingGesture)
		{
			return;
		}

		GestureStartRotation = OwningController.GetControlRotation();

		ControlledTarget = UYanKinesisAbility::FindKinesisTarget(
			Cast<APawn>(GetAvatarActorFromActorInfo()), TargetMaxDistance, TargetScreenRadiusRatio);

		ModifierPollTimer = System::SetTimer(this, n"PollModifierKeys", ModifierPollInterval, true);
		EngageTimer = System::SetTimer(this, n"HandleEngaged", EngageThreshold, false);
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		if (bEngaged)
		{
			SendKinesisEvent();
		}

		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void OnEndAbility(bool bWasCancelled)
	{
		System::ClearAndInvalidateTimerHandle(ModifierPollTimer);
		System::ClearAndInvalidateTimerHandle(EngageTimer);

		// 与 HandleEngaged 中的开始通知成对，未成立的会话不曾通知过，也就无须收尾
		if (bEngaged)
		{
			UYanKinesisAbility::NotifyKinesisControlState(ControlledTarget, GetAvatarActorFromActorInfo(), false);
			bEngaged = false;
		}
	}
	//~End UGameplayAbility Interface

	// 按满阈值方视为有意进入控制，此前松手不产生任何效果
	UFUNCTION()
	private void HandleEngaged()
	{
		bEngaged = true;

		UYanKinesisAbility::NotifyKinesisControlState(ControlledTarget, GetAvatarActorFromActorInfo(), true);

		OnControlEngaged(ControlledTarget);
	}

	/**
	 * 蓝图实现：会话正式成立时的表现。
	 * 目标为空表示准心未压住任何可控物，此时松手不会施力，起手表现应据此收敛。
	 */
	UFUNCTION(BlueprintEvent, Category = "Kinesis")
	void OnControlEngaged(AActor LockedTarget)
	{
	}

	UFUNCTION()
	private void PollModifierKeys()
	{
		APlayerController OwningController = GetOwningPlayerController();
		if (OwningController == nullptr)
		{
			return;
		}

		const float Now = System::GetGameTimeInSeconds();

		if (OwningController.IsInputKeyDown(PushKey))
		{
			LastPushDownTime = Now;
		}

		if (OwningController.IsInputKeyDown(PullKey))
		{
			LastPullDownTime = Now;
		}

		PollDestinationKey(OwningController);
	}

	// 目的地在按下那一刻取一次，按住不重选：轮询到的持续按下状态须收敛为上升沿
	private void PollDestinationKey(APlayerController OwningController)
	{
		const bool bDownNow = OwningController.IsInputKeyDown(DestinationKey);

		if (bDownNow && !bDestinationKeyDown)
		{
			AActor Picked = UYanKinesisAbility::FindKinesisTarget(
				Cast<APawn>(GetAvatarActorFromActorInfo()), TargetMaxDistance, TargetScreenRadiusRatio, ControlledTarget);

			// 未瞄准任何目标时保留上一次的锁定，避免手抖扫过空地就丢失目的地
			if (Picked != nullptr)
			{
				LockedDestination = Picked;
				OnDestinationLocked(Picked);
			}
		}

		bDestinationKeyDown = bDownNow;
	}

	/** 蓝图实现：锁定目的地时的表现，如在目标上标出指示。 */
	UFUNCTION(BlueprintEvent, Category = "Kinesis")
	void OnDestinationLocked(AActor Destination)
	{
	}

	// 事件在本地 ASC 上广播，推拉技能为 LocalPredicted，激活请求与本负载一并转发服务器
	private void SendKinesisEvent()
	{
		UAbilitySystemComponent ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC == nullptr || !KinesisEventTag.IsValid())
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventMagnitude = ResolveGestureAngle();
		Payload.Target         = ControlledTarget;
		Payload.OptionalObject = LockedDestination;
		AppendModifierTags(Payload.InstigatorTags);

		ASC.SendGameplayEvent(KinesisEventTag, Payload);
	}

	// 两键并存时前后意图相互抵消，一个都不加，交由解码端归零径向分量
	private void AppendModifierTags(FGameplayTagContainer& OutTags) const
	{
		const float Now = System::GetGameTimeInSeconds();
		const bool bPushHeld = WasModifierHeldRecently(LastPushDownTime, Now);
		const bool bPullHeld = WasModifierHeldRecently(LastPullDownTime, Now);

		if (bPushHeld == bPullHeld)
		{
			return;
		}

		OutTags.AddTag(bPushHeld ? PushModifierTag : PullModifierTag);
	}

	private bool WasModifierHeldRecently(float LastDownTime, float Now) const
	{
		return LastDownTime >= 0.0f && (Now - LastDownTime) <= ModifierLookbackSeconds;
	}

	// 未追踪手势时返回负值，推拉技能据此退化为无平面分量
	private float ResolveGestureAngle() const
	{
		APlayerController OwningController = GetOwningPlayerController();
		if (!bTrackingGesture || OwningController == nullptr)
		{
			return -1.0f;
		}

		const FRotator CurrentRotation = OwningController.GetControlRotation();
		const FVector2D Gesture(
			NormalizeDegrees(CurrentRotation.Yaw - GestureStartRotation.Yaw),
			NormalizeDegrees(CurrentRotation.Pitch - GestureStartRotation.Pitch));

		return UYanKinesisAbility::EncodeAimScreenDirection(Gesture, MinGestureDegrees);
	}

	// 视角跨越 ±180 边界时裸差值会突变一整圈，收敛回 (-180, 180] 才是真实转动量
	private float NormalizeDegrees(float Degrees) const
	{
		float Result = Degrees;

		while (Result > 180.0f)
		{
			Result -= 360.0f;
		}

		while (Result <= -180.0f)
		{
			Result += 360.0f;
		}

		return Result;
	}

	private APlayerController GetOwningPlayerController() const
	{
		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		return OwningPawn != nullptr ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
	}
}
