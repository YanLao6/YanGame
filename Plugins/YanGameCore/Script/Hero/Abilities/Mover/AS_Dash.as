/**
 * UAS_CharacterDash — 冲刺输入技能（点按触发）
 */
class UAS_CharacterDash : UMoverInputAbility
{
    /** 冲刺速度大小（cm/s） */
    UPROPERTY()
    float DashMagnitude = 1500;

    // 待触发标志：方向须在 ProduceMoverInput 里读，故 Activate 不直接入队
    private bool bPendingDash = false;

    //~Begin UMoverInputAbility Interface
    UFUNCTION(BlueprintOverride)
    void ActivateAbility()
    {
        // 仅限空中：IsFalling 覆盖跳跃上升段与自由下落，落地即不可冲刺。
        // 此处结束早于冷却标记建立，不会进入冷却
        UCharacterMoverComponent CharMoverComp = Cast<UCharacterMoverComponent>(CachedMoverComp);
        if (CharMoverComp == nullptr || !CharMoverComp.IsFalling())
        {
            return;
        }

        bPendingDash = true;
    }

    UFUNCTION(BlueprintOverride)
    void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
    {
        if (!bPendingDash)
        {
            return;
        }
        bPendingDash = false;

        // 本帧移动输入方向（世界空间），由 UYanHeroInputComponent 先行写入
        FCharacterDefaultInputs Inputs = YanMoverAngelscript::GetOrAddDefaultInputs(InputCmd);
        FVector MoveDir = YanMoverAngelscript::GetMoveInputWorldSpace(Inputs);

        FVector DashDir;
        if (MoveDir.IsNearlyZero())
        {
            // 无移动输入：回退到角色前向，避免原地无方向冲刺
            APawn Pawn = UVerbMessageHelpers::GetPlayerPawnFromObject(ActorInfo.AvatarActor);
            DashDir = (Pawn != nullptr) ? Pawn.GetActorForwardVector() : MoverComp.GetOwner().GetActorForwardVector();
        }
        else
        {
            DashDir = MoveDir.GetSafeNormal();
        }

        FVector DashVelocity = DashDir * DashMagnitude;
        YanMoverAngelscript::QueueChaosCharacterApplyVelocityEffect(MoverComp, DashVelocity, true);
    }
    
    UFUNCTION(BlueprintOverride)
    void InputReleased()
    {
        EndAbility();
    }
    //~End UMoverInputAbility Interface
}
