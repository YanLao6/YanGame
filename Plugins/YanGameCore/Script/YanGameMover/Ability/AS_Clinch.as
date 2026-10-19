/**
 * UAS_CharacterClinch — 贴身抓取输入技能（按住维持）
 *
 * 本技能只做两件事：在 game thread 扫描出落位点，再把抓取意图与落位点写进输入命令包，
 * 实际位移由 sim 侧 UChaosCharacterClinchCheck 与 FYanLayeredMove_Clinch 完成。
 * 落位点只在此处算一次并上传，服务器不重算——目标位置在发起端是插值外推的结果，
 * 两端各扫一次不保证得到同一落点，贴合位置会就此分歧。
 *
 * 抓取意图是持续标志而非脉冲：服务器须在 game thread 采样到它才能对被抓方下发效果，
 * 而 game tick 与仿真 tick 并非一一对应，脉冲会被漏掉。重复入队由 sim 侧按标签判重。
 *
 * 发起条件目前以「水平速度达阈值」近似空中冲刺途中的状态。
 * 若需严格限定为冲刺期间，须由冲刺技能向输入包写入一个持续标志供此处判读。
 */
class UAS_CharacterClinch : UMoverInputAbility
{
	/** 目标搜索半径（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SearchRadius = 400.f;

	/** 与速度方向的夹角在此以内才算撞上，用以排除擦身而过 */
	UPROPERTY(EditDefaultsOnly, Category = "Clinch", meta = (ClampMin = "1", ClampMax = "180", ForceUnits = "degrees"))
	float ImpactHalfAngle = 60.f;

	/** 两胶囊表面之间额外留出的间隙（cm），过小会让物理接触约束把双方推开 */
	UPROPERTY(EditDefaultsOnly, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SurfaceClearance = 5.f;

	/** 发起所需的最小水平速度（cm/s），用于把普通位移与冲刺撞击区分开 */
	UPROPERTY(EditDefaultsOnly, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinImpactSpeed = 700.f;

	/**
	 * 命中至双方一同下落之间的定格时长（毫秒）。
	 * 须大于「抓取意图上传服务器 + 被抓方效果下发回来」的往返耗时，否则被抓方的效果
	 * 尚未生效，共同的下落时刻已过，双方的高度就此错开且不再收敛。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "ms"))
	float FallDelayMs = 400.f;

	// 抓取期间持续为真，随每帧输入上传；松手即落回 false，贴身效果据此自行终结
	private bool bClinchActive = false;
	private FVector PendingAnchorPoint;
	private float64 PendingFallStartSimTimeMs = 0.0;

	//~Begin UMoverInputAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (CachedMoverComp == nullptr)
		{
			return;
		}

		// 此处读的是 game-thread 缓存态，只用于滤掉明显无效的按键；
		// 权威判定由 UChaosCharacterClinchCheck 的挂载模式保证
		UCharacterMoverComponent CharMoverComp = Cast<UCharacterMoverComponent>(CachedMoverComp);
		if (CharMoverComp == nullptr)
		{
			return;
		}

		if (CachedMoverComp.GetVelocity().Size2D() < MinImpactSpeed)
		{
			return;
		}

		FVector AnchorPoint;
		APawn Target = YanMoverAngelscript::FindClinchTarget(CachedMoverComp, SearchRadius, ImpactHalfAngle, SurfaceClearance, AnchorPoint);
		if (Target == nullptr)
		{
			return;
		}

		// 下落时刻在此一次取定并随输入上传，被抓方的效果取同一值，双方才能同时起落。
		// BaseSimTimeMs 处于服务器仿真时间轴，客户端预测时亦在同一轴上，两端可比
		PendingFallStartSimTimeMs = CachedMoverComp.GetLastTimeStep().BaseSimTimeMs + float64(FallDelayMs);
		PendingAnchorPoint = AnchorPoint;
		bClinchActive = true;
	}

	UFUNCTION(BlueprintOverride)
	void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
	{
		FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
		YanInputs.bWantsToClinch = bClinchActive;
		if (bClinchActive)
		{
			YanInputs.ClinchAnchorPoint = PendingAnchorPoint;
			YanInputs.ClinchFallStartSimTimeMs = PendingFallStartSimTimeMs;
		}

		// 挣脱位归被抓方的技能独占：双方同帧写同一字段会互相覆盖，
		// 发起方的解除因而改由抓取意图落回 false 表达
		YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		// 技能结束不注销 InputProducer（注销只发生在移除技能时），
		// 落回 false 的意图仍会随随后的每一帧上传，贴身效果据此终结
		bClinchActive = false;
		EndAbility();
	}
	//~End UMoverInputAbility Interface
}
