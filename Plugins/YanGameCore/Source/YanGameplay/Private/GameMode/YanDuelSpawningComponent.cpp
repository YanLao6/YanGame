#include "GameMode/YanDuelSpawningComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Player/ModularPlayerStart.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanDuelSpawningComponent)

UYanDuelSpawningComponent::UYanDuelSpawningComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

AActor* UYanDuelSpawningComponent::OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts)
{
	if (PlayerStarts.Num() == 0)
	{
		return nullptr;
	}

	const TArray<FVector> OpponentLocations = GatherOpponentLocations(Player);
	if (OpponentLocations.Num() == 0)
	{
		// 开局或对手尚未生成时无从比较，交回基类的随机选点
		return nullptr;
	}

	AModularPlayerStart* BestStart      = nullptr;
	int32                BestTier       = TNumericLimits<int32>::Max();
	float                BestMinDistSq  = -1.0f;

	for (AModularPlayerStart* Start : PlayerStarts)
	{
		if (Start == nullptr)
		{
			continue;
		}

		// 空闲优先于半占用，半占用优先于满占用；距离只在同一档内比较，
		// 否则会为了多几米距离把玩家生成在别人身上
		const int32 Tier = static_cast<int32>(Start->GetLocationOccupancy(Player));

		// 取该点到最近对手的距离作为其得分：多人时避免选到"离甲最远但贴着乙"的点
		float MinDistSq = TNumericLimits<float>::Max();
		for (const FVector& OpponentLocation : OpponentLocations)
		{
			MinDistSq = FMath::Min(MinDistSq, FVector::DistSquared(Start->GetActorLocation(), OpponentLocation));
		}

		if (Tier < BestTier || (Tier == BestTier && MinDistSq > BestMinDistSq))
		{
			BestTier      = Tier;
			BestMinDistSq = MinDistSq;
			BestStart     = Start;
		}
	}

	// 由基类统一 TryClaim，此处只负责选点
	return BestStart;
}

TArray<FVector> UYanDuelSpawningComponent::GatherOpponentLocations(const AController* Player) const
{
	TArray<FVector> Locations;

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return Locations;
	}

	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		const AController* Other = It->Get();
		if (Other == nullptr || Other == Player)
		{
			continue;
		}

		// 只认已落地的 Pawn：控制器可能正处在自身的重生间隙，此时它没有可供避开的位置
		if (const APawn* OtherPawn = Other->GetPawn())
		{
			Locations.Add(OtherPawn->GetActorLocation());
		}
	}

	return Locations;
}
