/**
 * 向一个指定位置 FYanCharacterInputs::GrappleAnchorPoint 牵引的Layer
 */
class UGrapplingMoveLogic : ULayeredMoveLogic
{
	/** 向锚点方向施加的主动拉力加速度（cm/s²） */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling", meta = (ClampMin = "0"))
	float PullAcceleration = 3000.f;

	/** 最大速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Grappling", meta = (ClampMin = "0"))
	float MaxSwingSpeed = 2500.f;

	UFUNCTION(BlueprintOverride)
	bool GenerateMove(const FMoverTimeStep& TimeStep, UMoverBlackboard SimBlackboard, const FMoverTickStartData& StartState, FProposedMove& OutProposedMove)
	{
		// 锚点从输入包读取，保证服务器 re-simulation 时与客户端使用相同数据
		FYanCharacterInputs YanInputs;
		if (!YanMoverAngelscript::GetYanCharacterInputs(StartState.InputCmd.InputCollection, YanInputs) || !YanInputs.bIsGrapplingActive)
		{
			return false;
		}

		FMoverDefaultSyncState SyncState;
		if (!YanMoverAngelscript::GetDefaultSyncState(StartState.SyncState.SyncStateCollection, SyncState))
		{
			return false;
		}

		const float   DeltaSeconds  = TimeStep.StepMs * 0.001f;
		const FVector CurrentPos    = YanMoverAngelscript::GetSyncStateLocation_WorldSpace(SyncState);
		FVector       Velocity      = YanMoverAngelscript::GetSyncStateVelocity_WorldSpace(SyncState);

		const FVector AnchorPoint   = YanInputs.GrappleAnchorPoint;

		const FVector ToAnchor  = AnchorPoint - CurrentPos;
		const float   Dist      = ToAnchor.Size();
		const FVector AnchorDir = (Dist > 0.0001f) ? (ToAnchor / Dist) : FVector::UpVector;

		// 重力
		Velocity += YanMoverAngelscript::GetGravityFromBlackboard(SimBlackboard) * DeltaSeconds;

		// 主动拉力
		Velocity += AnchorDir * PullAcceleration * DeltaSeconds;

		// 限速
		const float Speed = Velocity.Size();
		if (Speed > MaxSwingSpeed && Speed > 0.f)
		{
			Velocity *= MaxSwingSpeed / Speed;
		}

		OutProposedMove.LinearVelocity = Velocity;
		OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
		return true;
	}
}
