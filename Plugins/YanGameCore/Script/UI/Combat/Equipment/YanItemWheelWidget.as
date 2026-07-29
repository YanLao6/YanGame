/**
 * UYanItemWheelWidget — 按物品类别展开的径向选择轮盘。
 *
 * 在父类的扇区选择之上，把选项从"文本标签"换成"背包里的物品"：
 * 只收展示片段（UInventoryFragment_ItemDisplay）上 DisplayId 命中 RegistryType 的物品，
 * 因此一个 RegistryType 即一类可选物品，分类依据落在展示注册表类型上。
 * 选项顺序由展示注册表的数据表行序决定，与拾取先后无关。
 *
 * 圆环由选项数平分：一个选项时整圈皆是它，两个选项时上半圈为第一个、下半圈为第二个，
 * 依此类推。选中由父类按光标方向计算，扇区序号与选项下标一一对应。
 * 扇区没有各自的按钮 —— Slate 的命中测试基于控件矩形，无法裁成环形扇区，
 * 故命中一律由角度判定，圆环仅作绘制。
 *
 * 选项图标由脚本按扇区中心角摆到内圈半径上（第 0 项落在正上方），数量变化时重排。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UCanvasPanel OptionContainer —— 选项图标容器，图标坐标由本类计算，无需手工摆放
 *   UImage       RingImage       —— 圆环底图（可选），需使用带下列标量参数的材质：
 *                                   SectorCount / SelectedSector / InnerRadius / OuterRadius
 * WBP 默认值需设置：
 *   RegistryType      → 该轮盘展示的展示注册表类型
 *   OptionWidgetClass → 你的 UW_ItemSlot 蓝图子类（无按键提示）
 */
class UYanItemWheelWidget : UYanTechniqueWheelWidget
{
	/** 该轮盘收录的展示注册表类型，只有 DisplayId 属于此类型的物品才会出现 */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel")
	FDataRegistryType RegistryType;

	/** 选项图标容器，子控件由本类按扇区中心角定位 */
	UPROPERTY(BindWidget)
	UCanvasPanel OptionContainer;

	/** 圆环底图，其材质承担扇区分割与高亮绘制 */
	UPROPERTY(BindWidgetOptional)
	UImage RingImage;

	/** 选项图标控件类，复用物品槽位以共享图标加载链路 */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel")
	TSubclassOf<UW_ItemSlot> OptionWidgetClass;

	/** 图标中心距圆心的像素距离，即圆环内圈半径 */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel|Layout", Meta = (ClampMin = "0.0"))
	float IconRadius = 180.0f;

	/** 单个图标控件的尺寸 */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel|Layout")
	FVector2D IconSize = FVector2D(96.0f, 96.0f);

	/** 环形材质的内圈半径，按控件尺寸归一化 */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel|Ring", Meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RingInnerRadius = 0.25f;

	/** 环形材质的外圈半径，按控件尺寸归一化 */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel|Ring", Meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RingOuterRadius = 0.5f;

	/** 图标异步就绪后的补图轮询间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "ItemWheel", Meta = (ClampMin = "0.05"))
	float IconRefreshInterval = 0.1f;

	// 扇区序号 → 物品，与父类的 SelectedSector 同一套下标
	private TArray<UInventoryItemInstance> Options;
	private TArray<UW_ItemSlot> OptionWidgets;
	private UInventoryManagerComponent CachedInventory;
	private UEquipmentDisplaySubsystem CachedDisplaySubsystem;
	private UMaterialInstanceDynamic RingMaterial;
	private FTimerHandle IconTimer;
	// 已按此数量排布过图标，数量不变时不重复计算坐标
	private int32 LaidOutCount = -1;

	/**
	 * 按 RegistryType 收集背包物品并填充轮盘，返回选项数量。
	 * 需在压入图层后立即调用；返回 0 表示背包内无该类物品，调用方应撤下轮盘。
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemWheel")
	int32 SetupItemWheel()
	{
		CollectItems();

		TArray<FText> Labels;
		BuildLabels(Labels);
		// 交给父类确定扇区数、居中光标并给出初始选中项
		SetupWheel(Labels);

		if (!RefreshOptionWidgets() && Options.Num() > 0)
		{
			IconTimer = System::SetTimer(this, n"PollIconRefresh", IconRefreshInterval, true);
		}

		return Options.Num();
	}

	/** 当前光标指向的物品；轮盘为空时返回 null。 */
	UFUNCTION(BlueprintPure, Category = "ItemWheel")
	UInventoryItemInstance GetSelectedItem() const
	{
		const int32 SectorIndex = GetSelectedSector();
		return Options.IsValidIndex(SectorIndex) ? Options[SectorIndex] : nullptr;
	}

	//~Begin UYanTechniqueWheelWidget Interface
	UFUNCTION(BlueprintOverride)
	void OnSelectionChanged(int32 SectorIndex)
	{
		RefreshOptionWidgets();
		RefreshRing();
	}
	//~End UYanTechniqueWheelWidget Interface

	//~Begin UUserWidget Interface
	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		System::ClearAndInvalidateTimerHandle(IconTimer);
		Super::Destruct();
	}
	//~End UUserWidget Interface

	// 图标全部就绪后即停表，轮盘余下的存续期间不再轮询
	UFUNCTION()
	private void PollIconRefresh()
	{
		if (RefreshOptionWidgets())
		{
			System::ClearAndInvalidateTimerHandle(IconTimer);
		}
	}

	// 高亮跟随父类的选中扇区，故补图与改选共用同一次同步
	private bool RefreshOptionWidgets()
	{
		if (OptionContainer == nullptr || OptionWidgetClass.Get() == nullptr)
		{
			return true;
		}

		// 扇区下标与快捷栏槽位无关，不显示按键提示
		TArray<int32> NoKeySlots;
		const bool bAllReady = EquipmentSlotView::Sync(
			this, OptionContainer, OptionWidgetClass, OptionWidgets, Options, GetSelectedSector(), NoKeySlots);

		if (LaidOutCount != OptionWidgets.Num())
		{
			LayoutOptions();
		}
		return bAllReady;
	}

	// 第 i 项落在扇区中心角 i*360/N 上；屏幕 Y 轴向下，故正上方取 -Y
	private void LayoutOptions()
	{
		const int32 Count = OptionWidgets.Num();
		LaidOutCount = Count;
		if (Count == 0)
		{
			return;
		}

		FAnchors CenterAnchor;
		CenterAnchor.Minimum = FVector2D(0.5f, 0.5f);
		CenterAnchor.Maximum = FVector2D(0.5f, 0.5f);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			UCanvasPanelSlot CanvasSlot = Cast<UCanvasPanelSlot>(OptionWidgets[Index].Slot);
			if (CanvasSlot == nullptr)
			{
				continue;
			}

			const float AngleRadians = Math::DegreesToRadians(360.0f * Index / Count);
			CanvasSlot.SetAnchors(CenterAnchor);
			CanvasSlot.SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot.SetSize(IconSize);
			CanvasSlot.SetPosition(FVector2D(Math::Sin(AngleRadians), -Math::Cos(AngleRadians)) * IconRadius);
		}
	}

	// 扇区分割与高亮全部交给材质，脚本只喂参数
	private void RefreshRing()
	{
		if (RingImage == nullptr)
		{
			return;
		}

		if (RingMaterial == nullptr)
		{
			RingMaterial = RingImage.GetDynamicMaterial();
			if (RingMaterial == nullptr)
			{
				return;
			}
			RingMaterial.SetScalarParameterValue(n"InnerRadius", RingInnerRadius);
			RingMaterial.SetScalarParameterValue(n"OuterRadius", RingOuterRadius);
		}

		RingMaterial.SetScalarParameterValue(n"SectorCount", float(Options.Num()));
		RingMaterial.SetScalarParameterValue(n"SelectedSector", float(GetSelectedSector()));
	}

	private void CollectItems()
	{
		Options.Reset();

		UInventoryManagerComponent Inventory = GetInventory();
		if (Inventory == nullptr || RegistryType.Name == n"None")
		{
			return;
		}

		TArray<UInventoryItemInstance> AllItems = Inventory.GetAllItems();
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			if (MatchesRegistryType(AllItems[Index]))
			{
				Options.Add(AllItems[Index]);
			}
		}

		// 呈现顺序归策划表，背包里的先后顺序不参与
		UEquipmentDisplaySubsystem Display = GetDisplaySubsystem();
		if (Display != nullptr)
		{
			Display.SortItemsByDisplayOrder(RegistryType, Options);
		}
	}

	// 展示片段上的 DisplayId 决定物品归属哪个轮盘；未挂片段的物品不参与
	private bool MatchesRegistryType(UInventoryItemInstance Item) const
	{
		if (Item == nullptr)
		{
			return false;
		}

		const UInventoryFragment_ItemDisplay DisplayFragment = Cast<const UInventoryFragment_ItemDisplay>(
			Item.FindFragmentByClass(UInventoryFragment_ItemDisplay));
		return DisplayFragment != nullptr && DisplayFragment.DisplayId.RegistryType.Name == RegistryType.Name;
	}

	// 标签供 UMG 需要文字时使用；此处的查询同时预热了各选项的图标加载
	private void BuildLabels(TArray<FText>& OutLabels)
	{
		UEquipmentDisplaySubsystem Display = GetDisplaySubsystem();
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			FItemDisplayView View;
			if (Display != nullptr)
			{
				Display.GetDisplayForItem(Options[Index], View);
			}
			OutLabels.Add(View.DisplayName);
		}
	}

	private UInventoryManagerComponent GetInventory()
	{
		if (CachedInventory == nullptr)
		{
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				CachedInventory = UInventoryManagerComponent::Get(OwningController);
			}
		}
		return CachedInventory;
	}

	private UEquipmentDisplaySubsystem GetDisplaySubsystem()
	{
		if (CachedDisplaySubsystem == nullptr)
		{
			CachedDisplaySubsystem = Cast<UEquipmentDisplaySubsystem>(Subsystem::GetGameInstanceSubsystem(UEquipmentDisplaySubsystem));
		}
		return CachedDisplaySubsystem;
	}
}
