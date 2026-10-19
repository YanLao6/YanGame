// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"

#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "MediaSubtitlesPlayer.generated.h"

#define UE_API GAMESUBTITLES_API

class UMediaPlayer;
class UOverlays;
struct FFrame;

/**
 * 面向游戏的媒体字幕播放器。它需要与 Media Player 并存，
 * 且其 Play() / Pause() / Stop() 必须与对应 Media Player 的同名方法同时调用。
 */
UCLASS(MinimalAPI, BlueprintType)
class UMediaSubtitlesPlayer : public UObject, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:
	/** 该播放器使用的字幕资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitles Source")
	TObjectPtr<UOverlays> SourceSubtitles;

public:
	UE_API virtual void BeginDestroy() override;

	/** 开始播放当前设置的字幕。 */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void Play();

	/** 停止字幕播放器。 */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void Stop();

	/** 将字幕源切换为新的字幕集合。 */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void SetSubtitles(UOverlays* Subtitles);

	/** 把字幕播放绑定到某个 Media Player 的 tick 上。 */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void BindToMediaPlayer(UMediaPlayer* InMediaPlayer);

public:
	//~ FTickableGameObject interface
	UE_API virtual void       Tick(float DeltaSeconds) override;
	virtual ETickableTickType GetTickableTickType() const override { return (HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Always); }
	virtual TStatId           GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMediaSubtitlesPlayer, STATGROUP_Tickables); }

private:
	/** 所关联的 Media Player 引用 */
	TWeakObjectPtr<class UMediaPlayer> MediaPlayer;

	/** 字幕当前是否正在显示 */
	bool bEnabled;
};

#undef UE_API
