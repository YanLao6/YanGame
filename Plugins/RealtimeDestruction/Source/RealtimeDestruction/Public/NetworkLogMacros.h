// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

// NetworkLogMacros.h
// 网络调试日志宏系统

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

// 日志分类声明
DECLARE_LOG_CATEGORY_EXTERN(LogNetwork, Log, All);

//=============================================================================
// 网络角色字符串辅助函数
//=============================================================================

// 将网络角色枚举转换为字符串
inline FString GetNetRoleString(ENetRole Role)
{
    switch (Role)
    {
    case ROLE_None:             return TEXT("None");
    case ROLE_SimulatedProxy:   return TEXT("SimProxy");
    case ROLE_AutonomousProxy:  return TEXT("AutoProxy");
    case ROLE_Authority:        return TEXT("Authority");
    default:                    return TEXT("Unknown");
    }
}

// 从 World 获取网络模式字符串
inline FString GetNetModeString(UWorld* World)
{
    if (!World) return TEXT("[NoWorld]");

    switch (World->GetNetMode())
    {
    case NM_Standalone:         return TEXT("[Standalone]");
    case NM_DedicatedServer:    return TEXT("[Server]");
    case NM_ListenServer:       return TEXT("[ListenServer]");
    case NM_Client:             return TEXT("[Client]");
    default:                    return TEXT("[Unknown]");
    }
}

// 从 UObject 获取网络模式字符串
inline FString GetNetModeString(const UObject* Object)
{
    if (!Object) return TEXT("[NoObject]");
    return GetNetModeString(Object->GetWorld());
}

//=============================================================================
// Actor 网络信息提取函数
//=============================================================================

// 获取 Actor 的完整网络信息字符串
inline FString GetActorNetInfo(AActor* Actor)
{
    if (!Actor)
    {
        return TEXT("Actor=NULL");
    }

    FString OwnerName = Actor->GetOwner() ? Actor->GetOwner()->GetName() : TEXT("None");
    FString InstigatorName = Actor->GetInstigator() ? Actor->GetInstigator()->GetName() : TEXT("None");
    UNetConnection* NetConn = Actor->GetNetConnection();

    return FString::Printf(TEXT("%s | Local:%s Remote:%s | Owner:%s | NetConn:%s"),
        *Actor->GetName(),
        *GetNetRoleString(Actor->GetLocalRole()),
        *GetNetRoleString(Actor->GetRemoteRole()),
        *OwnerName,
        NetConn ? TEXT("Valid") : TEXT("NULL"));
}

// 从组件的拥有者 Actor 提取网络信息
inline FString GetComponentNetInfo(UActorComponent* Component)
{
    if (!Component)
    {
        return TEXT("Component=NULL");
    }

    AActor* Owner = Component->GetOwner();
    if (!Owner)
    {
        return FString::Printf(TEXT("%s | OwnerActor=NULL"), *Component->GetName());
    }

    return FString::Printf(TEXT("%s -> %s"), *Component->GetName(), *GetActorNetInfo(Owner));
}

//=============================================================================
// 基础网络日志宏
//=============================================================================

// 基础网络日志（自动显示网络模式）
#define NET_LOG(Object, Format, ...) \
    UE_LOG(LogNetwork, Log, TEXT("%s %s"), \
        *GetNetModeString(Object), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define NET_LOG_WARNING(Object, Format, ...) \
    UE_LOG(LogNetwork, Warning, TEXT("%s %s"), \
        *GetNetModeString(Object), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define NET_LOG_ERROR(Object, Format, ...) \
    UE_LOG(LogNetwork, Error, TEXT("%s %s"), \
        *GetNetModeString(Object), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

//=============================================================================
// Actor 网络日志宏
//=============================================================================

// 带 Actor 网络信息的日志输出
#define NET_LOG_ACTOR(Actor, Format, ...) \
    UE_LOG(LogNetwork, Log, TEXT("%s [%s] %s"), \
        *GetNetModeString(Actor), \
        *GetActorNetInfo(Actor), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define NET_LOG_ACTOR_WARNING(Actor, Format, ...) \
    UE_LOG(LogNetwork, Warning, TEXT("%s [%s] %s"), \
        *GetNetModeString(Actor), \
        *GetActorNetInfo(Actor), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define NET_LOG_ACTOR_ERROR(Actor, Format, ...) \
    UE_LOG(LogNetwork, Error, TEXT("%s [%s] %s"), \
        *GetNetModeString(Actor), \
        *GetActorNetInfo(Actor), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

//=============================================================================
// Component 网络日志宏
//=============================================================================

// 带 Component 网络信息的日志输出
#define NET_LOG_COMPONENT(Component, Format, ...) \
    UE_LOG(LogNetwork, Log, TEXT("%s [%s] %s"), \
        *GetNetModeString(Component), \
        *GetComponentNetInfo(Component), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define NET_LOG_COMPONENT_WARNING(Component, Format, ...) \
    UE_LOG(LogNetwork, Warning, TEXT("%s [%s] %s"), \
        *GetNetModeString(Component), \
        *GetComponentNetInfo(Component), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define NET_LOG_COMPONENT_ERROR(Component, Format, ...) \
    UE_LOG(LogNetwork, Error, TEXT("%s [%s] %s"), \
        *GetNetModeString(Component), \
        *GetComponentNetInfo(Component), \
        *FString::Printf(TEXT(Format), ##__VA_ARGS__))

//=============================================================================
// RPC 调试宏
//=============================================================================

// 在调用服务器 RPC 前进行验证和记录
#define NET_LOG_SERVER_RPC(Actor, RPCName) \
    { \
        UNetConnection* __NetConn = Actor ? Actor->GetNetConnection() : nullptr; \
        if (__NetConn) \
        { \
            UE_LOG(LogNetwork, Log, TEXT("%s [%s] Server RPC '%s' called - NetConnection valid"), \
                *GetNetModeString(Actor), *GetActorNetInfo(Actor), TEXT(#RPCName)); \
        } \
        else \
        { \
            UE_LOG(LogNetwork, Error, TEXT("%s [%s] Server RPC '%s' expected to fail - NetConnection NULL!"), \
                *GetNetModeString(Actor), *GetActorNetInfo(Actor), TEXT(#RPCName)); \
        } \
    }

// 客户端 RPC 日志
#define NET_LOG_CLIENT_RPC(Actor, RPCName) \
    UE_LOG(LogNetwork, Log, TEXT("%s [%s] Client RPC '%s' called"), \
        *GetNetModeString(Actor), *GetActorNetInfo(Actor), TEXT(#RPCName))

// 多播 RPC 日志
#define NET_LOG_MULTICAST_RPC(Actor, RPCName) \
    UE_LOG(LogNetwork, Log, TEXT("%s [%s] Multicast RPC '%s' called"), \
        *GetNetModeString(Actor), *GetActorNetInfo(Actor), TEXT(#RPCName))

//=============================================================================
// 所有权调试宏
//=============================================================================

// 打印 Actor 的完整所有权链
#define NET_LOG_OWNERSHIP(Actor) \
    { \
        if (Actor) \
        { \
            AActor* __Current = Actor; \
            FString __Chain = __Current->GetName(); \
            while (__Current->GetOwner()) \
            { \
                __Current = __Current->GetOwner(); \
                __Chain += TEXT(" -> ") + __Current->GetName(); \
            } \
            UE_LOG(LogNetwork, Warning, TEXT("%s === Ownership Chain === %s"), \
                *GetNetModeString(Actor), *__Chain); \
            UE_LOG(LogNetwork, Warning, TEXT("%s   HasAuthority: %s"), \
                *GetNetModeString(Actor), Actor->HasAuthority() ? TEXT("true") : TEXT("false")); \
            UE_LOG(LogNetwork, Warning, TEXT("%s   LocalRole: %s, RemoteRole: %s"), \
                *GetNetModeString(Actor), \
                *GetNetRoleString(Actor->GetLocalRole()), \
                *GetNetRoleString(Actor->GetRemoteRole())); \
            UE_LOG(LogNetwork, Warning, TEXT("%s   NetConnection: %s"), \
                *GetNetModeString(Actor), \
                Actor->GetNetConnection() ? TEXT("Valid") : TEXT("NULL")); \
            UE_LOG(LogNetwork, Warning, TEXT("%s   Instigator: %s"), \
                *GetNetModeString(Actor), \
                Actor->GetInstigator() ? *Actor->GetInstigator()->GetName() : TEXT("None")); \
        } \
        else \
        { \
            UE_LOG(LogNetwork, Error, TEXT("[Unknown] === Ownership Chain === Actor is NULL!")); \
        } \
    }

// 组件所有权调试 (基于所有者 Actor)
#define NET_LOG_COMPONENT_OWNERSHIP(Component) \
    { \
        if (Component) \
        { \
            AActor* __Owner = Component->GetOwner(); \
            UE_LOG(LogNetwork, Warning, TEXT("%s === Component Ownership === %s"), \
                *GetNetModeString(Component), *Component->GetName()); \
            NET_LOG_OWNERSHIP(__Owner); \
        } \
        else \
        { \
            UE_LOG(LogNetwork, Error, TEXT("[Unknown] === Component Ownership === Component is NULL!")); \
        } \
    }

//=============================================================================
// 条件网络日志宏 (仅服务器/客户端)
//=============================================================================

// 仅在服务器上输出日志
#define NET_LOG_SERVER_ONLY(Object, Format, ...) \
    { \
        UWorld* __World = Object ? Object->GetWorld() : nullptr; \
        if (__World && (__World->GetNetMode() == NM_DedicatedServer || __World->GetNetMode() == NM_ListenServer)) \
        { \
            NET_LOG(Object, Format, ##__VA_ARGS__); \
        } \
    }

// 仅在客户端上输出日志
#define NET_LOG_CLIENT_ONLY(Object, Format, ...) \
    { \
        UWorld* __World = Object ? Object->GetWorld() : nullptr; \
        if (__World && __World->GetNetMode() == NM_Client) \
        { \
            NET_LOG(Object, Format, ##__VA_ARGS__); \
        } \
    }

//=============================================================================
// 屏幕调试输出宏 (GEngine->AddOnScreenDebugMessage)
//=============================================================================

// 在屏幕上显示网络信息 (自动颜色: 服务器=绿色, 客户端=青色)
#define NET_SCREEN_LOG(Object, Duration, Format, ...) \
    { \
        if (GEngine) \
        { \
            UWorld* __World = Object ? Object->GetWorld() : nullptr; \
            FColor __Color = FColor::White; \
            FString __Prefix = TEXT("[Unknown]"); \
            if (__World) \
            { \
                switch (__World->GetNetMode()) \
                { \
                case NM_DedicatedServer: __Color = FColor::Green; __Prefix = TEXT("[Server]"); break; \
                case NM_ListenServer: __Color = FColor::Yellow; __Prefix = TEXT("[ListenServer]"); break; \
                case NM_Client: __Color = FColor::Cyan; __Prefix = TEXT("[Client]"); break; \
                case NM_Standalone: __Color = FColor::White; __Prefix = TEXT("[Standalone]"); break; \
                default: break; \
                } \
            } \
            GEngine->AddOnScreenDebugMessage(-1, Duration, __Color, \
                FString::Printf(TEXT("%s %s"), *__Prefix, *FString::Printf(TEXT(Format), ##__VA_ARGS__))); \
        } \
    }

// 使用唯一键的屏幕输出 (相同键会被更新)
#define NET_SCREEN_LOG_KEY(Object, Key, Duration, Format, ...) \
    { \
        if (GEngine) \
        { \
            UWorld* __World = Object ? Object->GetWorld() : nullptr; \
            FColor __Color = FColor::White; \
            FString __Prefix = TEXT("[Unknown]"); \
            if (__World) \
            { \
                switch (__World->GetNetMode()) \
                { \
                case NM_DedicatedServer: __Color = FColor::Green; __Prefix = TEXT("[Server]"); break; \
                case NM_ListenServer: __Color = FColor::Yellow; __Prefix = TEXT("[ListenServer]"); break; \
                case NM_Client: __Color = FColor::Cyan; __Prefix = TEXT("[Client]"); break; \
                case NM_Standalone: __Color = FColor::White; __Prefix = TEXT("[Standalone]"); break; \
                default: break; \
                } \
            } \
            GEngine->AddOnScreenDebugMessage(Key, Duration, __Color, \
                FString::Printf(TEXT("%s %s"), *__Prefix, *FString::Printf(TEXT(Format), ##__VA_ARGS__))); \
        } \
    }
