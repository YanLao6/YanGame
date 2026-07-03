// Copyright Chronicler.

#include "GameMode/ModularUserFacingExperienceDefinition.h"

#include "CommonSessionSubsystem.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUserFacingExperienceDefinition)

UCommonSession_HostSessionRequest* UModularUserFacingExperienceDefinition::CreateHostingRequest(const UObject* WorldContextObject) const
{
	const FString ExperienceName           = ExperienceID.PrimaryAssetName.ToString();
	const FString UserFacingExperienceName = GetPrimaryAssetId().PrimaryAssetName.ToString();

	UWorld*                            World        = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance*                     GameInstance = World ? World->GetGameInstance() : nullptr;
	UCommonSession_HostSessionRequest* Result       = nullptr;

	if (UCommonSessionSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UCommonSessionSubsystem>() : nullptr)
	{
		Result = Subsystem->CreateOnlineHostSessionRequest();
	}

	if (!Result)
	{
		// 没有用CommonUserSubsystem那句创建一个
		Result                       = NewObject<UCommonSession_HostSessionRequest>();
		Result->OnlineMode           = ECommonSessionOnlineMode::Online;
		Result->bUseLobbies          = true;
		Result->bUseLobbiesVoiceChat = false;
		// We always enable presence on this session because it is the primary session used for matchmaking. For online systems that care about presence, only the primary session should have presence enabled
		// 我们始终在该会话中启用Session，因为它是主要用于匹配的Session。对于关注在线状态的在线系统，只有主会话应启用在线状态
		Result->bUsePresence = !IsRunningDedicatedServer();
	}

	Result->MapID                    = MapID;
	Result->ModeNameForAdvertisement = UserFacingExperienceName;
	Result->ExtraArgs                = ExtraArgs;
	Result->ExtraArgs.Add(TEXT("Experience"), ExperienceName);
	Result->MaxPlayerCount = MaxPlayerCount;

	return Result;
}
