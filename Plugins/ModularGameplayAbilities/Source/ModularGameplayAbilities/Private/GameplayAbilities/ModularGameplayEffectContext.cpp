// Copyright Chronicler.

#include "GameplayAbilities/ModularGameplayEffectContext.h"

#include "Engine/HitResult.h"
#include "GameplayAbilities/ModularAbilitySourceInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGameplayEffectContext)

class FArchive;

FModularGameplayEffectContext* FModularGameplayEffectContext::ExtractEffectContext(struct FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* BaseEffectContext = Handle.Get();
	if ((BaseEffectContext != nullptr) && BaseEffectContext->GetScriptStruct()->IsChildOf(FModularGameplayEffectContext::StaticStruct()))
	{
		return static_cast<FModularGameplayEffectContext*>(BaseEffectContext);
	}

	return nullptr;
}

bool FModularGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	// CartridgeID 与 AbilitySourceObject 只在生成端的结算流程中使用，不复制

	return true;
}

#if UE_WITH_IRIS
namespace UE::Net
{
	// 转发给引擎的 FGameplayEffectContextNetSerializer。
	// 若 NetSerialize 之后开始序列化自有字段，必须改为实现自定义 NetSerializer。
	UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(ModularGameplayEffectContext, FGameplayEffectContextNetSerializer);
}
#endif

void FModularGameplayEffectContext::SetAbilitySource(const IModularAbilitySourceInterface* InObject, float InSourceLevel)
{
	AbilitySourceObject = MakeWeakObjectPtr(Cast<const UObject>(InObject));
}

const IModularAbilitySourceInterface* FModularGameplayEffectContext::GetAbilitySource() const
{
	return Cast<IModularAbilitySourceInterface>(AbilitySourceObject.Get());
}

const UPhysicalMaterial* FModularGameplayEffectContext::GetPhysicalMaterial() const
{
	if (const FHitResult* HitResultPtr = GetHitResult())
	{
		return HitResultPtr->PhysMaterial.Get();
	}

	return nullptr;
}
