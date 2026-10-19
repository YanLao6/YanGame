/**
 * UAS_ToggleMap — 全屏大地图开关。
 *
 * 本身不产生战斗效果，只把一次按键翻转大地图的开合：按下开，再按下关。
 *
 * 技能不驻留：每次按键都在同一帧内完成开合并结束，故"当前是否已打开"不靠技能状态
 * 判断，而是回头问控件自己还在不在图层上——大地图也可能被其他途径弹出，
 * 用技能状态记账会与实际不符。
 *
 * 压入 UI.Layer.Game 而非 Menu：对战期间开图不该冻结操作，
 * 玩家可以边看边动，也因此没有必要接管输入。
 *
 * 蓝图默认值需设置：
 *   MapWidgetClass → 全图取景的地图控件蓝图（bFollowSelf 关闭的 UYanMinimapWidget 子类）
 */
class UAS_ToggleMap : UYanGameplayAbility
{
	/** 大地图 UMG 类，需继承 UYanMinimapWidget */
	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TSubclassOf<UYanMinimapWidget> MapWidgetClass;

	/** 大地图压入的 UI 层 */
	UPROPERTY(EditDefaultsOnly, Category = "Map", Meta = (Categories = "UI.Layer"))
	FGameplayTag MapLayerTag = FGameplayTag::RequestGameplayTag(n"UI.Layer.Game");

	private UCommonActivatableWidget ActiveMap;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		// 地图只是本地呈现，专用服务器上激活即结束
		if (IsLocallyControlled())
		{
			ToggleMap();
		}
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
	//~End UGameplayAbility Interface

	private void ToggleMap()
	{
		if (ActiveMap != nullptr && ActiveMap.IsActivated())
		{
			UCommonUIExtensions::PopContentFromLayer(ActiveMap);
			ActiveMap = nullptr;
			return;
		}

		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		APlayerController OwningController = OwningPawn != nullptr ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
		if (OwningController == nullptr || MapWidgetClass.Get() == nullptr)
		{
			return;
		}

		ActiveMap = UCommonUIExtensions::PushContentToLayer_ForPlayer(
			OwningController.GetLocalPlayer(), MapLayerTag, MapWidgetClass);
	}
}
