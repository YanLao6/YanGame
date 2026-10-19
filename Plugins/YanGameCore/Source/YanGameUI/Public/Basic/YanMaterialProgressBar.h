// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"

#include "YanMaterialProgressBar.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWidgetAnimation;

UCLASS(Abstract, meta = (DisableNativeTick))
class YANGAMEUI_API UYanMaterialProgressBar : public UCommonUserWidget
{
	GENERATED_BODY()

protected:

	//~Begin UUserWidget Interface
	virtual void SynchronizeProperties() override;
	//~End UUserWidget Interface

#if WITH_EDITOR

	//~Begin UUserWidget Interface
	virtual void OnWidgetRebuilt() override;
	//~End UUserWidget Interface

#endif

	//~Begin UUserWidget Interface
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	//~End UUserWidget Interface

public:

	/** 设置材质进度终点。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void SetProgress(float Progress);

	/** 设置材质进度起点。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void SetStartProgress(float StartProgress);

	/** 设置填充渐变起始颜色。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void SetColorA(FLinearColor ColorA);

	/** 设置填充渐变终止颜色。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void SetColorB(FLinearColor ColorB);

	/** 设置未填充区域颜色。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void SetColorBackground(FLinearColor ColorBackground);

	/** 从指定起点播放填充动画。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void AnimateProgressFromStart(float Start, float End, float AnimSpeed = 1.0f);

	/** 从当前可见填充量播放至目标进度。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|UI")
	void AnimateProgressFromCurrent(float End, float AnimSpeed = 1.0f);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFillAnimationFinished);

	UPROPERTY(BlueprintAssignable, Category = "Yan|UI")
	FOnFillAnimationFinished OnFillAnimationFinished;

private:

	void SetProgress_Internal(float Progress);
	void SetStartProgress_Internal(float StartProgress);
	void SetColorA_Internal(FLinearColor ColorA);
	void SetColorB_Internal(FLinearColor ColorB);
	void SetColorBackground_Internal(FLinearColor ColorBackground);

	UMaterialInstanceDynamic* GetBarDynamicMaterial() const;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "CachedColorA"), Category = "Yan|UI")
	bool bOverrideDefaultColorA = false;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "Color A", EditCondition = "bOverrideDefaultColorA"), Category = "Yan|UI")
	FLinearColor CachedColorA;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "CachedColorB"), Category = "Yan|UI")
	bool bOverrideDefaultColorB = false;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "Color B", EditCondition = "bOverrideDefaultColorB"), Category = "Yan|UI")
	FLinearColor CachedColorB;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "CachedColorBackground"), Category = "Yan|UI")
	bool bOverrideDefaultColorBackground;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "Color Background", EditCondition = "bOverrideDefaultColorBackground"), Category = "Yan|UI")
	FLinearColor CachedColorBackground;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "Segments"), Category = "Yan|UI")
	bool bOverrideDefaultSegments = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bOverrideDefaultSegments"), Category = "Yan|UI")
	int32 Segments;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "SegmentEdge"), Category = "Yan|UI")
	bool bOverrideDefaultSegmentEdge = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bOverrideDefaultSegmentEdge"), Category = "Yan|UI")
	float SegmentEdge;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "FillEdgeSoftness"), Category = "Yan|UI")
	bool bOverrideDefaultFillEdgeSoftness;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bOverrideDefaultFillEdgeSoftness"), Category = "Yan|UI")
	float FillEdgeSoftness;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "GlowEdge"), Category = "Yan|UI")
	bool bOverrideDefaultGlowEdge = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bOverrideDefaultGlowEdge"), Category = "Yan|UI")
	float GlowEdge;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "GlowSoftness"), Category = "Yan|UI")
	bool bOverrideDefaultGlowSoftness = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bOverrideDefaultGlowSoftness"), Category = "Yan|UI")
	float GlowSoftness;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle = "OutlineScale"), Category = "Yan|UI")
	bool bOverrideDefaultOutlineScale = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bOverrideDefaultOutlineScale"), Category = "Yan|UI")
	float OutlineScale;

	UPROPERTY(EditAnywhere, Category = "Yan|UI")
	bool bUseStroke = true;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|UI")
	TObjectPtr<UMaterialInterface> StrokeMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|UI")
	TObjectPtr<UMaterialInterface> NoStrokeMaterial;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Design Time Progress"), Category = "Yan|UI")
	float DesignTime_Progress = 1.0f;
#endif

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess), Category = "Yan|UI")
	TObjectPtr<UImage> Image_Bar;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetAnim, AllowPrivateAccess), Category = "Yan|UI")
	TObjectPtr<UWidgetAnimation> BoundAnim_FillBar;

	UPROPERTY(Transient)
	mutable TObjectPtr<UMaterialInstanceDynamic> CachedMID;

	float CachedProgress = -1.0f;
	float CachedStartProgress = -1.0f;
};
