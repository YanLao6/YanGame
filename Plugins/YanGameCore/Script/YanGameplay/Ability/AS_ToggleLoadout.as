/**
 * UAS_ToggleLoadout — 特长选取界面开关。
 *
 * 本身不产生战斗效果，只把一次按键翻转界面的开合：按下开，再按下关。
 *
 * 技能不驻留：每次按键都在同一帧内完成开合并结束，故「当前是否已打开」不靠技能状态
 * 判断，而是回头问控件自己还在不在图层上。
 *
 * 无待选槽位时也允许打开，界面此时是一份只读的特长清单。禁止打开会让玩家无从确认
 * 自己已经选了什么。
 *
 * 压入 UI.Layer.GameMenu 而非 Menu：选特长发生在对局进行中。界面本身取 GameAndMenu
 * 输入模式，玩家可以边跑边选，本技能的按键也因此仍能抵达、用于关闭界面。
 *
 * 蓝图默认值需设置：
 *   SelectWidgetClass → 继承 UYanLoadoutSelectWidget 的 UMG 蓝图
 */
class UAS_ToggleLoadout : UYanGameplayAbility
{
	/** 特长选取界面的 UMG 类，需继承 UYanLoadoutSelectWidget */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TSubclassOf<UYanLoadoutSelectWidget> SelectWidgetClass;

	/** 界面压入的 UI 层 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", Meta = (Categories = "UI.Layer"))
	FGameplayTag SelectLayerTag;

	private UCommonActivatableWidget ActiveSelect;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		// 界面只是本地呈现，选择结果另经组件的 Server RPC 上行
		if (IsLocallyControlled())
		{
			ToggleSelect();
		}
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
	//~End UGameplayAbility Interface

	private void ToggleSelect()
	{
		if (ActiveSelect != nullptr && ActiveSelect.IsActivated())
		{
			UCommonUIExtensions::PopContentFromLayer(ActiveSelect);
			ActiveSelect = nullptr;
			return;
		}

		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		APlayerController OwningController = OwningPawn != nullptr ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
		if (OwningController == nullptr || SelectWidgetClass.Get() == nullptr)
		{
			return;
		}

		ActiveSelect = UCommonUIExtensions::PushContentToLayer_ForPlayer(
			OwningController.GetLocalPlayer(), SelectLayerTag, SelectWidgetClass);
	}
}
