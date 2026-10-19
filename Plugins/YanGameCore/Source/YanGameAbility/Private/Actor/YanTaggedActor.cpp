#include "Actor/YanTaggedActor.h"

#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanTaggedActor)

AYanTaggedActor::AYanTaggedActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void AYanTaggedActor::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(StaticGameplayTags);
}

#if WITH_EDITOR
bool AYanTaggedActor::CanEditChange(const FProperty* InProperty) const
{
	// 屏蔽 AActor::Tags，避免与 StaticGameplayTags 形成两套并行的标签来源
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Tags))
	{
		return false;
	}

	return Super::CanEditChange(InProperty);
}
#endif
