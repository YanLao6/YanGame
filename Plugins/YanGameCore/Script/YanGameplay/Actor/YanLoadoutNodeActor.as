/**
 * AYanLoadoutNodeActor — 资源点：站够时间即解锁一个特长槽位。
 *
 * 场上只需一处。它是特长解锁的唯一来源，因此位置决定了整局的争夺焦点——
 * 摆在场地中央即为「强制交战」，摆在边缘则把解锁与出界风险绑在一起。
 *
 * 双方同时在圈内时进度冻结，谁也读不了条。这条规则让资源点自带对抗：
 * 想解锁就得先把对方推出去，而不是各读各的。
 *
 * 进度按玩家记在其 PlayerState 的 UYanLoadoutComponent 上，不记在本 Actor 上。
 * 玩家死亡重生、乃至换一处资源点，进度都应当延续；记在 Actor 上则会随离场清零。
 * 离开圈内进度按 DecayDuration 衰减而非立即清零：被打飞一次不该等同于从头再来。
 *
 * 只在服务器推进。解锁结果经 UYanLoadoutComponent 复制回客户端。
 *
 * 摆放要点：
 *   - 触发体须完全落在可站立面之上，否则玩家踩在边缘会反复进出、进度在涨落之间空转
 *   - 不要与出界体积（ADeathActor）重叠
 *
 * 蓝图需设置（可选）：
 *   NodeVisual → 标示资源点范围的 Mesh；不设则资源点在游戏中不可见
 */
class AYanLoadoutNodeActor : AActor
{
	/** 占领判定范围，作为根组件 */
	UPROPERTY(DefaultComponent, RootComponent)
	USphereComponent TriggerVolume;
	default TriggerVolume.SphereRadius = 400.0;

	/** 资源点的可见标示；纯展示，不参与判定 */
	UPROPERTY(DefaultComponent, Attach = TriggerVolume)
	UStaticMeshComponent NodeVisual;

	/** 独占圈内时读满一格所需时长 */
	UPROPERTY(EditAnywhere, Category = "LoadoutNode", Meta = (ClampMin = "0.1", ForceUnits = "s"))
	float CaptureDuration = 6.0f;

	/** 离开圈内后从满进度衰减到零所需时长；设为 0 则离开即清零 */
	UPROPERTY(EditAnywhere, Category = "LoadoutNode", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DecayDuration = 12.0f;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		TriggerVolume.SetCollisionProfileName(n"OverlapAllDynamic");
		TriggerVolume.SetGenerateOverlapEvents(true);

		// 标示 Mesh 若带碰撞会把玩家挡在圈外，占领因而永远无法开始
		NodeVisual.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		if (!HasAuthority())
		{
			return;
		}

		AGameStateBase GameState = Gameplay::GetGameState();
		if (GameState == nullptr)
		{
			return;
		}

		TArray<AActor> Occupants;
		TriggerVolume.GetOverlappingActors(Occupants, APawn);

		// 争夺状态按「圈内有几个还需要解锁的玩家」判定：
		// 已集齐特长的人站在圈里不该还能卡住对方
		const bool bContested = CountActiveOccupants(GameState, Occupants) >= 2;

		// PlayerArray 元素是 TObjectPtr，foreach 迭代器转不成句柄，按下标取
		for (int32 Index = 0; Index < GameState.PlayerArray.Num(); Index++)
		{
			APlayerState PlayerState = GameState.PlayerArray[Index];

			UYanLoadoutComponent Loadout = ResolveActiveLoadout(PlayerState);
			if (Loadout == nullptr)
			{
				continue;
			}

			const bool bInside = IsPawnInside(Occupants, PlayerState.GetPawn());
			AdvanceCapture(Loadout, bInside, bContested, DeltaSeconds);
		}
	}

	// 推进单个玩家的占领进度；达标即解锁一格并从零重新开始
	private void AdvanceCapture(UYanLoadoutComponent Loadout, bool bInside, bool bContested, float DeltaSeconds)
	{
		float Progress = Loadout.GetCaptureProgress();

		if (bInside)
		{
			// 争夺中进度冻结，不涨也不退：拉锯本身就是代价
			if (bContested)
			{
				return;
			}

			Progress += DeltaSeconds / CaptureDuration;
		}
		else if (DecayDuration > 0.0f)
		{
			Progress -= DeltaSeconds / DecayDuration;
		}
		else
		{
			Progress = 0.0f;
		}

		if (Progress >= 1.0f)
		{
			Loadout.GrantSelectionSlot();
			Progress = 0.0f;
		}

		Loadout.SetCaptureProgress(Math::Clamp(Progress, 0.0f, 1.0f));
	}

	// 圈内仍有解锁余量的玩家数
	private int32 CountActiveOccupants(AGameStateBase GameState, const TArray<AActor>&in Occupants) const
	{
		int32 Count = 0;

		for (int32 Index = 0; Index < GameState.PlayerArray.Num(); Index++)
		{
			APlayerState PlayerState = GameState.PlayerArray[Index];

			UYanLoadoutComponent Loadout = ResolveActiveLoadout(PlayerState);
			if (Loadout != nullptr && IsPawnInside(Occupants, PlayerState.GetPawn()))
			{
				Count++;
			}
		}

		return Count;
	}

	// 取该玩家仍需解锁的特长组件；已集齐或无组件时返回 nullptr
	private UYanLoadoutComponent ResolveActiveLoadout(APlayerState PlayerState) const
	{
		if (PlayerState == nullptr)
		{
			return nullptr;
		}

		UYanLoadoutComponent Loadout = UYanLoadoutComponent::Get(PlayerState);
		if (Loadout == nullptr || Loadout.IsFullyUnlocked())
		{
			return nullptr;
		}

		return Loadout;
	}

	private bool IsPawnInside(const TArray<AActor>&in Occupants, APawn Candidate) const
	{
		if (Candidate == nullptr)
		{
			return false;
		}

		for (AActor Occupant : Occupants)
		{
			if (Cast<APawn>(Occupant) == Candidate)
			{
				return true;
			}
		}

		return false;
	}
}
