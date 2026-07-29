/**
 * UGA_Sprint — 疾跑输入技能（点按锁定）
 *
 * 疾跑为锁定式手感：本技能只在按下当帧投递一次进入脉冲
 * FYanCharacterInputs::bIsSprintJustPressed，之后能否维持疾跑全部交由 sim 内的
 * UChaosCharacterSprintCheck 依移动意愿裁决，速度覆盖由 FYanSprintModifier 承载。
 *
 * 松开按键不退出疾跑——技能结束只是停止投递脉冲，并不表达「停止疾跑」的意图。
 * 退出条件（停步、转身背离视线、离地、蹲伏）只存在于 sim 侧一处，本技能不复制。
 */
class UGA_Sprint : UMoverInputAbility
{
	// 待投递的进入脉冲；须在输入产出时机消费，故激活时不直接写输入
	private bool bPendingSprintStart = false;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bPendingSprintStart = true;
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		// 不清除待投递脉冲：点按过快时松手会早于本帧的输入产出，清除会吞掉这次疾跑
		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		// 每帧显式写入而非仅在有脉冲时写：脉冲位必须在投递后的次帧确定为 false，
		// 否则输入命令若被跨帧复用，残留的脉冲会让疾跑刚退出就立即重新进入
		FYanCharacterInputs YanInputs   = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bIsSprintJustPressed  = bPendingSprintStart;
		bPendingSprintStart             = false;
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface
}
