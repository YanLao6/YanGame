/**
 * UGA_Sprint — 疾跑输入技能（点按锁定）
 *
 * 疾跑为锁定式手感：按下一次即进入「待疾跑」状态，之后每帧持续投递
 * FYanCharacterInputs::bIsSprintJustPressed，直到 sim 内的 UChaosCharacterSprintCheck
 * 依移动意愿裁决起跑、FYanSprintModifier 挂上 Mover.IsSprinting 标签，本技能才消费该状态。
 * 站立、蹲伏或空中按下不会白按——待疾跑一直保持到维持条件成立的那一帧。
 *
 * 松开按键不退出疾跑——技能结束只是不再接收新的按下，并不表达「停止疾跑」的意图。
 * 退出条件（停步、转身背离视线、离地、蹲伏）只存在于 sim 侧一处，本技能不复制。
 */
class UGA_Sprint : UMoverInputAbility
{
	// 锁定只拦截起跑，已在疾跑中的不受影响
	default ActivationBlockedTags.AddTag(GameplayTags::Status_SprintLocked);
	// 待疾跑状态；只在观察到疾跑真正开始时清除，跨越技能激活期存活
	private bool bPendingSprintStart = false;

	// 由 FYanSprintModifier 在激活期间对外暴露，作为「疾跑已开始」的唯一可观测面
	private FGameplayTag SprintingTag = FGameplayTag::RequestGameplayTag(n"Mover.IsSprinting");

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		bPendingSprintStart = true;
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		// 不清除待疾跑状态：基类保证 ProduceMoverInput 在非激活期同样每帧调用，状态由此延续
		EndAbility();
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		// 先消费再写入：观察到起跑的这一帧即写 false，避免残留标志在疾跑刚退出时立即重新进入
		if (bPendingSprintStart && MoverComp != nullptr && MoverComp.HasGameplayTag(SprintingTag, true))
		{
			bPendingSprintStart = false;
		}

		// 每帧显式写入：输入命令包每帧重建，无意图时须写零值
		FYanCharacterInputs YanInputs  = MoverData::GetOrAddYanInputs(InputCmd);
		YanInputs.bIsSprintJustPressed = bPendingSprintStart;
		MoverData::CommitYanInputs(InputCmd, YanInputs);
	}
	//~End UMoverInputAbility Interface
}
