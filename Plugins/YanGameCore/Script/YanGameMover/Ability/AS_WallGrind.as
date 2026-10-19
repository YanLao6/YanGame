/**
 * UAS_CharacterWallGrind — 贴墙摩擦推进输入技能（按住维持）
 *
 * 本技能只做两件事：在 game thread 扫出贴墙折线，再把推进意图与折线写进输入命令包，
 * 实际位移由 sim 侧 UChaosCharacterWallGrindCheck 与 FYanLayeredMove_WallGrind 完成。
 * 折线只在此处扫一次并上传，服务器不重扫——沿墙扫描依赖 game-thread 的碰撞场景，
 * 两端各扫一次不保证得到同一条曲线，推进轨迹会就此分歧。
 *
 * 推进意图是持续标志而非脉冲：服务器须在 game thread 采样到它才能对被推方下发效果，
 * 而 game tick 与仿真 tick 并非一一对应，脉冲会被漏掉。重复入队由 sim 侧按标签判重。
 *
 * 发起条件目前以「水平速度达阈值」近似撞击途中的状态，与贴身抓取同一口径。
 */
class UAS_CharacterWallGrind : UMoverInputAbility
{
	/** 目标搜索半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SearchRadius = 400.f;

	/** 与速度方向的夹角在此以内才算撞上，用以排除擦身而过 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "1", ClampMax = "180", ForceUnits = "degrees"))
	float ImpactHalfAngle = 60.f;

	/** 发起所需的最小水平速度（cm/s），用于把普通位移与撞击区分开 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinImpactSpeed = 700.f;

	/** 自被推方位置向外探墙的距离（cm），超出即认为附近无墙，技能不发起 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "1", ForceUnits = "cm"))
	float WallProbeDistance = 200.f;

	/** 折线与墙面的距离（cm）：须容得下被推方的身位再加间隙，过小会让物理接触约束把双方推开 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "0", ForceUnits = "cm"))
	float PairSeparation = 100.f;

	/** 沿墙采样步长（cm）：越小越贴合曲面，代价是点数上限更快耗尽、可推进的距离变短 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "10", ForceUnits = "cm"))
	float PathStepDistance = 120.f;

	/**
	 * 摩擦减速度（cm/s²），此处只用于决定折线要扫多长。
	 * 应与 UChaosCharacterWallGrindCheck 上的同名值一致：那边据此决定实际滑多远。
	 * 不一致只会让两者中较短的一方先到，推进随之提前结束。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "0"))
	float Deceleration = 900.f;

	/** 折线点数上限，受输入包上限（16）收紧 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "2", ClampMax = "16"))
	int32 MaxPathPoints = 12;

	/**
	 * 命中至双方一同起滑之间的定格时长（毫秒）。
	 * 须大于「推进意图上传服务器 + 被推方效果下发回来」的往返耗时，否则被推方的效果
	 * 尚未生效，共同的起滑时刻已过，双方沿折线的进度就此错开且不再收敛。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "WallGrind", meta = (ClampMin = "0", ForceUnits = "ms"))
	float GrindDelayMs = 400.f;

	// 推进期间持续为真，随每帧输入上传；松手即落回 false，推进效果据此自行终结
	private bool bGrindActive = false;
	private TArray<FVector> PendingPath;
	private float PendingEntrySpeed = 0.f;
	private float64 PendingGrindStartSimTimeMs = 0.0;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (CachedMoverComp == nullptr)
		{
			return;
		}

		// 此处读的是 game-thread 缓存态，只用于滤掉明显无效的按键；
		// 权威判定由 UChaosCharacterWallGrindCheck 的挂载模式保证
		if (CachedMoverComp.GetVelocity().Size2D() < MinImpactSpeed)
		{
			return;
		}

		// 折线自被推方位置探墙：墙在被推方的另一侧，故须先确定推的是谁
		FVector AnchorPoint;
		APawn Target = YanMoverAngelscript::FindClinchTarget(CachedMoverComp, SearchRadius, ImpactHalfAngle, 0.f, AnchorPoint);
		if (Target == nullptr)
		{
			return;
		}

		TArray<FVector> Path;
		float32 EntrySpeed = 0.f;
		if (!YanMoverAngelscript::TraceWallGrindPath(CachedMoverComp,
		                                             Target.GetActorLocation(),
		                                             WallProbeDistance,
		                                             PairSeparation,
		                                             PathStepDistance,
		                                             Deceleration,
		                                             MaxPathPoints,
		                                             Path,
		                                             EntrySpeed))
		{
			return;
		}

		// 起滑时刻在此一次取定并随输入上传，被推方的效果取同一值，双方才能同时起滑。
		// BaseSimTimeMs 处于服务器仿真时间轴，客户端预测时亦在同一轴上，两端可比
		PendingGrindStartSimTimeMs = CachedMoverComp.GetLastTimeStep().BaseSimTimeMs + float64(GrindDelayMs);
		PendingPath = Path;
		PendingEntrySpeed = EntrySpeed;
		bGrindActive = true;
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bWantsToWallGrind = bGrindActive;
		if (bGrindActive)
		{
			// 折线在整个推进期间持续上传：服务器要在意图的上升沿采样到它才能构造被推方的效果，
			// 而发起端无从得知服务器是哪一帧采的样
			YanInputs.WallGrindPath = PendingPath;
			YanInputs.WallGrindEntrySpeed = PendingEntrySpeed;
			YanInputs.WallGrindStartSimTimeMs = PendingGrindStartSimTimeMs;
		}

		// 挣脱位归被推方的技能独占：双方同帧写同一字段会互相覆盖，
		// 发起方的解除因而改由推进意图落回 false 表达
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		// 技能结束不注销 InputProducer（注销只发生在移除技能时），
		// 落回 false 的意图仍会随随后的每一帧上传，推进效果据此终结
		bGrindActive = false;
		EndAbility();
	}
	//~End UMoverInputAbility Interface
}
