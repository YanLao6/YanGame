/**
 * AMinimapCaptureActor — 小地图的实时俯视捕获源。
 *
 * 正交相机俯视整个场地，渲染到运行时创建的 RenderTarget，由 UYanMinimapWidget 取用。
 * 视野固定罩住全场而非跟随玩家：1v1 的关键信息是"对手在哪、边缘在哪"，
 * 滚动视野反而会把这两样挤出画面。
 *
 * 捕获范围不在此处填写，而是从本地 UYanMinimapComponent 的 MapCenter / MapExtent 反推。
 * 两处各填一份的话对不上也不会报错，只会让标记整体偏移——故只保留一处权威。
 *
 * 默认按间隔捕获而非逐帧：地形只在平台塌陷时变化，逐帧重渲一张几乎不变的图是纯浪费。
 * 需要画面里的动态物体跟手时把 CaptureInterval 调小。
 *
 * 摆放：每张对战地图放一个即可，位置与旋转会在运行时被覆盖，随便放。
 */
class AMinimapCaptureActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneCaptureComponent2D Capture;
	default Capture.ProjectionType = ECameraProjectionMode::Orthographic;
	// 必须用 Final 系列：其余捕获源在 SceneCapturePixelShader.usf 里把 alpha 写成 0
	// （SCS_BaseColor 是 float4(BaseColor, 0)，SCS_SceneColorHDRNoAlpha 同样是 0，
	// 名字里的 NoAlpha 指"不写有意义的 alpha"而非"不透明"），
	// 贴到 Slate 的 Image 上会整片透明。Final 系列绕开该着色器，取后处理输出，alpha 为 1。
	default Capture.CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// 捕获由本类的定时器驱动，关掉引擎自带的逐帧与移动触发
	default Capture.bCaptureEveryFrame = false;
	default Capture.bCaptureOnMovement = false;

	/** RenderTarget 边长（像素），须为正方形以匹配 MapExtent 两轴共用 */
	UPROPERTY(EditAnywhere, Category = "Minimap", Meta = (ClampMin = "64"))
	int32 Resolution = 1024;

	/** 相机离场地中心的高度，须高于全部几何体 */
	UPROPERTY(EditAnywhere, Category = "Minimap")
	float CaptureHeight = 5000.0f;

	/** 捕获间隔（秒）。地形静止时可放大，需要动态物体跟手时调小 */
	UPROPERTY(EditAnywhere, Category = "Minimap", Meta = (ClampMin = "0.0"))
	float CaptureInterval = 0.5f;

	/** 供小地图控件取用的渲染目标，运行时创建 */
	UPROPERTY(BlueprintReadOnly, Category = "Minimap")
	UTextureRenderTarget2D RenderTarget;

	// 是否已从小地图组件读到范围并对齐；PlayerController 未就绪时会延到后续 tick
	private bool bAligned = false;

	private FTimerHandle CaptureTimer;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		// 专用服务器不渲染，建 RenderTarget 只会白占显存
		APlayerController LocalController = Gameplay::GetPlayerController(0);
		if (LocalController == nullptr || !LocalController.IsLocalController())
		{
			return;
		}

		RenderTarget = Rendering::CreateRenderTarget2D(Resolution, Resolution);
		if (RenderTarget == nullptr)
		{
			return;
		}

		Capture.TextureTarget = RenderTarget;

		// 立即试一次，成功则首帧就有画面；失败则由定时器继续重试
		CaptureTick();
		CaptureTimer = System::SetTimer(this, n"CaptureTick", Math::Max(CaptureInterval, 0.01f), true);
	}

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason EndPlayReason)
	{
		System::ClearAndInvalidateTimerHandle(CaptureTimer);
	}

	UFUNCTION()
	private void CaptureTick()
	{
		if (!bAligned)
		{
			// 对齐失败说明本地玩家或其小地图组件尚未就绪，下个间隔再试
			if (!TryAlignToMinimapBounds())
			{
				return;
			}
		}

		Capture.CaptureScene();
	}

	// 从本地玩家的小地图组件读取范围，据此摆好相机；组件缺失时返回 false
	private bool TryAlignToMinimapBounds()
	{
		APlayerController LocalController = Gameplay::GetPlayerController(0);
		if (LocalController == nullptr)
		{
			return false;
		}

		UYanMinimapComponent Minimap = UYanMinimapComponent::Get(LocalController);
		if (Minimap == nullptr)
		{
			return false;
		}

		// Pitch -90 使相机局部上方向落在世界 +X（北），右方向落在世界 +Y（东），
		// 与控件里"屏幕X = 世界Y、屏幕Y = -世界X"的换轴一致。改 Yaw 会让底图与标记错开。
		SetActorLocation(FVector(Minimap.MapCenter.X, Minimap.MapCenter.Y, CaptureHeight));
		SetActorRotation(FRotator(-90.0f, 0.0f, 0.0f));

		// MapExtent 是半宽，正交视野要的是全宽
		Capture.OrthoWidth = Minimap.MapExtent * 2.0f;

		bAligned = true;
		return true;
	}
}
