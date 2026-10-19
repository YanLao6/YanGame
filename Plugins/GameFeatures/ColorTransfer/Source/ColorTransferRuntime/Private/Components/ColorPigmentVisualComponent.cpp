#include "Components/ColorPigmentVisualComponent.h"

#include "Attributes/ColorPigmentSet.h"
#include "Components/ColorPigmentRulesComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/ModularPawnComponent_CharacterParts.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPigmentVisualComponent)

namespace ColorPigmentVisual
{
	// 材质里能区分的外来层数，更深的层并入最后一层显示
	constexpr int32 MaxLayerSlots = 4;

	const FName LayerColorParams[MaxLayerSlots] = {TEXT("LayerColor0"), TEXT("LayerColor1"), TEXT("LayerColor2"), TEXT("LayerColor3")};
	const FName LayerEdgeParams[MaxLayerSlots] = {TEXT("LayerEdge0"), TEXT("LayerEdge1"), TEXT("LayerEdge2"), TEXT("LayerEdge3")};
	const FName OwnColorParam(TEXT("OwnColor"));
	const FName SpreadOriginParam(TEXT("SpreadOrigin"));
	const FName PurpleAlphaParam(TEXT("PurpleAlpha"));
}

UColorPigmentVisualComponent::UColorPigmentVisualComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// 扩散起点取骨骼位置，须在动画更新之后读取
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UColorPigmentVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (UModularPawnComponent_CharacterParts* CharacterParts = GetOwner()->FindComponentByClass<UModularPawnComponent_CharacterParts>())
	{
		CharacterParts->OnCharacterPartsChanged.AddDynamic(this, &ThisClass::OnCharacterPartsChanged);
	}
}

void UColorPigmentVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearOverlayFromMeshes();

	Super::EndPlay(EndPlayReason);
}

void UColorPigmentVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 颜料属性集随 PlayerState 复制，模拟端要等它到达后才能开始画
	const UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(GetPawn<APawn>());
	if (!PigmentSet || PigmentSet->GetOwnPigment() == EColorPigment::None)
	{
		return;
	}

	if (!OverlayMID)
	{
		UMaterialInterface* Material = OverlayMaterial.LoadSynchronous();
		if (!Material)
		{
			SetComponentTickEnabled(false);
			return;
		}

		OverlayMID = UMaterialInstanceDynamic::Create(Material, this);
		ApplyOverlayToMeshes();
	}

	UpdateMaterialParameters(*PigmentSet);
}

void UColorPigmentVisualComponent::OnCharacterPartsChanged(UModularPawnComponent_CharacterParts* CharacterParts)
{
	if (OverlayMID)
	{
		ApplyOverlayToMeshes();
	}
}

void UColorPigmentVisualComponent::ApplyOverlayToMeshes()
{
	ClearOverlayFromMeshes();

	// 可见的身体来自外观部件（子 Actor），Pawn 自身的主网格只是不画东西的骨架
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes, /*bIncludeFromChildActors=*/true);

	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		// Nanite 蒙皮代理不绘制 Overlay，须退回非 Nanite 的渲染路径
		Mesh->SetForceDisableNanite(true);
		Mesh->SetOverlayMaterial(OverlayMID);
		OverlaidMeshes.Add(Mesh);
	}
}

void UColorPigmentVisualComponent::ClearOverlayFromMeshes()
{
	for (const TWeakObjectPtr<USkeletalMeshComponent>& WeakMesh : OverlaidMeshes)
	{
		if (USkeletalMeshComponent* Mesh = WeakMesh.Get(); Mesh && Mesh->GetOverlayMaterial() == OverlayMID)
		{
			Mesh->SetOverlayMaterial(nullptr);
			Mesh->SetForceDisableNanite(false);
		}
	}

	OverlaidMeshes.Reset();
}

void UColorPigmentVisualComponent::UpdateMaterialParameters(const UColorPigmentSet& PigmentSet)
{
	using namespace ColorPigmentVisual;

	// 各层外缘到指尖的归一化距离，由栈顶往外累加；未使用的层为 -1，材质里不占面积
	const TArray<FColorPigmentLayer>& Layers = PigmentSet.GetLayers();
	float Cumulative = 0.0f;

	for (int32 Slot = 0; Slot < MaxLayerSlots; ++Slot)
	{
		const int32 LayerIndex = Layers.Num() - 1 - Slot;
		if (LayerIndex < 0)
		{
			OverlayMID->SetScalarParameterValue(LayerEdgeParams[Slot], -1.0f);
			continue;
		}

		// 最后一个槽位延伸到全部外来颜料的外缘，更深的层并入其中
		Cumulative = Slot == MaxLayerSlots - 1
			? 1.0f - PigmentSet.GetOwnPigmentAmount() / UColorPigmentSet::TotalPigment
			: Cumulative + Layers[LayerIndex].Amount / UColorPigmentSet::TotalPigment;
		OverlayMID->SetScalarParameterValue(LayerEdgeParams[Slot], Cumulative);
		OverlayMID->SetVectorParameterValue(LayerColorParams[Slot], ColorTransfer::GetPigmentColor(Layers[LayerIndex].Pigment));
	}

	OverlayMID->SetVectorParameterValue(OwnColorParam, ColorTransfer::GetPigmentColor(PigmentSet.GetOwnPigment()));

	// Pawn 自身的主网格驱动全部姿态，外观部件与第一人称网格都跟随它
	if (const USkeletalMeshComponent* LeaderMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
	{
		OverlayMID->SetVectorParameterValue(SpreadOriginParam, FLinearColor(LeaderMesh->GetSocketLocation(SpreadOriginBone)));
	}

	const APlayerState* PlayerState = GetPlayerState<APlayerState>();
	const UColorPigmentRulesComponent* Rules = PlayerState ? PlayerState->FindComponentByClass<UColorPigmentRulesComponent>() : nullptr;
	OverlayMID->SetScalarParameterValue(PurpleAlphaParam, Rules ? Rules->GetPurpleAlpha() : 0.0f);
}
