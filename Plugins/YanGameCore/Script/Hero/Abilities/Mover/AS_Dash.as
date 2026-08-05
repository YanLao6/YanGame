/**
 * UAS_CharacterDash — 空中冲刺输入技能（点按触发）
 *
 * 本技能只把冲刺意图写进输入命令包，速度由 sim 侧 UChaosCharacterAirDashCheck 施加。
 * 从游戏线程下发的移动效果会被引擎标记为不可回滚，重模拟时无法重放，
 * 客户端预测出的冲刺会被服务器权威状态抹掉，故必须走输入包。
 */
class UAS_CharacterDash : UMoverInputAbility
{
    // 待触发标志：意图须在 ProduceMoverInput 里写入输入包，故 Activate 不直接施加效果
    private bool bPendingDash = false;

    //~Begin UMoverInputAbility Interface
    UFUNCTION(BlueprintOverride)
    void ActivateAbility()
    {
        // 仅限空中。此处读的是 game-thread 缓存态，只用于滤掉明显无效的按键，
        // 权威判定由 UChaosCharacterAirDashCheck 的挂载模式保证。
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
        // 每帧显式写入而非仅在有脉冲时写：脉冲位必须在投递后的次帧确定为 false，
        // 否则输入命令若被跨帧复用，残留的脉冲会触发第二次冲刺。
        // 方向与速度均不上传：前者由 FCharacterDefaultInputs 推导，后者取自 Transition 配置
        FYanCharacterInputs YanInputs = YanMoverAngelscript::GetOrAddYanInputs(InputCmd);
        YanInputs.bWantsToAirDash = bPendingDash;
        bPendingDash = false;
        YanMoverAngelscript::CommitYanInputs(InputCmd, YanInputs);
    }

    UFUNCTION(BlueprintOverride)
    void InputReleased()
    {
        EndAbility();
    }
    //~End UMoverInputAbility Interface
}
