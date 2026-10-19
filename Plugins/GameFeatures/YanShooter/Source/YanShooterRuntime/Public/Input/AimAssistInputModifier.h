// 瞄准辅助输入修饰器声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Math/IntRect.h"
#include "ScalableFloat.h"
#include "WorldCollision.h"
#include "Input/YanInputModifiers.h"
#include "DrawDebugHelpers.h"
#include "AimAssistInputModifier.generated.h"

class APlayerController;
class UInputAction;
class ULocalPlayer;
class UShapeComponent;
class UYanAimSensitivityData;
class UYanSettingsShared;

YANSHOOTERRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogAimAssist, Log, All);

/** 依当前 Pawn 与控制器整理出的视口数据，供目标可见性判定复用。 */
struct FAimAssistOwnerViewData
{
	FAimAssistOwnerViewData() { ResetViewData(); }

	/** 依当前玩家控制器刷新视图矩阵与朝向等数据。 */
	void UpdateViewData(const APlayerController* PC);

	/** 复位为默认值。 */
	void ResetViewData();

	/** 控制器与本地玩家均有效时才可用于计算。 */
	bool IsDataValid() const { return PlayerController != nullptr && LocalPlayer != nullptr; }

	FBox2D ProjectReticleToScreen(float ReticleWidth, float ReticleHeight, float ReticleDepth) const;
	FBox2D ProjectBoundsToScreen(const FBox& Bounds) const;
	FBox2D ProjectShapeToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;
	FBox2D ProjectBoxToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;
	FBox2D ProjectSphereToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;
	FBox2D ProjectCapsuleToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;

	const APlayerController* PlayerController = nullptr;

	const ULocalPlayer* LocalPlayer = nullptr;

	FMatrix ProjectionMatrix = FMatrix::Identity;

	FMatrix ViewProjectionMatrix = FMatrix::Identity;

	FIntRect ViewRect = FIntRect(0, 0, 0, 0);

	FTransform ViewTransform = FTransform::Identity;

	FVector ViewForward = FVector::ZeroVector;

	// 取 Actor 的位置与控制器的朝向组合而成
	FTransform PlayerTransform = FTransform::Identity;

	FTransform PlayerInverseTransform = FTransform::Identity;

	/** 本帧相对上一帧的位移。 */
	FVector DeltaMovement = FVector::ZeroVector;

	/** 所属队伍编号，取自 PlayerState；无 PlayerState 时为 INDEX_NONE。 */
	int32 TeamID = INDEX_NONE;
};

/** 跨帧缓存的单个瞄准辅助目标状态。 */
USTRUCT(BlueprintType)
struct FYanAimAssistTarget
{
	GENERATED_BODY()

	FYanAimAssistTarget() { ResetTarget(); }

	bool IsTargetValid() const { return TargetShapeComponent.IsValid(); }

	void ResetTarget();

	FRotator GetRotationFromMovement(const FAimAssistOwnerViewData& OwnerInfo) const;

	TWeakObjectPtr<UShapeComponent> TargetShapeComponent;

	FVector Location = FVector::ZeroVector;
	FVector DeltaMovement = FVector::ZeroVector;
	FBox2D ScreenBounds;

	float ViewDistance = 0.0f;
	float SortScore = 0.0f;

	float AssistTime = 0.0f;
	float AssistWeight = 0.0f;

	FTraceHandle VisibilityTraceHandle;

	uint8 bIsVisible : 1;

	uint8 bUnderAssistInnerReticle : 1;

	uint8 bUnderAssistOuterReticle : 1;

protected:
	float CalculateRotationToTarget2D(float TargetX, float TargetY, float OffsetY) const;
};

/** 目标筛选条件。 */
USTRUCT(BlueprintType)
struct FAimAssistFilter
{
	GENERATED_BODY()

	FAimAssistFilter()
		: bIncludeSameFriendlyTargets(false)
		, bExcludeInstigator(true)
		, bExcludeAllAttachedToInstigator(false)
		, bExcludeRequester(true)
		, bExcludeAllAttachedToRequester(false)
		, bTraceComplexCollision(false)
		, bExcludeDeadOrDying(true)
	{}

	/** 为真时，同队目标也纳入瞄准辅助。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	uint8 bIncludeSameFriendlyTargets : 1;

	/** 排除请求者的 Instigator。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeInstigator : 1;

	/** 排除挂在请求者 Instigator 之下的全部 Actor。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint32 bExcludeAllAttachedToInstigator : 1;

	/** 排除请求者自身。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeRequester : 1;

	/** 排除挂在请求者之下的全部 Actor。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeAllAttachedToRequester : 1;

	/** 使用复杂碰撞做检测。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bTraceComplexCollision : 1;

	/** 排除处于死亡状态的目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeDeadOrDying : 1;

	/** 拥有者属于这些类型的目标将被排除。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSet<TObjectPtr<UClass>> ExcludedClasses;

	/** 携带其中任一 Tag 的目标将被排除。 */
	FGameplayTagContainer ExclusionGameplayTags;

	/** 超出该距离的目标将被排除。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double TargetRange = 10000.0;
};

/** 存在有效目标时瞄准辅助的行为参数。 */
USTRUCT(BlueprintType)
struct FAimAssistSettings
{
	GENERATED_BODY()

	FAimAssistSettings();

	float GetTargetWeightForTime(float Time) const;
	float GetTargetWeightMaxTime() const;

	// 内圈辅助范围的世界空间宽度
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistInnerReticleWidth;

	// 内圈辅助范围的世界空间高度
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistInnerReticleHeight;

	// 外圈辅助范围的世界空间宽度
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistOuterReticleWidth;

	// 外圈辅助范围的世界空间高度
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistOuterReticleHeight;

	// 准星范围的世界空间宽度
	UPROPERTY(EditAnywhere)
	FScalableFloat TargetingReticleWidth;

	// 准星范围的世界空间高度
	UPROPERTY(EditAnywhere)
	FScalableFloat TargetingReticleHeight;

	// 搜集候选目标的距离；会按视场缩放，以便按屏幕占比而非绝对距离限制目标
	UPROPERTY(EditAnywhere)
	FScalableFloat TargetRange;

	// 目标被持续瞄准的时长到权重的映射，取值 0 到 1
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UCurveFloat> TargetWeightCurve = nullptr;

	// 目标位于内圈、腰射状态下，双方移动对牵引的贡献，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat PullInnerStrengthHip;

	// 目标位于外圈、腰射状态下，双方移动对牵引的贡献，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat PullOuterStrengthHip;

	// 目标位于内圈、瞄准状态下，双方移动对牵引的贡献，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat PullInnerStrengthAds;

	// 目标位于外圈、瞄准状态下，双方移动对牵引的贡献，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat PullOuterStrengthAds;

	// 牵引强度上升的指数插值速率；置 0 表示不插值
	UPROPERTY(EditAnywhere)
	FScalableFloat PullLerpInRate;

	// 牵引强度下降的指数插值速率；置 0 表示不插值
	UPROPERTY(EditAnywhere)
	FScalableFloat PullLerpOutRate;

	// 牵引带来的转向速率上限；置 0 表示不限制。会按视场缩放，使不同倍镜下手感一致
	UPROPERTY(EditAnywhere)
	FScalableFloat PullMaxRotationRate;

	// 目标位于内圈、腰射状态下对转向速率的减速量，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowInnerStrengthHip;

	// 目标位于外圈、腰射状态下对转向速率的减速量，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowOuterStrengthHip;

	// 目标位于内圈、瞄准状态下对转向速率的减速量，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowInnerStrengthAds;

	// 目标位于外圈、瞄准状态下对转向速率的减速量，取值 0 到 1
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowOuterStrengthAds;

	// 减速强度上升的指数插值速率；置 0 表示不插值
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowLerpInRate;

	// 减速强度下降的指数插值速率；置 0 表示不插值
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowLerpOutRate;

	// 减速后的转向速率下限；置 0 表示不限制。会按视场缩放
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowMinRotationRate;

	/** 单帧参与计算的目标数量上限。 */
	UPROPERTY(EditAnywhere)
	int32 MaxNumberOfTargets = 6;

	/** 准星投影所用的参考深度。 */
	UPROPERTY(EditAnywhere)
	float ReticleDepth = 3000.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_AssistWeight = 10.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_ViewDot = 50.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_ViewDotOffset = 40.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_ViewDistance = 0.25f;

	UPROPERTY(EditAnywhere)
	float StrengthScale = 1.0f;

	/** 是否使用异步可见性检测。 */
	UPROPERTY(EditAnywhere)
	uint8 bEnableAsyncVisibilityTrace : 1;

	/** 是否要求有输入才施加瞄准辅助。 */
	UPROPERTY(EditAnywhere)
	uint8 bRequireInput : 1;

	/** 是否施加牵引。 */
	UPROPERTY(EditAnywhere)
	uint8 bApplyPull : 1;

	/** 是否按移动输入施加横向牵引缩放。 */
	UPROPERTY(EditAnywhere)
	uint8 bApplyStrafePullScale : 1;

	/** 是否施加减速。 */
	UPROPERTY(EditAnywhere)
	uint8 bApplySlowing : 1;

	/** 是否按视角输入动态调整减速量。 */
	UPROPERTY(EditAnywhere)
	uint8 bUseDynamicSlow : 1;

	/** 是否按摇杆偏移量在偏航与俯仰之间混合视角速率。 */
	UPROPERTY(EditAnywhere)
	uint8 bUseRadialLookRates : 1;
};

/**
 * 瞄准辅助输入修饰器。
 *
 * 挂在视角输入上，为手柄玩家提供牵引与减速两种辅助。
 */
UCLASS(MinimalAPI)
class UAimAssistInputModifier : public UInputModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	FAimAssistSettings Settings {};

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	FAimAssistFilter Filter {};

	/** 代表玩家移动的输入动作。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	TObjectPtr<const UInputAction> MoveInputAction = nullptr;

	/** 本修饰器作用于常规视角还是瞄准状态。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	EYanTargetingType TargetingType = EYanTargetingType::Normal;

	/** 灵敏度档位到倍率的查表资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AssetBundles="Client,Server"))
	TObjectPtr<const UYanAimSensitivityData> SensitivityLevelTable = nullptr;

protected:
	//~Begin UInputModifier Interface
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
	//~End UInputModifier Interface

	// 交换目标缓存并重算当前可见目标与各自评分
	void UpdateTargetData(float DeltaTime);

	FRotator UpdateRotationalVelocity(APlayerController* PC, float DeltaTime, FVector CurrentLookInputValue, FVector CurrentMoveInputValue);

	// 计算单个目标的牵引与减速强度
	void CalculateTargetStrengths(const FYanAimAssistTarget& Target, float& OutPullStrength, float& OutSlowStrength) const;

	FRotator GetLookRates(const FVector& LookInput);

	void SwapTargetCaches() { TargetCacheIndex ^= 1; }
	const TArray<FYanAimAssistTarget>& GetPreviousTargetCache() const { return ((TargetCacheIndex == 0) ? TargetCache1 : TargetCache0); }
	TArray<FYanAimAssistTarget>& GetPreviousTargetCache()             { return ((TargetCacheIndex == 0) ? TargetCache1 : TargetCache0); }

	const TArray<FYanAimAssistTarget>& GetCurrentTargetCache() const  { return ((TargetCacheIndex == 0) ? TargetCache0 : TargetCache1); }
	TArray<FYanAimAssistTarget>& GetCurrentTargetCache()              { return ((TargetCacheIndex == 0) ? TargetCache0 : TargetCache1); }

	bool HasAnyCurrentTargets() const { return !GetCurrentTargetCache().IsEmpty(); }

	const float GetSensitivtyScalar(const UYanSettingsShared* SharedSettings) const;

	// 上一帧与本帧的目标，交替使用
	UPROPERTY()
	TArray<FYanAimAssistTarget> TargetCache0;

	UPROPERTY()
	TArray<FYanAimAssistTarget> TargetCache1;

	// 当前正在写入的缓存下标
	uint32 TargetCacheIndex;

	FAimAssistOwnerViewData OwnerViewData;

	float LastPullStrength = 0.0f;
	float LastSlowStrength = 0.0f;

#if ENABLE_DRAW_DEBUG
	float LastLookRateYaw;
	float LastLookRatePitch;

	FVector LastOutValue;
	FVector LastBaselineValue;

	bool bRegisteredDebug = false;

	void AimAssistDebugDraw(class UCanvas* Canvas, APlayerController* PC);
	FDelegateHandle DebugDrawHandle;
#endif
};
