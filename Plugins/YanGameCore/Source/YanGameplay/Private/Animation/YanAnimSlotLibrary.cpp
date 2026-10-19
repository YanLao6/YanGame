#include "Animation/YanAnimSlotLibrary.h"

#include "Animation/Skeleton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanAnimSlotLibrary)

bool UYanAnimSlotLibrary::EnsureSlotInGroup(USkeleton* Skeleton, FName SlotName, FName GroupName)
{
#if WITH_EDITOR
	if (!Skeleton || SlotName.IsNone() || GroupName.IsNone())
	{
		return false;
	}

	bool bChanged = false;
	if (!Skeleton->FindAnimSlotGroup(GroupName))
	{
		Skeleton->Modify();
		Skeleton->AddSlotGroupName(GroupName);
		bChanged = true;
	}

	if (!Skeleton->ContainsSlotName(SlotName))
	{
		Skeleton->Modify();
		Skeleton->RegisterSlotNode(SlotName);
		bChanged = true;
	}

	if (Skeleton->GetSlotGroupName(SlotName) != GroupName)
	{
		Skeleton->Modify();
		Skeleton->SetSlotGroupName(SlotName, GroupName);
		bChanged = true;
	}

	return bChanged;
#else
	return false;
#endif
}

FName UYanAnimSlotLibrary::GetSlotGroupName(const USkeleton* Skeleton, FName SlotName)
{
	return Skeleton ? Skeleton->GetSlotGroupName(SlotName) : FAnimSlotGroup::DefaultGroupName;
}
