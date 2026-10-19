// 战斗嘉奖宿主控件声明。

#pragma once

#include "CoreMinimal.h"
#include "Accolades/YanAccoladeDefinition.h"
#include "AsyncMixin.h"
#include "CommonUserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "YanAccoladeHostWidget.generated.h"

class USoundBase;
class UUserWidget;
struct FDataRegistryAcquireResult;
struct FModularNotificationMessage;

/** 一条已取到数据、正在等待加载或等待展示的嘉奖。 */
USTRUCT(BlueprintType)
struct FPendingAccoladeEntry
{
	GENERATED_BODY();

	UPROPERTY(BlueprintReadOnly)
	FYanAccoladeDefinitionRow Row;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> Icon = nullptr;

	UPROPERTY()
	TObjectPtr<UUserWidget> AllocatedWidget = nullptr;

	// 取数据是异步的，返回顺序未必与请求顺序一致，用序号还原展示次序
	int32 SequenceID = 0;

	bool bFinishedLoading = false;
};

/**
 * 战斗嘉奖宿主控件。
 *
 * 监听通知消息，按 DataRegistry 取出嘉奖数据并异步加载其图标与音效，
 * 再按请求顺序逐条展示；同一时刻只展示一条，新的嘉奖可顶掉带有指定 Tag 的旧嘉奖。
 * 单条嘉奖的具体外观由蓝图创建。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanAccoladeHostWidget : public UCommonUserWidget, public FAsyncMixin
{
	GENERATED_BODY()

public:
	/** 本宿主负责的展示位置，用于筛选投递到此处的嘉奖。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag LocationName;

	//~Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End UUserWidget Interface

	/** 销毁一条嘉奖的展示控件。 */
	UFUNCTION(BlueprintImplementableEvent)
	void DestroyAccoladeWidget(UUserWidget* Widget);

	/** 为一条嘉奖创建展示控件。 */
	UFUNCTION(BlueprintImplementableEvent)
	UUserWidget* CreateAccoladeWidget(const FPendingAccoladeEntry& Entry);

private:
	FGameplayMessageListenerHandle ListenerHandle;

	int32 NextDisplaySequenceID = 0;
	int32 AllocatedSequenceID = 0;

	FTimerHandle NextTimeToReconsiderHandle;

	// 正在异步加载的嘉奖，可能乱序完成
	UPROPERTY(Transient)
	TArray<FPendingAccoladeEntry> PendingAccoladeLoads;

	// 等待展示的嘉奖，首项为当前可见的那条
	UPROPERTY(Transient)
	TArray<FPendingAccoladeEntry> PendingAccoladeDisplays;

	void OnNotificationMessage(FGameplayTag Channel, const FModularNotificationMessage& Notification);
	void OnRegistryLoadCompleted(const FDataRegistryAcquireResult& AccoladeHandle, int32 SequenceID);

	void ConsiderLoadedAccolades();
	void PopDisplayedAccolade();
	void ProcessLoadedAccolade(const FPendingAccoladeEntry& Entry);
	void DisplayNextAccolade();
};
