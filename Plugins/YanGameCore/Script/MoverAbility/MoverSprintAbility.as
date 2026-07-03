/**
 * UMoverSprintAbility — 冲刺输入技能
 *
 * 管理冲刺的帧间状态，每帧将 bIsSprintJustPressed / bIsSprintPressed
 * 写入 FYanCharacterInputs，供 CharacterSprintCheck Transition 读取。
 */
class UMoverSprintAbility : UMoverInputAbility
{
	private bool bSprintPressed     = false;
	private bool bSprintJustPressed = false;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bSprintPressed     = true;
		bSprintJustPressed = true;
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		bSprintPressed = false;
		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bIsSprintJustPressed = bSprintJustPressed;
		YanInputs.bIsSprintPressed     = bSprintPressed;
		bSprintJustPressed             = false;
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface
}
