// 远程武器实例实现。

#include "Weapon/RangedWeaponInstance.h"

#include "ModalCameraComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Physics/PhysicalMaterialWithTags.h"
#include "Weapon/WeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedWeaponInstance)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG__Weapon_SteadyAimingCamera, "EM.Weapon.SteadyAimingCamera");

URangedWeaponInstance::URangedWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 为未手动配置的资源提供一组基础可用的热量曲线默认值。
	HeatToHeatPerShotCurve.EditorCurveData.AddKey(0.0f, 1.0f);
	HeatToCoolDownPerSecondCurve.EditorCurveData.AddKey(0.0f, 2.0f);
}

//~Begin UObject Interface
void URangedWeaponInstance::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// 资源加载完成后刷新调试面板显示。
	UpdateDebugVisualization();
#endif
}

//~End UObject Interface

#if WITH_EDITOR
//~Begin UObject Interface
void URangedWeaponInstance::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// 编辑器里改动任意属性后，立即更新调试可视化数值。
	UpdateDebugVisualization();
}

//~End UObject Interface

void URangedWeaponInstance::UpdateDebugVisualization()
{
	// 将运行时计算结果写回到编辑器可见字段，方便调参。
	ComputeHeatRange(/*输出*/ Debug_MinHeat, /*输出*/ Debug_MaxHeat);
	ComputeSpreadRange(/*输出*/ Debug_MinSpreadAngle, /*输出*/ Debug_MaxSpreadAngle);
	Debug_CurrentHeat                  = CurrentHeat;
	Debug_CurrentSpreadAngle           = CurrentSpreadAngle;
	Debug_CurrentSpreadAngleMultiplier = CurrentSpreadAngleMultiplier;
}
#endif

//~Begin UEquipmentInstance Interface
void URangedWeaponInstance::OnEquipped()
{
	Super::OnEquipped();

	// 初始热量从曲线区间中点开始，避免一开始就处于极端状态。
	float MinHeatRange;
	float MaxHeatRange;
	ComputeHeatRange(/*输出*/ MinHeatRange, /*输出*/ MaxHeatRange);
	CurrentHeat = (MinHeatRange + MaxHeatRange) * 0.5f;

	// 根据初始热量推导初始散布角。
	CurrentSpreadAngle = HeatToSpreadCurve.GetRichCurveConst()->Eval(CurrentHeat);

	// 初始化各类状态倍率为 1 倍。
	CurrentSpreadAngleMultiplier = 1.0f;
	StandingStillMultiplier      = 1.0f;
	JumpFallMultiplier           = 1.0f;
	CrouchingMultiplier          = 1.0f;
}

void URangedWeaponInstance::OnUnequipped()
{
	Super::OnUnequipped();
}

//~End UEquipmentInstance Interface

void URangedWeaponInstance::Tick(float DeltaSeconds)
{
	APawn* Pawn = GetPawn();
	check(Pawn != nullptr);

	// 每帧更新武器自身散布恢复，以及角色状态带来的额外倍率。
	const bool bMinSpread      = UpdateSpread(DeltaSeconds);
	const bool bMinMultipliers = UpdateMultipliers(DeltaSeconds);

	// 只有在允许首发精准且所有散布条件都回到最优时，才视为首发精准生效。
	bHasFirstShotAccuracy = bAllowFirstShotAccuracy && bMinMultipliers && bMinSpread;

#if WITH_EDITOR
	UpdateDebugVisualization();
#endif
}

void URangedWeaponInstance::ComputeHeatRange(float& MinHeat, float& MaxHeat)
{
	float Min1;
	float Max1;
	// 三条曲线都可能定义各自的 X 轴范围，需要综合求出合法热量区间。
	HeatToHeatPerShotCurve.GetRichCurveConst()->GetTimeRange(/*输出*/ Min1, /*输出*/ Max1);

	float Min2;
	float Max2;
	HeatToCoolDownPerSecondCurve.GetRichCurveConst()->GetTimeRange(/*输出*/ Min2, /*输出*/ Max2);

	float Min3;
	float Max3;
	HeatToSpreadCurve.GetRichCurveConst()->GetTimeRange(/*输出*/ Min3, /*输出*/ Max3);

	MinHeat = FMath::Min(FMath::Min(Min1, Min2), Min3);
	MaxHeat = FMath::Max(FMath::Max(Max1, Max2), Max3);
}

void URangedWeaponInstance::ComputeSpreadRange(float& MinSpread, float& MaxSpread)
{
	// 散布角范围直接由热量到散布曲线的值域决定。
	HeatToSpreadCurve.GetRichCurveConst()->GetValueRange(/*输出*/ MinSpread, /*输出*/ MaxSpread);
}

void URangedWeaponInstance::AddSpread()
{
	// 先根据当前热量采样单次开火应增加的热量。
	const float HeatPerShot = HeatToHeatPerShotCurve.GetRichCurveConst()->Eval(CurrentHeat);
	CurrentHeat             = ClampHeat(CurrentHeat + HeatPerShot);

	// 再把更新后的热量映射为最新散布角。
	CurrentSpreadAngle = HeatToSpreadCurve.GetRichCurveConst()->Eval(CurrentHeat);

#if WITH_EDITOR
	UpdateDebugVisualization();
#endif
}

//~Begin IModularAbilitySourceInterface Interface
float URangedWeaponInstance::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	const FRichCurve* Curve = DistanceDamageFalloff.GetRichCurveConst();
	// 若未配置距离衰减曲线，则默认伤害倍率恒为 1。
	return Curve->HasAnyData() ? Curve->Eval(Distance) : 1.0f;
}

float URangedWeaponInstance::GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	float CombinedMultiplier = 1.0f;
	if (const UPhysicalMaterialWithTags* PhysMatWithTags = Cast<const UPhysicalMaterialWithTags>(PhysicalMaterial))
	{
		// 命中材质上的多个标签时，将它们对应的倍率逐项相乘。
		for (const FGameplayTag MaterialTag : PhysMatWithTags->Tags)
		{
			if (const float* pTagMultiplier = MaterialDamageMultiplier.Find(MaterialTag))
			{
				CombinedMultiplier *= *pTagMultiplier;
			}
		}
	}

	return CombinedMultiplier;
}

//~End IModularAbilitySourceInterface Interface

bool URangedWeaponInstance::UpdateSpread(float DeltaSeconds)
{
	const float TimeSinceFired = GetWorld()->TimeSince(LastFireTime);

	if (TimeSinceFired > SpreadRecoveryCooldownDelay)
	{
		// 只有超过冷却延迟后，武器才开始按曲线设定进行降温与散布恢复。
		const float CooldownRate = HeatToCoolDownPerSecondCurve.GetRichCurveConst()->Eval(CurrentHeat);
		CurrentHeat              = ClampHeat(CurrentHeat - (CooldownRate * DeltaSeconds));
		CurrentSpreadAngle       = HeatToSpreadCurve.GetRichCurveConst()->Eval(CurrentHeat);
	}

	float MinSpread;
	float MaxSpread;
	ComputeSpreadRange(/*输出*/ MinSpread, /*输出*/ MaxSpread);

	return FMath::IsNearlyEqual(CurrentSpreadAngle, MinSpread, KINDA_SMALL_NUMBER);
}

bool URangedWeaponInstance::UpdateMultipliers(float DeltaSeconds)
{
	const float MultiplierNearlyEqualThreshold = 0.05f;

	APawn* Pawn = GetPawn();
	check(Pawn != nullptr);
	UCharacterMovementComponent* CharMovementComp = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());

	// 根据角色速度平滑计算静止状态散布倍率。
	const float PawnSpeed           = Pawn->GetVelocity().Size();
	const float MovementTargetValue = FMath::GetMappedRangeValueClamped(/*输入范围=*/ FVector2D(StandingStillSpeedThreshold, StandingStillSpeedThreshold + StandingStillToMovingSpeedRange),
	                                                                              /*输出范围=*/
	                                                                              FVector2D(SpreadAngleMultiplier_StandingStill, 1.0f),
	                                                                              /*输入值=*/
	                                                                              PawnSpeed);
	StandingStillMultiplier                  = FMath::FInterpTo(StandingStillMultiplier, MovementTargetValue, DeltaSeconds, TransitionRate_StandingStill);
	const bool bStandingStillMultiplierAtMin = FMath::IsNearlyEqual(StandingStillMultiplier, SpreadAngleMultiplier_StandingStill, SpreadAngleMultiplier_StandingStill * 0.1f);

	// 根据是否蹲伏平滑计算蹲伏状态散布倍率。
	const bool  bIsCrouching                = (CharMovementComp != nullptr) && CharMovementComp->IsCrouching();
	const float CrouchingTargetValue        = bIsCrouching ? SpreadAngleMultiplier_Crouching : 1.0f;
	CrouchingMultiplier                     = FMath::FInterpTo(CrouchingMultiplier, CrouchingTargetValue, DeltaSeconds, TransitionRate_Crouching);
	const bool bCrouchingMultiplierAtTarget = FMath::IsNearlyEqual(CrouchingMultiplier, CrouchingTargetValue, MultiplierNearlyEqualThreshold);

	// 根据是否在空中平滑计算跳跃或下落状态散布倍率。
	const bool  bIsJumpingOrFalling  = (CharMovementComp != nullptr) && CharMovementComp->IsFalling();
	const float JumpFallTargetValue  = bIsJumpingOrFalling ? SpreadAngleMultiplier_JumpingOrFalling : 1.0f;
	JumpFallMultiplier               = FMath::FInterpTo(JumpFallMultiplier, JumpFallTargetValue, DeltaSeconds, TransitionRate_JumpingOrFalling);
	const bool bJumpFallMultiplerIs1 = FMath::IsNearlyEqual(JumpFallMultiplier, 1.0f, MultiplierNearlyEqualThreshold);

	// 根据镜头混合进度计算瞄准状态的散布倍率。
	float AimingAlpha = 0.0f;
	if (const UModalCameraComponent* CameraComponent = UModalCameraComponent::FindCameraComponent(Pawn))
	{
		float        TopCameraWeight;
		FGameplayTag TopCameraTag;
		CameraComponent->GetBlendInfo(/*输出*/ TopCameraWeight, /*输出*/ TopCameraTag);

		AimingAlpha = (TopCameraTag == TAG__Weapon_SteadyAimingCamera) ? TopCameraWeight : 0.0f;
	}

	const float AimingMultiplier = FMath::GetMappedRangeValueClamped(/*输入范围=*/ FVector2D(0.0f, 1.0f),
	                                                                           /*输出范围=*/
	                                                                           FVector2D(1.0f, SpreadAngleMultiplier_Aiming),
	                                                                           /*输入值=*/
	                                                                           AimingAlpha);
	const bool bAimingMultiplierAtTarget = FMath::IsNearlyEqual(AimingMultiplier, SpreadAngleMultiplier_Aiming, KINDA_SMALL_NUMBER);

	// 汇总所有状态倍率，得到最终散布倍率。
	const float CombinedMultiplier = AimingMultiplier * StandingStillMultiplier * CrouchingMultiplier * JumpFallMultiplier;
	CurrentSpreadAngleMultiplier   = CombinedMultiplier;

	// 仅当所有倍率都回到最优状态时，才可视为满足最小散布条件。
	return bStandingStillMultiplierAtMin && bCrouchingMultiplierAtTarget && bJumpFallMultiplerIs1 && bAimingMultiplierAtTarget;
}
