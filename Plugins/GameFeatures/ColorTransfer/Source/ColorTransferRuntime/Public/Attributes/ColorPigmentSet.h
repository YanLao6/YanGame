#pragma once

#include "AttributeSets/YanAttributeSet.h"
#include "ColorTransferTypes.h"

#include "ColorPigmentSet.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

struct FGameplayEffectModCallbackData;

/**
 * 角色身上的颜料。
 *
 * 四种颜料的总量是 Attribute，供 GE、UI 与判定读取；外来颜料的先后顺序由 Layers 栈记录，
 * 自身颜料量 = 总量 100 - 全部外来层之和，因此"总和恒为 100"由结构本身保证。
 *
 * 颜料一律经 GameplayEffect 写 meta 属性改写（见 ColorPigmentEffects.h），在 PostGameplayEffectExecute
 * 中换算为 Layers 的变化，再由 Layers 重算四个颜色 Attribute。颜色 Attribute 只是 Layers 的镜像，
 * 直接修改它们的 GE 会被拒绝，否则会与栈失去同步。
 */
UCLASS(MinimalAPI, BlueprintType)
class UColorPigmentSet : public UYanAttributeSet
{
	GENERATED_BODY()

public:
	static constexpr float TotalPigment = 100.0f;

	ATTRIBUTE_ACCESSORS(UColorPigmentSet, Red);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, Blue);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, Black);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, White);

	ATTRIBUTE_ACCESSORS(UColorPigmentSet, InjectRed);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, InjectBlue);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, InjectBlack);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, InjectWhite);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, ShotConsumption);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, ForeignDecay);
	ATTRIBUTE_ACCESSORS(UColorPigmentSet, PigmentReset);

	/** 从 Actor（Pawn / Controller / PlayerState）解析颜料集；ASC 未就绪或未授予时返回 nullptr。 */
	static UE_API UColorPigmentSet* FindForActor(const AActor* Actor);

	/** 颜料对应的颜色 Attribute；None 返回无效 Attribute。 */
	static UE_API FGameplayAttribute GetPigmentAttribute(EColorPigment Pigment);

	/** 颜料对应的注入 meta Attribute；None 返回无效 Attribute。 */
	static UE_API FGameplayAttribute GetInjectAttribute(EColorPigment Pigment);

	/**
	 * 按当前颜料推算注入 Amount 时的实际注入量，与注入 GE 的结算规则一致。
	 *
	 * 外来颜料压栈，只挤占自身颜料，自身颜料耗尽后不再接收；
	 * 注入自身颜色则从栈顶起冲掉外来颜料。
	 */
	UE_API float CalcAcceptedInjection(EColorPigment Pigment, float Amount) const;

	/**
	 * 推算一次射击的射出量，OutPigment 为射出的颜色，与射击消耗 GE 的结算规则一致。
	 *
	 * 身上有外来颜料时射出栈顶颜色并扣除（最多扣到该层耗尽），腾出的量还给自身；
	 * 没有外来颜料时射出自身颜色且不扣除。
	 */
	UE_API float CalcShotOutput(float Amount, EColorPigment& OutPigment) const;

	/** 射击时会射出的颜色：栈顶外来颜色，没有则为自身颜色。 */
	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API EColorPigment GetShotPigment() const;

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	EColorPigment GetOwnPigment() const { return OwnPigment; }

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API float GetOwnPigmentAmount() const;

	/** 外来颜料栈，末尾为栈顶。 */
	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	const TArray<FColorPigmentLayer>& GetLayers() const { return Layers; }

	/** 颜料写完后广播一次，四个 Attribute 此时已全部同步；只在服务器上触发。 */
	FSimpleMulticastDelegate OnPigmentChanged;

	//~Begin UObject Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UObject Interface

protected:
	//~Begin UAttributeSet Interface
	UE_API virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	UE_API virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	UE_API virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	UE_API virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	//~End UAttributeSet Interface

	UFUNCTION()
	UE_API void OnRep_Red(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Blue(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Black(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_White(const FGameplayAttributeData& OldValue);

private:
	// 注入 meta 属性对应的颜料，非注入属性返回 None
	static EColorPigment GetInjectedPigment(const FGameplayAttribute& Attribute);

	static bool IsPigmentAttribute(const FGameplayAttribute& Attribute);

	// 取出 meta 属性累积的量并清零
	float ConsumeMetaAttribute(const FGameplayAttribute& Attribute);

	// 以下改写 Layers，返回是否有变化
	bool InjectPigment(EColorPigment Pigment, float Amount);
	bool DrainForShot(float Amount);
	bool DrainForeignPigment(float Amount);
	bool ResetToOwnPigment(EColorPigment InOwnPigment);

	// 按 OwnPigment 与 Layers 重算四个颜色 Attribute
	void SyncAttributesFromLayers();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Red, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Red;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Blue, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Blue;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Black, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Black;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_White, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData White;

	// meta：注入对应颜色的颜料
	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData InjectRed;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData InjectBlue;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData InjectBlack;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData InjectWhite;

	// meta：射击消耗，只扣栈顶外来层
	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ShotConsumption;

	// meta：外来颜料衰减，从栈顶起逐层扣除
	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ForeignDecay;

	// meta：清空外来颜料，自身颜色取自 Spec 上值为正的颜料 SetByCaller
	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData PigmentReset;

	// 由队伍决定，出生前为 None，此时不接收任何注入
	UPROPERTY(Replicated)
	EColorPigment OwnPigment = EColorPigment::None;

	// 外来颜料栈，末尾为栈顶；不含自身颜色
	UPROPERTY(Replicated)
	TArray<FColorPigmentLayer> Layers;
};

#undef UE_API
