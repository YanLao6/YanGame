#include "ModularGameplayExperienceCheat.h"

#include "ActorComponent/ModularExperienceComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/ModularExperienceDefinition.h"
#include "ModularGameplayExperiencesLogs.h"
#include "ModularSystemStatics.h"
#include "Misc/PackageName.h"

void UModularGameplayExperienceCheat::PlayNextGame()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority())
	{
		UE_LOG(LogModularGameplayExperiences, Warning, TEXT("PlayNextGame 仅能在服务器或房主窗口执行。"));
		return;
	}
	UModularSystemStatics::PlayNextGame(PlayerController);
}

void UModularGameplayExperienceCheat::ExperienceLevelTravel(const FString& MapPath)
{
	APlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority())
	{
		UE_LOG(LogModularGameplayExperiences, Warning, TEXT("ExperienceLevelTravel 仅能在服务器或房主窗口执行。"));
		return;
	}
	if (!FPackageName::IsValidLongPackageName(MapPath) || !FPackageName::DoesPackageExist(MapPath))
	{
		UE_LOG(LogModularGameplayExperiences, Warning, TEXT("请输入已存在地图的完整包路径，例如 ExperienceLevelTravel /ColorTransfer/Maps/L_Purple。收到：%s"), *MapPath);
		return;
	}
	UWorld* World = PlayerController->GetWorld();
	if (World == nullptr || !World->NextURL.IsEmpty() || World->IsInSeamlessTravel())
	{
		UE_LOG(LogModularGameplayExperiences, Warning, TEXT("当前 World 不可用或正在切关。"));
		return;
	}
	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	const FURL PreviousURL = WorldContext.LastURL;
	// 清除继承源，避免引擎多次合并相对 URL 时恢复旧 Experience。
	WorldContext.LastURL.RemoveOption(TEXT("Experience"));
	FURL URL = WorldContext.LastURL;
	URL.Map = MapPath;
	URL.AddOption(TEXT("SeamlessTravel"));
	FString TravelURL = URL.ToString();
	TravelURL.RemoveFromStart(URL.GetHostPortString());
	UE_LOG(LogModularGameplayExperiences, Display, TEXT("ExperienceLevelTravel：%s"), *TravelURL);
	if (!World->ServerTravel(TravelURL, /*bAbsolute=*/false))
	{
		WorldContext.LastURL = PreviousURL;
		UE_LOG(LogModularGameplayExperiences, Error, TEXT("ExperienceLevelTravel 请求被拒绝：%s"), *TravelURL);
	}
}

void UModularGameplayExperienceCheat::DumpExperience()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	const UModularExperienceComponent* ExperienceComponent = GameState != nullptr
		? GameState->FindComponentByClass<UModularExperienceComponent>() : nullptr;
	const bool bLoaded = ExperienceComponent != nullptr && ExperienceComponent->IsExperienceLoaded();
	const FString ExperienceName = bLoaded
		? ExperienceComponent->GetCurrentExperienceChecked()->GetPrimaryAssetId().ToString()
		: TEXT("未加载完成");
	UE_LOG(LogModularGameplayExperiences, Display, TEXT("World=%s NetMode=%d ExperienceComponent=%s Loaded=%s Experience=%s"),
		*GetPathNameSafe(World), World != nullptr ? static_cast<int32>(World->GetNetMode()) : -1,
		*GetNameSafe(ExperienceComponent), bLoaded ? TEXT("true") : TEXT("false"), *ExperienceName);
}
