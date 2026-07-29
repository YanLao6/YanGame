/**
 * GA_TogglePerspective — 切换第一/第三人称视角技能
 *
 * 激活时调用 AYanDefaultPawn::TogglePersonPerspective()，
 * 在第一人称与第三人称之间切换 SpringArm 臂长。
 * 推荐将 ActivationPolicy 设为 OnInputTriggered，绑定到切换视角输入动作。
 */
class UAS_TogglePerspective : UYanGameplayAbility
{
	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		// 提交技能（CD / 资源检查），失败则立即结束
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		// 取 Avatar，转型为 AYanDefaultPawn
		AYanDefaultPawn DefaultPawn = Cast<AYanDefaultPawn>(GetAvatarActorFromActorInfo());
		if (DefaultPawn == nullptr)
		{
			EndAbility();
			return;
		}

		// 在第一人称与第三人称之间切换（调整 SpringArm.TargetArmLength）
		DefaultPawn.TogglePersonPerspective();
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
	//~End UGameplayAbility Interface
}
