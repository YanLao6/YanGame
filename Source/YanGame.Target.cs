// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class YanGameTarget : TargetRules
{
	public YanGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("YanGame");

		// 联机故障（Steam 初始化失败、NetDriver 回退、连接超时）在无日志的 Shipping 包中
		// 表现完全一致，无法区分。正式上架前应关闭。
		bUseLoggingInShipping = true;
	}
}
