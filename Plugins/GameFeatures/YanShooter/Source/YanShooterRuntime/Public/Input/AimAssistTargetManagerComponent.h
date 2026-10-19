// 瞄准辅助目标管理组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "AimAssistTargetManagerComponent.generated.h"

#define UE_API YANSHOOTERRUNTIME_API

enum class ECommonInputType : uint8;

class APlayerController;
struct FAimAssistFilter;
struct FAimAssistOwnerViewData;
struct FAimAssistSettings;
struct FAimAssistTargetOptions;
struct FCollisionQueryParams;
struct FYanAimAssistTarget;

/**
 * 瞄准辅助目标管理组件。
 *
 * 搜集处于某名玩家视野内的全部瞄准辅助目标。
 * 目标须实现瞄准辅助目标接口，且位于运行时设置所指定的检测通道上。
 */
UCLASS(MinimalAPI, Blueprintable)
class UAimAssistTargetManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	/** 依据本地玩家的视口数据取出全部可见且有效的目标。 */
	UE_API void GetVisibleTargets(const FAimAssistFilter& Filter, const FAimAssistSettings& Settings, const FAimAssistOwnerViewData& OwnerData, const TArray<FYanAimAssistTarget>& OldTargets, OUT TArray<FYanAimAssistTarget>& OutNewTargets);

	/** 按当前输入方式取玩家控制器的视场缩放系数。 */
	static UE_API float GetFOVScale(const APlayerController* PC, ECommonInputType InputType);

	/** 返回搜寻目标所用的检测通道。 */
	UE_API ECollisionChannel GetAimAssistChannel() const;

protected:
	/** 判断目标是否通过过滤条件；返回 false 表示该目标不参与瞄准辅助。 */
	UE_API bool DoesTargetPassFilter(const FAimAssistOwnerViewData& OwnerData, const FAimAssistFilter& Filter, const FAimAssistTargetOptions& Target, const float AcceptableRange) const;

	/** 依据当前视口数据判定目标可见性。 */
	UE_API void DetermineTargetVisibility(FYanAimAssistTarget& Target, const FAimAssistSettings& Settings, const FAimAssistFilter& Filter, const FAimAssistOwnerViewData& OwnerData);

	/** 按过滤设置组装检测参数，忽略请求者或发起者等 Actor。 */
	UE_API void InitTargetSelectionCollisionParams(FCollisionQueryParams& OutParams, const AActor& RequestedBy, const FAimAssistFilter& Filter) const;
};

#undef UE_API
