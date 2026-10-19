// Copyright Chronicler.


#include "ModularAbilityTags.h"

namespace ModularAbilityTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead", "因持有者已死亡导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "因 Cooldown 未结束导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost", "未通过 Cost 检查导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsBlocked, "Ability.ActivateFail.TagsBlocked", "因 Block Tag 导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsMissing, "Ability.ActivateFail.TagsMissing", "因缺失 Required Tag 导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Networking, "Ability.ActivateFail.Networking", "未通过网络相关检查导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_ActivationGroup, "Ability.ActivateFail.ActivationGroup", "因 ActivationGroup 规则导致 Ability 激活失败。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Gate, "Ability.ActivateFail.Gate", "因 Spec 的门禁 Tag 未被提供方判为满足导致 Ability 激活失败。");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Behavior_SurvivesDeath, "Ability.Behavior.SurvivesDeath", "标记该 Ability 在死亡流程中不应被自动取消。");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Gate, "Ability.Gate", "门禁 Tag 的父标签；其下的子标签写在 AbilitySpec 上，须由登记在 ASC 上的提供方判为满足才能激活。");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Crouching, "Status.Crouching", "目标处于蹲伏状态。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_AutoRunning, "Status.AutoRunning", "目标处于自动奔跑状态。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death, "Status.Death", "目标具有死亡相关状态。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dying, "Status.Death.Dying", "目标处于濒死/死亡进行中的过程。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dead, "Status.Death.Dead", "目标已完成死亡流程。");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cheat_GodMode, "Cheat.GodMode", "调试无敌：伤害在结算前被清零。");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cheat_UnlimitedHealth, "Cheat.UnlimitedHealth", "调试不死：可受伤但生命值不低于 1。");

	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString)
	{
		const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

		if (!Tag.IsValid() && bMatchPartialString)
		{
			FGameplayTagContainer AllTags;
			Manager.RequestAllGameplayTags(AllTags, true);

			for (const FGameplayTag& TestTag : AllTags)
			{
				if (TestTag.ToString().Contains(TagString))
				{
					UE_LOG(LogGameplayTags, Display, TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *TestTag.ToString());
					Tag = TestTag;
					break;
				}
			}
		}

		return Tag;
	}
}
