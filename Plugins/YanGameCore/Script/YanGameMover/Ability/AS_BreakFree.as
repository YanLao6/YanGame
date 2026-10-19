/**
 * UAS_CharacterBreakFree — 挣脱贴身（点按触发）
 *
 * 被抓者退出定身的唯一途径。解除分两层，路径各不相同：
 *  - 移动上的钉制由 FYanLayeredMove_Clinch 承载，凭本地输入包中的 bWantsToBreakFree
 *    当帧解除。走输入包而非服务器往返，按键才没有延迟；服务器重模拟读到同一输入位，
 *    在同一模拟步得到相同结果，两端不会分歧。
 *  - 技能上的封锁由 Status.Grabbed 承载，须由服务器移除，故走 GAS 的常规激活流程。
 *
 * 本技能自身不判断能否激活：眩晕期间连它一并封锁、被抓期间只放行它，
 * 这些一律由 UModularAbilityTagRelationshipMapping 集中配置。
 */
class UAS_CharacterBreakFree : UMoverInputAbility
{
	// 待投递的挣脱意图：须在 ProduceMoverInput 里写入输入包，故 Activate 不直接解除
	private bool bPendingBreakFree = false;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (CachedMoverComp == nullptr)
		{
			return;
		}

		bPendingBreakFree = true;

		// 两端都会走到这里：客户端上是预测移除，服务器上才是权威结果
		MoverGameplay::ReleaseGrabbedState(CachedMoverComp.GetOwner());
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		// 每帧显式写入而非仅在有脉冲时写：脉冲位必须在投递后的次帧确定为 false，
		// 否则输入命令若被跨帧复用，残留的脉冲会让下一次被抓一诞生就被解除
		FYanCharacterInputs YanInputs = MoverData::GetOrAddYanInputs(InputCmd);
		YanInputs.bWantsToBreakFree = bPendingBreakFree;
		bPendingBreakFree = false;
		MoverData::CommitYanInputs(InputCmd, YanInputs);
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
	//~End UMoverInputAbility Interface
}
