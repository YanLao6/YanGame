/**
 * UMoverSlideAbility — 滑铲/蹲伏输入技能（同一按键）
 *
 * 按住期间同时表达两种意图：bWantsToSlide 写入 FYanCharacterInputs，
 * bWantsToCrouch 由 UChaosCharacterMoverComponent 每帧自动打包。
 * 二者各自由 sim 内的 Transition 裁决——速度足够时 CharacterSlideCheck 进入滑铲，
 * 任何情况下 ChaosCharacterCrouchCheck 都会压低胶囊，滑铲结束后自然过渡为蹲伏。
 *
 * 采用「同时写意图」而非在本技能内按速度分流：阈值只存在于 sim 侧一处，
 * 不会因两端阈值不一致而出现既不滑铲也不蹲伏的空档。
 */
class UMoverSlideAbility : UMoverInputAbility
{
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
		FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bWantsToSlide = bSlidePressed;
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
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
