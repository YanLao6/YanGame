/**
 * YanSpectateWidget — 观战状态条。
 *
 * 由 AYanSpectatorPawn 随观战 Pawn 的生灭挂入/撤出 HUD 扩展点，
 * 因此本界面存在本身就等价于"正在观战"，无需再判断观战状态。
 *
 * 作为 HUD 的叠加元素而非独立界面：CommonUI 层栈一次只显示栈顶，
 * 压进去会与 HUD 互相顶掉。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UTextBlock TargetNameText —— 当前观战目标名，自由视角时显示 FreeCameraLabel
 *   UTextBlock HintText       —— 按键提示（可选绑定）
 */
class UYanSpectateWidget : UYanUserWidget
{
	UPROPERTY(BindWidget)
	UTextBlock TargetNameText;

	UPROPERTY(BindWidgetOptional)
	UTextBlock HintText;

	/** 自由视角下的目标名占位文本 */
	UPROPERTY(EditDefaultsOnly, Category = "Spectate")
	FText FreeCameraLabel = FText::FromString("自由视角");

	/** 按键提示，内容随 IMC 的实际键位在 WBP 上维护 */
	UPROPERTY(EditDefaultsOnly, Category = "Spectate")
	FText HintLabel = FText::FromString("Q/E 切换目标  ·  F 自由视角  ·  V 退出观战");

	/** 观战目标的轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Spectate", Meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.2f;

	private FTimerHandle UpdateTimer;

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		if (HintText != nullptr)
		{
			HintText.SetText(HintLabel);
		}

		RefreshTargetName();
		UpdateTimer = System::SetTimer(this, n"RefreshTargetName", UpdateInterval, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(UpdateTimer);
	}

	UFUNCTION()
	private void RefreshTargetName()
	{
		if (TargetNameText == nullptr)
		{
			return;
		}

		AYanPlayerController YanPlayerController = Cast<AYanPlayerController>(GetOwningPlayer());
		if (YanPlayerController == nullptr)
		{
			return;
		}

		APlayerState TargetPlayerState = YanPlayerController.GetSpectateTargetPlayerState();
		if (TargetPlayerState == nullptr)
		{
			TargetNameText.SetText(FreeCameraLabel);
			return;
		}

		TargetNameText.SetText(FText::FromString(TargetPlayerState.GetPlayerName()));
	}
}
