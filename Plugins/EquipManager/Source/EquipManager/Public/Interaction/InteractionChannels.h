// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"

/**
 * 交互检测专用 Trace Channel。
 *
 * 对应 DefaultEngine.ini [/Script/Engine.CollisionProfile] 中
 * ECC_GameTraceChannel1 被命名为 "Interaction"。修改此处时请同步更新 ini。
 */
#define EquipManager_TraceChannel_Interaction					ECC_GameTraceChannel1
