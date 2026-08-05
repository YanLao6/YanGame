// Copyright YanGame.

#include "YanGASDebug/Public/YanGASCheatExtension.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGASCheatExtension)

DEFINE_LOG_CATEGORY_STATIC(LogYanGASDebug, Log, All);

UAbilitySystemComponent* UYanGASCheatExtension::ResolveAbilitySystemComponent() const
{
	const APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	// 优先 Pawn（Avatar），其次 PlayerState（部分项目 ASC 挂在 PlayerState 上）。
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn());
	if (!AbilitySystemComponent && PC->PlayerState)
	{
		AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->PlayerState);
	}

	if (!AbilitySystemComponent)
	{
		Report(TEXT("未找到 AbilitySystemComponent（检查当前 Pawn/PlayerState 是否持有 ASC）。"));
	}
	return AbilitySystemComponent;
}

bool UYanGASCheatExtension::FindAttribute(UAbilitySystemComponent* AbilitySystemComponent, const FString& AttributeName, FGameplayAttribute& OutAttribute)
{
	TArray<FGameplayAttribute> Attributes;
	AbilitySystemComponent->GetAllAttributes(Attributes);

	// 精确名优先，避免子串匹配到多个候选（如 Health / MaxHealth）时命中错误项。
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		if (Attribute.GetName().Equals(AttributeName, ESearchCase::IgnoreCase))
		{
			OutAttribute = Attribute;
			return true;
		}
	}

	for (const FGameplayAttribute& Attribute : Attributes)
	{
		if (Attribute.GetName().Contains(AttributeName, ESearchCase::IgnoreCase))
		{
			OutAttribute = Attribute;
			return true;
		}
	}
	return false;
}

void UYanGASCheatExtension::Report(const FString& Message) const
{
	UE_LOG(LogYanGASDebug, Display, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.0f, FColor::Cyan, Message);
	}
}

void UYanGASCheatExtension::SetAttribute(const FString& AttributeName, float Value)
{
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 修改 BaseValue 需要 Owner 权威；纯客户端上无效（会被服务器状态覆盖）。
	if (!AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		Report(TEXT("当前 ASC 无权威（纯客户端），改属性需在服务器/监听服务器执行。"));
		return;
	}

	FGameplayAttribute Attribute;
	if (!FindAttribute(AbilitySystemComponent, AttributeName, Attribute))
	{
		Report(FString::Printf(TEXT("未找到匹配 '%s' 的 Attribute（用 GasListAttributes 查看可用项）。"), *AttributeName));
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(Attribute, Value);
	Report(FString::Printf(TEXT("已设置 %s 的 BaseValue = %.2f"), *Attribute.GetName(), Value));
}

void UYanGASCheatExtension::AddAttribute(const FString& AttributeName, float Delta)
{
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		Report(TEXT("当前 ASC 无权威（纯客户端），改属性需在服务器/监听服务器执行。"));
		return;
	}

	FGameplayAttribute Attribute;
	if (!FindAttribute(AbilitySystemComponent, AttributeName, Attribute))
	{
		Report(FString::Printf(TEXT("未找到匹配 '%s' 的 Attribute（用 GasListAttributes 查看可用项）。"), *AttributeName));
		return;
	}

	const float NewBase = AbilitySystemComponent->GetNumericAttributeBase(Attribute) + Delta;
	AbilitySystemComponent->SetNumericAttributeBase(Attribute, NewBase);
	Report(FString::Printf(TEXT("已叠加 %s：BaseValue = %.2f（%+.2f）"), *Attribute.GetName(), NewBase, Delta));
}

void UYanGASCheatExtension::ApplyGameplayEffect(const FString& AttributeName, const FString& Op, float Magnitude)
{
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Instant GE 的属性修改需要权威端执行，否则会被服务器复制覆盖。
	if (!AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		Report(TEXT("当前 ASC 无权威（纯客户端），施加 GE 需在服务器/监听服务器执行。"));
		return;
	}

	FGameplayAttribute Attribute;
	if (!FindAttribute(AbilitySystemComponent, AttributeName, Attribute))
	{
		Report(FString::Printf(TEXT("未找到匹配 '%s' 的 Attribute（用 GasListAttributes 查看可用项）。"), *AttributeName));
		return;
	}

	// 解析修改方式：默认 AddBase；显式 "override" 走 Override。
	EGameplayModOp::Type ModOp = EGameplayModOp::AddBase;
	if (Op.Equals(TEXT("Override"), ESearchCase::IgnoreCase))
	{
		ModOp = EGameplayModOp::Override;
	}
	else if (!Op.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
	{
		Report(FString::Printf(TEXT("未知 Op '%s'，仅支持 Add / Override。"), *Op));
		return;
	}

	// 运行时构造一次性 Instant GE：不落资产、用完即弃，仅适用于 Instant。
	UGameplayEffect* GameplayEffect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("YanGASDebug_RuntimeGE")));
	GameplayEffect->DurationPolicy  = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute         = Attribute;
	ModifierInfo.ModifierOp        = ModOp;
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
	GameplayEffect->Modifiers.Add(ModifierInfo);

	AbilitySystemComponent->ApplyGameplayEffectToSelf(GameplayEffect, 1.0f, AbilitySystemComponent->MakeEffectContext());

	bool        bFound  = false;
	const float Current = AbilitySystemComponent->GetGameplayAttributeValue(Attribute, bFound);
	Report(FString::Printf(TEXT("已对 %s 施加 Instant GE（%s %.2f）→ Current=%.2f"),
	                       *Attribute.GetName(),
	                       ModOp == EGameplayModOp::Override ? TEXT("Override") : TEXT("Add"),
	                       Magnitude,
	                       Current));
}

void UYanGASCheatExtension::AddTag(const FString& TagName)
{
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), /*ErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		Report(FString::Printf(TEXT("无效 GameplayTag：'%s'"), *TagName));
		return;
	}

	// 优先走 Modular 的动态 Tag GE（复制安全）；不可用时回退到 LooseTag（本地、不复制）。
	if (UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(AbilitySystemComponent))
	{
		ModularASC->AddDynamicTagGameplayEffect(Tag);
		Report(FString::Printf(TEXT("已添加 Tag（动态 GE）：%s"), *Tag.ToString()));
	}
	else
	{
		AbilitySystemComponent->AddLooseGameplayTag(Tag);
		Report(FString::Printf(TEXT("已添加 LooseTag（本地不复制）：%s"), *Tag.ToString()));
	}
}

void UYanGASCheatExtension::RemoveTag(const FString& TagName)
{
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), /*ErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		Report(FString::Printf(TEXT("无效 GameplayTag：'%s'"), *TagName));
		return;
	}

	if (UModularAbilitySystemComponent* ModularASC = Cast<UModularAbilitySystemComponent>(AbilitySystemComponent))
	{
		ModularASC->RemoveDynamicTagGameplayEffect(Tag);
	}
	// LooseTag 与动态 GE 是两条独立路径，一并清理以保证幂等。
	AbilitySystemComponent->RemoveLooseGameplayTag(Tag);
	Report(FString::Printf(TEXT("已移除 Tag：%s"), *Tag.ToString()));
}

void UYanGASCheatExtension::CycleDebug()
{
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->MyHUD)
	{
		return;
	}

	if (!PC->MyHUD->bShowDebugInfo || !PC->MyHUD->DebugDisplay.Contains(TEXT("AbilitySystem")))
	{
		PC->MyHUD->ShowDebug(TEXT("AbilitySystem"));
	}
	PC->ConsoleCommand(TEXT("AbilitySystem.Debug.NextCategory"));
}
