/**
 * 默认英雄 Pawn，在 AYanPawn 提供的 GameplayCamera 基础上支持第一/三人称视角切换。
 *
 * 采用真第一人称：两具全身骨骼各服务一侧视角——FirstPersonMesh 供本地拥有者在第一人称下
 * 观看，SkeletalMesh 供远端玩家与本地第三人称视角。切换入口：SetThirdPerson / TogglePersonPerspective。
 *
 * 两台相机各自取景：GameplayCamera 经弹簧臂供第三人称，FirstPersonCamera 挂在
 * FirstPersonMesh 的 FPCamera 插槽供第一人称。切换视角即在拥有端激活其一并停用另一台，
 * 同时同步两具网格的可见性。
 *
 * 动画只在 SkeletalMesh 上求值；FirstPersonMesh 在初始化时复制其网格与相对变换，
 * 并以 LeaderPose 跟随，蒙太奇、武器动画层与瞄准偏移在两侧天然一致，且不付双份动画开销。
 */
class AYanDefaultPawn : AYanPawn
{
	default
	{
		// Replicate
		SetReplicateMovement(true);
		SetPhysicsReplicationMode(EPhysicsReplicationMode::Resimulation);
	
		// Controller
		bUseControllerRotationYaw = false;
		bUseControllerRotationPitch = false;
		bUseControllerRotationRoll = false;

		// Capsule
		Capsule.SetEnableGravity(true);
		Capsule.SetSimulatePhysics(true);
		Capsule.SetUpdateKinematicFromSimulation(true);
	}

	UPROPERTY(DefaultComponent)
	UNetworkPhysicsSettingsComponent NetworkPhysicsSettingsComponent;

	/**
	 * 全身骨骼：所有端都存在，命中判定、插槽定位与蒙太奇均以此为准。
	 * 远端玩家始终看到这具，本地拥有者仅在第三人称视角下看到；
	 * 第一人称下对拥有者不可见但保留投影，使玩家仍能看见自己的影子。
	 */
	UPROPERTY(DefaultComponent, Attach = Capsule)
	USkeletalMeshComponent SkeletalMesh;
	default
	{
		SkeletalMesh.AnimClass = UYanCharacterAnimInstance;
		SkeletalMesh.bCastHiddenShadow = true;
		// 承接 Weapon 通道的逐骨骼命中判定。命中形体只能有一具，
		// FirstPersonMesh 保持 NoCollision，否则服务端会对同一发子弹重复计中。
		SkeletalMesh.SetCollisionProfileName(n"YanPawnMesh");
	}

	/**
	 * 第一人称全身骨骼：真第一人称下玩家看到的就是这具身体，FirstPersonCamera 挂在
	 * 其 FPCamera 插槽上。仅本地拥有者可见。
	 * 不投影——阴影统一由 SkeletalMesh 提供，两具身体同时投影会出现双影。
	 * 网格与姿态均来自 SkeletalMesh（见 SetupFirstPersonMeshFollower），蓝图上无需配置。
	 */
	UPROPERTY(DefaultComponent, Attach = Capsule)
	USkeletalMeshComponent FirstPersonMesh;
	default
	{
		FirstPersonMesh.SetOnlyOwnerSee(true);
		FirstPersonMesh.SetCastShadow(false);
	}

	/**
	 * 相机弹簧臂：挂 Capsule 而非骨骼——第三人称镜头不应继承身体动画的起伏晃动。
	 * 第一人称不经弹簧臂，由 FirstPersonCamera 直接取景。
	 */
	UPROPERTY(DefaultComponent,Attach = Capsule)
	USpringArmComponent CameraBoom;
	default
	{
		CameraBoom.bUsePawnControlRotation = true;
		CameraBoom.bDoCollisionTest = true;
		CameraBoom.bEnableCameraLag = false;
		CameraBoom.SocketOffset = CameraSocketOffset;
	}

	/** 第三人称相机：经弹簧臂取景。不自动激活，由 ApplyPerspectiveCamera 在拥有端按视角接管 */
	UPROPERTY(DefaultComponent, Attach = CameraBoom)
	UGameplayCameraComponent GameplayCamera;
	default	GameplayCamera.bAutoActivate = false;

	/**
	 * 第一人称相机：挂在 FirstPersonMesh 的 FPCamera 插槽上，镜头随头部动画起伏。
	 * 朝向取绝对旋转并每帧写入 ControlRotation——头骨的动画旋转不得混进视角，
	 * 否则鼠标转向会滞后一帧且被脖子弯曲上限钳制。眼位微调直接改本组件的相对位置。
	 * 与 GameplayCamera 同一时刻只激活一台：Actor 取景只认首个激活的相机组件。
	 */
	UPROPERTY(DefaultComponent, Attach = FirstPersonMesh, AttachSocket = FPCamera)
	UGameplayCameraComponent FirstPersonCamera;
	default
	{
		FirstPersonCamera.bAutoActivate = false;
		FirstPersonCamera.bAbsoluteRotation = true;
	}

	/** 第三人称：弹簧臂长度（世界单位） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	float ThirdPersonArmLength = 380.f;

	/** 相对 SpringArm 原点的偏移（常用 Z 表示眼高），第一/三人称共用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	FVector CameraSocketOffset = FVector(0.f, 0.f, 0.f);

	/** 当前是否为第三人称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	bool bThirdPerson = true;

	/**
	 * 蹲伏时相机相对站立姿态的高度偏移（负值为下降）。
	 * 胶囊压低由物理线程即时完成，相机则在此按 CrouchCameraBlendSpeed 平滑跟随；
	 * 取值应约等于胶囊半高的降幅，使视点与角色头部保持一致。
	 * 滑铲同样写入蹲伏意图，故此偏移一并覆盖滑铲。第三人称经弹簧臂 SocketOffset 生效，
	 * 第一人称由 UYanFirstPersonHeadNode 经 GetStanceCameraOffsetZ 取用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch")
	float CrouchCameraOffsetZ = -44.f;

	/** 相机高度在站立/蹲伏之间过渡的插值速度（越大越快） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch", meta = (ClampMin = "0"))
	float CrouchCameraBlendSpeed = 8.f;

	/**
	 * 瞄准时的视场角（度）。
	 * 由 Rig 内的 UYanCameraNode 按 GetADSAlpha 在非瞄准值与此值之间插值。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|ADS")
	float ADSFieldOfView = 70.f;

	/** 瞄准视角进入与退出的插值速度（越大越快） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|ADS", meta = (ClampMin = "0"))
	float ADSBlendSpeed = 10.f;

	// 相机高度的当前值与目标值，由蹲伏姿态与滑铲模式共同驱动
	private float CurrentCameraOffsetZ = 0.f;
	private float TargetCameraOffsetZ  = 0.f;
	private bool bCrouchStance = false;
	private bool bSlidingMode = false;

	// 瞄准程度的当前值与目标值，0 为未瞄准、1 为完全瞄准
	private float CurrentADSAlpha = 0.f;
	private float TargetADSAlpha  = 0.f;

	// 瞄准标签的当前授予状态，用于避免重复增减计数
	private bool bADSActive = false;

	// 武器散布据此收紧的瞄准标签，与 URangedWeaponInstance 中的定义对应
	private FGameplayTag SteadyAimingTag = FGameplayTag::RequestGameplayTag(n"EM.Weapon.SteadyAimingCamera");

	// Mover 组件在 BeginPlay 缓存，供动画朝向接口的 getter 复用
	private UMoverComponent MoverComp;
	private UCharacterMoverComponent CharacterMoverComp;

	// 疾跑状态标签，由 FYanSprintModifier 在激活期间对外暴露
	private FGameplayTag SprintingTag = FGameplayTag::RequestGameplayTag(n"Mover.IsSprinting");

	// 外观部件组件（可选）：玩家实际看到的身体是它生成的子 Actor，而非本类的两具隐形驱动骨骼
	private UModularPawnComponent_CharacterParts CharacterPartsComp;

	//~Begin AYanPawn Interface
	// 初始化挂在 OnPawnInitialized 而非 BeginPlay：派生蓝图重写 Event BeginPlay 会覆盖本类实现
	UFUNCTION(BlueprintOverride)
	void OnPawnInitialized()
	{
		// Mover 由 AYanPawn 按 MoverComponentClass 在 PreInitializeComponents 创建；缺失时仅失去移动相关绑定，
		// 不能中止本函数，否则网格跟随与视角初始化会一并丢失
		MoverComp = GetComponentByClass(UMoverComponent);
		if (MoverComp != nullptr)
		{
			MoverComp.RegisterMove(UGrapplingMoveLogic);
			CharacterMoverComp = Cast<UCharacterMoverComponent>(MoverComp);
			if (CharacterMoverComp != nullptr)
			{
				CharacterMoverComp.OnStanceChanged.AddUFunction(this, n"HandleStanceChanged");
			}
		}
		SetupFirstPersonMeshFollower();

		// 部件由服务端权威添加、客户端经复制后才生成，晚于视角初始化，须在生成后补刷可见性
		CharacterPartsComp = GetComponentByClass(UModularPawnComponent_CharacterParts);
		if (CharacterPartsComp != nullptr)
		{
			CharacterPartsComp.OnCharacterPartsChanged.AddUFunction(this, n"HandleCharacterPartsChanged");
		}

		// FOV 等镜头参数由 Rig 内的 UYanCameraNode 驱动，此处仅初始化视角状态
		SetThirdPerson(bThirdPerson);
	}

	// GameplayCamera 的激活由 AYanPawn 在拥有端完成，此处只处理归属确定后的表现刷新
	UFUNCTION(BlueprintOverride)
	void OnPawnRestarted()
	{
		// Owner 此时已确定，刷新视角以应用正确的网格可见性
		SetThirdPerson(bThirdPerson);
	}
	//~End AYanPawn Interface

	//~Begin Actor Interface
	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		if (CameraBoom != nullptr && CurrentCameraOffsetZ != TargetCameraOffsetZ)
		{
			CurrentCameraOffsetZ = Math::FInterpTo(CurrentCameraOffsetZ, TargetCameraOffsetZ, DeltaSeconds, CrouchCameraBlendSpeed);
			CameraBoom.SocketOffset = CameraSocketOffset + FVector(0.f, 0.f, CurrentCameraOffsetZ);
		}

		// 视场角本身由 Rig 节点每帧读取本值后写入 CameraPose，此处只推进过渡
		if (CurrentADSAlpha != TargetADSAlpha)
		{
			CurrentADSAlpha = Math::FInterpTo(CurrentADSAlpha, TargetADSAlpha, DeltaSeconds, ADSBlendSpeed);
		}

		// 第一人称相机只从插槽继承位置，朝向每帧跟随 ControlRotation
		if (!bThirdPerson && FirstPersonCamera != nullptr && FirstPersonCamera.IsActive())
		{
			FirstPersonCamera.SetWorldRotation(GetControlRotation());
		}
	}
	//~End Actor Interface

	/**
	 * 在第一/三人称之间切换：切换取景相机，并刷新本地拥有者的网格可见性。
	 * @param bInThirdPerson true 为第三人称，false 为第一人称
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Person")
	void SetThirdPerson(bool bInThirdPerson)
	{
		bThirdPerson = bInThirdPerson;

		// 第一人称身体：OnlyOwnerSee 已限定远端不可见，此处按视角开关对拥有者的显示
		FirstPersonMesh.SetVisibility(!bThirdPerson);

		// 主网格：第一人称下对拥有者隐藏，其他客户端始终可见
		SkeletalMesh.SetOwnerNoSee(!bThirdPerson);

		ApplyPerspectiveToCharacterParts();
		ApplyPerspectiveCamera();

		if (CameraBoom != nullptr)
		{
			CameraBoom.TargetArmLength = ThirdPersonArmLength;
		}
	}

	/**
	 * 在拥有端把取景切到当前视角对应的相机。
	 * 另一台必须停用：Actor 取景只认首个激活的相机组件，两台同时激活时以注册顺序为准而非视角。
	 * 非拥有端两台都不激活，取景沿用 Actor 视点。
	 * 控制器尚未就绪时（OnPawnInitialized 阶段）直接返回，由 OnPawnRestarted 再调一次补上。
	 */
	private void ApplyPerspectiveCamera()
	{
		APlayerController PC = Cast<APlayerController>(GetController());
		if (PC == nullptr || !PC.IsLocalController())
		{
			return;
		}

		UGameplayCameraComponent ActiveCamera = bThirdPerson ? GameplayCamera : FirstPersonCamera;
		UGameplayCameraComponent InactiveCamera = bThirdPerson ? FirstPersonCamera : GameplayCamera;
		if (InactiveCamera != nullptr)
		{
			InactiveCamera.Deactivate();
		}
		if (ActiveCamera != nullptr)
		{
			ActiveCamera.ActivateCameraForPlayerController(PC);
		}
	}

	/**
	 * 把当前视角同步到外观部件子 Actor 的全部图元。
	 * 部件是独立 Actor，不继承本类骨骼的 OwnerNoSee；其 Owner 链指回本 Pawn，故同样可按拥有者裁剪。
	 * 与 SkeletalMesh 一样保留隐藏投影，第一人称下玩家仍能看到自己的影子。
	 */
	private void ApplyPerspectiveToCharacterParts()
	{
		if (CharacterPartsComp == nullptr)
		{
			return;
		}

		TArray<AActor> PartActors = CharacterPartsComp.GetCharacterPartActors();
		for (AActor PartActor : PartActors)
		{
			TArray<UActorComponent> Primitives = PartActor.GetComponentsByClass(UPrimitiveComponent);
			for (UActorComponent Component : Primitives)
			{
				UPrimitiveComponent Primitive = Cast<UPrimitiveComponent>(Component);
				Primitive.SetOwnerNoSee(!bThirdPerson);
				Primitive.SetCastHiddenShadow(true);
			}
		}
	}

	UFUNCTION()
	private void HandleCharacterPartsChanged(UModularPawnComponent_CharacterParts ComponentWithChangedParts)
	{
		ApplyPerspectiveToCharacterParts();
	}

	/** 在第一人称与第三人称之间切换 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Person")
	void TogglePersonPerspective()
	{
		SetThirdPerson(!bThirdPerson);
	}

	/**
	 * 进入或退出瞄准。
	 *
	 * 同时驱动两件事：本地视场角的过渡目标，以及武器散布所读的瞄准标签。
	 * 二者合并在此是为了不可能失步——散布收紧与镜头收窄必须同起同止。
	 * 标签走 ASC 而非 Pawn 上的状态，是因为散布计算位于通用插件层，只能经 GAS 取值。
	 * 本函数在服务端与拥有端都会随技能激活执行，故 TargetADSAlpha 无需复制。
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|ADS")
	void SetADS(bool bInADS)
	{
		TargetADSAlpha = bInADS ? 1.f : 0.f;

		// 标签按计数累加，重复进入会使退出一次清不干净，故只在状态翻转时增减
		if (bADSActive == bInADS)
		{
			return;
		}
		bADSActive = bInADS;

		UAbilitySystemComponent ASC = AbilitySystem::GetAbilitySystemComponent(this);
		if (ASC == nullptr)
		{
			return;
		}

		if (bInADS)
		{
			ASC.AddLooseGameplayTag(SteadyAimingTag);
		}
		else
		{
			ASC.RemoveLooseGameplayTag(SteadyAimingTag);
		}
	}

	/** 当前瞄准程度，供 Rig 内的镜头节点插值视场角 */
	UFUNCTION(BlueprintPure, Category = "Camera|ADS")
	float GetADSAlpha()
	{
		return CurrentADSAlpha;
	}

	/** 当前姿态引起的相机高度偏移（已插值，站立为 0），供第一人称 Rig 节点取用 */
	UFUNCTION(BlueprintPure, Category = "Camera|Crouch")
	float GetStanceCameraOffsetZ()
	{
		return CurrentCameraOffsetZ;
	}

	/**
	 * 两具网格中供游戏逻辑取用的那一具。
	 *
	 * 取主网格：它在所有端上都存在且姿态与服务端一致，
	 * 而 FirstPersonMesh 仅本地拥有者可见，用它做命中判定或插槽定位会在远端得到不同结果。
	 */
	UFUNCTION(BlueprintOverride)
	USkeletalMeshComponent GetGameplayMesh()
	{
		return SkeletalMesh;
	}

	/**
	 * 动画朝向接口：动画数据层（UYanCharacterAnimInstance）仅通过下列 getter 读取本 Pawn，
	 * Pawn 不反向依赖动画层。更换运动后端时只要这些语义不变，AnimGraph 无需改动。
	 */

	/** Mover 求解出的世界空间线速度（cm/s） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	FVector GetKinematicVelocity() const
	{
		return MoverComp != nullptr ? MoverComp.GetVelocity() : FVector::ZeroVector;
	}

	/** 是否处于腾空（跳跃 / 下落 / 离地滑行等非贴地状态） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	bool IsInAir() const
	{
		return CharacterMoverComp != nullptr && CharacterMoverComp.IsAirborne();
	}

	/** 是否处于蹲伏姿态 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	bool IsCrouching() const
	{
		return CharacterMoverComp != nullptr && CharacterMoverComp.IsCrouching();
	}

	/** 是否处于滑铲模式；模式名须与 CharacterSlideCheck 的转换目标一致 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	bool IsSliding() const
	{
		return MoverComp != nullptr && MoverComp.GetMovementModeName() == n"Sliding";
	}

	/** 本帧是否存在有效移动意图（区分「主动移动」与「被外力推动 / 惯性滑行」） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	bool HasMovementInput() const
	{
		return MoverComp != nullptr && MoverComp.GetMovementIntent().SizeSquared() > 0.01f;
	}

	/** 当前是否为第一人称视角 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	bool IsFirstPersonView() const
	{
		return !bThirdPerson;
	}

	/** 是否处于疾跑状态（读 Mover 侧的 Mover.IsSprinting 标签） */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	bool IsSprinting() const
	{
		return MoverComp != nullptr && MoverComp.HasGameplayTag(SprintingTag, true);
	}

	/**
	 * 本帧的世界空间移动意图（无输入时为零向量）。
	 * 语义对应 CharacterMovementComponent 的输入加速度方向——供动画层判定移动朝向与急转，
	 * 不可用速度差分替代：物理求解的速度带噪声，差分后会放大成剧烈摆动。
	 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	FVector GetMovementIntentWorld() const
	{
		return MoverComp != nullptr ? MoverComp.GetMovementIntent() : FVector::ZeroVector;
	}

	/** 重力加速度的 Z 分量（cm/s²，向下为负），供动画层预测跳跃顶点时刻 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	float GetGravityZ() const
	{
		return MoverComp != nullptr ? MoverComp.GetGravityAcceleration().Z : -980.f;
	}

	/**
	 * 胶囊底部到地面的距离（cm）：贴地时为 0，探测不到地面时取探测上限。
	 * 供动画层预判落地时机——腾空时才射线检测，每帧由动画数据层采样一次。
	 */
	UFUNCTION(BlueprintPure, Category = "Yan|Anim")
	float GetGroundDistance() const
	{
		if (!IsInAir())
		{
			return 0.f;
		}

		const float MaxTraceDistance = 2000.f;
		const float CapsuleHalfHeight = Capsule.GetScaledCapsuleHalfHeight();
		const FVector Start = GetActorLocation();
		const FVector End = Start - FVector(0.f, 0.f, MaxTraceDistance + CapsuleHalfHeight);

		TArray<AActor> ActorsToIgnore;
		FHitResult Hit;
		System::LineTraceSingle(Start, End, ETraceTypeQuery::Visibility, false,
			ActorsToIgnore, EDrawDebugTrace::None, Hit, true);

		return Hit.bBlockingHit ? Math::Max(Hit.Distance - CapsuleHalfHeight, 0.f) : MaxTraceDistance;
	}

	/**
	 * 让 FirstPersonMesh 成为 SkeletalMesh 的 LeaderPose 跟随者。
	 * 相对变换取自 leader，骨骼世界位置才会重合；网格资产仅在蓝图未配置时才沿用 leader 的——
	 * 主网格可能是不可见的驱动骨骼（外观由部件子 Actor 提供），第一人称身体须自带可见网格。
	 * SkeletalMesh 为 AlwaysTickPoseAndRefreshBones，第一人称下它对拥有者不可见也照常求值。
	 */
	private void SetupFirstPersonMeshFollower()
	{
		if (FirstPersonMesh == nullptr || SkeletalMesh == nullptr)
		{
			return;
		}

		if (FirstPersonMesh.SkeletalMeshAsset == nullptr)
		{
			FirstPersonMesh.SetSkeletalMeshAsset(SkeletalMesh.SkeletalMeshAsset);
		}
		FirstPersonMesh.SetRelativeTransform(SkeletalMesh.RelativeTransform);
		FirstPersonMesh.SetLeaderPoseComponent(SkeletalMesh);
	}

	// 姿态由物理线程裁决后经 Mover 事件回传，此处只更新相机高度目标值
	UFUNCTION()
	private void HandleStanceChanged(EStanceMode OldStance, EStanceMode NewStance)
	{
		bCrouchStance = (NewStance == EStanceMode::Crouch);
		RefreshStanceCameraTarget();
	}

	private void RefreshStanceCameraTarget()
	{
		TargetCameraOffsetZ = (bCrouchStance || bSlidingMode) ? CrouchCameraOffsetZ : 0.f;
	}
}
