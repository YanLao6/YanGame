/**
 * 角色动画数据层：每帧从 Pawn 侧的 BlueprintPure getter 采样运动学状态，
 * 归一化为 BlueprintReadOnly 属性供 AnimGraph 消费，本身不含任何动画资产引用。
 *
 * 更新分两阶段：
 *   UpdateEssentialValues —— 速度/加速度/朝向及其 _LastFrame 与派生量
 *   UpdateStates         —— 六条枚举状态轴，各自带历史记录（FYanStateHistory）
 * 另提供一组帧事件判定函数（IsStarting / IsPivoting / JustLanded ...）供状态机转移条件调用。
 *
 * 线程约定：只重写游戏线程的 BlueprintUpdateAnimation，不重写线程安全版本，
 * 因为 AngelScript 虚拟机不保证在动画 worker 线程上的可重入性。
 *
 * AnimGraph（状态机 / 混合空间 / Motion Matching 等）由后续新建的 Animation Blueprint
 * 承载，其 Parent Class 指向本类即可读取这些属性与判定。
 */
class UYanCharacterAnimInstance : UAnimInstance
{
	// ---- 配置（在 ABP 资产的 Class Defaults 上调） ----

	/** 低于此速度视为静止（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float MovingSpeedThreshold = 3.f;

	/** 步态：达到此速度算 Run（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float GaitWalkSpeedThreshold = 710.f;

	/** 步态：达到此速度且处于疾跑标签下算 Sprint，否则封顶 Run（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float GaitRunSpeedThreshold = 550.f;

	/** AccelerationAmount 归一化用的加速度上限（cm/s^2） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float MaxAnimAcceleration = 2048.f;

	/** 着地时上一帧竖直速度超过此值算重着地（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float HeavyLandSpeedThreshold = 700.f;

	/** 单帧位移超过此距离判定为传送（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float TeleportDistanceThreshold = 200.f;

	/** 急转判定所需的最低速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float PivotSpeedThreshold = 100.f;

	/** 急转判定：加速度方向与速度方向夹角超过此角度（度） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float PivotAngleThreshold = 45.f;

	/** 腾空时竖直速度高于此值算上升段（跳跃），否则算下落段（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float JumpVerticalSpeedThreshold = 10.f;

	/** 状态历史窗口：当前值停留超过此时长后 Recent 收敛到当前值（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float RecentStateWindow = 0.1f;

	/** MovementMode 用更长的历史窗口（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Anim|Config")
	float MovementModeRecentWindow = 0.2f;

	// ---- Essential Values ----

	/** 拥有本动画实例的 Pawn（初始化时解析一次，供蓝图通用访问） */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim")
	APawn OwnerPawn;

	/** Mover 求解出的世界空间线速度（cm/s） */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	FVector Velocity;

	/** 上一帧的 Velocity */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	FVector VelocityLastFrame;

	/** 水平面速度大小（cm/s） */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	float Speed2D;

	/** 速度大于 MovingSpeedThreshold */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	bool bHasVelocity;

	/** 最近一次非零速度（停步时用于决定收尾朝向） */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	FVector LastNonZeroVelocity;

	/** 由速度差分得到的动画加速度（水平，cm/s^2），非移动输入意图 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	FVector Acceleration;

	/** 上一帧的 Acceleration */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	FVector AccelerationLastFrame;

	/** 加速度大小归一化到 0~1 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	float AccelerationAmount;

	/** 加速度是否显著 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	bool bHasAcceleration;

	/** 速度方向相对角色朝向的夹角（-180~180，右为正） */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	float LocomotionDirectionAngle;

	/** 加速度方向相对角色朝向的夹角（-180~180，右为正） */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	float AccelerationDirectionAngle;

	/** 视线俯仰角（-90~90），驱动 aim offset */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	float AimPitch;

	/** 角色当前世界坐标 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	FVector CharacterLocation;

	/** 胶囊底部到地面的距离（cm），贴地为 0；腾空时驱动落地预判 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|Essential")
	float GroundDistance = 0.f;

	// ---- 原始状态读数（便于 AnimGraph 直接用） ----

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bHasMovementInput;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bIsInAir;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bIsCrouching;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bIsFirstPerson;

	/** 应播放移动动画：Moving 且贴地 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bShouldMove;

	/** 腾空上升段 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bIsJumping;

	/** 腾空下落段 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	bool bIsFalling;

	/** 着地瞬间锁存的竖直速度（cm/s，向下为负），供轻/重着地选片 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	float LandVelocity;

	// ---- 六条枚举状态轴（各带历史记录） ----

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	FYanStateHistory MovementModeHistory;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	FYanStateHistory RotationModeHistory;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	FYanStateHistory MovementStateHistory;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	FYanStateHistory GaitHistory;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	FYanStateHistory StanceHistory;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Anim|State")
	FYanStateHistory MovementDirectionHistory;

	// 强类型 Pawn；接口即其上一组 BlueprintPure getter
	private AYanDefaultPawn YanPawn;

	// 传送判定用，无需暴露
	private FVector CharacterLocationLastFrame;

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

		UpdateEssentialValues(DeltaTimeX);
		UpdateStates(DeltaTimeX);
	}
	//~End UAnimInstance Interface

	// ---- 强类型状态轴读取（供 AnimGraph 按枚举取值） ----

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanMovementMode GetMovementMode() const { return EYanMovementMode(MovementModeHistory.Current); }

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanRotationMode GetRotationMode() const { return EYanRotationMode(RotationModeHistory.Current); }

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanMovementState GetMovementState() const { return EYanMovementState(MovementStateHistory.Current); }

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanMovementState GetMovementStateRecent() const { return EYanMovementState(MovementStateHistory.Recent); }

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanGait GetGait() const { return EYanGait(GaitHistory.Current); }

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanStance GetStance() const { return EYanStance(StanceHistory.Current); }

	UFUNCTION(BlueprintPure, Category = "Yan|Anim|State")
	EYanMovementDirection GetMovementDirection() const { return EYanMovementDirection(MovementDirectionHistory.Current); }

	// ---- 帧事件判定（状态机转移条件用） ----

	/** 是否在移动（有速度） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool IsMoving() const
	{
		return Speed2D > MovingSpeedThreshold;
	}

	/** 是否刚起步：有输入、从 Idle 切到 Moving 且尚在起步窗口内 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool IsStarting() const
	{
		return bHasMovementInput
			&& MovementStateHistory.Current == int(EYanMovementState::Moving)
			&& MovementStateHistory.Recent == int(EYanMovementState::Idle)
			&& MovementStateHistory.TimeInState < 0.3f;
	}

	/** 是否正在急转：速度够快且加速度方向明显偏离速度方向 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool IsPivoting() const
	{
		if (Speed2D < PivotSpeedThreshold || !bHasAcceleration)
		{
			return false;
		}

		float Diff = AccelerationDirectionAngle - LocomotionDirectionAngle;
		while (Diff > 180.f) { Diff -= 360.f; }
		while (Diff < -180.f) { Diff += 360.f; }
		return Math::Abs(Diff) > PivotAngleThreshold;
	}

	/** 本帧是否刚着地 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool JustLanded() const
	{
		return MovementModeHistory.Current == int(EYanMovementMode::Grounded)
			&& MovementModeHistory.LastFrame == int(EYanMovementMode::InAir);
	}

	/** 本帧是否刚重着地 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool JustLandedHeavy() const
	{
		return JustLanded() && Math::Abs(LandVelocity) > HeavyLandSpeedThreshold;
	}

	/** 应结束移动动画：贴地且失去移动意图 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool ShouldStop() const
	{
		return !bIsInAir && !bHasMovementInput;
	}

	/** 已腾空时长（秒）；贴地时为 0 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	float GetTimeInAir() const
	{
		return bIsInAir ? MovementModeHistory.TimeInState : 0.f;
	}

	/** 当前姿态已持续时长（秒） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	float GetTimeInStance() const
	{
		return StanceHistory.TimeInState;
	}

	/** 本帧是否刚进入蹲伏 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool JustStartedCrouch() const
	{
		return StanceHistory.Current == int(EYanStance::Crouch)
			&& StanceHistory.LastFrame == int(EYanStance::Stand);
	}

	/** 本帧是否刚退出蹲伏 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool JustStoppedCrouch() const
	{
		return StanceHistory.Current == int(EYanStance::Stand)
			&& StanceHistory.LastFrame == int(EYanStance::Crouch);
	}

	/** 本帧是否发生传送（单帧位移过大） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim|Query")
	bool JustTeleported() const
	{
		return (CharacterLocation - CharacterLocationLastFrame).Size() > TeleportDistanceThreshold;
	}

	// ---- 内部 ----

	private void UpdateEssentialValues(float DeltaTimeX)
	{
		VelocityLastFrame = Velocity;
		Velocity = YanPawn.GetKinematicVelocity();
		Speed2D = Velocity.Size2D();
		bHasVelocity = Speed2D > MovingSpeedThreshold;
		if (bHasVelocity)
		{
			LastNonZeroVelocity = Velocity;
		}

		AccelerationLastFrame = Acceleration;
		FVector VelocityDelta = Velocity - VelocityLastFrame;
		VelocityDelta.Z = 0.f;
		Acceleration = DeltaTimeX > 0.f ? VelocityDelta / DeltaTimeX : FVector::ZeroVector;
		float AccelSize = Acceleration.Size();
		bHasAcceleration = AccelSize > 1.f;
		AccelerationAmount = Math::Clamp(AccelSize / MaxAnimAcceleration, 0.f, 1.f);

		bHasMovementInput = YanPawn.HasMovementInput();
		bIsInAir = YanPawn.IsInAir();
		bIsCrouching = YanPawn.IsCrouching();
		bIsFirstPerson = YanPawn.IsFirstPersonView();

		CharacterLocationLastFrame = CharacterLocation;
		CharacterLocation = YanPawn.GetActorLocation();
		GroundDistance = YanPawn.GetGroundDistance();

		FRotator OwnerRotation = YanPawn.GetActorRotation();
		LocomotionDirectionAngle = CalculateDirection(Velocity, OwnerRotation);
		AccelerationDirectionAngle = CalculateDirection(Acceleration, OwnerRotation);

		float ControlPitch = YanPawn.GetControlRotation().Pitch;
		AimPitch = ControlPitch > 180.f ? ControlPitch - 360.f : ControlPitch;
	}

	private void UpdateStates(float DeltaTimeX)
	{
		EYanMovementMode NewMovementMode = bIsInAir ? EYanMovementMode::InAir : EYanMovementMode::Grounded;
		MovementModeHistory.Update(int(NewMovementMode), DeltaTimeX, MovementModeRecentWindow);

		bIsJumping = bIsInAir && Velocity.Z > JumpVerticalSpeedThreshold;
		bIsFalling = bIsInAir && !bIsJumping;

		// 着地当帧锁存腾空期的竖直速度：落地后 Velocity.Z 立刻归零，事后取不到
		if (JustLanded())
		{
			LandVelocity = VelocityLastFrame.Z;
		}

		// 第一人称锁朝向侧移，第三人称面向移动方向；瞄准姿态后续再扩展
		EYanRotationMode NewRotationMode = bIsFirstPerson ? EYanRotationMode::Strafe : EYanRotationMode::OrientToMovement;
		RotationModeHistory.Update(int(NewRotationMode), DeltaTimeX, RecentStateWindow);

		EYanMovementState NewMovementState = (bHasVelocity && bHasMovementInput) ? EYanMovementState::Moving : EYanMovementState::Idle;
		MovementStateHistory.Update(int(NewMovementState), DeltaTimeX, RecentStateWindow);

		EYanGait NewGait = EYanGait::Walk;
		if (YanPawn.IsSprinting() && Speed2D >= GaitRunSpeedThreshold)
		{
			NewGait = EYanGait::Sprint;
		}
		else if (Speed2D >= GaitWalkSpeedThreshold)
		{
			NewGait = EYanGait::Run;
		}
		GaitHistory.Update(int(NewGait), DeltaTimeX, RecentStateWindow);

		EYanStance NewStance = bIsCrouching ? EYanStance::Crouch : EYanStance::Stand;
		StanceHistory.Update(int(NewStance), DeltaTimeX, RecentStateWindow);

		// 移动方向仅在移动时刷新，静止时保留上一次象限
		EYanMovementDirection NewDirection = EYanMovementDirection(MovementDirectionHistory.Current);
		if (bHasVelocity)
		{
			float AbsAngle = Math::Abs(LocomotionDirectionAngle);
			if (AbsAngle <= 60.f)
			{
				NewDirection = EYanMovementDirection::Forward;
			}
			else if (AbsAngle >= 120.f)
			{
				NewDirection = EYanMovementDirection::Backward;
			}
			else
			{
				NewDirection = LocomotionDirectionAngle > 0.f ? EYanMovementDirection::Right : EYanMovementDirection::Left;
			}
		}
		MovementDirectionHistory.Update(int(NewDirection), DeltaTimeX, RecentStateWindow);

		bShouldMove = MovementStateHistory.Current == int(EYanMovementState::Moving)
			&& MovementModeHistory.Current == int(EYanMovementMode::Grounded);
	}

	// 解析 Pawn 拥有者；BeginPlay 前动画实例可能已初始化，故允许延迟重试
	private void ResolveOwner()
	{
		OwnerPawn = TryGetPawnOwner();
		YanPawn = Cast<AYanDefaultPawn>(OwnerPawn);
	}
}
