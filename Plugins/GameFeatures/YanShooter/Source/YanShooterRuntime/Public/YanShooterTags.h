// 射击玩法的原生 GameplayTag 声明。

#pragma once

#include "NativeGameplayTags.h"

namespace YanShooterTags
{
	/** 一次击杀结算完成后广播，Instigator 为击杀者，Target 为被击杀者。 */
	YANSHOOTERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Message_Elimination);

	/** 一次伤害结算完成后广播，Magnitude 为伤害量。 */
	YANSHOOTERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Message_Damage);

	/** 助攻结算广播，Instigator 为助攻者，Magnitude 为其累计伤害。 */
	YANSHOOTERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Message_Assist);

	/** 战斗嘉奖的通知频道，PayloadTag 为嘉奖在数据注册表中的键。 */
	YANSHOOTERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Message_Accolade);
}
