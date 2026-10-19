// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class YanGame : ModuleRules
{
	public YanGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ModularGameplayExperiences",
			"ModularGameplayAbilities",
			"YanGameMover"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true

		StageSteamAppId(Target);
	}

	// SteamShared 自动写出 steam_appid.txt 的逻辑被 UE_BUILD_SHIPPING 排除，
	// 而 SteamAPI_Init 要求该文件与可执行文件同级，否则以开发用 AppId 启动必然失败。
	// 文件名不允许被平台层重映射或打入 pak，故声明为 SystemNonUFS。
	// 改用正式 AppId 发布时删除项目根的 steam_appid.txt 即可，此处会自动跳过。
	private void StageSteamAppId(ReadOnlyTargetRules Target)
	{
		if (Target.Configuration != UnrealTargetConfiguration.Shipping || Target.ProjectFile == null)
		{
			return;
		}

		string AppIdFile = Path.Combine(Target.ProjectFile.Directory.FullName, "steam_appid.txt");
		if (File.Exists(AppIdFile))
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/steam_appid.txt", AppIdFile, StagedFileType.SystemNonUFS);
		}
	}
}