# 操作指南

### 在线服务是试验性的，因此通用用户插件的初始版本在默认情况下不会使用它。要交换插件以使用新接口，你可以修改CommonUser.Build.cs，将bUseOnlineSubsystemV1布尔值更改为false，然后找到游戏的Config/DefaultEngine.ini配置文件，并添加以下行。

    [/Script/Engine.OnlineEngineInterface]
    bUseOnlineServicesV2=true

### 这将启用使用新接口时所需的版本依赖项。要使用EOS在线服务，你需要在包含[OnlineSubsystemEOS]小节的配置文件中禁用 Epic在线服务（EOS）在线子系统。例如，在 Lyra/Config/Custom/EOS/DefaultEngine.ini 文件中，你应该将bEnabled布尔值设置为 false。

	[OnlineSubsystemEOS]
	bEnabled=false

### 基本模块 OnlineSubsystem 定义服务指定的模块，并在引擎中进行注册。在初始化期间，在线子系统将尝试加载"Engine.ini"文件中指定的默认在线服务模块。对在线服务的所有访问都将通过此模块。

	[OnlineSubsystem]
	DefaultPlatformService=<Default Platform Identifier>

### ，你可以在创建打包版本时使用LyraGameEOS目标，或将以下选项添加到你的命令行：

	 -customconfig=EOS

