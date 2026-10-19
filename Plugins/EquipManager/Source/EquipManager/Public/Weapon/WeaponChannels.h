// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"

/**
 * 武器射击检测专用 Trace Channel。
 *
 * 对应 DefaultEngine.ini [/Script/Engine.CollisionProfile] 中
 * ECC_GameTraceChannel2 被命名为 "Weapon"。修改此处时请同步更新 ini。
 *
 * 与 Visibility 分离的意义在于：可以让某些物体挡视线但不挡子弹（反之亦然）。
 */
#define EquipManager_TraceChannel_Weapon					ECC_GameTraceChannel2
