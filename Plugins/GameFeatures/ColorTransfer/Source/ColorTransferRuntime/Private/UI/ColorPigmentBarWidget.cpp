#include "UI/ColorPigmentBarWidget.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ColorPigmentSet.h"
#include "CommonLocalPlayer.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "Messages/VerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPigmentBarWidget)

UColorPigmentBarWidget::UColorPigmentBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UColorPigmentBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SegmentBox)
	{
		SegmentBox->ClearChildren();
	}
	SegmentPool.Reset();
	ShownSegments.Reset();

	if (UCommonLocalPlayer* LocalPlayer = GetOwningLocalPlayer<UCommonLocalPlayer>())
	{
		PlayerStateSetHandle = LocalPlayer->CallAndRegister_OnPlayerStateSet(
			UCommonLocalPlayer::FPlayerStateSetDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePlayerStateSet));
	}
}

void UColorPigmentBarWidget::NativeDestruct()
{
	if (UCommonLocalPlayer* LocalPlayer = GetOwningLocalPlayer<UCommonLocalPlayer>())
	{
		LocalPlayer->OnPlayerStateSet.Remove(PlayerStateSetHandle);
	}
	PlayerStateSetHandle.Reset();

	UnbindAbilitySystem();

	Super::NativeDestruct();
}

void UColorPigmentBarWidget::HandlePlayerStateSet(UCommonLocalPlayer* LocalPlayer, APlayerState* PlayerState)
{
	BindAbilitySystem(UVerbMessageHelpers::GetAbilitySystemComponentFromObject(PlayerState));
	RefreshPigment();
}

void UColorPigmentBarWidget::BindAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (BoundAbilitySystem.Get() == ASC)
	{
		return;
	}

	UnbindAbilitySystem();

	if (!ASC)
	{
		return;
	}

	// 委托按 Attribute 挂在 ASC 上，颜料集尚未授予时也可先行绑定，授予后首次复制即触发
	for (const EColorPigment Pigment : {EColorPigment::Red, EColorPigment::Blue, EColorPigment::Black, EColorPigment::White})
	{
		const FGameplayAttribute Attribute = UColorPigmentSet::GetPigmentAttribute(Pigment);
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandlePigmentAttributeChanged);
		AttributeChangedHandles.Emplace(Attribute, Handle);
	}

	BoundAbilitySystem = ASC;
}

void UColorPigmentBarWidget::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : AttributeChangedHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}

	AttributeChangedHandles.Reset();
	BoundAbilitySystem.Reset();
}

void UColorPigmentBarWidget::HandlePigmentAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshPigment();
}

void UColorPigmentBarWidget::RefreshPigment()
{
	const UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(GetOwningPlayer());
	if (!PigmentSet)
	{
		return;
	}

	TArray<FShownSegment> Segments;
	Segments.Add({PigmentSet->GetOwnPigment(), FMath::Max(FMath::RoundToInt(PigmentSet->GetOwnPigmentAmount()), 0)});
	for (const FColorPigmentLayer& Layer : PigmentSet->GetLayers())
	{
		Segments.Add({Layer.Pigment, FMath::Max(FMath::RoundToInt(Layer.Amount), 0)});
	}

	if (Segments != ShownSegments)
	{
		ShownSegments = MoveTemp(Segments);
		ApplySegments();
		RefreshText();
	}
}

void UColorPigmentBarWidget::ApplySegments()
{
	if (!SegmentBox)
	{
		return;
	}

	for (int32 Index = 0; Index < ShownSegments.Num(); Index++)
	{
		UImage* Segment = GetOrCreateSegment(Index);
		const FShownSegment& Shown = ShownSegments[Index];
		const FColorPigmentBarEntry* Entry = FindEntry(Shown.Pigment);
		if (!Entry || Shown.Amount <= 0)
		{
			Segment->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Segment->SetColorAndOpacity(Entry->Color);
		Segment->SetVisibility(ESlateVisibility::HitTestInvisible);

		// Fill 权重直接取颜料量：各段权重和恒为 100，段宽即占比
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Segment->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = static_cast<float>(Shown.Amount);
			BoxSlot->SetSize(Size);
		}
	}

	for (int32 Index = ShownSegments.Num(); Index < SegmentPool.Num(); Index++)
	{
		SegmentPool[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UColorPigmentBarWidget::RefreshText()
{
	if (!PigmentText)
	{
		return;
	}

	TArray<FString> Parts;
	for (const FShownSegment& Shown : ShownSegments)
	{
		const FColorPigmentBarEntry* Entry = FindEntry(Shown.Pigment);
		if (Entry && Shown.Amount > 0)
		{
			Parts.Add(FString::Printf(TEXT("%s %d"), *Entry->Label.ToString(), Shown.Amount));
		}
	}
	PigmentText->SetText(FText::FromString(FString::Join(Parts, TEXT(" / "))));
}

UImage* UColorPigmentBarWidget::GetOrCreateSegment(int32 Index)
{
	while (SegmentPool.Num() <= Index)
	{
		UImage* Segment = NewObject<UImage>(this);
		Segment->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* BoxSlot = SegmentBox->AddChildToHorizontalBox(Segment))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
		}
		SegmentPool.Add(Segment);
	}

	return SegmentPool[Index];
}

const FColorPigmentBarEntry* UColorPigmentBarWidget::FindEntry(EColorPigment Pigment) const
{
	const FGameplayAttribute Attribute = UColorPigmentSet::GetPigmentAttribute(Pigment);
	if (!Attribute.IsValid())
	{
		return nullptr;
	}

	return Entries.FindByPredicate([&Attribute](const FColorPigmentBarEntry& Entry)
	{
		return Entry.Attribute == Attribute;
	});
}
