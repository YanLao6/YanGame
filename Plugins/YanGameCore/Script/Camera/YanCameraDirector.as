/**
 * UYanCameraDirector — 相机导演（Camera Director）求值器
 *
 * 视角切换逻辑：读取拥有者 `AYanDefaultPawn::bThirdPerson`，在第一/第三人称 Rig 之间选择。
 * 当未配置 ThirdPersonRig 时，退化为始终激活 FirstPersonRig 的单 Rig 模式。
 *
 * 使用方式（编辑器接线）：
 * 1. 打开 `GameplayCamera` 引用的 CameraAsset（即 UDefaultCamera），将其 Camera Director 设为
 *    `Blueprint Camera Director`；
 * 2. 把该 Director 的 `CameraDirectorEvaluatorClass` 选为本类 `UYanCameraDirector`；
 * 3. 在本类实例上指定 FirstPersonRig / ThirdPersonRig 两个 Rig 资产。
 */
class UYanCameraDirector : UBlueprintCameraDirectorEvaluator
{
	/** 第一人称 Camera Rig；单 Rig 模式下始终使用它 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	UCameraRigAsset FirstPersonRig;

	/** 第三人称 Camera Rig；为空时退化为单 Rig 模式 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	UCameraRigAsset ThirdPersonRig;

	//~Begin UBlueprintCameraDirectorEvaluator Interface
	UFUNCTION(BlueprintOverride)
	void RunCameraDirector(float DeltaTime, UObject EvaluationContextOwner, FBlueprintCameraDirectorEvaluationParams Params)
	{
		// 默认使用第一人称 Rig
		UCameraRigAsset Rig = FirstPersonRig;

		// 读取拥有者 Pawn 的视角状态，决定是否切换到第三人称 Rig
		AYanDefaultPawn Pawn = Cast<AYanDefaultPawn>(FindEvaluationContextOwnerActor(AYanDefaultPawn));
		if (Pawn != nullptr && Pawn.bThirdPerson && ThirdPersonRig != nullptr)
		{
			Rig = ThirdPersonRig;
		}

		// 声明本帧生效的 Rig，Rig 之间的过渡混合由相机系统自动处理
		if (Rig != nullptr)
		{
			ActivateCameraRig(Rig);
		}
	}
	//~End UBlueprintCameraDirectorEvaluator Interface
}
