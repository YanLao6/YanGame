#pragma once

#include "ColorTransferTypes.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AActor;
class UColorPigmentSet;

/** 各颜色的 SetByCaller 键：注入 GE 中为注入量，重置 GE 中值为正者即自身颜色。 */
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ColorPigment_Red);
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ColorPigment_Blue);
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ColorPigment_Black);
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ColorPigment_White);

/** 射击消耗与外来颜料衰减的扣除量。 */
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ColorPigment_Amount);

/**
 * 颜料 GE 的施加入口，均只在服务器上生效；所用 GE 取自 UColorTransferGameData。
 *
 * EffectCauser 写入 EffectContext；其 Instigator（Pawn 即自身）有 ASC 时以之为 GE 来源，否则以目标自身为来源。
 * 需要结算结果的接口先按 UColorPigmentSet 的 Calc* 推算，GE 成功执行后才返回该值，
 * Instant GE 在服务器上同步结算，推算与实际结果一致。
 */
namespace ColorTransfer
{
	/** 颜色对应的 SetByCaller 键；None 返回空 Tag。 */
	UE_API FGameplayTag GetPigmentSetByCallerTag(EColorPigment Pigment);

	/** 注入颜料，返回实际注入量。 */
	UE_API float AuthApplyInjection(UColorPigmentSet& Target, EColorPigment Pigment, float Amount, AActor* EffectCauser);

	/** 为一次射击取出颜料，返回射出量，OutPigment 为射出的颜色；射出自身颜色时不扣除，也不施加 GE。 */
	UE_API float AuthApplyShotConsumption(UColorPigmentSet& Shooter, float Amount, EColorPigment& OutPigment, AActor* EffectCauser);

	/** 外来颜料自然衰减，腾出的量还给自身颜色。 */
	UE_API void AuthApplyForeignDecay(UColorPigmentSet& Target, float Amount, AActor* EffectCauser);

	/** 清空外来颜料并把全部颜料设为 OwnPigment，出生时调用。 */
	UE_API void AuthApplyReset(UColorPigmentSet& Target, EColorPigment OwnPigment);

	/**
	 * 从外部容器的颜料栈顶吸收至多 MaxAmount，扣除 SourceLayers 并返回扣除量。
	 *
	 * 自身颜色总能吸走（否则容器里的自身颜色永远取不出）；外来颜色只按实际注入量扣除，装不下的留在容器里。
	 */
	UE_API float AuthAbsorbFromTopLayer(UColorPigmentSet& Target, TArray<FColorPigmentLayer>& SourceLayers, float MaxAmount, AActor* EffectCauser);
}

#undef UE_API
