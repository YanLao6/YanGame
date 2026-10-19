#include "Player/YanLoadoutComponent.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "ModularAbilityPlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanLoadoutComponent)

UYanLoadoutComponent::UYanLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UYanLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner != nullptr && Owner->HasAuthority())
	{
		// 候选表可能短于配置的初始槽位，钳到实际可选数量，否则界面会出现永远填不满的空槽
		UnlockedSlotCount = FMath::Clamp(InitialSlotCount, 0, AvailableSpecialties.Num());
	}
}

void UYanLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYanLoadoutComponent, SelectedIndices);
	DOREPLIFETIME(UYanLoadoutComponent, UnlockedSlotCount);

	// 占领进度只是本人的读条，对手看见与否不影响任何判定
	DOREPLIFETIME_CONDITION(UYanLoadoutComponent, CaptureProgress, COND_OwnerOnly);
}

void UYanLoadoutComponent::RequestSelectSpecialty(int32 Index)
{
	ServerSelectSpecialty(Index);
}

void UYanLoadoutComponent::ServerSelectSpecialty_Implementation(int32 Index)
{
	// 客户端界面理应已禁用越界、重复与无槽位的按钮，此处防的是伪造请求
	if (!AvailableSpecialties.IsValidIndex(Index) || IsSpecialtySelected(Index) || !HasPendingSelection())
	{
		return;
	}

	SelectedIndices.Add(Index);
	GrantSpecialty(Index);

	// 服务器不会收到自身的 OnRep，须手动广播，否则监听方在主机端收不到通知
	OnLoadoutChanged.Broadcast();
}

void UYanLoadoutComponent::GrantSelectionSlot()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || IsFullyUnlocked())
	{
		return;
	}

	++UnlockedSlotCount;

	OnLoadoutChanged.Broadcast();
	Owner->ForceNetUpdate();
}

void UYanLoadoutComponent::SetCaptureProgress(float NewProgress)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	const float Clamped = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Clamped, CaptureProgress))
	{
		return;
	}

	CaptureProgress = Clamped;

	// PlayerState 的复制频率约每秒一次，读条不催更就会卡成整段跳变。
	// 但也不能逐帧催更——一条纯展示的数值不该占满 PlayerState 的复制预算，
	// 故按肉眼可辨的步长推送，两端与端点必推。
	const bool bAtEndpoint = Clamped <= 0.0f || Clamped >= 1.0f;
	if (bAtEndpoint || FMath::Abs(Clamped - LastNetPushedProgress) >= CaptureProgressNetStep)
	{
		LastNetPushedProgress = Clamped;
		Owner->ForceNetUpdate();
	}
}

void UYanLoadoutComponent::OnRep_LoadoutState()
{
	OnLoadoutChanged.Broadcast();
}

void UYanLoadoutComponent::GrantSpecialty(int32 Index)
{
	if (!AvailableSpecialties.IsValidIndex(Index))
	{
		return;
	}

	UModularAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// 凭据累积在同一份 GrantedHandles 上：已选特长不回收，新特长只叠加
	if (const UModularAbilitySet* AbilitySet = AvailableSpecialties[Index].AbilitySet)
	{
		AbilitySet->GiveToAbilitySystem(ASC, &GrantedHandles, this);
	}
}

UModularAbilitySystemComponent* UYanLoadoutComponent::GetOwningAbilitySystemComponent() const
{
	if (const AModularAbilityPlayerState* OwningPlayerState = Cast<AModularAbilityPlayerState>(GetOwner()))
	{
		return OwningPlayerState->GetModularAbilitySystemComponent();
	}

	return nullptr;
}
