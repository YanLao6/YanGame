/**
 * UMoverGrapplingAbility — 钩锁输入技能
 *
 * 状态机：Idle → Firing（AYanHookProjectile 飞行中）→ Hooked（写入牵引意图）
 *
 * 激活：从摄像机位置沿前向 Spawn AYanHookProjectile 并 Launch。
 *       OnHookHit 命中后缓存着力点，命中的若是 Pawn 则一并缓存对方的 MoverComponent。
 *       OnHookMissed 超出射程后结束技能并清理 Projectile 引用。
 * 结束：按键松开（K2_InputReleased）、钩锁持续超过 MaxGrapplingDurationSeconds、
 *       被钩者失效，或 ExitCheck 判定钩子脱离视角水平前方时结束技能，
 *       由 OnEndAbility 统一销毁 Projectile 并重置状态。
 *       ProduceMoverInput 内裁定的结束只登记请求，实际结束推迟一帧，原因见 RequestEndAbility。
 *
 * 本技能只写输入、不直接切换模式：每帧向 FYanCharacterInputs 写入牵引意图与汇合点。
 * 发起者一侧的进入由 UChaosGrapplingEnterCheck、退出由 UChaosGrapplingExitCheck
 * 在 sim 内依据牵引意图与输入视角 Yaw 对称裁决，保证服务器 re-simulation 与客户端一致。
 *
 * 钩住 Pawn 时谁动由双方的离地状态裁定：站在地上的一方被悬空的一方钩住时不动，
 * 同为地面或同为空中则双方向中点靠拢。裁定逐帧重做，离地状态中途变化即改变牵引走向。
 * 被钩者一侧的牵引走不了输入包（其输入包只有它自己的本地端能写），
 * 改由服务器的 UYanGrappleAuthorityComponent 读 bGrapplePullsTarget 后下发。
 *
 * 双方须同时起动，故发起者的牵引在钩中 Pawn 时延后 TargetPullSyncDelayMs：
 * 发起者这一侧本地预测、当帧即生效，被钩者那一侧却要先上传服务器再下发回来。
 * 不等这一段，发起者会先动，双方看上去不是被同一根绳子拽着。
 * 该判定在 game thread 做即可——判定结果由 bIsGrapplingActive 这个位本身上传，
 * 服务器重放读到的是同一个值，无须像贴身抓取那样再上传一个共同时刻。
 */
class UMoverGrapplingAbility : UMoverInputAbility
{
	default AbilityTags.AddTag(GameplayTags::Ability_Mover_Grappling);

	/** 钩锁头 Actor 类（须在 Blueprint 子类中指定） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Grappling")
	TSubclassOf<AYanHookProjectile> HookProjectileClass;

	/** 钩锁头飞行速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Grappling", meta = (ClampMin = "100"))
	float HookTravelSpeed = 2000.f;

	/** 钩锁最大持续时间（秒），超出后自动结束技能。含等待对方起动的那一段 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Grappling", meta = (ClampMin = "0.1"))
	float MaxGrapplingDurationSeconds = 2.f;

	/**
	 * 判定双方互拉之后，发起者延后起拉的时长（毫秒）。
	 *
	 * 发起者的牵引本地预测、当帧生效，被钩者的牵引却要先上传服务器、再经 sim action
	 * 下发回来，两者相差一个往返加上 sim action 的调度延迟。不等这一段，发起者会先动，
	 * 双方看上去不是被同一根绳子拽着。
	 *
	 * 取值须大于该往返；配得过大则表现为钩中后有一段明显的滞涩。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Mover|Grappling", meta = (ClampMin = "0", ForceUnits = "ms"))
	float TargetPullSyncDelayMs = 300.f;

	private bool               bIsHooked              = false;
	private bool               bEndRequested          = false;
	private FVector            AnchorPoint;
	private AYanHookProjectile PendingProjectile      = nullptr;
	// SimTimeMs 是帧步长（DeltaTimeMS）而非绝对时间戳，须自行累加
	private float              HookedTimeAccumSeconds = 0.f;

	// 钩住的 Pawn 及其 Mover；钩住墙壁时两者为 null，牵引退化为朝固定锚点的单向拉扯
	private APawn                    HookedPawn      = nullptr;
	private UCharacterMoverComponent HookedMoverComp = nullptr;

	/**
	 * 发起者获准起拉的绝对仿真时刻（毫秒），负值表示尚未算定。
	 *
	 * 在「首次向服务器上传牵引被钩者的意图」那一帧取定，而非命中那一帧：只有对方也要动，
	 * 才存在需要对齐的两个起动时刻。对方站在地上不被拉时干等没有意义，
	 * 离地状态中途变化而首次形成互拉，同样由这个条件抓住。
	 *
	 * 时间基准取 UMoverComponent::GetLastTimeStep——它处于服务器仿真时间轴，
	 * 客户端预测时亦在同一轴上。
	 */
	private float64 SelfPullStartSimTimeMs = -1.0;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void OnMoverComponentSet(UMoverComponent OldMoverComp, UMoverComponent NewMoverComp)
	{
		if (IsValid(OldMoverComp))
		{
			OldMoverComp.OnPostFinalize.UnbindObject(this);
			OldMoverComp.OnMovementModeChanged.UnbindObject(this);
		}
		if (IsValid(NewMoverComp))
		{
			NewMoverComp.OnPostFinalize.AddUFunction(this, n"HandlePostFinalize");
			NewMoverComp.OnMovementModeChanged.AddUFunction(this, n"HandleMovementModeChanged");
		}
	}

	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bIsHooked              = false;
		bEndRequested          = false;
		PendingProjectile      = nullptr;
		HookedTimeAccumSeconds = 0.f;
		HookedPawn             = nullptr;
		HookedMoverComp        = nullptr;
		SelfPullStartSimTimeMs = -1.0;

		// 投射物为本地受控端的 cosmetic + 命中探测器；服务器不 spawn，
		// 仅依据复制来的输入（bIsGrapplingActive + GrappleAnchorPoint）驱动钩锁模式。
		// 非本地受控端保持技能激活，由复制输入与 EndAbility 复制驱动生命周期。
		APawn OwnerPawn = Cast<APawn>(CachedMoverComp != nullptr ? CachedMoverComp.GetOwner() : nullptr);
		if (OwnerPawn == nullptr || !OwnerPawn.IsLocallyControlled())
		{
			return;
		}

		FVector CameraLocation, CameraForward;
		AYanHookProjectile Projectile = nullptr;

		if (HookProjectileClass != nullptr && CachedMoverComp != nullptr)
		{
			if (YanMoverAngelscript::GetOwnerViewLocationAndForward(CachedMoverComp, CameraLocation, CameraForward))
			{
				FRotator SpawnRot = CameraForward.ToOrientationRotator();
				Projectile = Cast<AYanHookProjectile>(YanMoverAngelscript::SpawnHookProjectile(CachedMoverComp, HookProjectileClass, CameraLocation, SpawnRot));
				if (Projectile != nullptr)
				{
					PendingProjectile = Projectile;
					Projectile.OnHookHit.AddUFunction(this, n"HandleHookHit");
					Projectile.OnHookMissed.AddUFunction(this, n"HandleHookMissed");
					Projectile.Launch(CameraForward, HookTravelSpeed);

					return;
				}
			}
		}
		EndAbility();
	}


	UFUNCTION(BlueprintOverride)
	void OnEndAbility(bool bWasCancelled)
	{
		if (PendingProjectile != nullptr && IsValid(PendingProjectile))
		{
			PendingProjectile.DestroyActor();
		}
		PendingProjectile      = nullptr;
		bIsHooked              = false;
		bEndRequested          = false;
		HookedTimeAccumSeconds = 0.f;
		HookedPawn             = nullptr;
		HookedMoverComp        = nullptr;
		SelfPullStartSimTimeMs = -1.0;
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		// 已登记结束的这一帧起不再写入牵引意图，sim 侧当帧即由 ExitCheck 退出钩锁
		if (bEndRequested)
		{
			return;
		}

		FYanCharacterInputs YanInputs  = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bIsGrapplingActive   = false;
		YanInputs.bGrapplePullsTarget  = false;

		if (bIsHooked)
		{
			// SimTimeMs 是本帧步长（DeltaTimeMS），累加得到钩锁持续时间
			HookedTimeAccumSeconds += SimTimeMs * 0.001f;
			if (HookedTimeAccumSeconds >= MaxGrapplingDurationSeconds)
			{
				RequestEndAbility();
				return;
			}

			// 被钩者中途失效（死亡、离开关卡）后钩锁失去着力点
			if (HookedMoverComp != nullptr && !IsValid(HookedPawn))
			{
				RequestEndAbility();
				return;
			}

			// 钩住墙壁时着力点固定，发起者单向被拉过去
			bool    bPullSelf   = true;
			bool    bPullTarget = false;
			FVector MeetPoint   = AnchorPoint;
			FVector HookLocation = AnchorPoint;

			if (HookedMoverComp != nullptr)
			{
				UCharacterMoverComponent SelfMoverComp = Cast<UCharacterMoverComponent>(MoverComp);
				if (SelfMoverComp == nullptr)
				{
					RequestEndAbility();
					return;
				}

				const bool bSelfOnGround   = SelfMoverComp.IsOnGround();
				const bool bTargetOnGround = HookedMoverComp.IsOnGround();

				// 站在地上的一方被悬空的一方钩住时不动，另一方被拉过来；
				// 同为地面或同为空中则双方都动
				bPullSelf   = !(bSelfOnGround && !bTargetOnGround);
				bPullTarget = !(bTargetOnGround && !bSelfOnGround);

				const FVector SelfLocation   = MoverComp.GetOwner().GetActorLocation();
				const FVector TargetLocation = HookedPawn.GetActorLocation();
				HookLocation = TargetLocation;

				// 不动的一方本就在汇合点上，故双方朝同一点收敛即涵盖全部三种情形
				if (bPullSelf && bPullTarget)
				{
					MeetPoint = (SelfLocation + TargetLocation) * 0.5f;
				}
				else if (bPullSelf)
				{
					MeetPoint = TargetLocation;
				}
				else
				{
					MeetPoint = SelfLocation;
				}

				YanInputs.GrappleTargetLocation = TargetLocation;

				bPullSelf = bPullSelf && ShouldSelfPullYet(MoverComp, bPullTarget);
			}

			YanInputs.bIsGrapplingActive  = bPullSelf;
			YanInputs.bGrapplePullsTarget = bPullTarget;
			YanInputs.GrappleAnchorPoint  = MeetPoint;
			YanInputs.GrappleHookLocation = HookLocation;
		}
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface

	// 仿真事件回到 game thread 后结束技能，避免退出模式后持续输入再次发起牵引。
	UFUNCTION()
	private void HandleMovementModeChanged(const FName&in PreviousMode, const FName&in NewMode)
	{
		if (bIsHooked && PreviousMode == n"Grappling" && NewMode == n"Falling" && IsValid(CachedMoverComp) && IsLocallyControlled())
		{
			FYanCharacterInputs YanInputs;
			if (YanMoverAngelscript::GetYanCharacterInputs(CachedMoverComp.GetLastInputCmd().InputCollection, YanInputs)
				&& YanInputs.bIsGrapplingActive)
			{
				RequestEndAbility();
			}
		}
	}

	// 仅拉动被钩者或等待同步起拉时，发起者尚未进入 Grappling，也须检查钩子的有效方向。
	UFUNCTION()
	private void HandlePostFinalize(const FMoverSyncState&in SyncState, const FMoverAuxStateContext&in AuxState)
	{
		if (!bIsHooked || bEndRequested || !IsValid(CachedMoverComp) || !IsLocallyControlled())
		{
			return;
		}

		FYanCharacterInputs YanInputs;
		if (YanMoverAngelscript::GetYanCharacterInputs(CachedMoverComp.GetLastInputCmd().InputCollection, YanInputs)
			&& (YanInputs.bIsGrapplingActive || YanInputs.bGrapplePullsTarget)
			&& UChaosGrapplingExitCheck::IsHookOutsideForwardHemisphere(SyncState, CachedMoverComp.GetLastInputCmd()))
		{
			RequestEndAbility();
		}
	}

	/**
	 * 登记结束请求，真正的 EndAbility 推迟到下一帧的 game thread。
	 *
	 * ProduceMoverInput 运行在 Chaos 的物理输入注入窗口内——该窗口位于本帧数据封包之后、
	 * 外部时间戳递增之前，其中销毁 Actor 会以过期时间戳写入 Chaos 的忽略碰撞缓冲区，
	 * 令下一次封包的时间戳校验失败。就地结束技能还是不可回滚的副作用，
	 * 而该回调在 re-simulation 时会被重放。故此处只置位，销毁与状态重置交给 OnEndAbility。
	 *
	 * 重复登记无副作用；技能若被其他来源先行结束，OnEndAbility 会清掉本位，
	 * 定时器回调届时不再触发结束。
	 */
	private void RequestEndAbility()
	{
		if (bEndRequested)
		{
			return;
		}

		bEndRequested = true;
		System::SetTimerForNextTick(this, "FlushEndRequest");
	}

	UFUNCTION()
	private void FlushEndRequest()
	{
		if (!bEndRequested)
		{
			return;
		}

		bEndRequested = false;
		EndAbility();
	}

	/**
	 * 发起者本帧是否已获准起拉：等到被钩者那一侧的牵引也该生效了，双方才一同起动。
	 *
	 * 起拉时刻只在钩中 Pawn 后的第一帧算定一次，此后不再改动。若改为每次形成互拉时重新对齐，
	 * 已经被拉着飞的发起者会在对方离地的那一刻二次停顿，比不对齐更难看。
	 */
	private bool ShouldSelfPullYet(UMoverComponent MoverComp, bool bPullTarget)
	{
		const float64 NowSimTimeMs = MoverComp.GetLastTimeStep().BaseSimTimeMs;

		if (SelfPullStartSimTimeMs < 0.0)
		{
			// 首帧对方就不该动时无从对齐，也不必等：发起者当帧即可被拉走
			SelfPullStartSimTimeMs = bPullTarget ? (NowSimTimeMs + float64(TargetPullSyncDelayMs)) : NowSimTimeMs;
		}

		return NowSimTimeMs >= SelfPullStartSimTimeMs;
	}

	/** AYanHookProjectile::OnHookHit 回调 — 命中后激活钩锁运动 */
	UFUNCTION()
	void HandleHookHit(AYanHookProjectile Projectile, const FHitResult&in Hit)
	{
		bIsHooked   = true;
		AnchorPoint = Hit.ImpactPoint;

		// 钩中带 Mover 的 Pawn 时改为双方互拉；没有 Mover 的 Pawn 拉不动，按墙壁处理
		HookedPawn      = Cast<APawn>(Hit.GetActor());
		HookedMoverComp = HookedPawn != nullptr ? Cast<UCharacterMoverComponent>(UMoverComponent::Get(HookedPawn)) : nullptr;
		if (HookedMoverComp == nullptr)
		{
			HookedPawn = nullptr;
		}

		// 命中只置持续标志（下一次 ProduceMoverInput 写入）；
		// 进入由 UChaosGrapplingEnterCheck、退出由 UChaosGrapplingExitCheck 在 sim 内对称裁决。
	}

	/** AYanHookProjectile::OnHookMissed 回调 — Projectile 自毁后结束技能 */
	UFUNCTION()
	private void HandleHookMissed(AYanHookProjectile Projectile)
	{
		EndAbility();
	}
}
