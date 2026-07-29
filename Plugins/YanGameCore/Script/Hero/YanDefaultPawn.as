/**
 * 默认英雄 Pawn，在 AYanPawn 提供的 GameplayCamera 基础上支持第一/三人称视角切换。
 *
 * 视角通过 SpringArm 的 TargetArmLength 控制，并同步切换本地拥有者的网格可见性。
 * 切换入口：SetThirdPerson / TogglePersonPerspective。
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

	UPROPERTY(DefaultComponent)
	USkeletalMeshComponent SkeletalMesh;

	/** SpringArm：在 BeginPlay 中通过 GetOrCreate 挂载到 Capsule，Camera 再挂到其末端 */
	UPROPERTY(DefaultComponent)
	USpringArmComponent CameraBoom;
	default
	{
		CameraBoom.bUsePawnControlRotation = true;
		CameraBoom.bDoCollisionTest = true;
		CameraBoom.bEnableCameraLag = false;
		CameraBoom.SocketOffset = CameraSocketOffset;
	}

	UPROPERTY(DefaultComponent)
	UGameplayCameraComponent GameplayCamera;
	default	GameplayCamera.bAutoActivate = false;

	/** 第一人称：TargetArmLength（通常为 0） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	float FirstPersonArmLength = 0.f;

	/** 第三人称：弹簧臂长度（世界单位） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	float ThirdPersonArmLength = 380.f;

	/** 相对 SpringArm 原点的偏移（常用 Z 表示眼高），第一/三人称共用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	FVector CameraSocketOffset = FVector(0.f, 0.f, 0.f);

	/** 当前是否为第三人称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Person")
	bool bThirdPerson = false;

	/**
	 * 蹲伏时相机相对站立姿态的高度偏移（负值为下降）。
	 * 胶囊压低由物理线程即时完成，相机则在此按 CrouchCameraBlendSpeed 平滑跟随；
	 * 取值应约等于胶囊半高的降幅，使视点与角色头部保持一致。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch")
	float CrouchCameraOffsetZ = -44.f;

	/** 相机高度在站立/蹲伏之间过渡的插值速度（越大越快） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch", meta = (ClampMin = "0"))
	float CrouchCameraBlendSpeed = 8.f;

	// 相机高度的当前值与目标值，仅由姿态变化驱动
	private float CurrentCameraOffsetZ = 0.f;
	private float TargetCameraOffsetZ  = 0.f;

	//~Begin Actor Interface
	UFUNCTION(BlueprintOverride)
	void ConstructionScript()
	{
		SkeletalMesh.AttachToComponent(GetRootComponent());

		CameraBoom.AttachToComponent(SkeletalMesh);
		GameplayCamera.AttachToComponent(CameraBoom);
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UMoverComponent MoverComp = GetComponentByClass(UMoverComponent);
		MoverComp.RegisterMove(UGrapplingMoveLogic);

		UCharacterMoverComponent CharMoverComp = Cast<UCharacterMoverComponent>(MoverComp);
		if (CharMoverComp != nullptr)
		{
			CharMoverComp.OnStanceChanged.AddUFunction(this, n"HandleStanceChanged");
		}

		// FOV 等镜头参数由 Rig 内的 UYanCameraNode 驱动，此处仅初始化视角状态
		SetThirdPerson(bThirdPerson);
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		if (CameraBoom == nullptr || CurrentCameraOffsetZ == TargetCameraOffsetZ)
		{
			return;
		}

		CurrentCameraOffsetZ = Math::FInterpTo(CurrentCameraOffsetZ, TargetCameraOffsetZ, DeltaSeconds, CrouchCameraBlendSpeed);
		CameraBoom.SocketOffset = CameraSocketOffset + FVector(0.f, 0.f, CurrentCameraOffsetZ);
	}

	UFUNCTION(BlueprintOverride)
	void Possessed(AController NewController)
	{
		// 相机激活是本地视图行为，统一经 Client RPC 在拥有端触发
		Possessed_Client(NewController);

		// Owner 此时已确定，刷新视角以应用正确的网格可见性
		SetThirdPerson(bThirdPerson);
	}
	//~End Actor Interface

	/**
	 * 在第一/三人称之间切换：调整 SpringArm 臂长，并刷新本地拥有者的网格可见性。
	 * @param bInThirdPerson true 为第三人称，false 为第一人称
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Person")
	void SetThirdPerson(bool bInThirdPerson)
	{
		bThirdPerson = bInThirdPerson;

		// 第一人称仅对本地拥有者隐藏网格，其他客户端仍可见
		SkeletalMesh.SetOwnerNoSee(!bThirdPerson);

		if (CameraBoom != nullptr)
		{
			CameraBoom.TargetArmLength = bThirdPerson ? ThirdPersonArmLength : FirstPersonArmLength;
		}
	}

	/** 在第一人称与第三人称之间切换 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Person")
	void TogglePersonPerspective()
	{
		SetThirdPerson(!bThirdPerson);
	}

	// 姿态由物理线程裁决后经 Mover 事件回传，此处只更新相机高度目标值
	UFUNCTION()
	private void HandleStanceChanged(EStanceMode OldStance, EStanceMode NewStance)
	{
		TargetCameraOffsetZ = (NewStance == EStanceMode::Crouch) ? CrouchCameraOffsetZ : 0.f;
	}

	// 在拥有端激活 GameplayCamera；相机求值是本地视图行为，非本地代理无需激活
	UFUNCTION(Client)
	private void Possessed_Client(AController NewController)
	{
		APlayerController PC = Cast<APlayerController>(NewController);
		if (PC != nullptr)
		{
			GameplayCamera.ActivateCameraForPlayerController(PC);
		}
	}
}
