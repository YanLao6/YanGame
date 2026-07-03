/**
 * UMoverSlideAbility — 滑铲输入技能
 *
 * 管理滑铲的持续按键状态，每帧将 bWantsToSlide 写入 FYanCharacterInputs。
 * 仅在 Walking 或 Sliding 模式下写入，防止 Falling 等模式意外触发 CharacterSlideCheck。
 */
class UMoverSlideAbility : UMoverInputAbility
{
	private bool bSlidePressed = false;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bSlidePressed = true;
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		bSlidePressed = false;
		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		if (MoverComp != nullptr)
		{
			FName CurrentMode = MoverComp.GetMovementModeName();
			YanInputs.bWantsToSlide = bSlidePressed;
		}
		else
		{
			YanInputs.bWantsToSlide = false;
		}
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface
}
