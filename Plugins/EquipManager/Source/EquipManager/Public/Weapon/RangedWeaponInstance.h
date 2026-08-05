// 远程武器实例声明。

#pragma once

#include "Curves/CurveFloat.h"

#include "WeaponInstance.h"
#include "GameplayAbilities/ModularAbilitySourceInterface.h"

#include "RangedWeaponInstance.generated.h"

class UPhysicalMaterial;

/**
 * 远程武器实例。
 *
 * 表示一个被生成并附加到 Pawn 上的远程武器，负责管理散布、热量、距离衰减和物理材质伤害系数。
 */
UCLASS(MinimalAPI)
class URangedWeaponInstance : public UWeaponInstance, public IModularAbilitySourceInterface
{
	GENERATED_BODY()

public:
	/** 构造远程武器实例并写入基础曲线默认值。 */
	URangedWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UObject Interface
	// 资源加载完成后刷新编辑器调试可视化数据。
	virtual void PostLoad() override;
	//~End UObject Interface

#if WITH_EDITOR
	//~Begin UObject Interface
	// 属性在编辑器内被修改后刷新调试可视化数据。
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

	// 更新编辑器面板中展示的调试数值。
	void UpdateDebugVisualization();
#endif

	/** 返回每发弹药对应生成的子弹数量，例如霰弹枪可大于 1。 */
	int32 GetBulletsPerCartridge() const
	{
		return BulletsPerCartridge;
	}
	
	/** 返回当前散布角度，单位为度，表示完整夹角。 */
	float GetCalculatedSpreadAngle() const
	{
		return CurrentSpreadAngle;
	}

	/** 返回当前综合散布倍率；若拥有首发精准，则结果固定为 0。 */
	float GetCalculatedSpreadAngleMultiplier() const
	{
		return bHasFirstShotAccuracy ? 0.0f : CurrentSpreadAngleMultiplier;
	}

	/** 判断当前是否满足首发精准条件。 */
	bool HasFirstShotAccuracy() const
	{
		return bHasFirstShotAccuracy;
	}

	/** 返回控制弹着分布聚拢程度的散布指数。 */
	float GetSpreadExponent() const
	{
		return SpreadExponent;
	}

	/** 返回该武器可造成伤害的最大距离。 */
	float GetMaxDamageRange() const
	{
		return MaxDamageRange;
	}

	/** 返回子弹检测时使用的扫掠半径，0 表示退化为线性射线。 */
	float GetBulletTraceSweepRadius() const
	{
		return BulletTraceSweepRadius;
	}

protected:
#if WITH_EDITORONLY_DATA
	/** 调试显示：热量曲线允许的最小热量值。 */
	UPROPERTY(VisibleAnywhere, Category = "Spread|Fire Params")
	float Debug_MinHeat = 0.0f;

	/** 调试显示：热量曲线允许的最大热量值。 */
	UPROPERTY(VisibleAnywhere, Category = "Spread|Fire Params")
	float Debug_MaxHeat = 0.0f;

	/** 调试显示：当前配置下可能出现的最小散布角。 */
	UPROPERTY(VisibleAnywhere, Category="Spread|Fire Params", meta=(ForceUnits=deg))
	float Debug_MinSpreadAngle = 0.0f;

	/** 调试显示：当前配置下可能出现的最大散布角。 */
	UPROPERTY(VisibleAnywhere, Category="Spread|Fire Params", meta=(ForceUnits=deg))
	float Debug_MaxSpreadAngle = 0.0f;

	/** 调试显示：当前实时热量值。 */
	UPROPERTY(VisibleAnywhere, Category="Spread Debugging")
	float Debug_CurrentHeat = 0.0f;

	/** 调试显示：当前实时散布角。 */
	UPROPERTY(VisibleAnywhere, Category="Spread Debugging", meta = (ForceUnits=deg))
	float Debug_CurrentSpreadAngle = 0.0f;

	/** 调试显示：当前综合后的散布倍率。 */
	UPROPERTY(VisibleAnywhere, Category = "Spread Debugging", meta=(ForceUnits=x))
	float Debug_CurrentSpreadAngleMultiplier = 1.0f;

#endif

	/**
	 * 散布指数。
	 * 数值越大，子弹越倾向于集中在准星中心附近；1 表示在散布范围内均匀分布。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=0.1), Category="Spread|Fire Params")
	float SpreadExponent = 1.0f;

	/**
	 * 热量到散布角的映射曲线。
	 * 曲线 X 轴通常决定武器的有效热量区间，Y 轴则定义最小到最大散布角范围。
	 */
	UPROPERTY(EditAnywhere, Category = "Spread|Fire Params")
	FRuntimeFloatCurve HeatToSpreadCurve;

	/**
	 * 当前热量到单次射击增热量的映射曲线。
	 * 常见做法是使用平坦曲线表示每次开火固定增加热量，也可以通过曲线形状强化过热惩罚。
	 */
	UPROPERTY(EditAnywhere, Category="Spread|Fire Params")
	FRuntimeFloatCurve HeatToHeatPerShotCurve;
	
	/**
	 * 当前热量到每秒降温速度的映射曲线。
	 * 可用于控制高热量时降温变慢、低热量时快速恢复等不同手感。
	 */
	UPROPERTY(EditAnywhere, Category="Spread|Fire Params")
	FRuntimeFloatCurve HeatToCoolDownPerSecondCurve;

	/** 开火后需要等待多久才开始进入散布恢复，单位为秒。 */
	UPROPERTY(EditAnywhere, Category="Spread|Fire Params", meta=(ForceUnits=s))
	float SpreadRecoveryCooldownDelay = 0.0f;

	/** 当武器散布和角色状态倍率都处于最小值时，是否启用首发精准。 */
	UPROPERTY(EditAnywhere, Category="Spread|Fire Params")
	bool bAllowFirstShotAccuracy = false;

	/** 进入瞄准镜头模式时应用的散布倍率。 */
	UPROPERTY(EditAnywhere, Category="Spread|Player Params", meta=(ForceUnits=x))
	float SpreadAngleMultiplier_Aiming = 1.0f;

	/**
	 * 静止或低速移动时应用的散布倍率。
	 * 当速度超过阈值后会逐渐插值回 1。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params", meta=(ForceUnits=x))
	float SpreadAngleMultiplier_StandingStill = 1.0f;

	/** 静止散布倍率在状态切换时的插值速度；数值越大切换越快，0 表示瞬间切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params")
	float TransitionRate_StandingStill = 5.0f;

	/** 低于或等于该速度时视为静止状态，单位为厘米每秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params", meta=(ForceUnits="cm/s"))
	float StandingStillSpeedThreshold = 80.0f;

	/** 超出静止阈值后的缓冲速度区间，用于平滑衰减静止散布加成。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params", meta=(ForceUnits="cm/s"))
	float StandingStillToMovingSpeedRange = 20.0f;


	/** 蹲伏状态下应用的散布倍率，会按照过渡速率平滑切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params", meta=(ForceUnits=x))
	float SpreadAngleMultiplier_Crouching = 1.0f;

	/** 蹲伏散布倍率在状态切换时的插值速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params")
	float TransitionRate_Crouching = 5.0f;


	/** 跳跃或下落状态下应用的散布倍率，会按照过渡速率平滑切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params", meta=(ForceUnits=x))
	float SpreadAngleMultiplier_JumpingOrFalling = 1.0f;

	/** 跳跃或下落散布倍率在状态切换时的插值速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread|Player Params")
	float TransitionRate_JumpingOrFalling = 5.0f;

	/** 每发弹药实际生成的子弹数量，霰弹枪等武器通常会大于 1。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Config")
	int32 BulletsPerCartridge = 1;

	/** 武器能造成伤害的最大有效距离，单位为厘米。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Config", meta=(ForceUnits=cm))
	float MaxDamageRange = 25000.0f;

	/** 子弹命中检测的扫掠半径；当值为 0 时使用普通线性射线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Config", meta=(ForceUnits=cm))
	float BulletTraceSweepRadius = 0.0f;

	/**
	 * 距离到基础伤害倍率的映射曲线，单位为厘米。
	 * 若曲线无数据，则视为武器伤害不随距离衰减。
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon Config")
	FRuntimeFloatCurve DistanceDamageFalloff;

	/**
	 * 物理材质标签对应的伤害倍率表。
	 * 命中目标后会读取物理材质上的标签，若命中多个标签则按乘法叠乘。
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon Config")
	TMap<FGameplayTag, float> MaterialDamageMultiplier;

private:
	// 上一次开火发生时的世界时间。
	double LastFireTime = 0.0;

	// 当前热量值。
	float CurrentHeat = 0.0f;

	// 当前散布角，单位为度，表示完整夹角。
	float CurrentSpreadAngle = 0.0f;

	// 当前是否满足首发精准条件。
	bool bHasFirstShotAccuracy = false;

	// 当前综合后的散布倍率。
	float CurrentSpreadAngleMultiplier = 1.0f;

	// 当前静止状态散布倍率。
	float StandingStillMultiplier = 1.0f;

	// 当前跳跃或下落状态散布倍率。
	float JumpFallMultiplier = 1.0f; 
	
	// 当前蹲伏状态散布倍率。
	float CrouchingMultiplier = 1.0f;

public:
	/** 每帧更新热量、散布与角色状态倍率。 */
	void Tick(float DeltaSeconds);

	//~Begin UEquipmentInstance Interface
	// 装备远程武器时初始化热量、散布和状态倍率。
	virtual void OnEquipped();
	// 卸下远程武器时执行基类清理逻辑。
	virtual void OnUnequipped();
	//~End UEquipmentInstance Interface

	/** 在每次射击后增加当前武器热量，并重新计算散布角。 */
	void AddSpread();

	//~Begin IModularAbilitySourceInterface Interface
	// 根据命中距离返回伤害衰减倍率。
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
	// 根据命中的物理材质标签返回伤害衰减倍率。
	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
	//~End IModularAbilitySourceInterface Interface

private:
	// 计算热量曲线对应的散布角最小值与最大值。
	void ComputeSpreadRange(float& MinSpread, float& MaxSpread);
	// 计算当前武器配置允许的最小热量与最大热量。
	void ComputeHeatRange(float& MinHeat, float& MaxHeat);

	// 将热量限制在曲线定义的合法范围内。
	inline float ClampHeat(float NewHeat)
	{
		float MinHeat;
		float MaxHeat;
		ComputeHeatRange(/*输出*/ MinHeat, /*输出*/ MaxHeat);

		return FMath::Clamp(NewHeat, MinHeat, MaxHeat);
	}

	// 更新散布恢复逻辑，并返回当前散布是否已回到最小值。
	bool UpdateSpread(float DeltaSeconds);

	// 更新各类角色状态倍率，并返回它们是否都已达到最小散布条件。
	bool UpdateMultipliers(float DeltaSeconds);
};
