/**
 * 分层 Locomotion 动画蓝图的基类，接管自 Lyra 迁入资产原先的 ULyraAnimInstance。
 *
 * 只提供动画蓝图自身无法完成的两件事：地面距离采样，以及 GAS 标签到蓝图变量的映射。
 * 速度、朝向、步态等运动学数据由 ABP 在 thread-safe 更新中自行解算，因此本类刻意
 * 不持有同类属性——基类与 ABP 蓝图变量同名会使 ABP 在编译期创建属性失败。
 *
 * 与 [UYanCharacterAnimInstance] 的分工：后者面向「数据全部由脚本提供、AnimGraph 只消费」
 * 的动画蓝图；本类面向自带完整数据层的分层 Locomotion 蓝图，两者不可互换。
 */
class UYanLocomotionAnimInstance : UAnimInstance
{
	/**
	 * GAS 标签到派生 ABP 布尔变量的映射，在 ABP 资产的 Class Defaults 上配置。
	 * 标签增删时由 ASC 回调直接写入目标变量，AnimGraph 无需自行查询标签。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	/** 胶囊底部到地面的距离（cm），贴地为 0；腾空时驱动落地预判 */
	UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
	float GroundDistance = -1.f;

	// ---- Mover 侧读数 ----
	// 迁入的 ABP 原先经 property access 读 CharacterMovementComponent，本项目为 Mover 体系，
	// 该组件恒为 null。故在游戏线程采样到下列属性，ABP 改读它们——属性读取本身线程安全，
	// 而 Pawn 上的 getter 未标记 thread-safe，不能在 ABP 的 worker 线程更新中直接调用。

	/** 贴地标志，替代 CharacterMovementComponent::IsMovingOnGround */
	UPROPERTY(BlueprintReadOnly, Category = "Mover State")
	bool MoverIsOnGround = false;

	/** 蹲伏标志，替代 CharacterMovementComponent::IsCrouching */
	UPROPERTY(BlueprintReadOnly, Category = "Mover State")
	bool MoverIsCrouching = false;

	/** 滑铲标志，驱动 LocomotionSM 的滑铲状态；模式经 Mover 复制，各端一致 */
	UPROPERTY(BlueprintReadOnly, Category = "Mover State")
	bool MoverIsSliding = false;

	/**
	 * 世界空间水平加速度（cm/s²，Z 恒为 0），由速度差分得到，
	 * 替代 CharacterMovementComponent::GetCurrentAcceleration。
	 * 就地清零竖直分量，是因为消费方只取水平向判定急转，无需在图里再做一次投影。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mover State")
	FVector MoverAcceleration = FVector::ZeroVector;

	/** 重力 Z 分量（cm/s²，向下为负），替代 CharacterMovementComponent::GetGravityZ */
	UPROPERTY(BlueprintReadOnly, Category = "Mover State")
	float MoverGravityZ = -980.f;

	/** 上一帧速度（cm/s），替代 CharacterMovementComponent::GetLastUpdateVelocity */
	UPROPERTY(BlueprintReadOnly, Category = "Mover State")
	FVector MoverLastUpdateVelocity = FVector::ZeroVector;

	// ---- 停步距离预测参数 ----
	// 供 PredictGroundMovementStopLocation 估算刹车距离以驱动停步的距离匹配。
	// Mover 的减速模型与 CharacterMovementComponent 不同，无法逐项对应，
	// 故取为可调参数，默认值沿用 CMC 的出厂设置，按停步观感微调即可。

	/** 是否使用独立的制动摩擦 */
	UPROPERTY(EditDefaultsOnly, Category = "Mover Braking")
	bool MoverUseSeparateBrakingFriction = false;

	/** 制动摩擦系数，仅在启用独立制动摩擦时生效 */
	UPROPERTY(EditDefaultsOnly, Category = "Mover Braking")
	float MoverBrakingFriction = 0.f;

	/** 地面摩擦系数 */
	UPROPERTY(EditDefaultsOnly, Category = "Mover Braking")
	float MoverGroundFriction = 8.f;

	/** 制动摩擦倍率 */
	UPROPERTY(EditDefaultsOnly, Category = "Mover Braking")
	float MoverBrakingFrictionFactor = 2.f;

	/** 地面制动减速度（cm/s²） */
	UPROPERTY(EditDefaultsOnly, Category = "Mover Braking")
	float MoverBrakingDeceleration = 2000.f;

	// 运动学读数的唯一来源，即 Pawn 上那组 BlueprintPure getter
	private AYanDefaultPawn YanPawn;

	// 标签映射只需绑定一次
	private bool bTagPropertyMapInitialized = false;

	//~Begin UAnimInstance Interface
	UFUNCTION(BlueprintOverride)
	void BlueprintInitializeAnimation()
	{
		ResolveOwner();
	}

	UFUNCTION(BlueprintOverride)
	void BlueprintUpdateAnimation(float DeltaTimeX)
	{
		if (YanPawn == nullptr)
		{
			ResolveOwner();
			if (YanPawn == nullptr)
			{
				return;
			}
		}

		TryInitializeTagPropertyMap();

		GroundDistance = YanPawn.GetGroundDistance();
		MoverIsOnGround = !YanPawn.IsInAir();
		MoverIsCrouching = YanPawn.IsCrouching();
		MoverIsSliding = YanPawn.IsSliding();
		MoverGravityZ = YanPawn.GetGravityZ();

		MoverLastUpdateVelocity = YanPawn.GetKinematicVelocity();

		// 取移动意图而非速度差分：差分会把物理求解的速度噪声放大数十倍，使方向判定
		// 在相邻象限间反复跳变，表现为移动动画抽搐。消费方只用其水平方向与非零判定。
		MoverAcceleration = YanPawn.GetMovementIntentWorld();
		MoverAcceleration.Z = 0.f;
	}
	//~End UAnimInstance Interface

	// 解析 Pawn 拥有者；BeginPlay 前动画实例可能已初始化，故允许延迟重试
	private void ResolveOwner()
	{
		YanPawn = Cast<AYanDefaultPawn>(TryGetPawnOwner());
	}

	// ASC 在 Possess 后才就绪，可能晚于动画实例初始化，故每帧重试直至绑定成功
	private void TryInitializeTagPropertyMap()
	{
		if (bTagPropertyMapInitialized)
		{
			return;
		}

		UAbilitySystemComponent ASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(YanPawn);
		if (ASC == nullptr)
		{
			return;
		}

		GameplayTagPropertyMap.Initialize(this, ASC);
		bTagPropertyMapInitialized = true;
	}
}
