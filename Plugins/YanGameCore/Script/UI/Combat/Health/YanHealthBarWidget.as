/**
 * YanHealthBarWidget — 玩家生命条（含延迟掉血表现）。
 *
 * 条状生命值由两层叠放的 ProgressBar 呈现：
 *   - 前层红条（HealthBar）    ：当前血量分数，受伤瞬间即减。
 *   - 后层黄条（DamageLagBar） ：滞后血量分数，受伤后停留一小段再缓慢下落，
 *                                红黄之间露出的黄色即"刚失去的那段血"。
 *   - HealthText 显示 "当前/上限"。
 *
 * 数据来源：AngelScript 未暴露属性变化委托，故按定时器轮询 ASC 的属性当前值，
 * 这同时也是黄条逐帧缓动所必需的驱动方式。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UProgressBar DamageLagBar  —— 后层，黄色滞后条（置于底层）
 *   UProgressBar HealthBar     —— 前层，红色当前条（叠在黄条之上，背景需透明）
 *   UTextBlock   HealthText    —— "当前/上限" 文本
 *
 * WBP 默认值需设置（否则读不到血量）：
 *   HealthAttribute    → UYanHealthSet.Health
 *   MaxHealthAttribute → UYanHealthSet.MaxHealth
 */
class UYanHealthBarWidget : UYanUserWidget
{
	/** 后层黄色滞后条：呈现刚失去、尚未消散的血量 */
	UPROPERTY(BindWidget)
	UProgressBar DamageLagBar;

	/** 前层红色当前条：呈现当前血量，背景需透明以透出黄条 */
	UPROPERTY(BindWidget)
	UProgressBar HealthBar;

	/** "当前/上限" 文本 */
	UPROPERTY(BindWidget)
	UTextBlock HealthText;

	/** 当前血量属性，绑定至 UYanHealthSet.Health */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	FGameplayAttribute HealthAttribute;

	/** 血量上限属性，绑定至 UYanHealthSet.MaxHealth */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	FGameplayAttribute MaxHealthAttribute;

	/** 当前血量条填充色 */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	FLinearColor HealthColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.0f);

	/** 滞后掉血条填充色 */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar")
	FLinearColor DamageLagColor = FLinearColor(1.0f, 0.82f, 0.1f, 1.0f);

	/** 受伤后黄条停留时长，之后才开始下落（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar", Meta = (ClampMin = "0.0"))
	float DamageLagHold = 0.1f;

	/** 黄条下落速度，单位为"血量分数/秒" */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar", Meta = (ClampMin = "0.0"))
	float DamageLagDrainSpeed = 0.5f;

	/** 轮询与缓动的刷新间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "HealthBar", Meta = (ClampMin = "0.01"))
	float UpdateInterval = 0.03f;

	// ASC 依赖 PlayerState 复制，Construct 时未必就绪，故惰性获取
	private UAbilitySystemComponent CachedAbilitySystem;

	// 当前血量分数（红条）
	private float CurrentFraction = 1.0f;

	// 滞后血量分数（黄条），始终不小于当前分数
	private float LaggedFraction = 1.0f;

	// 黄条下落前的剩余停留时间
	private float HoldRemaining = 0.0f;

	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		// 强制红/黄配色，设计时预览与运行时一致
		if (HealthBar != nullptr)
		{
			HealthBar.SetFillColorAndOpacity(HealthColor);
		}
		if (DamageLagBar != nullptr)
		{
			DamageLagBar.SetFillColorAndOpacity(DamageLagColor);
		}
	}

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		// 首帧把红黄条对齐到当前血量，避免从满血缓动到实际值
		float Fraction = 0.0f;
		if (TryGetHealthFraction(Fraction))
		{
			CurrentFraction = Fraction;
			LaggedFraction = Fraction;
		}
		RefreshBars();

		UpdateTimer = System::SetTimer(this, n"UpdateHealthBar", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	// 惰性获取本地玩家 Pawn 上的 ASC
	private UAbilitySystemComponent GetAbilitySystem()
	{
		if (CachedAbilitySystem == nullptr)
		{
			APawn OwningPawn = GetOwningPlayerPawn();
			if (OwningPawn != nullptr)
			{
				CachedAbilitySystem = AbilitySystem::GetAbilitySystemComponent(OwningPawn);
			}
		}
		return CachedAbilitySystem;
	}

	// 读取当前血量分数；ASC 或属性无效时返回 false
	private bool TryGetHealthFraction(float& OutFraction)
	{
		UAbilitySystemComponent ASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwningPlayer());
		if (ASC == nullptr)
		{
			return false;
		}

		bool bHealthFound = false;
		bool bMaxFound = false;
		const float Health = ASC.GetGameplayAttributeValue(HealthAttribute, bHealthFound);
		const float MaxHealth = ASC.GetGameplayAttributeValue(MaxHealthAttribute, bMaxFound);
		if (!bHealthFound || !bMaxFound || MaxHealth <= 0.0f)
		{
			return false;
		}

		OutFraction = Math::Clamp(Health / MaxHealth, 0.0f, 1.0f);
		return true;
	}

	UFUNCTION()
	private void UpdateHealthBar()
	{
		UAbilitySystemComponent ASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwningPlayer());
		if (ASC == nullptr)
		{
			return;
		}

		bool bHealthFound = false;
		bool bMaxFound = false;
		const float Health = ASC.GetGameplayAttributeValue(HealthAttribute, bHealthFound);
		const float MaxHealth = ASC.GetGameplayAttributeValue(MaxHealthAttribute, bMaxFound);
		if (!bHealthFound || !bMaxFound || MaxHealth <= 0.0f)
		{
			return;
		}

		const float NewFraction = Math::Clamp(Health / MaxHealth, 0.0f, 1.0f);

		// 血量下降视为受伤：重置黄条停留计时
		if (NewFraction < CurrentFraction)
		{
			HoldRemaining = DamageLagHold;
		}
		CurrentFraction = NewFraction;

		if (CurrentFraction >= LaggedFraction)
		{
			// 回血或未变：黄条不应领先红条，直接贴合
			LaggedFraction = CurrentFraction;
			HoldRemaining = 0.0f;
		}
		else if (HoldRemaining > 0.0f)
		{
			// 停留阶段：黄条保持在受伤前的水平
			HoldRemaining -= UpdateInterval;
		}
		else
		{
			// 下落阶段：黄条按恒定速度逼近红条
			LaggedFraction = Math::Max(CurrentFraction, LaggedFraction - DamageLagDrainSpeed * UpdateInterval);
		}

		RefreshBars();
		RefreshText(Health, MaxHealth);
	}

	private void RefreshBars()
	{
		if (HealthBar != nullptr)
		{
			HealthBar.SetPercent(CurrentFraction);
		}
		if (DamageLagBar != nullptr)
		{
			DamageLagBar.SetPercent(LaggedFraction);
		}
	}

	private void RefreshText(float Health, float MaxHealth)
	{
		if (HealthText == nullptr)
		{
			return;
		}

		const int32 CurrentValue = Math::RoundToInt(Health);
		const int32 MaxValue = Math::RoundToInt(MaxHealth);
		HealthText.SetText(FText::FromString(String::Conv_IntToString(CurrentValue) + "/" + String::Conv_IntToString(MaxValue)));
	}
}
