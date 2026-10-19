/**
 * UYanFirstPersonHeadNode — 第一人称头骨相机节点（Blueprint Camera Node）
 *
 * 每帧把 CameraPose 的位置改写为第一人称骨骼头骨的世界位置，使镜头完全跟随头部动画
 * （走跑起伏、蹲伏下沉、受击晃动都自然带上），不存在相机与身体分离的偏差。
 *
 * FirstPersonMesh 尚未配置网格、找不到头骨时，退化为组件原点 + EyeOffset，并叠加
 * Pawn 的姿态相机偏移——ChaosMover 蹲伏只缩胶囊顶部、Actor 位置不动，
 * 不叠加则蹲伏与滑铲时镜头纹丝不动。有头骨时下沉由动画给出，不再叠加以免双重下降。
 *
 * 朝向默认沿用上游（弹簧臂给出的 ControlRotation）：鼠标到视角 1:1 且无延迟，俯仰可达满量程。
 * 若要连头骨朝向一起继承，打开 bInheritHeadRotation——此时视角转动经
 * ControlRotation → AnimGraph 的 AimPitch → 头骨 → 相机，至少晚一帧且受脖子弯曲上限钳制，
 * 需要 AnimGraph 已在用 AimPitch 驱动头部，否则镜头会锁死朝前。
 *
 * 使用方式（编辑器接线）：在 FirstPersonRig 的节点图中加入 Blueprint Camera Node，
 * 将其 CameraNodeEvaluatorClass 指定为本类。
 */
class UYanFirstPersonHeadNode : UBlueprintCameraNodeEvaluator
{
	/** 取位置的骨骼/插槽名 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	FName HeadSocketName = n"head";

	/** 眼位相对头骨的偏移（头骨局部空间，cm）；头骨枢轴在颅底，眼睛通常要往前上方挪 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	FVector EyeOffset = FVector(0.f, 0.f, 0.f);

	/** 是否连头骨朝向一起继承；关闭时只取位置，朝向沿用上游 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	bool bInheritHeadRotation = false;

	/** 继承头骨朝向时的姿态修正：头骨局部轴系与视线方向并不一致，需在此校正 */
	UPROPERTY(EditAnywhere, Category = "Yan|Camera")
	FRotator HeadRotationCorrection = FRotator(0.f, 0.f, 0.f);

	//~Begin UBlueprintCameraNodeEvaluator Interface
	UFUNCTION(BlueprintOverride)
	void TickCameraNode(float DeltaTime)
	{
		AYanDefaultPawn Pawn = Cast<AYanDefaultPawn>(FindEvaluationContextOwnerActor(AYanDefaultPawn));
		if (Pawn == nullptr || Pawn.FirstPersonMesh == nullptr)
		{
			return;
		}

		FBlueprintCameraPose Pose = GetCurrentCameraPose();

		if (!Pawn.FirstPersonMesh.DoesSocketExist(HeadSocketName))
		{
			const FTransform MeshTransform = Pawn.FirstPersonMesh.GetWorldTransform();
			Pose.Location = MeshTransform.TransformPosition(EyeOffset) + FVector(0.f, 0.f, Pawn.GetStanceCameraOffsetZ());
			SetCurrentCameraPose(Pose);
			return;
		}

		FTransform HeadTransform = Pawn.FirstPersonMesh.GetSocketTransform(HeadSocketName);
		Pose.Location = HeadTransform.TransformPosition(EyeOffset);

		if (bInheritHeadRotation)
		{
			Pose.Rotation = (HeadTransform.Rotator().Quaternion() * HeadRotationCorrection.Quaternion()).Rotator();
		}

		SetCurrentCameraPose(Pose);
	}
	//~End UBlueprintCameraNodeEvaluator Interface
}
