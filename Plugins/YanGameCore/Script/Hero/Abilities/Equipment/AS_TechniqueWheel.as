/**
 * UAS_TechniqueWheel — 术式轮盘输入宿主。
 *
 * 唯一绑定 InputTag.Technique 的技能，本身不产生任何战斗效果，
 * 只负责把一次按键分流成"直接施放"与"先选再放"：
 *
 *   按下 ─┬─ HoldThreshold 内松开 → 施放当前选中术式
 *         └─ 超过 HoldThreshold  → 打开轮盘，松开时结算轮盘选中项
 *
 * 两条路径终点相同，长按只是多给了一次改选机会，故选中项是常驻的（sticky）。
 * 轮盘为 All + NoCapture 输入模式，游戏侧通路不断，按键释放仍由本技能接收，
 * 因此轮盘无需参与开合判定。
 *
 * 轮盘同时承载两类选项：可施放的术式，以及持有修复机会时展开的每条熔断记录，
 * 一层内完成选择，不再有第二层菜单。
 *
 * 选中的术式若已在运行，则改为广播 Event.Technique.Toggle 请求其结束，
 * 以此实现反转术式的"再按一次停止"。这依赖场上同时只有一个 Toggle 型术式；
 * 若将来出现第二个，需要为事件附加术式身份而非沿用单一 Tag。
 */
class UAS_TechniqueWheel : UYanGameplayAbility
{
	/** 轮盘 UMG 类，需继承 UYanTechniqueWheelWidget */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	TSubclassOf<UYanTechniqueWheelWidget> WheelWidgetClass;

	/** 区分点按与长按的时长阈值（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	float HoldThreshold = 0.1f;

	/** 轮盘压入的 UI 层 */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel", Meta = (Categories = "UI.Layer"))
	FGameplayTag WheelLayerTag = FGameplayTag::RequestGameplayTag(n"UI.Layer.Game");

	/** 修复类选项的标签前缀，如"修复 " */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	FText RestoreOptionPrefix;

	// 常驻选中项，短按直接施放它；术式被熔断后句柄失效，激活时自然落空
	private FGameplayAbilitySpecHandle SelectedTechniqueHandle;

	// 扇区序号 → 选项。BurntIndex 为 -1 表示施放项，否则为修复项在熔断记录中的下标
	private TArray<FGameplayAbilitySpecHandle> OptionHandles;
	private TArray<int32> OptionBurntIndices;

	private FTimerHandle HoldTimer;
	private UYanTechniqueWheelWidget ActiveWheel;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		ActiveWheel = nullptr;

		// 仅本地客户端需要轮盘；专用服务器上等待输入释放即可
		if (IsLocallyControlled())
		{
			HoldTimer = System::SetTimer(this, n"OpenWheel", HoldThreshold, false);
		}
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		System::ClearAndInvalidateTimerHandle(HoldTimer);

		if (ActiveWheel != nullptr)
		{
			const int32 SelectedSector = ActiveWheel.GetSelectedSector();
			UCommonUIExtensions::PopContentFromLayer(ActiveWheel);
			ActiveWheel = nullptr;
			CommitSector(SelectedSector);
		}
		else
		{
			ActivateTechnique(SelectedTechniqueHandle);
		}

		EndAbility();
	}
	//~End UGameplayAbility Interface

	UFUNCTION()
	private void OpenWheel()
	{
		UYanTechniqueComponent TechniqueComp = GetTechniqueComponent();
		APlayerController PlayerController = GetOwningPlayerController();
		if (TechniqueComp == nullptr || PlayerController == nullptr || WheelWidgetClass.Get() == nullptr)
		{
			return;
		}

		TArray<FText> Labels;
		BuildOptions(TechniqueComp, Labels);
		if (Labels.Num() == 0)
		{
			return;
		}

		UCommonActivatableWidget Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(
			PlayerController.GetLocalPlayer(), WheelLayerTag, WheelWidgetClass);

		ActiveWheel = Cast<UYanTechniqueWheelWidget>(Pushed);
		if (ActiveWheel != nullptr)
		{
			ActiveWheel.SetupWheel(Labels);
		}
	}

	// 可施放术式在前，持有修复机会时把每条熔断记录追加为修复项
	private void BuildOptions(UYanTechniqueComponent TechniqueComp, TArray<FText>& OutLabels)
	{
		OptionHandles.Reset();
		OptionBurntIndices.Reset();

		TArray<FGameplayAbilitySpecHandle> AvailableHandles;
		TArray<FText> AvailableNames;
		TechniqueComp.CollectAvailableTechniques(AvailableHandles, AvailableNames);

		for (int32 Index = 0; Index < AvailableHandles.Num(); ++Index)
		{
			OptionHandles.Add(AvailableHandles[Index]);
			OptionBurntIndices.Add(-1);
			OutLabels.Add(AvailableNames[Index]);
		}

		if (!TechniqueComp.HasRestoreOpportunity())
		{
			return;
		}

		TArray<int32> BurntIndices;
		TArray<FText> BurntNames;
		TechniqueComp.CollectRestorableTechniques(BurntIndices, BurntNames);

		for (int32 Index = 0; Index < BurntIndices.Num(); ++Index)
		{
			OptionHandles.Add(FGameplayAbilitySpecHandle());
			OptionBurntIndices.Add(BurntIndices[Index]);
			OutLabels.Add(FText::FromString(RestoreOptionPrefix.ToString() + BurntNames[Index].ToString()));
		}
	}

	private void CommitSector(int32 SectorIndex)
	{
		if (!OptionBurntIndices.IsValidIndex(SectorIndex))
		{
			return;
		}

		const int32 BurntIndex = OptionBurntIndices[SectorIndex];
		if (BurntIndex >= 0)
		{
			UYanTechniqueComponent TechniqueComp = GetTechniqueComponent();
			if (TechniqueComp != nullptr)
			{
				TechniqueComp.ServerCommitRestore(BurntIndex);
			}
			return;
		}

		SelectedTechniqueHandle = OptionHandles[SectorIndex];
		ActivateTechnique(SelectedTechniqueHandle);
	}

	private void ActivateTechnique(FGameplayAbilitySpecHandle TechniqueHandle)
	{
		UAbilitySystemComponent ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC == nullptr)
		{
			return;
		}

		FGameplayAbilitySpec TechniqueSpec;
		if (!ASC.FindAbilitySpecFromHandle(TechniqueHandle, TechniqueSpec))
		{
			return;
		}

		if (TechniqueSpec.IsActive())
		{
			FGameplayEventData TogglePayload;
			ASC.SendGameplayEvent(FGameplayTag::RequestGameplayTag(n"Event.Technique.Toggle"), TogglePayload);
			return;
		}

		ASC.TryActivateAbility(TechniqueHandle);
	}

	private UYanTechniqueComponent GetTechniqueComponent() const
	{
		return UYanTechniqueComponent::Get(GetAvatarActorFromActorInfo());
	}

	private APlayerController GetOwningPlayerController() const
	{
		APawn OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		return OwningPawn != nullptr ? Cast<APlayerController>(OwningPawn.GetController()) : nullptr;
	}
}
