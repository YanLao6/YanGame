/**
 * UAS_CharacterBlink — 瞬移输入技能（点按触发）
 *
 * 本技能负责两件事：在 game thread 扫描出瞬移终点，再把终点写进输入命令包，
 * 实际位移由 sim 侧 UChaosCharacterBlinkCheck 与 FYanLayeredMove_BlinkTo 完成。
 * 终点只在此处算一次并上传，服务器不重算——碰撞扫描依赖 game-thread 场景，
 * 两端各扫一次不保证得到同一落点，轨迹会就此分歧。
 */
class UAS_CharacterBlink : UMoverInputAbility
{
    /** 最大瞬移距离（cm），沿途被挡则停在障碍前 */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0", ForceUnits = "cm"))
    float BlinkDistance = 900.f;

    /**
     * 位移时长（秒），须与 UChaosCharacterBlinkCheck::BlinkDurationMs 保持一致。
     * 收招硬直自位移结束起算，两处对不上就会提前或滞后开窗。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0", ForceUnits = "s"))
    float BlinkDurationSeconds = 0.15f;

    /**
     * 位移结束后施加的收招硬直效果，其持续时长即窗口长度；留空即没有硬直。
     *
     * 硬直是瞬移的定价方式之一：能量之外还要付出一段暴露的收招时间，对手在这段窗口里
     * 的攻击是确定命中的。效果本身只授予标签，「哪些技能被禁」配在各技能的
     * ActivationBlockedTags 上，本类不必知道有哪些技能存在。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    TSubclassOf<UGameplayEffect> RecoveryEffect;

    // 待投递的瞬移意图与终点：须在 ProduceMoverInput 里写入输入包，故 Activate 不直接施加效果
    private bool bPendingBlink = false;
    private FVector PendingTargetPoint;

    private FTimerHandle RecoveryTimer;

    //~Begin UMoverInputAbility Interface
    UFUNCTION(BlueprintOverride)
    void ActivateAbility()
    {
        if (CachedMoverComp == nullptr)
        {
            return;
        }

        FVector ViewLocation;
        FVector ViewForward;
        if (!YanMoverAngelscript::GetOwnerViewLocationAndForward(CachedMoverComp, ViewLocation, ViewForward))
        {
            return;
        }

        // 压平到水平面：瞬移取视角的朝向而非俯仰，否则抬头会飞天、低头会扎地
        FVector BlinkDir = FVector(ViewForward.X, ViewForward.Y, 0.0).GetSafeNormal();
        if (BlinkDir.IsNearlyZero())
        {
            // 视线接近垂直时水平分量退化，回退到角色朝向
            BlinkDir = CachedMoverComp.GetOwner().GetActorForwardVector().GetSafeNormal2D();
        }

        FVector TargetPoint;
        if (!YanMoverAngelscript::SweepBlinkTarget(CachedMoverComp, BlinkDir, BlinkDistance, TargetPoint))
        {
            return;
        }

        PendingTargetPoint = TargetPoint;
        bPendingBlink = true;

        ScheduleRecovery();
    }

    // 硬直只由服务器安排：这是留给对手的确定命中窗口，判定权不能交给被瞬移的一方
    private void ScheduleRecovery()
    {
        AActor Avatar = GetAvatarActorFromActorInfo();
        if (RecoveryEffect.Get() == nullptr || Avatar == nullptr || !Avatar.HasAuthority())
        {
            return;
        }

        System::ClearAndInvalidateTimerHandle(RecoveryTimer);
        RecoveryTimer = System::SetTimer(this, n"ApplyBlinkRecovery", Math::Max(BlinkDurationSeconds, 0.01f), false);
    }

    /**
     * 位移结束时挂上硬直。
     *
     * 计时器有意不在 EndAbility 里清除：本技能点按即用，松手当帧就结束，
     * 而位移还在 sim 里跑着——跟着技能一起清掉，硬直就永远等不到。
     */
    UFUNCTION()
    private void ApplyBlinkRecovery()
    {
        AActor Avatar = GetAvatarActorFromActorInfo();
        if (Avatar == nullptr)
        {
            return;
        }

        UAbilitySystemComponent ASC = AbilitySystem::GetAbilitySystemComponent(Avatar);
        if (ASC == nullptr)
        {
            return;
        }

        FGameplayEffectContextHandle Context = ASC.MakeEffectContext();
        FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(RecoveryEffect, 1.0f, Context);
        if (Spec.IsValid())
        {
            ASC.ApplyGameplayEffectSpecToSelf(Spec);
        }
    }

    UFUNCTION(BlueprintOverride)
    void ProduceMoverInput(UMoverComponent MoverComp, int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
    {
        // 每帧显式写入而非仅在有脉冲时写：脉冲位必须在投递后的次帧确定为 false，
        // 否则输入命令若被跨帧复用，残留的脉冲会触发第二次瞬移
        FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
        YanInputs.bWantsToBlink = bPendingBlink;
        if (bPendingBlink)
        {
            YanInputs.BlinkTargetPoint = PendingTargetPoint;
        }
        bPendingBlink = false;
        YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
    }

    UFUNCTION(BlueprintOverride)
    void InputReleased()
    {
        EndAbility();
    }
    //~End UMoverInputAbility Interface
}
