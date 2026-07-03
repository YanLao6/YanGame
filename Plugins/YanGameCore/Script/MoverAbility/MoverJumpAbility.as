/**
 * UMoverJumpAbility — 跳跃输入技能
 *
 * 管理跳跃的帧间状态，每帧将 bIsJumpJustPressed / bIsJumpPressed
 * 写入 FCharacterDefaultInputs，供 Mover 跳跃 Transition 读取。
 */
class UMoverJumpAbility : UMoverInputAbility
{
	private bool bJumpPressed     = false;
	private bool bJumpJustPressed = false;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bJumpJustPressed = true;
		bJumpPressed     = true;
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		bJumpPressed = false;
		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FCharacterDefaultInputs Inputs = YanMoverAngelscript::GetOrAddDefaultInputs(InputCmd);
		Inputs.bIsJumpJustPressed = bJumpJustPressed;
		Inputs.bIsJumpPressed     = bJumpPressed;
		bJumpJustPressed          = false;
		YanMoverAngelscript::CommitDefaultInputs(InputCmd, Inputs);
	}
	//~End UMoverInputAbility Interface
}
