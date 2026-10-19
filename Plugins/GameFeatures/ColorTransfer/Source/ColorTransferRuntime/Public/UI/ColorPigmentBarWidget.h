#pragma once

#include "AttributeSet.h"
#include "ColorTransferTypes.h"
#include "YanUserWidget.h"

#include "ColorPigmentBarWidget.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class APlayerState;
class UAbilitySystemComponent;
class UCommonLocalPlayer;
class UHorizontalBox;
class UImage;
class UTextBlock;
struct FOnAttributeChangeData;

/** 颜料段配置：一种颜料对应一套外观 */
USTRUCT(BlueprintType)
struct FColorPigmentBarEntry
{
	GENERATED_BODY()

	/** 颜料量属性，指向 UColorPigmentSet 的对应属性，用于把栈层映射到本配置 */
	UPROPERTY(EditAnywhere, Category = "Pigment")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, Category = "Pigment")
	FLinearColor Color = FLinearColor::White;

	/** 数值文本中的颜料名 */
	UPROPERTY(EditAnywhere, Category = "Pigment")
	FText Label;
};

/**
 * 角色身上颜料构成条。
 *
 * 按 UColorPigmentSet 的颜料栈自左向右排列：最左为自身颜料（栈底），其后依次为外来颜料层，
 * 最右为栈顶，新注入的颜色总出现在最右；同色层不相邻时各占一段。
 * 四种颜料总和恒为 100，段宽按量占比分配；文本按同样顺序列出各段（如 "红 60 / 蓝 20 / 红 20"）。
 *
 * 本地 PlayerState 就绪后监听其 ASC 上四个颜色 Attribute 的变化，任一变化即按颜料栈重建；
 * 颜色 Attribute 与颜料栈同属 UColorPigmentSet，随同一次属性复制到达。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   UHorizontalBox SegmentBox  —— 分段容器，色段于运行时填入，设计期应留空
 *   UTextBlock     PigmentText —— 数值文本（可选）
 */
UCLASS(MinimalAPI, Abstract)
class UColorPigmentBarWidget : public UYanUserWidget
{
	GENERATED_BODY()

public:
	UE_API UColorPigmentBarWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	//~End UUserWidget Interface

	/** 分段容器，子控件由本控件独占管理 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Pigment", Meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SegmentBox;

	/** 颜料数值文本 */
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Pigment", Meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PigmentText;

	/** 各颜料的外观，按 Attribute 匹配 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Pigment")
	TArray<FColorPigmentBarEntry> Entries;

private:
	// 一段的呈现结果，Amount 取整后用于比较是否需要刷新
	struct FShownSegment
	{
		EColorPigment Pigment = EColorPigment::None;
		int32 Amount = 0;

		bool operator==(const FShownSegment& Other) const
		{
			return Pigment == Other.Pigment && Amount == Other.Amount;
		}
	};

	// PlayerState 可能晚于控件构造复制到达，也可能因无缝切图被替换，每次设置都重新绑定其 ASC
	void HandlePlayerStateSet(UCommonLocalPlayer* LocalPlayer, APlayerState* PlayerState);
	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	void UnbindAbilitySystem();
	void HandlePigmentAttributeChanged(const FOnAttributeChangeData& ChangeData);

	void RefreshPigment();
	void ApplySegments();
	void RefreshText();

	UImage* GetOrCreateSegment(int32 Index);
	const FColorPigmentBarEntry* FindEntry(EColorPigment Pigment) const;

	// 色段池，按需增长；多余的折叠而不移除
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SegmentPool;

	// 自左向右：自身颜料在前，外来层按栈序在后
	TArray<FShownSegment> ShownSegments;

	FDelegateHandle PlayerStateSetHandle;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	// 与颜色 Attribute 一一对应
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeChangedHandles;
};

#undef UE_API
