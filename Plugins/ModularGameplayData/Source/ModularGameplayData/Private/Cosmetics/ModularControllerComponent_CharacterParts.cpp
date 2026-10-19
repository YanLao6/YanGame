// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/ModularControllerComponent_CharacterParts.h"

#include "Cosmetics/ModularCharacterPartTypes.h"
#include "Cosmetics/ModularCosmeticDeveloperSettings.h"
#include "Cosmetics/ModularPawnComponent_CharacterParts.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularControllerComponent_CharacterParts)

UModularControllerComponent_CharacterParts::UModularControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UModularControllerComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();

	// 部件由服务端权威下发，客户端只接收复制结果
	if (!HasAuthority())
	{
		return;
	}

	if (AController* OwningController = GetController<AController>())
	{
		OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

		// 组件晚于 Possess 创建时，需补一次下发
		if (APawn* ControlledPawn = GetPawn<APawn>())
		{
			OnPossessedPawnChanged(nullptr, ControlledPawn);
		}
	}

	ApplyDeveloperSettings();
}

void UModularControllerComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllCharacterParts();

	Super::EndPlay(EndPlayReason);
}

UModularPawnComponent_CharacterParts* UModularControllerComponent_CharacterParts::GetPawnCustomizer() const
{
	if (const APawn* ControlledPawn = GetPawn<APawn>())
	{
		return ControlledPawn->FindComponentByClass<UModularPawnComponent_CharacterParts>();
	}

	return nullptr;
}

void UModularControllerComponent_CharacterParts::AddCharacterPart(const FModularCharacterPart& NewPart)
{
	AddCharacterPartInternal(NewPart, EModularCharacterPartSource::Natural);
}

void UModularControllerComponent_CharacterParts::AddCharacterPartInternal(const FModularCharacterPart& NewPart, EModularCharacterPartSource Source)
{
	FModularControllerCharacterPartEntry& NewEntry = CharacterParts.AddDefaulted_GetRef();
	NewEntry.Part                                  = NewPart;
	NewEntry.Source                                = Source;

	if (UModularPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		// 被作弊屏蔽的部件只登记不下发，作弊关闭后由 SetSuppressionOnNaturalParts 补上
		if (NewEntry.Source != EModularCharacterPartSource::NaturalSuppressedViaCheat)
		{
			NewEntry.Handle = PawnCustomizer->AddCharacterPart(NewPart);
		}
	}
}

void UModularControllerComponent_CharacterParts::RemoveCharacterPart(const FModularCharacterPart& PartToRemove)
{
	for (auto EntryIt = CharacterParts.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (!FModularCharacterPart::AreEquivalentParts(EntryIt->Part, PartToRemove))
		{
			continue;
		}

		if (UModularPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
		{
			PawnCustomizer->RemoveCharacterPart(EntryIt->Handle);
		}

		EntryIt.RemoveCurrent();
		break;
	}
}

void UModularControllerComponent_CharacterParts::RemoveAllCharacterParts()
{
	if (UModularPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		for (FModularControllerCharacterPartEntry& Entry : CharacterParts)
		{
			PawnCustomizer->RemoveCharacterPart(Entry.Handle);
		}
	}

	CharacterParts.Reset();
}

void UModularControllerComponent_CharacterParts::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// 从旧 Pawn 上摘下，句柄随之失效
	if (UModularPawnComponent_CharacterParts* OldCustomizer = OldPawn ? OldPawn->FindComponentByClass<UModularPawnComponent_CharacterParts>() : nullptr)
	{
		for (FModularControllerCharacterPartEntry& Entry : CharacterParts)
		{
			OldCustomizer->RemoveCharacterPart(Entry.Handle);
			Entry.Handle.Reset();
		}
	}

	if (UModularPawnComponent_CharacterParts* NewCustomizer = NewPawn ? NewPawn->FindComponentByClass<UModularPawnComponent_CharacterParts>() : nullptr)
	{
		for (FModularControllerCharacterPartEntry& Entry : CharacterParts)
		{
			// 本函数可能以空 OldPawn 重复调用，句柄仍有效时不重复下发
			if (!Entry.Handle.IsValid() && Entry.Source != EModularCharacterPartSource::NaturalSuppressedViaCheat)
			{
				Entry.Handle = NewCustomizer->AddCharacterPart(Entry.Part);
			}
		}
	}
}

void UModularControllerComponent_CharacterParts::ApplyDeveloperSettings()
{
#if UE_WITH_CHEAT_MANAGER
	const UModularCosmeticDeveloperSettings* Settings = GetDefault<UModularCosmeticDeveloperSettings>();

	const bool bSuppressNaturalParts = (Settings->CheatMode == EModularCosmeticCheatMode::ReplaceParts) && (Settings->CheatCosmeticCharacterParts.Num() > 0);
	SetSuppressionOnNaturalParts(bSuppressNaturalParts);

	// 先清掉上一轮由开发者设置下发的部件，再整体重下，避免增量比对
	UModularPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();
	for (auto It = CharacterParts.CreateIterator(); It; ++It)
	{
		if (It->Source != EModularCharacterPartSource::AppliedViaDeveloperSettingsCheat)
		{
			continue;
		}

		if (PawnCustomizer != nullptr)
		{
			PawnCustomizer->RemoveCharacterPart(It->Handle);
		}

		It.RemoveCurrent();
	}

	for (const FModularCharacterPart& PartDesc : Settings->CheatCosmeticCharacterParts)
	{
		AddCharacterPartInternal(PartDesc, EModularCharacterPartSource::AppliedViaDeveloperSettingsCheat);
	}
#endif
}

void UModularControllerComponent_CharacterParts::AddCheatPart(const FModularCharacterPart& NewPart, bool bSuppressNaturalParts)
{
#if UE_WITH_CHEAT_MANAGER
	SetSuppressionOnNaturalParts(bSuppressNaturalParts);
	AddCharacterPartInternal(NewPart, EModularCharacterPartSource::AppliedViaCheatManager);
#endif
}

void UModularControllerComponent_CharacterParts::ClearCheatParts()
{
#if UE_WITH_CHEAT_MANAGER
	UModularPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();

	for (auto It = CharacterParts.CreateIterator(); It; ++It)
	{
		if (It->Source != EModularCharacterPartSource::AppliedViaCheatManager)
		{
			continue;
		}

		if (PawnCustomizer != nullptr)
		{
			PawnCustomizer->RemoveCharacterPart(It->Handle);
		}

		It.RemoveCurrent();
	}

	// 控制台作弊清空后，开发者设置的部件应重新生效
	ApplyDeveloperSettings();
#endif
}

void UModularControllerComponent_CharacterParts::SetSuppressionOnNaturalParts(bool bSuppressed)
{
#if UE_WITH_CHEAT_MANAGER
	UModularPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();

	for (FModularControllerCharacterPartEntry& Entry : CharacterParts)
	{
		if ((Entry.Source == EModularCharacterPartSource::Natural) && bSuppressed)
		{
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(Entry.Handle);
				Entry.Handle.Reset();
			}

			Entry.Source = EModularCharacterPartSource::NaturalSuppressedViaCheat;
		}
		else if ((Entry.Source == EModularCharacterPartSource::NaturalSuppressedViaCheat) && !bSuppressed)
		{
			if (PawnCustomizer != nullptr)
			{
				Entry.Handle = PawnCustomizer->AddCharacterPart(Entry.Part);
			}

			Entry.Source = EModularCharacterPartSource::Natural;
		}
	}
#endif
}
