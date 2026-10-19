#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"

#include "ColorTransferTypes.generated.h"

/** 颜料种类。None 只作"无有效颜料"的哨兵，不会出现在角色身上。 */
UENUM(BlueprintType)
enum class EColorPigment : uint8
{
	None,
	Red,
	Blue,
	Black,
	White
};

namespace ColorTransfer
{
	/** 红蓝相遇后的紫色，紫爆、中继晶体自爆与融合球共用。 */
	inline const FLinearColor PurpleColor(0.45f, 0.05f, 1.0f);

	/** 颜料的显示颜色，射线与角色表面共用，保证两处看到的是同一种颜色。 */
	inline FLinearColor GetPigmentColor(EColorPigment Pigment)
	{
		switch (Pigment)
		{
		case EColorPigment::Red:
			return FLinearColor(1.0f, 0.03f, 0.03f);
		case EColorPigment::Blue:
			return FLinearColor(0.03f, 0.2f, 1.0f);
		case EColorPigment::Black:
			return FLinearColor(0.01f, 0.01f, 0.01f);
		case EColorPigment::White:
			return FLinearColor(1.0f, 1.0f, 1.0f);
		default:
			return FLinearColor(0.5f, 0.5f, 0.5f);
		}
	}
}

/** 角色身上的一层外来颜料；与栈顶同色的注入并入栈顶，不新开一层。 */
USTRUCT(BlueprintType)
struct FColorPigmentLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yan|ColorTransfer")
	EColorPigment Pigment = EColorPigment::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float Amount = 0.0f;
};

/** 颜料栈（末尾为栈顶）的共用规则，角色与中继晶体之间倒颜料时两端语义一致。 */
namespace ColorTransfer
{
	/** Weapon_Capsule：颜色射线的检测通道，命中角色胶囊，世界几何体已在碰撞预设中补回 Block；机关要截断射线时对此通道 Block。 */
	constexpr ECollisionChannel ShotTraceChannel = ECC_GameTraceChannel3;

	/** 压入颜料，与栈顶同色则并入栈顶。 */
	inline void PushLayer(TArray<FColorPigmentLayer>& Layers, EColorPigment Pigment, float Amount)
	{
		if (!Layers.IsEmpty() && Layers.Last().Pigment == Pigment)
		{
			Layers.Last().Amount += Amount;
		}
		else
		{
			Layers.Add({Pigment, Amount});
		}
	}

	/** 从栈顶起逐层扣除至多 MaxAmount，扣空的层出栈，返回实际扣除量。 */
	inline float DrainLayers(TArray<FColorPigmentLayer>& Layers, float MaxAmount)
	{
		float Drained = 0.0f;
		while (Drained < MaxAmount && !Layers.IsEmpty())
		{
			FColorPigmentLayer& Top = Layers.Last();
			const float Taken = FMath::Min(Top.Amount, MaxAmount - Drained);
			Top.Amount -= Taken;
			Drained += Taken;

			if (Top.Amount <= UE_KINDA_SMALL_NUMBER)
			{
				Layers.Pop();
			}
		}

		return Drained;
	}

	/** 只扣栈顶这一层，至多扣空为止。 */
	inline float DrainTopLayer(TArray<FColorPigmentLayer>& Layers, float MaxAmount)
	{
		if (Layers.IsEmpty())
		{
			return 0.0f;
		}

		return DrainLayers(Layers, FMath::Min(MaxAmount, Layers.Last().Amount));
	}

	inline float GetLayersTotal(const TArray<FColorPigmentLayer>& Layers)
	{
		float Total = 0.0f;
		for (const FColorPigmentLayer& Layer : Layers)
		{
			Total += Layer.Amount;
		}

		return Total;
	}

	/** 按量加权混合各层的显示颜色；总量为 0 时返回透明色。 */
	inline FLinearColor MixPigments(const TArray<FColorPigmentLayer>& Layers)
	{
		const float Total = GetLayersTotal(Layers);
		if (Total <= UE_KINDA_SMALL_NUMBER)
		{
			return FLinearColor::Transparent;
		}

		FLinearColor Mixed = FLinearColor::Black;
		for (const FColorPigmentLayer& Layer : Layers)
		{
			Mixed += GetPigmentColor(Layer.Pigment) * (Layer.Amount / Total);
		}

		Mixed.A = 1.0f;
		return Mixed;
	}
}

/** 一发颜色射线的结算信息，由射击技能在服务器上构造。 */
USTRUCT(BlueprintType)
struct FColorShotHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer")
	EColorPigment Pigment = EColorPigment::None;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer")
	float Amount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|ColorTransfer")
	FHitResult Hit;
};

UINTERFACE(MinimalAPI, Meta = (CannotImplementInterfaceInBlueprint))
class UColorShotReceiver : public UInterface
{
	GENERATED_BODY()
};

/** 关卡机关接收颜色射线的入口；角色之间的传色走 UColorPigmentSet，不经过本接口。 */
class IColorShotReceiver
{
	GENERATED_BODY()

public:
	/** 仅在服务器上调用。 */
	virtual void ReceiveColorShot(const FColorShotHit& Shot) = 0;
};

class UColorPigmentSet;

UINTERFACE(MinimalAPI, Meta = (CannotImplementInterfaceInBlueprint))
class UColorPigmentSource : public UInterface
{
	GENERATED_BODY()
};

/** 可经交互从栈顶取出颜料的机关，供 UAbility_TakePaint 统一结算。 */
class IColorPigmentSource
{
	GENERATED_BODY()

public:
	/** 从栈顶取至多 MaxAmount 注入 PigmentSet，返回实际取出量。仅在服务器上调用。 */
	virtual float AuthTakeTopPigment(UColorPigmentSet* PigmentSet, float MaxAmount) = 0;
};
