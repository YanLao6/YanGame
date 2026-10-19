/**
 * YanTechniqueWheelWidget — 径向选择轮盘。
 *
 * 输入模式为 All + NoCapture：键盘仍走游戏侧，角色移动不受影响；
 * 鼠标转为自由光标，不再产生 Look 轴输入，故选择时镜头不会跟着转。
 * 游戏侧输入通路保持畅通，发起技能因此能照常收到按键释放，
 * 本类不参与开合判定，只负责呈现与选中计算。
 *
 * 选中项由光标相对屏幕中心的方向决定，正上方为第 0 扇区、顺时针递增；
 * 光标位于死区内时保持上一次选择，避免回中时选项抖动。
 *
 * 本类只认扇区序号，扇区代表什么由调用方解释。
 *
 * UMG 蓝图需实现 OnWheelSetup / OnSelectionChanged 绘制扇区与高亮。
 */
class UYanTechniqueWheelWidget : UYanUserWidget
{
	default InputConfig = EModularInputWidgetInputMode::GameAndMenu;
	default GameMouseCaptureMode = EMouseCaptureMode::NoCapture;

	/** 光标距屏幕中心小于此像素距离时不改变选择 */
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	float DeadZoneRadius = 40.0f;

	private int32 SectorCount = 0;
	private int32 SelectedSector = -1;
	private FTimerHandle SelectionTimer;

	/** 填充轮盘并把光标居中，需在压入图层后立即调用。 */
	UFUNCTION(BlueprintCallable, Category = "Wheel")
	void SetupWheel(TArray<FText> Labels)
	{
		SectorCount = Labels.Num();
		SelectedSector = SectorCount > 0 ? 0 : -1;

		CenterCursor();
		OnWheelSetup(Labels);
		OnSelectionChanged(SelectedSector);
	}

	/** 当前选中的扇区序号；-1 表示轮盘为空。 */
	UFUNCTION(BlueprintPure, Category = "Wheel")
	int32 GetSelectedSector() const
	{
		return SelectedSector;
	}

	/** UMG 实现：按标签生成扇区。 */
	UFUNCTION(BlueprintEvent, Category = "Wheel")
	void OnWheelSetup(TArray<FText> Labels)
	{
	}

	/** UMG 实现：高亮当前扇区。 */
	UFUNCTION(BlueprintEvent, Category = "Wheel")
	void OnSelectionChanged(int32 SectorIndex)
	{
	}

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		SelectionTimer = System::SetTimer(this, n"UpdateSelectionFromCursor", 0.016f, true);
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(SelectionTimer);
	}

	// 光标方向 → 扇区。屏幕 Y 轴向下，故以 -Y 为正上方基准
	UFUNCTION()
	private void UpdateSelectionFromCursor()
	{
		if (SectorCount <= 1)
		{
			return;
		}

		const FVector2D ViewportSize = WidgetLayout::GetViewportSize();
		const FVector2D CursorPosition = WidgetLayout::GetMousePositionOnViewport();
		const FVector2D Offset = CursorPosition - ViewportSize * 0.5f;

		if (Offset.Size() < DeadZoneRadius)
		{
			return;
		}

		float AngleDegrees = Math::RadiansToDegrees(Math::Atan2(Offset.X, -Offset.Y));
		if (AngleDegrees < 0.0f)
		{
			AngleDegrees += 360.0f;
		}

		const float SectorSize = 360.0f / SectorCount;
		const int32 NewSector = int32((AngleDegrees + SectorSize * 0.5f) / SectorSize) % SectorCount;

		if (NewSector != SelectedSector)
		{
			SelectedSector = NewSector;
			OnSelectionChanged(SelectedSector);
		}
	}

	// 每次打开都从正上方起算，避免沿用上一次的光标位置
	private void CenterCursor()
	{
		APlayerController PlayerController = GetOwningPlayer();
		if (PlayerController == nullptr)
		{
			return;
		}

		const FVector2D ViewportSize = WidgetLayout::GetViewportSize();
		PlayerController.SetMouseLocation(int32(ViewportSize.X * 0.5f), int32(ViewportSize.Y * 0.5f));
	}
}
