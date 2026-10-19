// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularTeamSubsystem.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "ModularTeamAgentInterface.h"
#include "ModularTeamCheats.h"
#include "ModularTeamDisplayAsset.h"
#include "ModularTeamInfoBase.h"
#include "ModularTeamPrivateInfo.h"
#include "ModularTeamPublicInfo.h"
#include "ModularTeams.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTeamSubsystem)

class FSubsystemCollectionBase;

//////////////////////////////////////////////////////////////////////
// FModularTeamTrackingInfo

void FModularTeamTrackingInfo::SetTeamInfo(AModularTeamInfoBase* Info)
{
	if (AModularTeamPublicInfo* NewPublicInfo = Cast<AModularTeamPublicInfo>(Info))
	{
		ensure((PublicInfo == nullptr) || (PublicInfo == NewPublicInfo));
		PublicInfo = NewPublicInfo;

		UModularTeamDisplayAsset* OldDisplayAsset = DisplayAsset;
		DisplayAsset = NewPublicInfo->GetTeamDisplayAsset();

		if (OldDisplayAsset != DisplayAsset)
		{
			OnTeamDisplayAssetChanged.Broadcast(DisplayAsset);
		}
	}
	else if (AModularTeamPrivateInfo* NewPrivateInfo = Cast<AModularTeamPrivateInfo>(Info))
	{
		ensure((PrivateInfo == nullptr) || (PrivateInfo == NewPrivateInfo));
		PrivateInfo = NewPrivateInfo;
	}
	else
	{
		checkf(false, TEXT("Expected a public or private team info but got %s"), *GetPathNameSafe(Info));
	}
}

void FModularTeamTrackingInfo::RemoveTeamInfo(AModularTeamInfoBase* Info)
{
	if (PublicInfo == Info)
	{
		PublicInfo = nullptr;
	}
	else if (PrivateInfo == Info)
	{
		PrivateInfo = nullptr;
	}
	else
	{
		ensureMsgf(false, TEXT("Expected a previously registered team info but got %s"), *GetPathNameSafe(Info));
	}
}

//////////////////////////////////////////////////////////////////////
// UModularTeamSubsystem

UModularTeamSubsystem::UModularTeamSubsystem()
{
}

void UModularTeamSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	auto AddTeamCheats = [](UCheatManager* CheatManager)
	{
		CheatManager->AddCheatManagerExtension(NewObject<UModularTeamCheats>(CheatManager));
	};

	CheatManagerRegistrationHandle = UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(AddTeamCheats));
}

void UModularTeamSubsystem::Deinitialize()
{
	UCheatManager::UnregisterFromOnCheatManagerCreated(CheatManagerRegistrationHandle);

	Super::Deinitialize();
}

bool UModularTeamSubsystem::RegisterTeamInfo(AModularTeamInfoBase* TeamInfo)
{
	if (!ensure(TeamInfo))
	{
		return false;
	}

	const int32 TeamId = TeamInfo->GetTeamId();
	if (ensure(TeamId != INDEX_NONE))
	{
		FModularTeamTrackingInfo& Entry = TeamMap.FindOrAdd(TeamId);
		Entry.SetTeamInfo(TeamInfo);

		return true;
	}

	return false;
}

bool UModularTeamSubsystem::UnregisterTeamInfo(AModularTeamInfoBase* TeamInfo)
{
	if (!ensure(TeamInfo))
	{
		return false;
	}

	const int32 TeamId = TeamInfo->GetTeamId();
	if (ensure(TeamId != INDEX_NONE))
	{
		// 找不到登记项通常意味着这是上一个世界残留的 Actor，忽略即可
		if (FModularTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
		{
			Entry->RemoveTeamInfo(TeamInfo);

			return true;
		}
	}

	return false;
}

bool UModularTeamSubsystem::ChangeTeamForActor(AActor* ActorToChange, int32 NewTeamIndex)
{
	const FGenericTeamId NewTeamID = IntegerToGenericTeamId(NewTeamIndex);

	// 队伍归属的权威载体是 PlayerState，Pawn 与 Controller 只做转发
	if (APlayerState* PlayerState = const_cast<APlayerState*>(FindPlayerStateFromActor(ActorToChange)))
	{
		if (IModularTeamAgentInterface* PSWithTeamInterface = Cast<IModularTeamAgentInterface>(PlayerState))
		{
			PSWithTeamInterface->SetGenericTeamId(NewTeamID);
			return true;
		}
	}

	if (IModularTeamAgentInterface* TeamActor = Cast<IModularTeamAgentInterface>(ActorToChange))
	{
		TeamActor->SetGenericTeamId(NewTeamID);
		return true;
	}

	return false;
}

int32 UModularTeamSubsystem::FindTeamFromObject(const UObject* TestObject) const
{
	// 对象本身就是队伍代理
	if (const IModularTeamAgentInterface* ObjectWithTeamInterface = Cast<IModularTeamAgentInterface>(TestObject))
	{
		return GenericTeamIdToInteger(ObjectWithTeamInterface->GetGenericTeamId());
	}

	if (const AActor* TestActor = Cast<const AActor>(TestObject))
	{
		// 投射物之类的 Actor 由 Instigator 决定归属
		if (const IModularTeamAgentInterface* InstigatorWithTeamInterface = Cast<IModularTeamAgentInterface>(TestActor->GetInstigator()))
		{
			return GenericTeamIdToInteger(InstigatorWithTeamInterface->GetGenericTeamId());
		}

		// 队伍信息 Actor 本身不实现队伍代理接口，单独处理
		if (const AModularTeamInfoBase* TeamInfo = Cast<AModularTeamInfoBase>(TestActor))
		{
			return TeamInfo->GetTeamId();
		}

		// 回退到关联的 PlayerState
		if (const APlayerState* PlayerState = FindPlayerStateFromActor(TestActor))
		{
			if (const IModularTeamAgentInterface* PSWithTeamInterface = Cast<IModularTeamAgentInterface>(PlayerState))
			{
				return GenericTeamIdToInteger(PSWithTeamInterface->GetGenericTeamId());
			}
		}
	}

	return INDEX_NONE;
}

const APlayerState* UModularTeamSubsystem::FindPlayerStateFromActor(const AActor* PossibleTeamActor) const
{
	const APlayerState* FoundPlayerState = nullptr;

	if (PossibleTeamActor != nullptr)
	{
		if (const APawn* Pawn = Cast<const APawn>(PossibleTeamActor))
		{
			FoundPlayerState = Pawn->GetPlayerState();
		}
		else if (const AController* Controller = Cast<const AController>(PossibleTeamActor))
		{
			FoundPlayerState = Controller->PlayerState;
		}
		else
		{
			FoundPlayerState = Cast<const APlayerState>(PossibleTeamActor);
		}
	}

	// 只承认实现了队伍代理接口的 PlayerState，否则调用方拿到的归属无从解析
	if (Cast<IModularTeamAgentInterface>(FoundPlayerState) != nullptr)
	{
		return FoundPlayerState;
	}

	return nullptr;
}

EModularTeamComparison UModularTeamSubsystem::CompareTeams(const UObject* A, const UObject* B, int32& TeamIdA, int32& TeamIdB) const
{
	TeamIdA = FindTeamFromObject(Cast<const AActor>(A));
	TeamIdB = FindTeamFromObject(Cast<const AActor>(B));

	if ((TeamIdA == INDEX_NONE) || (TeamIdB == INDEX_NONE))
	{
		return EModularTeamComparison::InvalidArgument;
	}

	return (TeamIdA == TeamIdB) ? EModularTeamComparison::OnSameTeam : EModularTeamComparison::DifferentTeams;
}

EModularTeamComparison UModularTeamSubsystem::CompareTeams(const UObject* A, const UObject* B) const
{
	int32 TeamIdA;
	int32 TeamIdB;
	return CompareTeams(A, B, /*out*/ TeamIdA, /*out*/ TeamIdB);
}

void UModularTeamSubsystem::FindTeamFromActor(const UObject* TestObject, bool& bIsPartOfTeam, int32& TeamId) const
{
	TeamId = FindTeamFromObject(TestObject);
	bIsPartOfTeam = TeamId != INDEX_NONE;
}

void UModularTeamSubsystem::AddTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount)
{
	auto FailureHandler = [&](const FString& ErrorMessage)
	{
		UE_LOG(LogModularTeams, Error, TEXT("AddTeamTagStack(TeamId: %d, Tag: %s, StackCount: %d) %s"), TeamId, *Tag.ToString(), StackCount, *ErrorMessage);
	};

	if (FModularTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		if (Entry->PublicInfo)
		{
			if (Entry->PublicInfo->HasAuthority())
			{
				Entry->PublicInfo->TeamTags.AddStack(Tag, StackCount);
			}
			else
			{
				FailureHandler(TEXT("failed because it was called on a client"));
			}
		}
		else
		{
			FailureHandler(TEXT("failed because there is no team info spawned yet (called too early, before the experience was ready)"));
		}
	}
	else
	{
		FailureHandler(TEXT("failed because it was passed an unknown team id"));
	}
}

void UModularTeamSubsystem::RemoveTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount)
{
	auto FailureHandler = [&](const FString& ErrorMessage)
	{
		UE_LOG(LogModularTeams, Error, TEXT("RemoveTeamTagStack(TeamId: %d, Tag: %s, StackCount: %d) %s"), TeamId, *Tag.ToString(), StackCount, *ErrorMessage);
	};

	if (FModularTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		if (Entry->PublicInfo)
		{
			if (Entry->PublicInfo->HasAuthority())
			{
				Entry->PublicInfo->TeamTags.RemoveStack(Tag, StackCount);
			}
			else
			{
				FailureHandler(TEXT("failed because it was called on a client"));
			}
		}
		else
		{
			FailureHandler(TEXT("failed because there is no team info spawned yet (called too early, before the experience was ready)"));
		}
	}
	else
	{
		FailureHandler(TEXT("failed because it was passed an unknown team id"));
	}
}

int32 UModularTeamSubsystem::GetTeamTagStackCount(int32 TeamId, FGameplayTag Tag) const
{
	if (const FModularTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		const int32 PublicStackCount = (Entry->PublicInfo != nullptr) ? Entry->PublicInfo->TeamTags.GetStackCount(Tag) : 0;
		const int32 PrivateStackCount = (Entry->PrivateInfo != nullptr) ? Entry->PrivateInfo->TeamTags.GetStackCount(Tag) : 0;
		return PublicStackCount + PrivateStackCount;
	}

	UE_LOG(LogModularTeams, Verbose, TEXT("GetTeamTagStackCount(TeamId: %d, Tag: %s) failed because it was passed an unknown team id"), TeamId, *Tag.ToString());
	return 0;
}

bool UModularTeamSubsystem::TeamHasTag(int32 TeamId, FGameplayTag Tag) const
{
	return GetTeamTagStackCount(TeamId, Tag) > 0;
}

bool UModularTeamSubsystem::DoesTeamExist(int32 TeamId) const
{
	return TeamMap.Contains(TeamId);
}

TArray<int32> UModularTeamSubsystem::GetTeamIDs() const
{
	TArray<int32> Result;
	TeamMap.GenerateKeyArray(Result);
	Result.Sort();
	return Result;
}

bool UModularTeamSubsystem::CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowDamageToSelf) const
{
	if (bAllowDamageToSelf)
	{
		if ((Instigator == Target) || (FindPlayerStateFromActor(Cast<AActor>(Instigator)) == FindPlayerStateFromActor(Cast<AActor>(Target))))
		{
			return true;
		}
	}

	int32 InstigatorTeamId;
	int32 TargetTeamId;
	const EModularTeamComparison Relationship = CompareTeams(Instigator, Target, /*out*/ InstigatorTeamId, /*out*/ TargetTeamId);
	if (Relationship == EModularTeamComparison::DifferentTeams)
	{
		return true;
	}

	if ((Relationship == EModularTeamComparison::InvalidArgument) && (InstigatorTeamId != INDEX_NONE))
	{
		// 无队伍归属的目标（如训练假人）只要具备 ASC 就允许被伤害
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Cast<const AActor>(Target)) != nullptr;
	}

	return false;
}

UModularTeamDisplayAsset* UModularTeamSubsystem::GetTeamDisplayAsset(int32 TeamId, int32 ViewerTeamId)
{
	// ViewerTeamId 预留给相对配色规则，当前实现忽略

	if (FModularTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		return Entry->DisplayAsset;
	}

	return nullptr;
}

UModularTeamDisplayAsset* UModularTeamSubsystem::GetEffectiveTeamDisplayAsset(int32 TeamId, UObject* ViewerTeamAgent)
{
	return GetTeamDisplayAsset(TeamId, FindTeamFromObject(ViewerTeamAgent));
}

void UModularTeamSubsystem::NotifyTeamDisplayAssetModified(UModularTeamDisplayAsset* /*ModifiedAsset*/)
{
	// 编辑期改动量小，直接通知全部观察者而不区分具体被改动的资产
	for (const auto& KVP : TeamMap)
	{
		const FModularTeamTrackingInfo& TrackingInfo = KVP.Value;

		TrackingInfo.OnTeamDisplayAssetChanged.Broadcast(TrackingInfo.DisplayAsset);
	}
}

FOnModularTeamDisplayAssetChangedDelegate& UModularTeamSubsystem::GetTeamDisplayAssetChangedDelegate(int32 TeamId)
{
	return TeamMap.FindOrAdd(TeamId).OnTeamDisplayAssetChanged;
}
