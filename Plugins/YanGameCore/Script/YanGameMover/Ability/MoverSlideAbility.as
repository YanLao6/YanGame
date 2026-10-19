/**
 * UMoverSlideAbility — 滑铲/蹲伏/脱墙输入技能（同一按键）
 *
 * 按住期间同时表达三种意图：bWantsToSlide 与 bWantsToDetachFromWall 写入 FYanCharacterInputs，
 * bWantsToCrouch 由 UChaosCharacterMoverComponent 每帧自动打包。
 * 三者各自由 sim 内的 Transition 裁决——速度足够时 CharacterSlideCheck 进入滑铲，
 * 任何情况下 ChaosCharacterCrouchCheck 都会压低胶囊，滑铲结束后自然过渡为蹲伏，
 * 附着墙面时 ChaosWallClimbExitCheck 据此主动脱墙转入下落。
 *
 * 采用「同时写意图」而非在本技能内按状态分流：阈值与状态判定只存在于 sim 侧一处，
 * 不会因两端不一致而出现哪种意图都不生效的空档。
 *
 * 本技能只产出意图，不负责表现。滑铲动作由 LocomotionSM 读取
 * UYanLocomotionAnimInstance::MoverIsSliding 驱动，不经 ASC 蒙太奇，
 * 因而不会与开火等技能蒙太奇争夺 ASC 的唯一蒙太奇槽位。
 */
class UMoverSlideAbility : UMoverInputAbility
{
	default ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
	private bool bSlidePressed = false;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bSlidePressed = true;
		SetCrouchIntent(true);
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		bSlidePressed = false;
		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void OnEndAbility(bool bWasCancelled)
	{
		// 技能被取消时也须解除蹲伏意图，否则胶囊会一直保持压低
		bSlidePressed = false;
		SetCrouchIntent(false);
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FYanCharacterInputs YanInputs = MoverData::GetOrAddYanInputs(InputCmd);
		YanInputs.bWantsToSlide = bSlidePressed;
		YanInputs.bWantsToDetachFromWall = bSlidePressed;
		MoverData::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface

	// 蹲伏意图交给 MoverComponent 持有，其 ProduceInput 每帧将其打包进输入命令
	private void SetCrouchIntent(bool bWantsToCrouch)
	{
		UCharacterMoverComponent CharMoverComp = Cast<UCharacterMoverComponent>(CachedMoverComp);
		if (CharMoverComp == nullptr)
		{
			return;
		}

		if (bWantsToCrouch)
		{
			CharMoverComp.Crouch();
		}
		else
		{
			CharMoverComp.UnCrouch();
		}
	}
}
