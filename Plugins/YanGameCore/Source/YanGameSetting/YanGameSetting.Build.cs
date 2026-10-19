using UnrealBuildTool;

public class YanGameSetting : ModuleRules
{
	public YanGameSetting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameSettings",
				"CommonUI",
				"UMG",
				"EnhancedInput",
				// 按键绑定的公开接口直接使用 FKey
				"InputCore",
				// 音频输出设备项在公开头中持有 FAudioOutputDeviceInfo 与回调委托类型
				"AudioMixer",
				// 性能统计的公开头继承 IPerformanceDataConsumer（ChartCreation.h）
				"Engine",
				"GameplayTags",
				// UDeveloperSettingsBackedByCVars 与 UPlatformSettings 作为性能/音频配置的基类
				"DeveloperSettings",
				// 共享设置的公开头持有字幕外观枚举（SubtitleDisplayOptions.h）
				"GameSubtitles",
				// 前端性能设置 Action 的公开头继承 UGameFeatureAction
				"GameFeatures"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"RHI",
				// FGameSettingDataSourceDynamic 在公开头中持有 FCachedPropertyPath，
				// 而 GameSettings 只把 PropertyPath 列为私有依赖，此处需自行链接
				"PropertyPath",
				// 语言项切换后需弹出重启提示对话框
				"CommonGame",
				"CommonInput",
				// 音频输出设备的增删与默认设备变更通知
				"AudioMixerCore",
				// 性能统计读取服务器 tick 频率
				"ModularGameplayExperiences",
				// 性能统计读取本地 Pawn 的 Mover 速度
				"Mover",
				// 控制总线与混音，音量设置经此生效
				"AudioModulation",
				// 加载画面期间切换混音
				"CommonLoadingScreen"
			}
		);
	}
}
