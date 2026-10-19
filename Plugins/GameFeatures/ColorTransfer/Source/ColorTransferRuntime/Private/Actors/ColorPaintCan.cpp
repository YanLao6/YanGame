#include "Actors/ColorPaintCan.h"

#include "Abilities/Ability_TakePaint.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPaintCan)

#define LOCTEXT_NAMESPACE "ColorPaintCan"

namespace ColorPaintCan
{
	const FName LayerCountParam(TEXT("LayerCount"));
	const FName LayerColorParams[AColorPaintCan::MaxLayers] = {TEXT("Layer0Color"), TEXT("Layer1Color"), TEXT("Layer2Color"), TEXT("Layer3Color"), TEXT("Layer4Color"), TEXT("Layer5Color")};
	const FName LayerTopParams[AColorPaintCan::MaxLayers] = {TEXT("Layer0Top"), TEXT("Layer1Top"), TEXT("Layer2Top"), TEXT("Layer3Top"), TEXT("Layer4Top"), TEXT("Layer5Top")};
}

AColorPaintCan::AColorPaintCan()
{
	CanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CanMesh"));
	SetRootComponent(CanMesh);

	LiquidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LiquidMesh"));
	LiquidMesh->SetupAttachment(CanMesh);
	LiquidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionOption.Text = LOCTEXT("TakePaint", "取颜料");
	InteractionOption.InteractionAbilityToGrant = UAbility_TakePaint::StaticClass();

	bReplicates = true;
	SetReplicatingMovement(false);
}

void AColorPaintCan::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		LiquidMaterial = LiquidMesh->CreateDynamicMaterialInstance(0);
		OnRep_StoredLayers();
	}
}

void AColorPaintCan::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (Shot.Pigment == EColorPigment::None)
	{
		return;
	}

	const bool bOpensNewLayer = StoredLayers.IsEmpty() || StoredLayers.Last().Pigment != Shot.Pigment;
	if (bOpensNewLayer && StoredLayers.Num() >= MaxLayers)
	{
		return;
	}

	const float Accepted = FMath::Min(Shot.Amount, Capacity - ColorTransfer::GetLayersTotal(StoredLayers));
	if (Accepted <= 0.0f)
	{
		return;
	}

	ColorTransfer::PushLayer(StoredLayers, Shot.Pigment, Accepted);
	OnRep_StoredLayers();
}

void AColorPaintCan::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder)
{
	if (!StoredLayers.IsEmpty())
	{
		OptionBuilder.AddInteractionOption(InteractionOption);
	}
}

float AColorPaintCan::AuthTakeTopPigment(UColorPigmentSet* PigmentSet, float MaxAmount)
{
	const float Taken = PigmentSet ? ColorTransfer::AuthAbsorbFromTopLayer(*PigmentSet, StoredLayers, MaxAmount, this) : 0.0f;
	if (Taken > 0.0f)
	{
		OnRep_StoredLayers();
	}

	return Taken;
}

void AColorPaintCan::OnRep_StoredLayers()
{
	if (!LiquidMaterial)
	{
		return;
	}

	const int32 VisibleLayers = FMath::Min(StoredLayers.Num(), MaxLayers);

	float LayerTop = 0.0f;
	for (int32 LayerIndex = 0; LayerIndex < MaxLayers; ++LayerIndex)
	{
		FLinearColor LayerColor = FLinearColor::Transparent;
		if (LayerIndex < VisibleLayers)
		{
			LayerTop = FMath::Min(LayerTop + StoredLayers[LayerIndex].Amount / Capacity, 1.0f);
			LayerColor = ColorTransfer::GetPigmentColor(StoredLayers[LayerIndex].Pigment);
		}

		LiquidMaterial->SetVectorParameterValue(ColorPaintCan::LayerColorParams[LayerIndex], LayerColor);
		LiquidMaterial->SetScalarParameterValue(ColorPaintCan::LayerTopParams[LayerIndex], LayerTop);
	}

	LiquidMaterial->SetScalarParameterValue(ColorPaintCan::LayerCountParam, VisibleLayers);
}

void AColorPaintCan::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorPaintCan, StoredLayers);
}

#undef LOCTEXT_NAMESPACE
