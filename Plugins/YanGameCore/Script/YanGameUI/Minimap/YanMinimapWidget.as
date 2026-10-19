/**
 * UYanMinimapWidget — 竞技场地图控件，同时充当角落小地图与全屏大地图。
 *
 * 取景统一为"中心 + 半径"：中心是一处世界坐标，半径以米计，表示中心到地图边缘的实地距离。
 * 半径不从场地尺寸推导，因此同一张底图可以按任意比例呈现，缩放也不必换算成场地的百分比。
 *
 * 小地图把中心钉在自机上；大地图开放滚轮缩放与左键拖动，拖动一次后中心即脱离自机，
 * 停在玩家看的地方——观察期间被自机拽回去会让拖动失去意义。
 *
 * 底图是场景里 AMinimapCaptureActor 的实时 RenderTarget（俯视全场、低频重渲），
 * 缩放与平移只作用于控件侧的渲染变换，捕获范围与开销恒定不变。
 * 取景半径超出捕获范围时底图四周留空，那是没有地图数据的区域，不是绘制缺陷。
 * 场景里没有该 Actor 时退回 TerrainTexture 静态底图。
 *
 * 自机箭头与比例尺由 OnPaint 矢量绘制而非贴图：地图在不同分辨率与缩放下尺寸差异极大，
 * 位图缩放会糊掉朝向与刻度这两项唯一有效的信息。
 *
 * 换轴规则：世界 X 向北、Y 向东，屏幕 X 向右、Y 向下，故 屏幕X = 世界Y、屏幕Y = -世界X。
 *
 * 位置逐帧刷新：底图随取景中心移动，低频更新会肉眼可见地跳。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UImage       TerrainImage    —— 地形底图，须置于容器底层且填满控件
 *   UCanvasPanel MarkerContainer —— 标记容器，须与 TerrainImage 完全重叠且填满
 *
 * WBP 默认值需设置：
 *   OpponentMarkerClass  → 敌方标记控件类，一个带 UImage 的 UserWidget 即可
 *   TerrainTexture       → 可选；仅在场景里没有捕获 Actor 时作为静态底图
 *   开启 bAllowNavigation 时，ActivatedVisibility 须为 Visible，
 *   否则 Slate 的命中测试不会把鼠标事件冒泡到本控件，滚轮与拖动都收不到。
 *
 * 场地中心与捕获范围来自 UYanMinimapComponent 的 MapCenter / MapExtent，
 * 捕获 Actor 也从同一处反推，底图与标记因此天然对齐。
 */
class UYanMinimapWidget : UYanUserWidget
{
	/** 地形底图，置于标记之下 */
	UPROPERTY(BindWidget)
	UImage TerrainImage;

	/** 标记容器，须与底图完全重叠 */
	UPROPERTY(BindWidget)
	UCanvasPanel MarkerContainer;

	/** 静态底图，仅在场景里没有 AMinimapCaptureActor 时使用 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	UTexture2D TerrainTexture;

	/** 敌方标记控件类，一个带 UImage 的 UserWidget 即可 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	TSubclassOf<UUserWidget> OpponentMarkerClass;

	/** 取景半径（米）：取景中心到地图边缘的实地距离 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|View", Meta = (ClampMin = "1.0"))
	float ViewRadiusMeters = 12.0f;

	/** 取景中心跟随自机；大地图应关闭，改由打开时对准自机、其后交给拖动 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|View")
	bool bFollowSelf = true;

	/** 开放滚轮缩放与左键拖动 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Navigation")
	bool bAllowNavigation = false;

	/** 缩放下限：能放到多大（米） */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Navigation", Meta = (ClampMin = "1.0"))
	float MinViewRadiusMeters = 5.0f;

	/** 缩放上限：能拉到多远（米）。实际上限还会被场地尺寸压制，拉不到底图之外 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Navigation", Meta = (ClampMin = "1.0"))
	float MaxViewRadiusMeters = 250.0f;

	/** 每格滚轮的缩放倍率 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Navigation", Meta = (ClampMin = "1.01"))
	float ZoomStepRatio = 1.25f;

	/** 绘制比例尺，读数取 1/2/5 整数档 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Navigation")
	bool bShowScaleBar = false;

	/** 比例尺配色 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Navigation")
	FLinearColor ScaleBarColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	/** 标记尺寸（像素） */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Layout")
	FVector2D MarkerSize = FVector2D(20.0f, 20.0f);

	/** 敌方在视野内时的标记着色 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Layout")
	FLinearColor OpponentColor = FLinearColor(1.0f, 0.28f, 0.16f, 1.0f);

	/** 敌方已在视野外、标记被夹在边界时的着色 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Layout")
	FLinearColor OpponentOffMapColor = FLinearColor(1.0f, 0.28f, 0.16f, 0.45f);

	/** 自机箭头是否随视角旋转；关闭则恒定朝上 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|SelfArrow")
	bool bRotateSelfArrow = true;

	/** 自机箭头半高（像素），箭头总高为其两倍 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|SelfArrow", Meta = (ClampMin = "4.0"))
	float SelfArrowSize = 11.0f;

	/** 自机箭头填充色 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|SelfArrow")
	FLinearColor SelfArrowColor = FLinearColor(0.24f, 1.0f, 0.36f, 1.0f);

	/** 自机箭头描边色，用于压住浅色地形 */
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|SelfArrow")
	FLinearColor SelfArrowOutlineColor = FLinearColor(0.02f, 0.08f, 0.03f, 1.0f);

	// 组件挂在 PlayerController 上，控件 Construct 时未必已就绪，故惰性获取
	private UYanMinimapComponent CachedMinimap;

	// 敌方标记池，长度按当前敌方数量增减
	private TArray<UUserWidget> OpponentMarkers;

	// 实时底图是否已接上；捕获 Actor 的 RenderTarget 在其 BeginPlay 才创建，故需重试
	private bool bTerrainBound = false;

	// 供 OnPaint 取用的自机状态，逐帧在 Tick 里刷新
	private bool bHasSelf = false;
	private FVector2D SelfWorldNormalized = FVector2D::ZeroVector;
	private float SelfYawDegrees = 0.0f;

	// 取景状态：中心为世界坐标（cm），半径同样以 cm 参与换算，只在对外呈现时折成米
	private FVector2D ViewCenterWorld = FVector2D::ZeroVector;
	private bool bViewCenterValid = false;
	private bool bFollowingSelf = true;
	private float ViewRadiusCm = 1200.0f;

	// 场地范围的副本，OnPaint 是 const 取不到组件，故随 Tick 缓存
	private FVector2D MapCenterCm = FVector2D::ZeroVector;
	private float MapExtentCm = 2500.0f;

	// 底图缩放倍数与非正方形控件的等比修正
	private float TerrainScale = 1.0f;
	private FVector2D AspectScale = FVector2D(1.0f, 1.0f);

	// 布局尺寸的副本，鼠标换算与 OnPaint 共用
	private FVector2D CachedLocalSize = FVector2D::ZeroVector;
	private float CachedSquareSize = 0.0f;

	private bool bDragging = false;
	private FVector2D LastDragLocal = FVector2D::ZeroVector;

	// 打开前的光标显示状态，关闭时原样还回去
	private bool bRestoreMouseCursor = false;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		// 设计时也套上底图，便于在编辑器里直接看清版式
		if (TerrainImage != nullptr && TerrainTexture != nullptr)
		{
			TerrainImage.SetBrushFromTexture(TerrainTexture);
		}
	}

	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		ViewRadiusCm = ClampRadius(ViewRadiusMeters * 100.0f);
		bFollowingSelf = bFollowSelf;
		bViewCenterValid = false;
		bDragging = false;

		// 底图放大后会越出控件，不裁剪会盖住相邻 HUD
		SetClipping(EWidgetClipping::ClipToBounds);

		if (bAllowNavigation)
		{
			// 命中测试不可见的控件不会出现在 Slate 的事件路径里，滚轮与拖动都收不到
			SetVisibility(ESlateVisibility::Visible);

			// 输入模式为 Game 时光标默认不显示，而拖动地图需要看得见指针
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				bRestoreMouseCursor = OwningController.bShowMouseCursor;
				OwningController.bShowMouseCursor = true;
			}
		}
	}

	UFUNCTION(BlueprintOverride)
	void Destruct()
	{
		if (!bAllowNavigation)
		{
			return;
		}

		APlayerController OwningController = GetOwningPlayer();
		if (OwningController != nullptr)
		{
			OwningController.bShowMouseCursor = bRestoreMouseCursor;
		}
	}

	UFUNCTION(BlueprintOverride)
	void Tick(FGeometry MyGeometry, float InDeltaTime)
	{
		TryBindTerrainSource();

		UYanMinimapComponent Minimap = GetMinimap();
		if (Minimap == nullptr)
		{
			return;
		}

		MapCenterCm = Minimap.MapCenter;
		MapExtentCm = Math::Max(Minimap.MapExtent, 1.0f);

		CachedLocalSize = MyGeometry.GetLocalSize();
		UpdateAspectScale(CachedLocalSize);

		RefreshSelf(Minimap);
		UpdateViewCenter();
		EnforceViewBounds();

		TerrainScale = MapExtentCm / ViewRadiusCm;
		UpdateTerrainTransform();
		RefreshOpponents(Minimap);
	}

	UFUNCTION(BlueprintOverride)
	void OnPaint(FPaintContext& Context) const
	{
		const FVector2D LocalSize = Context.GetAllottedGeometry().GetLocalSize();
		if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
		{
			return;
		}

		if (bHasSelf)
		{
			const FVector2D ViewPosition = WorldToView(NormalizedToWorld(SelfWorldNormalized));
			// 自机越出取景时不画箭头：钉在边缘的箭头会被误读成还在场内
			if (Math::Abs(ViewPosition.X) <= 1.0f && Math::Abs(ViewPosition.Y) <= 1.0f)
			{
				DrawSelfArrow(Context, ViewToLocalPixels(ViewPosition, LocalSize), bRotateSelfArrow ? SelfYawDegrees : 0.0f);
			}
		}

		if (bShowScaleBar)
		{
			DrawScaleBar(Context, LocalSize);
		}
	}

	//~Begin UUserWidget Interface
	UFUNCTION(BlueprintOverride)
	FEventReply OnMouseWheel(FGeometry MyGeometry, FPointerEvent MouseEvent)
	{
		if (!bAllowNavigation)
		{
			return FEventReply::Unhandled();
		}

		ZoomByWheel(MouseEvent.GetWheelDelta());
		return FEventReply::Handled();
	}

	UFUNCTION(BlueprintOverride)
	FEventReply OnMouseButtonDown(FGeometry MyGeometry, FPointerEvent MouseEvent)
	{
		if (!bAllowNavigation || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FEventReply::Unhandled();
		}

		bDragging = true;
		LastDragLocal = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		// 捕获鼠标，指针拖出控件范围后仍能继续收到移动事件
		FEventReply Reply = FEventReply::Handled();
		Reply.CaptureMouse(this);
		return Reply;
	}

	UFUNCTION(BlueprintOverride)
	FEventReply OnMouseMove(FGeometry MyGeometry, FPointerEvent MouseEvent)
	{
		if (!bDragging)
		{
			return FEventReply::Unhandled();
		}

		const FVector2D CurrentLocal = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		PanByLocalDelta(CurrentLocal - LastDragLocal);
		LastDragLocal = CurrentLocal;

		return FEventReply::Handled();
	}

	UFUNCTION(BlueprintOverride)
	FEventReply OnMouseButtonUp(FGeometry MyGeometry, FPointerEvent MouseEvent)
	{
		if (!bDragging || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FEventReply::Unhandled();
		}

		bDragging = false;

		FEventReply Reply = FEventReply::Handled();
		Reply.ReleaseMouseCapture();
		return Reply;
	}
	//~End UUserWidget Interface

	/** 把取景拉回自机并恢复跟随，供绑定"回到自身"操作。 */
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void RecenterOnSelf()
	{
		bFollowingSelf = true;
		bViewCenterValid = false;
	}

	/** 当前取景直径（米），供外部显示读数。 */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	float GetViewDiameterMeters() const
	{
		return ViewRadiusCm * 0.02f;
	}

	// 把捕获 Actor 的 RenderTarget 接到底图上；接上后不再查找
	private void TryBindTerrainSource()
	{
		if (bTerrainBound || TerrainImage == nullptr)
		{
			return;
		}

		TArray<AActor> FoundActors;
		Gameplay::GetAllActorsOfClass(AMinimapCaptureActor, FoundActors);

		for (AActor FoundActor : FoundActors)
		{
			AMinimapCaptureActor CaptureActor = Cast<AMinimapCaptureActor>(FoundActor);
			if (CaptureActor == nullptr || CaptureActor.RenderTarget == nullptr)
			{
				continue;
			}

			// RenderTarget 不是 UTexture2D，只能走 SetBrushResourceObject
			TerrainImage.SetBrushResourceObject(CaptureActor.RenderTarget);
			bTerrainBound = true;
			return;
		}
	}

	private void RefreshSelf(UYanMinimapComponent Minimap)
	{
		FYanMinimapMarker Marker;
		if (!Minimap.GetSelfMarker(Marker))
		{
			// 重生间隙没有 Pawn；保留最后一次位置，取景不必弹回场地中心
			bHasSelf = false;
			return;
		}

		bHasSelf = true;
		SelfWorldNormalized = Marker.NormalizedPosition;
		SelfYawDegrees = Minimap.GetViewYawDegrees();
	}

	// 跟随时取景中心即自机；脱离跟随后只由拖动改变，首帧仍对准自机
	private void UpdateViewCenter()
	{
		if (bViewCenterValid && !bFollowingSelf)
		{
			return;
		}

		if (bHasSelf)
		{
			ViewCenterWorld = NormalizedToWorld(SelfWorldNormalized);
			bViewCenterValid = true;
		}
		else if (!bViewCenterValid)
		{
			// 还没有自机可对准时先落在场地中心，避免首帧取景落在世界原点
			ViewCenterWorld = MapCenterCm;
			bViewCenterValid = true;
		}
	}

	// 取景不得越出底图：拖动与缩放各自夹一遍容易漏，统一在这里收口，逐帧生效
	private void EnforceViewBounds()
	{
		if (!bAllowNavigation)
		{
			// 跟随取景由自机位置决定，越界与否不是玩家能操作的，夹住反而破坏"以自己为中心"
			return;
		}

		// 视野拉得比场地还大时，多出来的部分没有地图数据可看
		ViewRadiusCm = Math::Min(ViewRadiusCm, MapExtentCm);

		// 中心离场地中心最远只能到"场地半径减视野半径"，此时视野边缘正好贴住底图边缘
		const float Limit = Math::Max(MapExtentCm - ViewRadiusCm, 0.0f);
		ViewCenterWorld.X = Math::Clamp(ViewCenterWorld.X, MapCenterCm.X - Limit, MapCenterCm.X + Limit);
		ViewCenterWorld.Y = Math::Clamp(ViewCenterWorld.Y, MapCenterCm.Y - Limit, MapCenterCm.Y + Limit);
	}

	// 底图铺满控件，控件却未必是正方形；按短边收成正方形，长边方向留边而非拉伸地形
	private void UpdateAspectScale(FVector2D LocalSize)
	{
		if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
		{
			return;
		}

		CachedSquareSize = Math::Min(LocalSize.X, LocalSize.Y);
		AspectScale = FVector2D(CachedSquareSize / LocalSize.X, CachedSquareSize / LocalSize.Y);
	}

	// 底图整幅代表 MapCenter ± MapExtent，缩放到当前取景后把场地中心挪到它该在的位置
	private void UpdateTerrainTransform()
	{
		if (TerrainImage == nullptr)
		{
			return;
		}

		TerrainImage.SetRenderScale(FVector2D(AspectScale.X * TerrainScale, AspectScale.Y * TerrainScale));

		const FVector2D ScreenPosition = ToScreenSpace(WorldToView(MapCenterCm));
		TerrainImage.SetRenderTranslation(FVector2D(
			ScreenPosition.X * CachedSquareSize * 0.5f,
			ScreenPosition.Y * CachedSquareSize * 0.5f));
	}

	private void RefreshOpponents(UYanMinimapComponent Minimap)
	{
		TArray<FYanMinimapMarker> Markers;
		Minimap.GetOtherMarkers(Markers);

		SyncOpponentPool(Markers.Num());

		for (int32 Index = 0; Index < OpponentMarkers.Num(); ++Index)
		{
			UUserWidget MarkerWidget = OpponentMarkers[Index];
			if (MarkerWidget == nullptr)
			{
				continue;
			}

			const FVector2D ViewPosition = WorldToView(NormalizedToWorld(Markers[Index].NormalizedPosition));
			const FVector2D ClampedPosition = FVector2D(
				Math::Clamp(ViewPosition.X, -1.0f, 1.0f),
				Math::Clamp(ViewPosition.Y, -1.0f, 1.0f));

			PlaceMarker(MarkerWidget, ClampedPosition);

			// 视野外的对手正是最该被看见的时刻，故不隐藏，只用半透明区分
			const bool bOffView = !ViewPosition.Equals(ClampedPosition, 0.001f);
			MarkerWidget.SetColorAndOpacity(bOffView ? OpponentOffMapColor : OpponentColor);
		}
	}

	// 池长度与敌方数量对齐；数量在死亡与重生之间会变化，故每次都校对
	private void SyncOpponentPool(int32 DesiredCount)
	{
		if (MarkerContainer == nullptr || OpponentMarkerClass.Get() == nullptr)
		{
			return;
		}

		while (OpponentMarkers.Num() > DesiredCount)
		{
			const int32 LastIndex = OpponentMarkers.Num() - 1;
			if (OpponentMarkers[LastIndex] != nullptr)
			{
				OpponentMarkers[LastIndex].RemoveFromParent();
			}
			OpponentMarkers.RemoveAt(LastIndex);
		}

		APlayerController OwningController = GetOwningPlayer();
		while (OpponentMarkers.Num() < DesiredCount)
		{
			UUserWidget MarkerWidget = WidgetBlueprint::CreateWidget(OpponentMarkerClass, OwningController);
			if (MarkerWidget == nullptr)
			{
				// 类配错时直接收手，否则本帧会空转到 DesiredCount 次
				return;
			}

			MarkerContainer.AddChild(MarkerWidget);
			ApplyMarkerLayout(MarkerWidget);
			OpponentMarkers.Add(MarkerWidget);
		}
	}

	// 锚点定位所需的槽位设置，只在控件加入容器时做一次
	private void ApplyMarkerLayout(UWidget MarkerWidget)
	{
		if (MarkerWidget == nullptr)
		{
			return;
		}

		// 标记不参与命中测试，否则会挡住地图自身的拖动
		MarkerWidget.SetVisibility(ESlateVisibility::HitTestInvisible);

		UCanvasPanelSlot CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget.Slot);
		if (CanvasSlot == nullptr)
		{
			return;
		}

		CanvasSlot.SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot.SetSize(MarkerSize);
		// 位置全部由锚点承担，偏移恒为零
		CanvasSlot.SetPosition(FVector2D::ZeroVector);
	}

	// 定位不用像素偏移而用锚点：锚点是控件尺寸的比例，地图缩放到任何尺寸都不必重算
	private void PlaceMarker(UWidget MarkerWidget, FVector2D ViewPosition)
	{
		UCanvasPanelSlot CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget.Slot);
		if (CanvasSlot == nullptr)
		{
			return;
		}

		const FVector2D ScreenPosition = ToScreenSpace(ViewPosition);
		const FVector2D AnchorPoint = FVector2D(
			0.5f + ScreenPosition.X * 0.5f * AspectScale.X,
			0.5f + ScreenPosition.Y * 0.5f * AspectScale.Y);

		FAnchors MarkerAnchors;
		MarkerAnchors.Minimum = AnchorPoint;
		MarkerAnchors.Maximum = AnchorPoint;
		CanvasSlot.SetAnchors(MarkerAnchors);
	}

	// 归一化坐标是相对 MapCenter / MapExtent 的，取景换算一律先还原成世界坐标
	private FVector2D NormalizedToWorld(FVector2D WorldNormalized) const
	{
		return FVector2D(
			MapCenterCm.X + WorldNormalized.X * MapExtentCm,
			MapCenterCm.Y + WorldNormalized.Y * MapExtentCm);
	}

	// 世界坐标 → 取景归一化，[-1, 1] 即当前可见范围
	private FVector2D WorldToView(FVector2D WorldPosition) const
	{
		return FVector2D(
			(WorldPosition.X - ViewCenterWorld.X) / ViewRadiusCm,
			(WorldPosition.Y - ViewCenterWorld.Y) / ViewRadiusCm);
	}

	// 世界轴（X 北、Y 东）→ 屏幕轴（X 右、Y 下）
	private FVector2D ToScreenSpace(FVector2D WorldAxisPosition) const
	{
		return FVector2D(WorldAxisPosition.Y, -WorldAxisPosition.X);
	}

	// 取景归一化 → 控件局部像素，正方形取景居中放置
	private FVector2D ViewToLocalPixels(FVector2D ViewPosition, FVector2D LocalSize) const
	{
		const float SquareSize = Math::Min(LocalSize.X, LocalSize.Y);
		const FVector2D ScreenPosition = ToScreenSpace(ViewPosition);

		return FVector2D(
			LocalSize.X * 0.5f + ScreenPosition.X * SquareSize * 0.5f,
			LocalSize.Y * 0.5f + ScreenPosition.Y * SquareSize * 0.5f);
	}

	// 拖动是把地图内容拉过来，故取景中心朝相反方向移动
	private void PanByLocalDelta(FVector2D LocalDelta)
	{
		if (CachedSquareSize <= 0.0f)
		{
			return;
		}

		// 拖过之后不再跟随，否则下一帧就被自机拽回去
		bFollowingSelf = false;

		const float WorldPerPixel = (ViewRadiusCm * 2.0f) / CachedSquareSize;
		ViewCenterWorld.X += LocalDelta.Y * WorldPerPixel;
		ViewCenterWorld.Y -= LocalDelta.X * WorldPerPixel;
	}

	// 一格滚轮即一档缩放，倍率恒定使得连续缩放的手感与当前比例无关
	private void ZoomByWheel(float WheelDelta)
	{
		if (Math::IsNearlyZero(WheelDelta))
		{
			return;
		}

		float Factor = 1.0f;
		const int32 StepCount = Math::Max(Math::Abs(int32(WheelDelta)), 1);
		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Factor *= ZoomStepRatio;
		}

		ViewRadiusCm = ClampRadius(WheelDelta > 0.0f ? ViewRadiusCm / Factor : ViewRadiusCm * Factor);
	}

	private float ClampRadius(float RadiusCm) const
	{
		return Math::Clamp(RadiusCm, MinViewRadiusMeters * 100.0f, MaxViewRadiusMeters * 100.0f);
	}

	// 箭头形状：尖端朝上，尾部内凹，凹口让朝向在小尺寸下仍能一眼读出
	private void DrawSelfArrow(FPaintContext& Context, FVector2D Center, float YawDegrees) const
	{
		const float Size = SelfArrowSize;
		const FVector2D Tip   = Center + RotateVector(FVector2D( 0.00f, -1.00f) * Size, YawDegrees);
		const FVector2D Left  = Center + RotateVector(FVector2D(-0.62f,  0.72f) * Size, YawDegrees);
		const FVector2D Notch = Center + RotateVector(FVector2D( 0.00f,  0.30f) * Size, YawDegrees);
		const FVector2D Right = Center + RotateVector(FVector2D( 0.62f,  0.72f) * Size, YawDegrees);

		// 描边先画、填充后压：深色只在轮廓外侧露出半个线宽，浅色地形上也能看清边界
		TArray<FVector2D> Outline;
		Outline.Add(Tip);
		Outline.Add(Left);
		Outline.Add(Notch);
		Outline.Add(Right);
		Outline.Add(Tip);
		Context.DrawLines(Outline, SelfArrowOutlineColor, Math::Max(Size * 0.28f, 2.0f), true);

		FillTriangle(Context, Tip, Left, Notch);
		FillTriangle(Context, Tip, Notch, Right);
	}

	// Slate 没有填充图元，实心三角用自顶点向底边发散的线束铺出
	private void FillTriangle(FPaintContext& Context, FVector2D Apex, FVector2D BaseStart, FVector2D BaseEnd) const
	{
		const FVector2D BaseSpan = BaseEnd - BaseStart;
		// 线束间距压到线宽之下才不会露出缝隙
		const float Thickness = 2.0f;
		const int32 StepCount = Math::Max(int32(BaseSpan.Size() / (Thickness * 0.5f)), 4);

		for (int32 Step = 0; Step <= StepCount; ++Step)
		{
			const float Alpha = float(Step) / float(StepCount);

			TArray<FVector2D> Segment;
			Segment.Add(Apex);
			Segment.Add(BaseStart + BaseSpan * Alpha);
			Context.DrawLines(Segment, SelfArrowColor, Thickness, true);
		}
	}

	// 比例尺读数取整数档：缩放时长度连续变、读数跳档，比反过来更易读
	private void DrawScaleBar(FPaintContext& Context, FVector2D LocalSize) const
	{
		const float SquareSize = Math::Min(LocalSize.X, LocalSize.Y);
		if (SquareSize <= 0.0f)
		{
			return;
		}

		const float PixelsPerMeter = (SquareSize * 0.5f) / (ViewRadiusCm * 0.01f);
		const float SegmentMeters = PickScaleSegmentMeters(PixelsPerMeter);
		const float SegmentPixels = SegmentMeters * PixelsPerMeter;

		const float Margin = 20.0f;
		const float BaseY = LocalSize.Y - Margin;
		const float CapHeight = 7.0f;

		TArray<FVector2D> Bar;
		Bar.Add(FVector2D(Margin, BaseY - CapHeight));
		Bar.Add(FVector2D(Margin, BaseY));
		Bar.Add(FVector2D(Margin + SegmentPixels, BaseY));
		Bar.Add(FVector2D(Margin + SegmentPixels, BaseY - CapHeight));
		Context.DrawLines(Bar, ScaleBarColor, 2.0f, true);

		Context.DrawText(FormatMeters(SegmentMeters), FVector2D(Margin, BaseY - CapHeight - 20.0f), ScaleBarColor);
	}

	// 目标长度约 120 像素，向 1/2/5 × 10^n 取档
	private float PickScaleSegmentMeters(float PixelsPerMeter) const
	{
		const float TargetMeters = 120.0f / Math::Max(PixelsPerMeter, 0.0001f);

		// 读数保持整数米：最小取景半径也有数米，再细的档位没有意义
		float Magnitude = 1.0f;
		for (int32 Step = 0; Step < 8 && TargetMeters / Magnitude >= 10.0f; ++Step)
		{
			Magnitude *= 10.0f;
		}

		const float Normalized = TargetMeters / Magnitude;
		if (Normalized >= 5.0f)
		{
			return 5.0f * Magnitude;
		}
		if (Normalized >= 2.0f)
		{
			return 2.0f * Magnitude;
		}
		return Magnitude;
	}

	private FString FormatMeters(float Meters) const
	{
		return String::Conv_IntToString(int32(Meters + 0.5f)) + " m";
	}

	// 屏幕坐标系 Y 轴向下，正角度即顺时针，与世界 Yaw 同向
	private FVector2D RotateVector(FVector2D Vector, float AngleDegrees) const
	{
		const float Radians = Math::DegreesToRadians(AngleDegrees);
		const float SinValue = Math::Sin(Radians);
		const float CosValue = Math::Cos(Radians);

		return FVector2D(
			Vector.X * CosValue - Vector.Y * SinValue,
			Vector.X * SinValue + Vector.Y * CosValue);
	}

	private UYanMinimapComponent GetMinimap()
	{
		if (CachedMinimap == nullptr)
		{
			APlayerController OwningController = GetOwningPlayer();
			if (OwningController != nullptr)
			{
				CachedMinimap = UYanMinimapComponent::Get(OwningController);
			}
		}
		return CachedMinimap;
	}
}
