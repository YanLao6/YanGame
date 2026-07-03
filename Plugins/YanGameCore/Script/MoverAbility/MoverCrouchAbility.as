/**
 * UMoverCrouchAbility — 蹲伏输入技能
 *
 * 每次按下时切换 UCharacterMoverComponent 的蹲伏状态（Toggle）。
 * 不需要 ProduceMoverInput，蹲伏通过直接调用 MoverComp 接口实现。
 */
class UMoverCrouchAbility : UMoverInputAbility
{
	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		UCharacterMoverComponent CharMoverComp = Cast<UCharacterMoverComponent>(CachedMoverComp);
		if (CharMoverComp == nullptr)
		{
			EndAbility();
			return;
		}

		if (CharMoverComp.IsCrouching())
		{
			CharMoverComp.UnCrouch();
		}
		else
		{
			CharMoverComp.Crouch();
		}

		// 保持激活状态直到按键松开，防止 Repeating Input 导致连续触发 Toggle
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
	//~End UMoverInputAbility Interface
}
