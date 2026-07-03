// 装备定义实现。


#include "Equipment/EquipmentDefinition.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentDefinition)

UEquipmentDefinition::UEquipmentDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 为新建资源提供一个默认实例类型，便于直接在编辑器中继续配置。
	InstanceType = UEquipmentDefinition::StaticClass();
}
