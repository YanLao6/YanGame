#include "Actors/ColorStackedCrystal.h"

#include "Abilities/Ability_TakePaint.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferTravel.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorStackedCrystal)

#define LOCTEXT_NAMESPACE "ColorStackedCrystal"

namespace ColorStackedCrystal
{
	// 射击扣除的量可能带浮点误差，区间两端各放宽这点
	constexpr float AmountTolerance = 0.01f;

	// 全部点亮后留给玩家看清结果的时间
	constexpr float TravelDelay = 1.5f;

	const FName ColorParam(TEXT("Color"));
	const FName EmissiveParam(TEXT("EmissiveStrength"));

	const FLinearColor EmptyColor(0.3f, 0.3f, 0.3f);

	constexpr float EmptyEmissive = 0.0f;
	constexpr float UnlitEmissive = 0.2f;
	constexpr float LitEmissive = 4.0f;
	constexpr float CompletedEmissive = 8.0f;
}

AColorStackedCrystal::AColorStackedCrystal()
{
	InteractionOption.Text = LOCTEXT("TakePaint", "取颜料");
	InteractionOption.InteractionAbilityToGrant = UAbility_TakePaint::StaticClass();

	bReplicates = true;
	SetReplicatingMovement(false);
}

void AColorStackedCrystal::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		for (const FComponentReference& CellReference : CellMeshes)
		{
			UStaticMeshComponent* Cell = Cast<UStaticMeshComponent>(CellReference.GetComponent(this));
			CellMaterials.Add(Cell ? Cell->CreateDynamicMaterialInstance(0) : nullptr);
		}

		OnRep_Visual();
	}
}

void AColorStackedCrystal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

bool AColorStackedCrystal::IsAmountInRange(float Amount) const
{
	return Amount >= MinAmountPerCell - ColorStackedCrystal::AmountTolerance && Amount <= MaxAmountPerCell + ColorStackedCrystal::AmountTolerance;
}

TArray<AColorStackedCrystal::FCellState> AColorStackedCrystal::EvaluateCells() const
{
	TArray<FCellState> Cells;
	Cells.SetNum(CellMeshes.Num());

	int32 CellIndex = 0;
	for (const FColorPigmentLayer& Layer : StoredLayers)
	{
		if (!Cells.IsValidIndex(CellIndex))
		{
			break;
		}

		Cells[CellIndex].Pigment = Layer.Pigment;
		Cells[CellIndex].bLit = Layer.Amount >= MinAmountPerCell - ColorStackedCrystal::AmountTolerance;
		++CellIndex;

		// 溢出的颜料直接点亮下一格，不看下一层有多少
		if (Layer.Amount > MaxAmountPerCell + ColorStackedCrystal::AmountTolerance && Cells.IsValidIndex(CellIndex))
		{
			Cells[CellIndex].Pigment = Layer.Pigment;
			Cells[CellIndex].bLit = true;
			++CellIndex;
		}
	}

	return Cells;
}

void AColorStackedCrystal::ReceiveColorShot(const FColorShotHit& Shot)
{
	if (bCompleted || Shot.Pigment == EColorPigment::None || Shot.Amount <= 0.0f)
	{
		return;
	}

	ColorTransfer::PushLayer(StoredLayers, Shot.Pigment, Shot.Amount);

	OnRep_Visual();
	AuthCheckCompletion();
}

void AColorStackedCrystal::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder)
{
	if (!bCompleted && !StoredLayers.IsEmpty())
	{
		OptionBuilder.AddInteractionOption(InteractionOption);
	}
}

float AColorStackedCrystal::AuthTakeTopPigment(UColorPigmentSet* PigmentSet, float MaxAmount)
{
	if (bCompleted || !PigmentSet)
	{
		return 0.0f;
	}

	const float Taken = ColorTransfer::AuthAbsorbFromTopLayer(*PigmentSet, StoredLayers, MaxAmount, this);
	if (Taken > 0.0f)
	{
		OnRep_Visual();
		AuthCheckCompletion();
	}

	return Taken;
}

void AColorStackedCrystal::AuthCheckCompletion()
{
	if (bCompleted || RequiredSequence.IsEmpty() || StoredLayers.Num() != RequiredSequence.Num())
	{
		return;
	}

	for (int32 LayerIndex = 0; LayerIndex < StoredLayers.Num(); ++LayerIndex)
	{
		const FColorPigmentLayer& Layer = StoredLayers[LayerIndex];
		if (Layer.Pigment != RequiredSequence[LayerIndex] || !IsAmountInRange(Layer.Amount))
		{
			return;
		}
	}

	bCompleted = true;
	OnRep_Visual();

	GetWorldTimerManager().SetTimer(TravelTimer, this, &ThisClass::TravelToNextLevel, ColorStackedCrystal::TravelDelay, /*bLoop=*/false);
}

void AColorStackedCrystal::TravelToNextLevel()
{
	ColorTransfer::ServerTravelToLevel(this, NextLevel);
}

void AColorStackedCrystal::OnRep_Visual()
{
	const TArray<FCellState> Cells = EvaluateCells();

	for (int32 CellIndex = 0; CellIndex < CellMaterials.Num(); ++CellIndex)
	{
		UMaterialInstanceDynamic* CellMaterial = CellMaterials[CellIndex];
		if (!CellMaterial || !Cells.IsValidIndex(CellIndex))
		{
			continue;
		}

		const FCellState& Cell = Cells[CellIndex];

		float Emissive = ColorStackedCrystal::EmptyEmissive;
		if (Cell.Pigment != EColorPigment::None)
		{
			Emissive = bCompleted ? ColorStackedCrystal::CompletedEmissive : (Cell.bLit ? ColorStackedCrystal::LitEmissive : ColorStackedCrystal::UnlitEmissive);
		}

		CellMaterial->SetVectorParameterValue(ColorStackedCrystal::ColorParam, Cell.Pigment != EColorPigment::None ? ColorTransfer::GetPigmentColor(Cell.Pigment) : ColorStackedCrystal::EmptyColor);
		CellMaterial->SetScalarParameterValue(ColorStackedCrystal::EmissiveParam, Emissive);
	}
}

void AColorStackedCrystal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorStackedCrystal, StoredLayers);
	DOREPLIFETIME(AColorStackedCrystal, bCompleted);
}

#undef LOCTEXT_NAMESPACE
