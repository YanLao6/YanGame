#pragma once

#include "NumberPops/YanNumberPopComponent.h"

#include "YanNumberPopComponent_MeshText.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UYanDamagePopStyle;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/** 同一网格对应的组件池。 */
USTRUCT()
struct FPooledNumberPopComponentList
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Components;
};

/** 正在显示中的一条飘字，到期后归还组件池。 */
USTRUCT()
struct FLiveNumberPopEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Component = nullptr;

	// 归还目标池，指向 PooledComponentMap 中的元素
	FPooledNumberPopComponentList* Pool = nullptr;

	// 归还的世界时间
	float ReleaseTime = 0.0f;

	FLiveNumberPopEntry() {}

	FLiveNumberPopEntry(UStaticMeshComponent* InComponent, FPooledNumberPopComponentList* InPool, float InReleaseTime)
		: Component(InComponent)
		  , Pool(InPool)
		  , ReleaseTime(InReleaseTime)
	{}
};

/** 构造一条飘字期间的临时数据。 */
struct FTempNumberPopInfo
{
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	TArray<UMaterialInstanceDynamic*> MeshMIDs;

	TArray<int32> DamageNumberArray;
};

/**
 * 以静态网格文字绘制伤害飘字。
 * 每个数字由材质参数驱动动画，组件按网格分池复用以避免频繁创建销毁。
 */
UCLASS(MinimalAPI, Blueprintable)
class UYanNumberPopComponent_MeshText : public UYanNumberPopComponent
{
	GENERATED_BODY()

public:
	UE_API UYanNumberPopComponent_MeshText(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UYanNumberPopComponent Interface
	UE_API virtual void AddNumberPop(const FYanNumberPopRequest& NewRequest) override;
	//~End UYanNumberPopComponent Interface

protected:
	void SetMaterialParameters(const FYanNumberPopRequest& Request, FTempNumberPopInfo& NewDamageNumberInfo, const FTransform& CameraTransform, const FVector& NumberLocation);

	FLinearColor DetermineColor(const FYanNumberPopRequest& Request) const;
	UStaticMesh* DetermineStaticMesh(const FYanNumberPopRequest& Request) const;

	// 归还已超出生命期的组件，并为下一条排定定时器
	void ReleaseNextComponents();

	// 按顺序匹配的样式列表
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	TArray<TObjectPtr<UYanDamagePopStyle>> Styles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Number Pop|Style")
	float ComponentLifespan;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	float DistanceFromCameraBeforeDoublingSize;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	float CriticalHitSizeMultiplier;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Font")
	float FontXSize;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Font")
	float FontYSize;

	// 数字 1 比其他数字窄，前后需要收紧间距
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Font")
	float SpacingPercentageForOnes;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	float NumberOfNumberRotations;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName SignDigitParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName ColorParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName AnimationLifespanParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName IsCriticalHitParameterName;

	// 飘字默认在材质中被拉近相机以免被遮挡，该参数可关闭此行为
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName MoveToCameraParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	TArray<FName> PositionParameterNames;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	TArray<FName> ScaleRotationAngleParameterNames;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	TArray<FName> DurationParameterNames;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, FPooledNumberPopComponentList> PooledComponentMap;

	// 按到期时间升序排列
	UPROPERTY(Transient)
	TArray<FLiveNumberPopEntry> LiveComponents;

	FTimerHandle ReleaseTimerHandle;
};

#undef UE_API
