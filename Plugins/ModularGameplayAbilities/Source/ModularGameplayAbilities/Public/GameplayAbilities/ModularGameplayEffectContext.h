// Copyright Chronicler.

#pragma once

#include "GameplayEffectTypes.h"

#include "ModularGameplayEffectContext.generated.h"

class AActor;
class FArchive;
class IModularAbilitySourceInterface;
class UObject;
class UPhysicalMaterial;

/**
 * 扩展的效果上下文。
 *
 * 在引擎默认上下文之上追加能力来源（武器实例等）与弹药批次标识，
 * 使 Execution 能在结算时反查伤害来源自身携带的衰减规则。
 */
USTRUCT()
struct FModularGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FModularGameplayEffectContext()
		: FGameplayEffectContext()
	{
	}

	FModularGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
		: FGameplayEffectContext(InInstigator, InEffectCauser)
	{
	}

	/** 从句柄中取出本类型上下文；类型不匹配或为空时返回 nullptr。 */
	static MODULARGAMEPLAYABILITIES_API FModularGameplayEffectContext* ExtractEffectContext(struct FGameplayEffectContextHandle Handle);

	/** 设置能力来源对象。 */
	MODULARGAMEPLAYABILITIES_API void SetAbilitySource(const IModularAbilitySourceInterface* InObject, float InSourceLevel);

	/** 返回能力来源接口；该字段不参与网络复制，仅在服务端有效。 */
	MODULARGAMEPLAYABILITIES_API const IModularAbilitySourceInterface* GetAbilitySource() const;

	/** 返回命中结果携带的物理材质，没有命中信息时返回 nullptr。 */
	MODULARGAMEPLAYABILITIES_API const UPhysicalMaterial* GetPhysicalMaterial() const;

	//~Begin FGameplayEffectContext Interface
	virtual FGameplayEffectContext* Duplicate() const override
	{
		FModularGameplayEffectContext* NewContext = new FModularGameplayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			// 深拷贝命中结果，避免两份上下文共享同一块数据
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FModularGameplayEffectContext::StaticStruct();
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
	//~End FGameplayEffectContext Interface

public:
	/** 同一次击发产生的所有子弹共享的批次标识，-1 表示未分配。 */
	UPROPERTY()
	int32 CartridgeID = -1;

protected:
	// 能力来源对象，应实现 IModularAbilitySourceInterface；当前不参与网络复制
	UPROPERTY()
	TWeakObjectPtr<const UObject> AbilitySourceObject;
};

template<>
struct TStructOpsTypeTraits<FModularGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FModularGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
