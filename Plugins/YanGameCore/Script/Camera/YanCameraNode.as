/**
 * UYanCameraNode — Rig 内部镜头参数节点（Blueprint Camera Node）
 *
 * 继承自 `UBlueprintCameraNodeEvaluator`，承担 GameplayCameras 的「Rig 节点层」职责：
 * 每帧在 Camera Rig 求值过程中改写 CameraPose 的镜头参数（FOV、画幅约束、裁剪面等）。
 *
 * 这是设置 FOV 等参数的正确入口：相机系统的输出会以 Rig 求值得到的 CameraPose 为准，
 * 因此不应直接写 `GetOutputCameraComponent()`（其值每帧都会被 Pose 覆盖）。
 *
 * 使用方式（编辑器接线）：在目标 Camera Rig 的节点图中加入 `Blueprint Camera Node`，
 * 将其求值器类（CameraNodeEvaluatorClass）指定为本类 `UYanCameraNode`，并配置下列参数。
 */
class UYanCameraNode : UBlueprintCameraNodeEvaluator
{
	/** 视场角（度）；写入 CameraPose.FieldOfView */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	float FieldOfView = 90.f;

	/** 是否约束宽高比；关闭后镜头充满视口 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	bool bConstrainAspectRatio = false;

	/** 近裁剪面（世界单位）；<= 0 时不覆盖，沿用上游默认值 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	float NearClippingPlane = -1.f;

	//~Begin UBlueprintCameraNodeEvaluator Interface
	UFUNCTION(BlueprintOverride)
	void TickCameraNode(float DeltaTime)
	{
		// 取出当前 Pose，改写镜头参数后写回；这是唯一会被输出端采纳的途径
		FBlueprintCameraPose Pose = GetCurrentCameraPose();

		// CameraPose 为「焦距优先」模型：FocalLength > 0 时 GetEffectiveFieldOfView 会无视 FieldOfView。
		// 故设 FOV 的同时必须把 FocalLength 置为无效（< 0），否则 FieldOfView 不生效。
		Pose.FieldOfView = FieldOfView;
		Pose.FocalLength = -1.f;
		Pose.ConstrainAspectRatio = bConstrainAspectRatio;

		// 仅在配置了有效值时覆盖近裁剪面，避免无意间改写上游设定
		if (NearClippingPlane > 0.f)
		{
			Pose.NearClippingPlane = NearClippingPlane;
		}

		SetCurrentCameraPose(Pose);
	}
	//~End UBlueprintCameraNodeEvaluator Interface
}
