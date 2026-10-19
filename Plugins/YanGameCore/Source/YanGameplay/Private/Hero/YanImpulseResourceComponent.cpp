#include "Hero/YanImpulseResourceComponent.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/YanImpulseSet.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Pawn.h"
#include "Messages/VerbMessageHelpers.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanImpulseResourceComponent)

UYanImpulseResourceComponent::UYanImpulseResourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UYanImpulseResourceComponent::BeginPlay()
{
	Super::BeginPlay();

	// 演化只在服务器发生，客户端拿到的是复制过来的结果
	SetComponentTickEnabled(GetOwner() != nullptr && GetOwner()->HasAuthority());
}

void UYanImpulseResourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void UYanImpulseResourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UAbilitySystemComponent* ASC = ResolveAndBindAbilitySystem();
	if (ASC == nullptr)
	{
		return;
	}

	const APawn* OwningPawn = GetPawn<APawn>();
	if (OwningPawn == nullptr)
	{
		return;
	}

	if (const UMoverComponent* MoverComp = OwningPawn->FindComponentByClass<UMoverComponent>())
	{
		TickChargeRegen(*ASC, *MoverComp, DeltaTime);
	}

	TickLoadDecay(*ASC, DeltaTime);
}

UAbilitySystemComponent* UYanImpulseResourceComponent::ResolveAndBindAbilitySystem()
{
	// ASC 在 PlayerState 上，其复制晚于 Pawn 的 BeginPlay，故每帧解析而非缓存一次即止
	UAbilitySystemComponent* ASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwner());
	if (ASC == nullptr)
	{
		return nullptr;
	}

	if (BoundAbilitySystem.Get() != ASC)
	{
		UnbindAbilitySystem();

		LoadChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UYanImpulseSet::GetLoadAttribute())
		                       .AddUObject(this, &UYanImpulseResourceComponent::HandleLoadChanged);
		BoundAbilitySystem = ASC;
	}

	return ASC;
}

void UYanImpulseResourceComponent::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
	{
		if (LoadChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UYanImpulseSet::GetLoadAttribute()).Remove(LoadChangedHandle);
		}
	}

	LoadChangedHandle.Reset();
	BoundAbilitySystem.Reset();
}

void UYanImpulseResourceComponent::TickChargeRegen(UAbilitySystemComponent& ASC, const UMoverComponent& MoverComp, float DeltaSeconds)
{
	const float MaxCharge     = ASC.GetNumericAttribute(UYanImpulseSet::GetMaxChargeAttribute());
	const float CurrentCharge = ASC.GetNumericAttribute(UYanImpulseSet::GetChargeAttribute());
	if (MaxCharge <= 0.f || CurrentCharge >= MaxCharge)
	{
		PendingCharge = 0.f;
		return;
	}

	const UCharacterMoverComponent* CharMover = Cast<UCharacterMoverComponent>(&MoverComp);
	const bool                      bOnGround = CharMover != nullptr && CharMover->IsOnGround();
	const bool                      bClimbing = !bOnGround && MoverComp.GetMovementModeName() == WallClimbModeName;

	float RegenPerSecond = 0.f;
	float RegenCeiling   = MaxCharge;

	if (bOnGround)
	{
		// 「0.4 秒内充满」是整条能量条的时长，故速率随上限走而非每格固定
		RegenPerSecond = MaxCharge / FMath::Max(GroundRefillSeconds, KINDA_SMALL_NUMBER);
	}
	else if (bClimbing)
	{
		RegenPerSecond = 1.f / FMath::Max(WallClimbSecondsPerCharge, KINDA_SMALL_NUMBER);
		RegenCeiling   = FMath::Min(MaxCharge, WallClimbChargeCap);
	}

	if (RegenPerSecond <= 0.f || CurrentCharge >= RegenCeiling)
	{
		PendingCharge = 0.f;
		return;
	}

	PendingCharge += RegenPerSecond * DeltaSeconds;

	const float WholeCharges = FMath::FloorToFloat(PendingCharge);
	if (WholeCharges < 1.f)
	{
		return;
	}

	PendingCharge -= WholeCharges;

	// 上限按本次恢复来源的天花板收敛，避免攀爬一路补到满格
	const float Granted = FMath::Min(WholeCharges, RegenCeiling - CurrentCharge);
	if (Granted > 0.f)
	{
		ASC.ApplyModToAttribute(UYanImpulseSet::GetChargeAttribute(), EGameplayModOp::Additive, Granted);
	}
}

void UYanImpulseResourceComponent::TickLoadDecay(UAbilitySystemComponent& ASC, float DeltaSeconds)
{
	TimeSinceLoadGain += DeltaSeconds;

	if (LoadDecayPerSecond <= 0.f || TimeSinceLoadGain < LoadDecayDelaySeconds)
	{
		return;
	}

	const float CurrentLoad = ASC.GetNumericAttribute(UYanImpulseSet::GetLoadAttribute());
	if (CurrentLoad <= 0.f)
	{
		return;
	}

	const float Decay = FMath::Min(LoadDecayPerSecond * DeltaSeconds, CurrentLoad);
	ASC.ApplyModToAttribute(UYanImpulseSet::GetLoadAttribute(), EGameplayModOp::Additive, -Decay);
}

void UYanImpulseResourceComponent::HandleLoadChanged(const FOnAttributeChangeData& ChangeData)
{
	// 只有上升才重置脱战计时。衰减本身也会触发本回调，若一并重置，
	// 第一次衰减就会把计时推回起点，此后每帧削一点、再推回，负荷永远掉不完
	if (ChangeData.NewValue > ChangeData.OldValue)
	{
		TimeSinceLoadGain = 0.f;
	}
}
