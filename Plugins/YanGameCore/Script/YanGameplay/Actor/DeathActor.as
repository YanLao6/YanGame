/**
 * ADeathActor — 出界判定体积。
 *
 * 玩家重叠即判定出界：向其发送 GameplayEvent.Death，由 UYanDeathAbility 接管
 * 清零负荷、开放特长改选与安排重生。
 *
 * 不施加 GameplayEffect：出界是一次性瞬时事件，没有需要驻留的状态，
 * 而 GE 授予的 Tag 既带不了载荷，也会在 Tag 尚未移除时压掉第二次触发。
 *
 * 只在服务器上判定。重叠回调两端都会触发，若客户端也自行发事件，
 * 会在服务器尚未确认时就播放死亡表现，被打飞擦过边缘却生还时尤其明显。
 *
 * 摆放：做成一个兜住场地下方与四周的大盒子，玩家被击飞后无论朝哪个方向出界都会撞进来。
 * 落进虚空的碎块与投射物不在此处理——它们没有重生流程，交给 World Settings 的 KillZ 清理即可。
 */
class ADeathActor : AActor
{
	/** 出界触发盒，作为根组件 */
	UPROPERTY(DefaultComponent, RootComponent)
	UBoxComponent TriggerBox;
	default TriggerBox.BoxExtent = FVector(5000.0, 5000.0, 200.0);

	// 事件标签由 C++ 侧的 UE_DEFINE_GAMEPLAY_TAG 声明，脚本按名索取
	private FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(n"GameplayEvent.Death");

	// 死亡技能激活期间会挂上此标签，用于挡掉同一次死亡的重复触发
	private FGameplayTag DeadStatusTag = FGameplayTag::RequestGameplayTag(n"Status.Death.Dead");

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		TriggerBox.SetCollisionProfileName(n"OverlapAllDynamic");
		TriggerBox.SetGenerateOverlapEvents(true);
		TriggerBox.OnComponentBeginOverlap.AddUFunction(this, n"HandleBeginOverlap");
	}

	UFUNCTION()
	private void HandleBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor,
		UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in Hit)
	{
		if (!HasAuthority())
		{
			return;
		}

		// 只有 Pawn 有重生流程；碎块与投射物掉进来无需惊动 GAS
		APawn OtherPawn = Cast<APawn>(OtherActor);
		if (OtherPawn == nullptr)
		{
			return;
		}

		UAbilitySystemComponent TargetASC = AbilitySystem::GetAbilitySystemComponent(OtherPawn);
		if (TargetASC == nullptr)
		{
			return;
		}

		// 盒子较厚时同一次坠落可能触发多次重叠，重复发事件会重置死亡技能里的重生定时器
		FGameplayTagContainer OwnedTags;
		TargetASC.GetOwnedGameplayTags(OwnedTags);
		if (OwnedTags.HasTag(DeadStatusTag))
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag   = DeathEventTag;
		Payload.Instigator = this;
		Payload.Target     = OtherPawn;

		AbilitySystem::SendGameplayEventToActor(OtherPawn, DeathEventTag, Payload);
	}
}
