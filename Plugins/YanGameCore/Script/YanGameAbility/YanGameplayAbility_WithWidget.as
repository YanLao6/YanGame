/**
 *
 */
class UYanGameplayAbility_WithWidget : UYanGameplayAbility
{
	/** 随本技能显示的控件类，按下标与 WidgetExtensionPointTags 配对 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability HUD Widget")
	TArray<TSubclassOf<UUserWidget>> WidgetClasses;

	/** 控件在 HUD 布局中的落点标签，按下标与 WidgetClasses 配对 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability HUD Widget", Meta = (Categories = "HUD.Slot"))
	TArray<FGameplayTag> WidgetExtensionPointTags;

	// 已注册的扩展句柄，移除技能时据此撤销
	private TArray<FUIExtensionHandle> WidgetExtensionHandles;

	//~Begin UModularGameplayAbility Interface
	UFUNCTION(BlueprintOverride)
	void OnAbilityAdded()
	{
		UUIExtensionSubsystem ExtensionSubsystem = UUIExtensionSubsystem::Get();
		if (ExtensionSubsystem == nullptr)
		{
			return;
		}

		AActor ContextActor = GetOwningActorFromActorInfo();

		for (int Index = 0; Index < WidgetClasses.Num(); ++Index)
		{
			if (!WidgetExtensionPointTags.IsValidIndex(Index))
			{
				break;
			}

			FUIExtensionHandle Handle = ExtensionSubsystem.RegisterExtensionAsWidgetForContext(
				WidgetExtensionPointTags[Index],
				WidgetClasses[Index],
				ContextActor);

			WidgetExtensionHandles.Add(Handle);
		}
	}

	UFUNCTION(BlueprintOverride)
	void OnAbilityRemoved()
	{
		for (int Index = 0; Index < WidgetExtensionHandles.Num(); ++Index)
		{
			UUIExtensionHandleFunctions::Unregister(WidgetExtensionHandles[Index]);
		}

		WidgetExtensionHandles.Empty();
	}
	//~End UModularGameplayAbility Interface
}
