/**
 * UMoverGrapplingAbility — 钩锁输入技能
 *
 * 状态机：Idle → Firing（AYanHookProjectile 飞行中）→ Hooked（激活 GrapplingMoveLogic）
 *
 * 激活：从摄像机位置沿前向 Spawn AYanHookProjectile 并 Launch。
 *       OnHookHit 命中后缓存锚点并把 bIsGrapplingActive 置为持续 true。
 *       OnHookMissed 超出射程后结束技能并清理 Projectile 引用。
 * 结束：按键松开（K2_InputReleased）或钩锁持续超过 MaxGrapplingDurationSeconds 时
 *       调用 K2_EndAbility，由 EndAbility 统一销毁 Projectile 并重置状态。
 *
 * 本技能只写输入、不直接切换模式：每帧向 FYanCharacterInputs 写入 bIsGrapplingActive
 * 与 GrappleAnchorPoint。进入由 UChaosGrapplingEnterCheck、退出由 UChaosGrapplingExitCheck
 * 在 sim 内依据 bIsGrapplingActive 对称裁决，保证服务器 re-simulation 与客户端一致。
 */
class UMoverGrapplingAbility : UMoverInputAbility
{
	default AbilityTags.AddTag(GameplayTags::Ability_Mover_Grappling);

	/** 钩锁头 Actor 类（须在 Blueprint 子类中指定） */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling")
	TSubclassOf<AYanHookProjectile> HookProjectileClass;

	/** 钩锁头飞行速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling", meta = (ClampMin = "100"))
	float HookTravelSpeed = 2000.f;

	/** 钩锁最大持续时间（秒），超出后自动结束技能 */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling", meta = (ClampMin = "0.1"))
	float MaxGrapplingDurationSeconds = 2.f;

	private bool               bIsHooked              = false;
	private FVector            AnchorPoint;
	private AYanHookProjectile PendingProjectile      = nullptr;
	// SimTimeMs 是帧步长（DeltaTimeMS）而非绝对时间戳，须自行累加
	private float              HookedTimeAccumSeconds = 0.f;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bIsHooked              = false;
		PendingProjectile      = nullptr;
		HookedTimeAccumSeconds = 0.f;

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
		HookedTimeAccumSeconds = 0.f;
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bIsGrapplingActive = bIsHooked;

		if (bIsHooked)
		{
			// SimTimeMs 是本帧步长（DeltaTimeMS），累加得到钩锁持续时间
			HookedTimeAccumSeconds += SimTimeMs * 0.001f;
			if (HookedTimeAccumSeconds >= MaxGrapplingDurationSeconds)
			{
				EndAbility();
				return;
			}

			// 锚点进入视点后半球（与前向夹角 > 90°）时终止钩锁
			FVector CamLocation, CamForward;
			if (YanMoverAngelscript::GetOwnerViewLocationAndForward(MoverComp, CamLocation, CamForward))
			{
				FVector ToAnchor = (AnchorPoint - CamLocation).GetSafeNormal();
				if (ToAnchor.DotProduct(CamForward) < 0.f)
				{
					EndAbility();
					return;
				}
			}

			YanInputs.GrappleAnchorPoint = AnchorPoint;
		}
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface

	/** AYanHookProjectile::OnHookHit 回调 — 命中后激活钩锁运动 */
	UFUNCTION()
	void HandleHookHit(AYanHookProjectile Projectile, const FHitResult&in Hit)
	{
		bIsHooked   = true;
		AnchorPoint = Hit.ImpactPoint;
		// 命中只置持续标志 bIsGrapplingActive（下一次 ProduceMoverInput 写入）；
		// 进入由 UChaosGrapplingEnterCheck、退出由 UChaosGrapplingExitCheck 在 sim 内对称裁决。
	}

	/** AYanHookProjectile::OnHookMissed 回调 — Projectile 自毁后结束技能 */
	UFUNCTION()
	private void HandleHookMissed(AYanHookProjectile Projectile)
	{
		EndAbility();
	}
}
