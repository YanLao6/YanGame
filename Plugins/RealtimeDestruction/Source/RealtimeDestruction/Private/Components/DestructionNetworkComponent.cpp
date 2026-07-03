// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

// DestructionNetworkComponent.cpp

#include "Components/DestructionNetworkComponent.h"
#include "Components/RealtimeDestructibleMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NetworkLogMacros.h"
#include "Debug/DestructionDebugger.h"
#include "HAL/PlatformTime.h"

//////////////////////////////////////////////////////////////////////////
// UDestructionNetworkComponent 实现
//////////////////////////////////////////////////////////////////////////

UDestructionNetworkComponent::UDestructionNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 设置网络复制
	SetIsReplicatedByDefault(true);
}

void UDestructionNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	// 确认是否附加到PlayerController
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DestructionNetworkComponent: 이 컴포넌트는 PlayerController에 추가해야 합니다. 현재 Owner: %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
	}

}

void UDestructionNetworkComponent::RequestDestruction(
	URealtimeDestructibleMeshComponent* DestructComp,
	const FRealtimeDestructionRequest& Request)
{
	if (!DestructComp)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const ENetMode NetMode = World->GetNetMode();

	// 如果是服务器，则在服务器上直接处理
	if (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer)
	{
		// 仅监听服务器：在主机屏幕上显示销毁效果
		if (NetMode == NM_ListenServer)
		{
			DestructComp->RequestDestruction(Request);
		}

		FRealtimeDestructionOp Op;
		Op.Request = Request;

		// 如果使用服务器批处理，则将操作加入队列
		if (DestructComp->bUseServerBatching)
		{
			DestructComp->EnqueueForServerBatch(Op);
		}
		else
		{
			// 在调试器中记录Multicast RPC（包括数据大小）
			if (UDestructionDebugger* Debugger = World->GetSubsystem<UDestructionDebugger>())
			{
				Debugger->RecordMulticastRPCWithSize(1, DestructComp->bUseCompactMulticast);
			}

			// 即使批处理关闭，也检查压缩选项
			if (DestructComp->bUseCompactMulticast)
			{
				TArray<FCompactDestructionOp> CompactOps;
				CompactOps.Add(FCompactDestructionOp::Compress(Op.Request, 0));
				DestructComp->MulticastApplyOpsCompact(CompactOps);
			}
			else
			{
				TArray<FRealtimeDestructionOp> Ops;
				Ops.Add(Op);
				DestructComp->MulticastApplyOps(Ops);
			}
		}
	}
	// 如果是客户端，向服务器发送RPC
	else if (NetMode == NM_Client)
	{
		// 记录客户端发送数据大小
		if (UDestructionDebugger* Debugger = World->GetSubsystem<UDestructionDebugger>())
		{
			Debugger->RecordServerRPCWithSize(bUseCompactData);
		}

		if (bUseCompactData)
		{
			// 压缩方式 (11 bytes)
			FCompactDestructionOp CompactOp = FCompactDestructionOp::Compress(Request, LocalSequence++);
			ServerApplyDestructionCompact(DestructComp, CompactOp);
		}
		else
		{
			// 传统方式 (32+ bytes)
			FRealtimeDestructionRequest RequestWithTime = Request;
			RequestWithTime.ClientSendTime = FPlatformTime::Seconds();
			ServerApplyDestruction(DestructComp, RequestWithTime);
		}
	}
	// 如果是单人游戏，立即应用
	else // NM_Standalone
	{
		// NET_LOG_COMPONENT(this, "Standalone - 即时销毁 (Radius: %.1f)", Request.HoleRadius);
		DestructComp->RequestDestruction(Request);
	}
}

void UDestructionNetworkComponent::ServerApplyDestruction_Implementation(
	URealtimeDestructibleMeshComponent* DestructComp,
	const FRealtimeDestructionRequest& Request)
{
	// NET_LOG_COMPONENT(this, "接收到Server RPC - 处理销毁请求");

	UWorld* World = GetWorld();

	// 在调试器中记录Server RPC（非压缩方式，包括数据大小）
	if (UDestructionDebugger* Debugger = World ? World->GetSubsystem<UDestructionDebugger>() : nullptr)
	{
		Debugger->RecordServerRPCWithSize(false);

		// 记录客户端信息
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (PC)
		{
			int32 ClientId = PC->GetUniqueID();
			FString PlayerName = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("Unknown");
			Debugger->RecordClientRequest(ClientId, PlayerName, false);
		}
	}

	if (!DestructComp)
	{
		NET_LOG_COMPONENT_WARNING(this, "DestructComp为空");
		return;
	}

	// 请求验证
	EDestructionRejectReason RejectReason;
	if (bEnableValidation && !ValidateDestructionRequest(DestructComp, Request, RejectReason))
	{
		NET_LOG_COMPONENT_WARNING(this, "销毁请求验证失败 - 请求被拒绝，原因: %d", static_cast<uint8>(RejectReason));

		// 在调试器中记录验证失败
		if (UDestructionDebugger* Debugger = World ? World->GetSubsystem<UDestructionDebugger>() : nullptr)
		{
			APlayerController* PC = Cast<APlayerController>(GetOwner());
			int32 ClientId = PC ? PC->GetUniqueID() : -1;
			Debugger->RecordValidationFailure(ClientId);
		}

		// 通知客户端请求被拒绝 (Sequence为0 - 非压缩版本无序列号)
		DestructComp->ClientDestructionRejected(0, RejectReason);
		return;
	}

	// 客户端发来的Request没有ToolMeshPtr - 用ShapeParams重新创建
	FRealtimeDestructionRequest ModifiedRequest = Request;
	if (!ModifiedRequest.ToolMeshPtr.IsValid())
	{
		ModifiedRequest.ToolMeshPtr = DestructComp->CreateToolMeshPtrFromShapeParams(
			ModifiedRequest.ToolShape,
			ModifiedRequest.ShapeParams
		);
	}

	//// 仅监听服务器：在主机屏幕上显示销毁效果
	//if (World && World->GetNetMode() == NM_ListenServer)
	//{
	//	DestructComp->RequestDestruction(ModifiedRequest);
	//} 
	if (World && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer))
	{
		DestructComp->RequestDestruction(ModifiedRequest);
	}


	// 向所有客户端传播销毁效果
	FRealtimeDestructionOp Op;
	Op.Request = ModifiedRequest;

	// 如果使用服务器批处理，则将操作加入队列
	if (DestructComp->bUseServerBatching)
	{
		DestructComp->EnqueueForServerBatch(Op);
	}
	else
	{
		// 在调试器中记录Multicast RPC（包括数据大小）
		if (UDestructionDebugger* Debugger = World ? World->GetSubsystem<UDestructionDebugger>() : nullptr)
		{
			Debugger->RecordMulticastRPCWithSize(1, DestructComp->bUseCompactMulticast);
		}

		// 即使批处理关闭，也检查压缩选项
		if (DestructComp->bUseCompactMulticast)
		{
			TArray<FCompactDestructionOp> CompactOps;
			// 包含客户端计算的ChunkIndex进行压缩
			CompactOps.Add(FCompactDestructionOp::Compress(Op.Request, 0));
			DestructComp->MulticastApplyOpsCompact(CompactOps);
		}
		else
		{
			TArray<FRealtimeDestructionOp> Ops;
			Ops.Add(Op);
			DestructComp->MulticastApplyOps(Ops);
		}
	}
}

void UDestructionNetworkComponent::ServerApplyDestructionCompact_Implementation(
	URealtimeDestructibleMeshComponent* DestructComp,
	const FCompactDestructionOp& CompactOp)
{
	UWorld* World = GetWorld();

	// 在调试器中记录Server RPC（压缩方式，包括数据大小）
	if (UDestructionDebugger* Debugger = World ? World->GetSubsystem<UDestructionDebugger>() : nullptr)
	{
		Debugger->RecordServerRPCWithSize(true);

		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (PC)
		{
			int32 ClientId = PC->GetUniqueID();
			FString PlayerName = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("Unknown");
			Debugger->RecordClientRequest(ClientId, PlayerName, true);  // true = 压缩方式
		}
	}

	if (!DestructComp)
	{
		NET_LOG_COMPONENT_WARNING(this, "DestructComp为空 (Compact)");
		return;
	}

	// 解压缩
	FRealtimeDestructionRequest Request = CompactOp.Decompress();

	// 请求验证
	EDestructionRejectReason RejectReason;
	if (bEnableValidation && !ValidateDestructionRequest(DestructComp, Request, RejectReason))
	{
		NET_LOG_COMPONENT_WARNING(this, "销毁请求验证失败 (Compact) - 请求被拒绝，原因: %d", static_cast<uint8>(RejectReason));

		// 通知客户端请求被拒绝
		DestructComp->ClientDestructionRejected(CompactOp.Sequence, RejectReason);
		return;
	}

	// 重新创建ToolMeshPtr (解压后也没有)
	if (!Request.ToolMeshPtr.IsValid())
	{
		Request.ToolMeshPtr = DestructComp->CreateToolMeshPtrFromShapeParams(
			Request.ToolShape,
			Request.ShapeParams
		);
	}

	//// 仅监听服务器：在主机屏幕上显示销毁效果
	//if (World && World->GetNetMode() == NM_ListenServer)
	//{
	//	DestructComp->RequestDestruction(Request);
	//}

	 // 在服务器上处理销毁 (Listen Server + Dedicated Server)
	if (World && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer))
	{
		DestructComp->RequestDestruction(Request);
	}

	// 向所有客户端传播销毁效果
	FRealtimeDestructionOp Op;
	Op.Request = Request;

	// 如果使用服务器批处理，则将操作加入队列
	if (DestructComp->bUseServerBatching)
	{
		DestructComp->EnqueueForServerBatch(Op);
	}
	else
	{
		// 在调试器中记录Multicast RPC（包括数据大小）
		if (UDestructionDebugger* Debugger = World ? World->GetSubsystem<UDestructionDebugger>() : nullptr)
		{
			Debugger->RecordMulticastRPCWithSize(1, DestructComp->bUseCompactMulticast);
		}

		// 即使批处理关闭，也检查压缩选项
		if (DestructComp->bUseCompactMulticast)
		{
			TArray<FCompactDestructionOp> CompactOps;
			// 包含客户端计算的ChunkIndex进行压缩
			CompactOps.Add(FCompactDestructionOp::Compress(Op.Request, 0));
			DestructComp->MulticastApplyOpsCompact(CompactOps);
		}
		else
		{
			TArray<FRealtimeDestructionOp> Ops;
			Ops.Add(Op);
			DestructComp->MulticastApplyOps(Ops);
		}
	}
}



bool UDestructionNetworkComponent::ValidateDestructionRequest(
	URealtimeDestructibleMeshComponent* DestructComp,
	const FRealtimeDestructionRequest& Request,
	EDestructionRejectReason& OutReason) const
{
	OutReason = EDestructionRejectReason::None;

	if (!DestructComp)
	{
		OutReason = EDestructionRejectReason::InvalidPosition;
		return false;
	}

	// 半径验证
	if (Request.ShapeParams.Radius > MaxAllowedRadius)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DestructionNetworkComponent: 请求的半径(%.1f)超过了最大允许值(%.1f)"),
			Request.ShapeParams.Radius, MaxAllowedRadius);
		OutReason = EDestructionRejectReason::InvalidPosition;
		return false;
	}

	// 调用RealtimeDestructibleMeshComponent的验证方法
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!DestructComp->ValidateDestructionRequest(Request, PC, OutReason))
	{
		return false;
	}

	return true;
}


