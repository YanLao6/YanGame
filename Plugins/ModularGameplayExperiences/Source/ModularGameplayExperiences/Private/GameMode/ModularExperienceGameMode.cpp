// Copyright Chronicler.


#include "GameMode/ModularExperienceGameMode.h"

#include "GameMapsSettings.h"
#include "ModularGameplayExperiencesLogs.h"
#include "ActorComponent/ModularExperienceComponent.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "ActorComponent/ModularPlayerSpawningComponent.h"
#include "DataAsset/ModularAssetManager.h"
#include "GameMode/ModularExperienceDefinition.h"
#include "GameMode/ModularWorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ModularExperiencePlayerState.h"
#include "CommonUserSubsystem.h"
#include "ModularGameState.h"
#include "GameMode/ModularExperienceGameState.h"
#include "GameMode/ModularUserFacingExperienceDefinition.h"


AModularExperienceGameMode::AModularExperienceGameMode(const FObjectInitializer& ObjectInitializer)
{
	GameStateClass = AModularExperienceGameState::StaticClass();
}

const UModularPawnData* AModularExperienceGameMode::GetPawnDataForController(const AController* InController) const
{
	// 若 PlayerState 上已有 PawnData，优先使用。
	if (InController != nullptr)
	{
		if (const AModularExperiencePlayerState* PlayerState = InController->GetPlayerState<AModularExperiencePlayerState>())
		{
			if (const UModularPawnData* PawnData = PlayerState->GetPawnData<UModularPawnData>())
			{
				return PawnData;
			}
		}
	}

	// 否则回退到当前 Experience 的 DefaultPawnData
	check(GameState);
	const UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);

	if (ExperienceComponent->IsExperienceLoaded())
	{
		const UModularExperienceDefinition* Experience = ExperienceComponent->GetCurrentExperienceChecked();
		if (!Experience->DefaultPawnData.IsNull())
		{
			// 正常路径：Equipped Bundle 加载完成后软引用已在内存，Get() 即可；
			// 此处用 LoadSynchronous 兜底，防止极端情况下 bundle 数据尚未落地（如资产保存前 PIE）
			return Experience->DefaultPawnData.LoadSynchronous();
		}

		// Experience 已加载但仍无 PawnData：回退 AssetManager 默认
		return UModularAssetManager::Get().GetDefaultPawnData();
	}

	// Experience 尚未加载：尚无可用 PawnData
	return nullptr;
}

void AModularExperienceGameMode::RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset)
{
	if (bForceReset && (Controller != nullptr))
	{
		Controller->Reset();
	}

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		GetWorldTimerManager().SetTimerForNextTick(PC, &APlayerController::ServerRestartPlayer_Implementation);
	}
	//@todo 支持通用 Bot 重生路径。
}

bool AModularExperienceGameMode::ControllerCanRestart(AController* Controller)
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (!Super::PlayerCanRestart_Implementation(PC))
		{
			return false;
		}
	}
	else
	{
		// Bot：等价于基类 PlayerCanRestart 的核心判定
		if ((Controller == nullptr) || Controller->IsPendingKillPending())
		{
			return false;
		}
	}

	if (UModularPlayerSpawningComponent* PlayerSpawningComponent = GameState->FindComponentByClass<UModularPlayerSpawningComponent>())
	{
		return PlayerSpawningComponent->ControllerCanRestart(Controller);
	}

	return true;
}

UClass* AModularExperienceGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const UModularPawnData* PawnData = GetPawnDataForController(InController))
	{
		if (PawnData->PawnClass)
		{
			return PawnData->PawnClass;
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AModularExperienceGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// 延至下一帧再解析 Match/Experience，给 Startup 设置初始化时间
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

APawn* AModularExperienceGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator         = GetInstigator();
	SpawnInfo.ObjectFlags        |= RF_Transient; // 默认玩家 Pawn 不落盘到地图
	SpawnInfo.bDeferConstruction = true;

	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo))
		{
			if (UModularPawnComponent* PawnExtComp = UModularPawnComponent::FindModularPawnComponent(SpawnedPawn))
			{
				if (const UModularPawnData* PawnData = GetPawnDataForController(NewPlayer))
				{
					PawnExtComp->SetPawnData(PawnData);
				}
				else
				{
					UE_LOG(LogModularGameplayExperiences, Error, TEXT("Game mode was unable to set PawnData on the spawned pawn [%s]."), *GetNameSafe(SpawnedPawn));
				}
			}

			SpawnedPawn->FinishSpawning(SpawnTransform);

			return SpawnedPawn;
		}
		UE_LOG(LogModularGameplayExperiences, Error, TEXT("Game mode was unable to spawn Pawn of class [%s] at [%s]."), *GetNameSafe(PawnClass), *SpawnTransform.ToHumanReadableString());
	}
	else
	{
		UE_LOG(LogModularGameplayExperiences, Error, TEXT("Game mode was unable to spawn Pawn due to NULL pawn class."));
	}

	return nullptr;
}

bool AModularExperienceGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	return false;
}

void AModularExperienceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Experience 未加载完成前不启动玩家；早到的玩家在 OnExperienceLoaded 里 Restart
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

AActor* AModularExperienceGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UModularPlayerSpawningComponent* PlayerSpawningComponent = GameState->FindComponentByClass<UModularPlayerSpawningComponent>())
	{
		return PlayerSpawningComponent->ChoosePlayerStart(Player);
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void AModularExperienceGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	if (UModularPlayerSpawningComponent* PlayerSpawningComponent = GameState->FindComponentByClass<UModularPlayerSpawningComponent>())
	{
		PlayerSpawningComponent->FinishRestartPlayer(NewPlayer, StartRotation);
	}

	Super::FinishRestartPlayer(NewPlayer, StartRotation);
}

bool AModularExperienceGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return ControllerCanRestart(Player);
}

void AModularExperienceGameMode::InitGameState()
{
	Super::InitGameState();

	// 监听 Experience 加载完成
	//@todo 确保项目 GameState 始终带默认 ExperienceComponent；缺失将触发 check 崩溃
	UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

bool AModularExperienceGameMode::UpdatePlayerStartSpot(AController*   Player,
                                                       const FString& Portal,
                                                       FString&       OutErrorMessage)
{
	// PostLogin 真实 Spawn 前不做选点；此时分队等系统尚未运行
	return true;
}

void AModularExperienceGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);

	OnGameModePlayerInitialized.Broadcast(this, NewPlayer);
}

void AModularExperienceGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
	Super::FailedToRestartPlayer(NewPlayer);

	// Spawn 失败：在存在 Pawn Class 时尝试下一帧再 Restart（避免无效死循环）
	if (GetDefaultPawnClassForController(NewPlayer))
	{
		if (APlayerController* NewPC = Cast<APlayerController>(NewPlayer))
		{
			// 真实玩家：若 PlayerCanRestart 已 false 则不再重试
			if (PlayerCanRestart(NewPC))
			{
				RequestPlayerRestartNextFrame(NewPlayer, false);
			}
			else
			{
				UE_LOG(LogModularGameplayExperiences,
				       Verbose,
				       TEXT("FailedToRestartPlayer(%s) and PlayerCanRestart returned false, so we're not going to try again."),
				       *GetPathNameSafe(NewPlayer));
			}
		}
		else
		{
			RequestPlayerRestartNextFrame(NewPlayer, false);
		}
	}
	else
	{
		UE_LOG(LogModularGameplayExperiences,
		       Verbose,
		       TEXT("FailedToRestartPlayer(%s) but there's no pawn class so giving up."),
		       *GetPathNameSafe(NewPlayer));
	}
}

void AModularExperienceGameMode::OnUserInitializedForDedicatedServer(const UCommonUserInfo*   UserInfo,
                                                                     bool                     bSuccess,
                                                                     FText                    Error,
                                                                     ECommonUserPrivilege     RequestedPrivilege,
                                                                     ECommonUserOnlineContext OnlineContext)
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		// 解绑一次性回调
		UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();
		UserSubsystem->OnUserInitializeComplete.RemoveDynamic(this, &AModularExperienceGameMode::OnUserInitializedForDedicatedServer);

		if (bSuccess)
		{
			// 在线登录成功：全 Online Hosting
			UE_LOG(LogModularGameplayExperiences, Log, TEXT("Dedicated server online login succeeded, starting online server"));
			HostDedicatedServerMatch(ECommonSessionOnlineMode::Online);
		}
		else
		{
			// 登录失败仍尝试 Hosting（无 Online），便于局域网/测试
			UE_LOG(LogModularGameplayExperiences, Log, TEXT("Dedicated server online login failed, starting LAN-only server"));
			HostDedicatedServerMatch(ECommonSessionOnlineMode::LAN);
		}
	}
}

void AModularExperienceGameMode::OnExperienceLoaded(const UModularExperienceDefinition* CurrentExperience)
{
	// 为已连接但尚无 Pawn 的 PlayerController 执行 Restart
	//@todo 当前仅遍历 PlayerController；Bot 与其它 Controller 路径需与 GetDefaultPawnClass 逻辑对齐
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Cast<APlayerController>(*Iterator);
		if ((PlayerController == nullptr) || (PlayerController->GetPawn() != nullptr))
		{
			continue;
		}
		if (!PlayerCanRestart(PlayerController))
		{
			continue;
		}
		RestartPlayer(PlayerController);
	}
}

bool AModularExperienceGameMode::IsExperienceLoaded() const
{
	check(GameState);
	const UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
	check(ExperienceComponent);

	return ExperienceComponent->IsExperienceLoaded();
}

void AModularExperienceGameMode::OnMatchAssignmentGiven(const FPrimaryAssetId& ExperienceId, const FString& ExperienceIdSource)
{
	if (ExperienceId.IsValid())
	{
		UE_LOG(LogModularGameplayExperiences,
		       Log,
		       TEXT("Identified experience %s (Source: %s)"),
		       *ExperienceId.ToString(),
		       *ExperienceIdSource);

		UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>();
		check(ExperienceComponent);
		ExperienceComponent->SetCurrentExperience(ExperienceId);
	}
	else
	{
		UE_LOG(LogModularGameplayExperiences,
		       Error,
		       TEXT("Failed to identify experience, loading screen will stay up forever"));
	}
}

void AModularExperienceGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
	FPrimaryAssetId ExperienceId;
	FString         ExperienceIdSource;

	// ExperienceId 决议优先级（高者优先）：Matchmaking -> URL Options -> DeveloperSettings(PIE) -> CommandLine -> WorldSettings -> DedicatedServer 流程 -> 默认资产

	if (UWorld* World = GetWorld(); !World)
	{
		return;
	}

	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		ExperienceId                        = FPrimaryAssetId(FPrimaryAssetType(UModularExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
		ExperienceIdSource                  = TEXT("OptionsString");
	}

	//@todo PIE 下支持 DeveloperSettings 覆盖 Experience

	// CommandLine：Experience=...
	if (!ExperienceId.IsValid())
	{
		FString ExperienceFromCommandLine;
		if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
		{
			ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
			if (!ExperienceId.PrimaryAssetType.IsValid())
			{
				ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UModularExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
			}
			ExperienceIdSource = TEXT("CommandLine");
		}
	}

	// AModularWorldSettings 默认 Experience
	if (!ExperienceId.IsValid())
	{
		if (AModularWorldSettings* TypedWorldSettings = Cast<AModularWorldSettings>(GetWorldSettings()))
		{
			ExperienceId       = TypedWorldSettings->GetDefaultGameplayExperience();
			ExperienceIdSource = TEXT("WorldSettings");
		}
	}

	UModularAssetManager& AssetManager = UModularAssetManager::Get();
	if (FAssetData Dummy; ExperienceId.IsValid() && !AssetManager.GetPrimaryAssetData(ExperienceId, /*out*/ Dummy))
	{
		UE_LOG(LogModularGameplayExperiences, Error, TEXT("EXPERIENCE: Wanted to use %s but couldn't find it, falling back to the default)"), *ExperienceId.ToString());
		ExperienceId = FPrimaryAssetId();
	}

	// 最后回退：默认 Experience 资产
	if (!ExperienceId.IsValid())
	{
		if (TryDedicatedServerLogin())
		{
			// Dedicated Server：登录流程内会触发 Hosting，此处提前返回
			return;
		}

		//@TODO: 改为从配置文件读取默认 PrimaryAssetId
		ExperienceId       = FPrimaryAssetId(FPrimaryAssetType("ModularExperienceDefinition"), FName("ED_DefaultExperience"));
		ExperienceIdSource = TEXT("Default");
	}

	OnMatchAssignmentGiven(ExperienceId, ExperienceIdSource);
}

bool AModularExperienceGameMode::TryDedicatedServerLogin()
{
	// 示例：Dedicated Server 在默认地图上尝试 Online 登录（项目可深度定制）
	const FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
	const UWorld* World      = GetWorld();
	if (const UGameInstance* GameInstance = GetGameInstance();
		GameInstance
		&& World
		&& World->GetNetMode() == NM_DedicatedServer
		&& World->URL.Map == DefaultMap)
	{
		// 仅在 Dedicated Server + 默认地图时走在线登录分支
		UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();

		// DS 可能需要平台在线登录
		UserSubsystem->OnUserInitializeComplete.AddDynamic(this, &AModularExperienceGameMode::OnUserInitializedForDedicatedServer);

		// 无本地 User，索引 0 表示默认平台用户（由 Online 子系统处理）
		if (!UserSubsystem->TryToLoginForOnlinePlay(0))
		{
			OnUserInitializedForDedicatedServer(nullptr, false, FText(), ECommonUserPrivilege::CanPlayOnline, ECommonUserOnlineContext::Default);
		}

		return true;
	}

	return false;
}

void AModularExperienceGameMode::HostDedicatedServerMatch(ECommonSessionOnlineMode OnlineMode)
{
	FPrimaryAssetType UserExperienceType = UModularUserFacingExperienceDefinition::StaticClass()->GetFName();

	// 命令行解析 UserFacingExperience / Playlist
	FPrimaryAssetId UserExperienceId;
	if (FString UserExperienceFromCommandLine;
		FParse::Value(FCommandLine::Get(), TEXT("UserExperience="), UserExperienceFromCommandLine)
		|| FParse::Value(FCommandLine::Get(), TEXT("Playlist="), UserExperienceFromCommandLine))
	{
		UserExperienceId = FPrimaryAssetId::ParseTypeAndName(UserExperienceFromCommandLine);
		if (!UserExperienceId.PrimaryAssetType.IsValid())
		{
			UserExperienceId = FPrimaryAssetId(FPrimaryAssetType(UserExperienceType), FName(*UserExperienceFromCommandLine));
		}
	}

	// DS 启动阶段可同步加载全部 UserFacingExperience 再匹配
	UModularAssetManager& AssetManager = UModularAssetManager::Get();
	if (const TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssetsWithType(UserExperienceType);
		ensure(Handle.IsValid()))
	{
		Handle->WaitUntilComplete();
	}

	TArray<UObject*> UserExperiences;
	AssetManager.GetPrimaryAssetObjectList(UserExperienceType, UserExperiences);
	const UModularUserFacingExperienceDefinition* FoundExperience   = nullptr;
	const UModularUserFacingExperienceDefinition* DefaultExperience = nullptr;

	for (UObject* Object : UserExperiences)
	{
		if (const UModularUserFacingExperienceDefinition* UserExperience = Cast<UModularUserFacingExperienceDefinition>(Object);
			ensure(UserExperience))
		{
			if (UserExperience->GetPrimaryAssetId() == UserExperienceId)
			{
				FoundExperience = UserExperience;
				break;
			}

			if (UserExperience->bIsDefaultExperience && DefaultExperience == nullptr)
			{
				DefaultExperience = UserExperience;
			}
		}
	}

	if (FoundExperience == nullptr)
	{
		FoundExperience = DefaultExperience;
	}

	if (const UGameInstance* GameInstance = GetGameInstance(); ensure(FoundExperience && GameInstance))
	{
		// 创建并提交 HostSession（触发地图 Travel 等）
		if (UCommonSession_HostSessionRequest* HostRequest = FoundExperience->CreateHostingRequest(this); ensure(HostRequest))
		{
			HostRequest->OnlineMode = OnlineMode;

			//@todo 允许从项目层覆盖更多 Hosting 参数

			UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
			SessionSubsystem->HostSession(nullptr, HostRequest);

			// Session 子系统负责后续旅行流程
		}
	}
}
