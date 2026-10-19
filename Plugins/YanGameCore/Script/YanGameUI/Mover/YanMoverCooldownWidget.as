/** 冷却格配置：一个疲劳通道对应一格 */
struct FYanMoverCooldownEntry
{
	/** 冷却所在的疲劳通道 */
	UPROPERTY(EditAnywhere, Category = "Cooldown")
	EYanFatigueChannel Channel = EYanFatigueChannel::AirDash;

	/** 就绪时格内显示的名称 */
	UPROPERTY(EditAnywhere, Category = "Cooldown")
	FText Label;
}

/**
 * UYanMoverCooldownWidget — 移动技能冷却 HUD。
 *
 * 空中冲刺、二段跳等冷却不走 GE，而是记在 Mover SyncState 的疲劳通道里（见 FYanFatigueState），
 * 故按定时器轮询本地 Pawn 的 Mover 同步状态；本地受控端读到的是预测值，按键当帧即有反馈。
 *
 * 每格只在技能用过之后出现：
 *   - bRecoverAfterLanding：已用但尚未落地，冷却不走，空格、显示 LandingHint
 *   - 通道有其余记录     ：恢复中，按 1 - 疲劳度填充、显示剩余秒数
 *   - 通道无记录        ：就绪；刚转好的格满格显示名称并在 FadeOutDuration 内淡出，从未用过或已淡出的格折叠
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UHorizontalBox EntryBox —— 冷却格容器，格子于运行时按 Entries 填入，设计期应留空
 */
class UYanMoverCooldownWidget : UYanUserWidget
{
	/** 冷却格容器，子控件由本控件独占管理 */
	UPROPERTY(BindWidget)
	UHorizontalBox EntryBox;

	/** 自左向右的冷却格 */
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
	TArray<FYanMoverCooldownEntry> Entries;

	/** 已用但尚未落地时的提示 */
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
	FText LandingHint = FText::FromString("落地");

	/** 单格尺寸（像素） */
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown|Style")
	FVector2D EntrySize = FVector2D(72.0f, 28.0f);

	/** 相邻格间距（像素） */
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown|Style", Meta = (ClampMin = "0.0"))
	float EntrySpacing = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Cooldown|Style")
	FLinearColor ReadyColor = FLinearColor(0.20f, 0.80f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Cooldown|Style")
	FLinearColor RecoveringColor = FLinearColor(0.95f, 0.65f, 0.10f, 1.0f);

	/** 冷却转好后淡出所用时长（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown|Style", Meta = (ClampMin = "0.0"))
	float FadeOutDuration = 1.0f;

	/** 轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown", Meta = (ClampMin = "0.01"))
	float UpdateInterval = 0.05f;

	// 与 Entries 同序；持有引用同时兼作防 GC
	private TArray<USizeBox> EntryRoots;
	private TArray<UProgressBar> EntryBars;
	private TArray<UTextBlock> EntryTexts;

	// 冷却转好的时刻；小于 0 表示仍在冷却或格子已隐藏
	private TArray<float> ReadyTimes;

	// 用过且尚未淡出完毕，即当前应显示
	private TArray<bool> EntryShown;

	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		BuildEntries();
		RefreshCooldowns();
		UpdateTimer = System::SetTimer(this, n"RefreshCooldowns", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	// 格数由配置决定，只在构造时建一次
	private void BuildEntries()
	{
		if (EntryBox == nullptr)
		{
			return;
		}

		EntryBox.ClearChildren();
		EntryRoots.Empty();
		EntryBars.Empty();
		EntryTexts.Empty();
		ReadyTimes.Empty();
		EntryShown.Empty();

		for (int32 Index = 0; Index < Entries.Num(); Index++)
		{
			USizeBox SizeBox = Cast<USizeBox>(NewObject(this, USizeBox));
			UOverlay Overlay = Cast<UOverlay>(NewObject(this, UOverlay));
			UProgressBar Bar = Cast<UProgressBar>(NewObject(this, UProgressBar));
			UTextBlock Text = Cast<UTextBlock>(NewObject(this, UTextBlock));
			if (SizeBox == nullptr || Overlay == nullptr || Bar == nullptr || Text == nullptr)
			{
				continue;
			}

			SizeBox.SetWidthOverride(EntrySize.X);
			SizeBox.SetHeightOverride(EntrySize.Y);
			SizeBox.AddChild(Overlay);

			UOverlaySlot BarSlot = Overlay.AddChildToOverlay(Bar);
			if (BarSlot != nullptr)
			{
				BarSlot.SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				BarSlot.SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}

			Text.SetJustification(ETextJustify::Center);
			UOverlaySlot TextSlot = Overlay.AddChildToOverlay(Text);
			if (TextSlot != nullptr)
			{
				TextSlot.SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
				TextSlot.SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
			}

			SizeBox.SetVisibility(ESlateVisibility::Collapsed);
			EntryBox.AddChildToHorizontalBox(SizeBox);

			EntryRoots.Add(SizeBox);
			EntryBars.Add(Bar);
			EntryTexts.Add(Text);
			ReadyTimes.Add(-1.0f);
			EntryShown.Add(false);
		}
	}

	UFUNCTION()
	private void RefreshCooldowns()
	{
		// Pawn 会随重生更换，每次轮询重新解析
		APawn OwningPawn = GetOwningPlayerPawn();
		UMoverComponent MoverComp = OwningPawn != nullptr ? UMoverComponent::Get(OwningPawn) : nullptr;

		const float Now = System::GetGameTimeInSeconds();
		bool bShownChanged = false;

		for (int32 Index = 0; Index < EntryBars.Num(); Index++)
		{
			const bool bWasShown = EntryShown[Index];

			FYanFatigueEntry Fatigue;
			const bool bCooling = MoverComp != nullptr && YanMoverAngelscript::GetFatigueEntry(MoverComp, Entries[Index].Channel, Fatigue);
			if (bCooling)
			{
				EntryShown[Index] = true;
				ReadyTimes[Index] = -1.0f;
				if (Fatigue.bRecoverAfterLanding)
				{
					ShowEntry(Index, 0.0f, RecoveringColor, LandingHint, 1.0f);
				}
				else
				{
					const float Remaining = Fatigue.RecoveryPerSecond > 0.0f ? Fatigue.Fatigue / Fatigue.RecoveryPerSecond : 0.0f;
					ShowEntry(Index, 1.0f - Fatigue.Fatigue, RecoveringColor, FText::FromString(FormatSeconds(Remaining)), 1.0f);
				}
			}
			else if (bWasShown)
			{
				// 刚转好：满格亮出名称，随后淡出并折叠
				if (ReadyTimes[Index] < 0.0f)
				{
					ReadyTimes[Index] = Now;
				}

				const float Elapsed = Now - ReadyTimes[Index];
				if (Elapsed >= FadeOutDuration)
				{
					EntryShown[Index] = false;
					ReadyTimes[Index] = -1.0f;
				}
				else
				{
					ShowEntry(Index, 1.0f, ReadyColor, Entries[Index].Label, 1.0f - Elapsed / FadeOutDuration);
				}
			}

			if (EntryShown[Index] != bWasShown)
			{
				EntryRoots[Index].SetVisibility(EntryShown[Index] ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				bShownChanged = true;
			}
		}

		if (bShownChanged)
		{
			RefreshSpacing();
		}
	}

	private void ShowEntry(int32 Index, float Percent, FLinearColor FillColor, FText Text, float Opacity)
	{
		EntryBars[Index].SetPercent(Math::Clamp(Percent, 0.0f, 1.0f));
		EntryBars[Index].SetFillColorAndOpacity(FillColor);
		EntryTexts[Index].SetText(Text);
		EntryRoots[Index].SetRenderOpacity(Math::Clamp(Opacity, 0.0f, 1.0f));
	}

	// 间距只加在可见格之间，折叠的格不留空位
	private void RefreshSpacing()
	{
		bool bHasVisibleBefore = false;
		for (int32 Index = 0; Index < EntryRoots.Num(); Index++)
		{
			if (!EntryShown[Index])
			{
				continue;
			}

			UHorizontalBoxSlot BoxSlot = Cast<UHorizontalBoxSlot>(EntryRoots[Index].Slot);
			if (BoxSlot != nullptr)
			{
				BoxSlot.SetPadding(FMargin(bHasVisibleBefore ? EntrySpacing : 0.0f, 0.0f, 0.0f, 0.0f));
			}
			bHasVisibleBefore = true;
		}
	}

	// 保留一位小数并向上取整，仍在冷却时不会显示 0.0
	private FString FormatSeconds(float Seconds) const
	{
		const int32 Tenths = Math::Max(0, Math::CeilToInt(Seconds * 10.0f));
		return String::Conv_IntToString(Math::IntegerDivisionTrunc(Tenths, 10)) + "." + String::Conv_IntToString(Tenths % 10);
	}
}
