/**
 * UAS_KineticPull — 牵引：把视野锥内的目标一并朝自己拽过来。
 *
 * 按下即结算，一次性给每个目标一个指向施法者的速度，此后目标自由飞行不再受力。
 * 磁铁式的持续吸附是另一条通道（FYanLayeredMove_RadialAttraction），不在本类范围内。
 *
 * 网络模型：
 *  目标玩家的 Mover 输入包只有其本地端能写，跨 Pawn 施力无法走输入包路径。
 *  故筛选与施力一律在服务器执行，经 PullEffect 落到目标 ASC，
 *  再由 UMoverLaunchExecutionCalculation 以 Authority sim action 下发到目标 Mover。
 *  本地端只播放施法表现。
 *
 * 瞄准取自上传服务器的输入包而非本地摄像机：两端口径一致，服务器不会因视角差异
 * 选中另一批目标。方向也由服务器按其实时位置求得，不采信客户端算好的向量。
 *
 * 「谁会被拽」交由 PullEffect 裁决：本类只把锥内的候选交给 GAS，
 * 免疫、阵营、护盾等规则在 GE 资产上配置。
 *
 * 候选来自场内全部 Pawn 而非碰撞查询：牵引对象是战斗单位，判定条件是「带 ASC」
 * 而非「有碰撞体」，且对局内 Pawn 数量有限，遍历成本低于一次 overlap。
 *
 * 蓝图默认值需设置：
 *   PullEffect → UMoverLaunchGameplayEffect 或其子类
 */
class UAS_KineticPull : UYanGameplayAbility
{
	/** 生效距离上限（cm），自视点起算 */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxDistance = 1500.0f;

	/**
	 * 生效距离下限（cm）：比这更近的目标不受牵引。
	 *
	 * 贴脸即拽是牵引最难制衡的用法——站在虚空边缘一拽即杀，且对手没有反应余地。
	 * 留出下限后，牵引成为「拉开距离才成立」的手段，近身缠斗仍须靠普攻。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinDistance = 400.0f;

	/** 视野锥半角（度）：目标与视线夹角不超过此值才生效，总张角为其两倍 */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0", ClampMax = "180"))
	float ConeHalfAngleDegrees = 45.0f;

	/** 贴身目标（距离趋近 0）施加的速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float NearPullSpeed = 0.0f;

	/** 处于 MaxDistance 边缘的目标施加的速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float FarPullSpeed = 2400.0f;

	/**
	 * 速度随距离变化的指数，作用于 距离/MaxDistance：1 为线性，大于 1 则提速集中在远端。
	 * 两档速度反配（近大于远）即得越近吸得越狠的力场手感，本项同样适用。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0.01"))
	float SpeedDistanceExponent = 1.0f;

	/**
	 * 掺入拉扯方向的向上分量比例，0 为纯水平。
	 * 站在地面上的目标须先离地才拉得动——水平速度会被行走模式的地面约束当帧吃掉。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "0", ClampMax = "1"))
	float UpwardRatio = 0.25f;

	/** 单次生效的目标数上限，避免密集人群下的网络尖峰；超出时保留最贴近准心的若干个 */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull", Meta = (ClampMin = "1"))
	int32 MaxTargets = 8;

	/** 施加到目标的击飞 GE，须使用 UMoverLaunchExecutionCalculation（即 UMoverLaunchGameplayEffect 或其子类） */
	UPROPERTY(EditDefaultsOnly, Category = "KineticPull")
	TSubclassOf<UGameplayEffect> PullEffect;

	//~Begin UGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void ActivateAbility()
	{
		if (!CommitAbility())
		{
			EndAbility();
			return;
		}

		if (IsLocallyControlled())
		{
			OnPullCast();
		}

		AActor Avatar = GetAvatarActorFromActorInfo();
		if (Avatar != nullptr && Avatar.HasAuthority())
		{
			PullTargetsInCone(Avatar);
		}
	}

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
	//~End UGameplayAbility Interface

	/** 蓝图实现：本地施法表现（起手动作、音效、镜头等），仅在本地控制端触发。 */
	UFUNCTION(BlueprintEvent, Category = "KineticPull")
	void OnPullCast()
	{
	}

	// 服务器权威的施力入口
	private void PullTargetsInCone(AActor Avatar)
	{
		UAbilitySystemComponent InstigatorASC = GetAbilitySystemComponentFromActorInfo();
		UMoverComponent MoverComp = UMoverComponent::Get(Avatar);

		if (InstigatorASC == nullptr || MoverComp == nullptr || PullEffect.Get() == nullptr)
		{
			return;
		}

		// 视点既是筛选的球心，也是拉扯方向的终点：两者同源，筛选范围才不会与施力方向脱节
		FVector ViewLocation;
		FVector ViewForward;
		if (!YanMoverAngelscript::GetOwnerViewLocationAndForward(MoverComp, ViewLocation, ViewForward))
		{
			return;
		}

		TArray<AActor> Targets;
		GatherTargetsInCone(Avatar, ViewLocation, ViewForward, Targets);

		for (AActor Target : Targets)
		{
			UAbilitySystemComponent TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Target);
			if (TargetASC == nullptr)
			{
				continue;
			}

			YanMoverAngelscript::ApplyLaunchEffectToTarget(InstigatorASC, TargetASC, PullEffect, ComputePullVelocity(Target.GetActorLocation(), ViewLocation));
		}
	}

	// 收集锥内带 ASC 的 Pawn，按与视线的贴合度降序，超出 MaxTargets 时丢弃最偏离准心的
	private void GatherTargetsInCone(AActor Avatar, FVector ViewLocation, FVector ViewForward, TArray<AActor>& OutTargets) const
	{
		const float CosHalfAngle = Math::Cos(Math::DegreesToRadians(ConeHalfAngleDegrees));

		TArray<float> Alignments;
		TArray<AActor> Pawns;
		Gameplay::GetAllActorsOfClass(APawn, Pawns);

		for (AActor PawnActor : Pawns)
		{
			if (PawnActor == Avatar || UVerbMessageHelpers::GetAbilitySystemComponentFromObject(PawnActor) == nullptr)
			{
				continue;
			}

			const FVector ToTarget = PawnActor.GetActorLocation() - ViewLocation;
			const float Distance = ToTarget.Size();

			// 下限同时兜住「与视点重合时方向不成立」这一退化情形，故不再单独判零
			if (Distance > MaxDistance || Distance < Math::Max(MinDistance, 1.0f))
			{
				continue;
			}

			const float Alignment = ViewForward.DotProduct(ToTarget / Distance);
			if (Alignment < CosHalfAngle)
			{
				continue;
			}

			InsertByAlignment(OutTargets, Alignments, PawnActor, Alignment);
		}
	}

	// 按贴合度降序插入并把长度压在 MaxTargets 内：满员时挤掉的应是最偏的那个，而非碰巧最后遍历到的
	private void InsertByAlignment(TArray<AActor>& OutTargets, TArray<float>& OutAlignments, AActor Candidate, float Alignment) const
	{
		int32 InsertIndex = OutTargets.Num();

		for (int32 Index = 0; Index < OutTargets.Num(); ++Index)
		{
			if (Alignment > OutAlignments[Index])
			{
				InsertIndex = Index;
				break;
			}
		}

		if (InsertIndex >= MaxTargets)
		{
			return;
		}

		OutTargets.Insert(Candidate, InsertIndex);
		OutAlignments.Insert(Alignment, InsertIndex);

		if (OutTargets.Num() > MaxTargets)
		{
			OutTargets.RemoveAt(OutTargets.Num() - 1);
			OutAlignments.RemoveAt(OutAlignments.Num() - 1);
		}
	}

	// 方向自目标指向施法者视点，掺入向上分量后归一化；速率另按距离求得
	private FVector ComputePullVelocity(FVector TargetLocation, FVector ViewLocation) const
	{
		const FVector ToInstigator = ViewLocation - TargetLocation;
		const FVector PullDirection = (ToInstigator.GetSafeNormal2D() + FVector::UpVector * UpwardRatio).GetSafeNormal();

		return PullDirection * ResolvePullSpeed(ToInstigator.Size());
	}

	/**
	 * 按目标与施法者的距离在近远两档速度之间插值。
	 *
	 * 默认远快近慢：等速拉扯下远处目标的飞行时间随距离线性拉长，久到玩家来不及接后续。
	 * NearPullSpeed 为零且指数为一时速度正比于距离，到达时间不再随距离改变，
	 * 连招的节奏才不随交战距离漂移。该时间以 MaxDistance / FarPullSpeed 为下限，
	 * 实际值因上抬分量与重力偏长，须以实测为准。
	 */
	private float ResolvePullSpeed(float Distance) const
	{
		// 距离已在筛选阶段限于 MaxDistance 内，此处的钳制只为兜住 MaxDistance 配为零的退化情形
		const float NormalizedDistance = MaxDistance > 1.0f ? Math::Clamp(Distance / MaxDistance, 0.0f, 1.0f) : 1.0f;

		return Math::Lerp(NearPullSpeed, FarPullSpeed, Math::Pow(NormalizedDistance, SpeedDistanceExponent));
	}
}
