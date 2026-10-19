/**
 * YanImpulseBarWidget — 负荷与机动能量 HUD。
 *
 * 负荷（Load）是连续量，以单条 ProgressBar 呈现，填充色随负荷比例由绿转红，
 * 条后跟随 "当前/上限" 文本。机动能量（Charge）以整格计价，故不合并为一条，
 * 而是每格单独成条横向排列：格数随当前能量增减，末格按小数部分呈现充能进度。
 *
 * 数据来源：AngelScript 未暴露属性变化委托，故按定时器轮询 ASC 的属性当前值。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UProgressBar   LoadBar   —— 负荷条
 *   UTextBlock     LoadText  —— "当前/上限" 文本
 *   UHorizontalBox ChargeBox —— 能量格容器，格子于运行时填入，设计期应留空
 *
 * WBP 默认值需设置（否则读不到数值）：
 *   LoadAttribute    → UYanImpulseSet.Load
 *   MaxLoadAttribute → UYanImpulseSet.MaxLoad
 *   ChargeAttribute  → UYanImpulseSet.Charge
 */
class UYanImpulseBarWidget : UYanUserWidget
{
	/** 负荷条，填充色由本控件按负荷比例覆写 */
	UPROPERTY(BindWidget)
	UProgressBar LoadBar;

	/** 负荷 "当前/上限" 文本 */
	UPROPERTY(BindWidget)
	UTextBlock LoadText;

	/** 能量格容器，子控件由本控件独占管理 */
	UPROPERTY(BindWidget)
	UHorizontalBox ChargeBox;

	/** 当前负荷属性，绑定至 UYanImpulseSet.Load */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse")
	FGameplayAttribute LoadAttribute;

	/** 负荷上限属性，绑定至 UYanImpulseSet.MaxLoad */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse")
	FGameplayAttribute MaxLoadAttribute;

	/** 当前机动能量属性，绑定至 UYanImpulseSet.Charge */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse")
	FGameplayAttribute ChargeAttribute;

	/** 零负荷时的条色 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Load")
	FLinearColor LoadLowColor = FLinearColor(0.10f, 0.85f, 0.20f, 1.0f);

	/** 半负荷时的条色。绿红两端直接插值中途会经过浑浊的褐色，故取一处中间色分两段过渡 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Load")
	FLinearColor LoadMidColor = FLinearColor(0.95f, 0.85f, 0.10f, 1.0f);

	/** 满负荷时的条色 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Load")
	FLinearColor LoadHighColor = FLinearColor(0.90f, 0.10f, 0.10f, 1.0f);

	/** 单个能量格的尺寸（像素）。格宽固定，使格数变化成为唯一的视觉信号 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge")
	FVector2D ChargeSlotSize = FVector2D(48.0f, 12.0f);

	/** 相邻能量格之间的间隔（像素） */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge", Meta = (ClampMin = "0.0"))
	float ChargeSlotSpacing = 4.0f;

	/** 能量格填充色 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge")
	FLinearColor ChargeColor = FLinearColor(0.20f, 0.70f, 1.00f, 1.0f);

	/** 属性轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse", Meta = (ClampMin = "0.01"))
	float UpdateInterval = 0.03f;

	// 已生成的能量格，下标即格序；持有引用同时兼作防 GC
	private TArray<UProgressBar> ChargeBars;

	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		// 设计期给出零负荷配色，避免预览停留在 UMG 的默认蓝
		if (LoadBar != nullptr)
		{
			LoadBar.SetFillColorAndOpacity(LoadLowColor);
		}
	}

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		RefreshImpulse();
		UpdateTimer = System::SetTimer(this, n"RefreshImpulse", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	UFUNCTION()
	private void RefreshImpulse()
	{
		// ASC 随 PlayerState 复制，构造时未必就绪，故每次轮询重新解析
		UAbilitySystemComponent ASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwningPlayer());
		if (ASC == nullptr)
		{
			return;
		}

		RefreshLoad(ASC);
		RefreshCharge(ASC);
	}

	private void RefreshLoad(UAbilitySystemComponent ASC)
	{
		bool bLoadFound = false;
		bool bMaxFound = false;
		const float Load = ASC.GetGameplayAttributeValue(LoadAttribute, bLoadFound);
		const float MaxLoad = ASC.GetGameplayAttributeValue(MaxLoadAttribute, bMaxFound);
		if (!bLoadFound || !bMaxFound || MaxLoad <= 0.0f)
		{
			return;
		}

		const float Fraction = Math::Clamp(Load / MaxLoad, 0.0f, 1.0f);

		if (LoadBar != nullptr)
		{
			LoadBar.SetPercent(Fraction);
			LoadBar.SetFillColorAndOpacity(ResolveLoadColor(Fraction));
		}

		if (LoadText != nullptr)
		{
			const FString Current = String::Conv_IntToString(Math::RoundToInt(Load));
			const FString Max = String::Conv_IntToString(Math::RoundToInt(MaxLoad));
			LoadText.SetText(FText::FromString(Current + "/" + Max));
		}
	}

	private void RefreshCharge(UAbilitySystemComponent ASC)
	{
		if (ChargeBox == nullptr)
		{
			return;
		}

		bool bChargeFound = false;
		const float Charge = ASC.GetGameplayAttributeValue(ChargeAttribute, bChargeFound);
		if (!bChargeFound)
		{
			return;
		}

		// 末格可能停在充能中途，故按向上取整决定格数；容差避免整数值被浮点误差多算一格
		const int32 DesiredCount = Math::Max(0, Math::CeilToInt(Charge - 0.001f));
		if (ChargeBars.Num() != DesiredCount)
		{
			RebuildChargeBars(DesiredCount);
		}

		for (int32 Index = 0; Index < ChargeBars.Num(); Index++)
		{
			ChargeBars[Index].SetPercent(Math::Clamp(Charge - float(Index), 0.0f, 1.0f));
		}
	}

	// 格数变化时整列重建；格数稳定时只更新填充，不触碰控件树
	private void RebuildChargeBars(int32 Count)
	{
		ChargeBox.ClearChildren();
		ChargeBars.Empty();

		for (int32 Index = 0; Index < Count; Index++)
		{
			UProgressBar Bar = Cast<UProgressBar>(NewObject(this, UProgressBar));
			USizeBox SizeBox = Cast<USizeBox>(NewObject(this, USizeBox));
			if (Bar == nullptr || SizeBox == nullptr)
			{
				continue;
			}

			Bar.SetFillColorAndOpacity(ChargeColor);

			// 固定格宽，使格数增减而非格宽变化承载信息
			SizeBox.SetWidthOverride(ChargeSlotSize.X);
			SizeBox.SetHeightOverride(ChargeSlotSize.Y);
			SizeBox.AddChild(Bar);

			UHorizontalBoxSlot BoxSlot = ChargeBox.AddChildToHorizontalBox(SizeBox);
			if (BoxSlot != nullptr)
			{
				const float LeftPadding = Index > 0 ? ChargeSlotSpacing : 0.0f;
				BoxSlot.SetPadding(FMargin(LeftPadding, 0.0f, 0.0f, 0.0f));
			}

			ChargeBars.Add(Bar);
		}
	}

	// 负荷比例映射为条色：0 → 低位色，0.5 → 中间色，1 → 高位色
	private FLinearColor ResolveLoadColor(float Fraction) const
	{
		if (Fraction <= 0.5f)
		{
			return LerpColor(LoadLowColor, LoadMidColor, Fraction * 2.0f);
		}
		return LerpColor(LoadMidColor, LoadHighColor, (Fraction - 0.5f) * 2.0f);
	}

	private FLinearColor LerpColor(FLinearColor From, FLinearColor To, float Alpha) const
	{
		return FLinearColor(
			Math::Lerp(From.R, To.R, Alpha),
			Math::Lerp(From.G, To.G, Alpha),
			Math::Lerp(From.B, To.B, Alpha),
			Math::Lerp(From.A, To.A, Alpha));
	}
}
