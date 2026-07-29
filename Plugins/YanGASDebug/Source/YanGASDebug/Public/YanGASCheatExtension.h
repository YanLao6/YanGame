// Copyright YanGame.

#pragma once

#include "GameFramework/CheatManager.h"
#include "YanGASCheatExtension.generated.h"

class UAbilitySystemComponent;
struct FGameplayAttribute;

/**
 * GAS 运行时调试命令扩展。
 *
 * 通过控制台读写当前控制 Pawn 的 Attribute、增删 GameplayTag。
 * 由 FYanGASDebugModule 自动挂载到 CheatManager，仅非 Shipping 生效。
 * 命令统一以 "Gas" 前缀，便于在控制台自动补全中检索。
 */
UCLASS(NotBlueprintable)
class YANGASDEBUG_API UYanGASCheatExtension : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	/** 将匹配 AttributeName 的 Attribute 的 BaseValue 直接设为 Value（精确名优先，其次子串匹配）。 */
	UFUNCTION(Exec)
	void SetAttribute(const FString& AttributeName, float Value);

	/** 在匹配 Attribute 的当前 BaseValue 上叠加 Delta。 */
	UFUNCTION(Exec)
	void AddAttribute(const FString& AttributeName, float Delta);

	/**
	 * 运行时构造一个 Instant GameplayEffect 施加到自身，走完整 GE 执行管线
	 * （Pre/PostGameplayEffectExecute、ApplicationRequirements、immunity 等）。
	 * @param AttributeName 目标 Attribute（精确名优先，其次子串匹配）
	 * @param Op            修改方式："Add" -> AddBase，"Override" -> Override
	 * @param Magnitude     修改量
	 */
	UFUNCTION(Exec)
	void ApplyGameplayEffect(const FString& AttributeName, const FString& Op, float Magnitude);

	/** 为当前 ASC 添加 GameplayTag（优先走复制安全的动态 GE，失败回退 LooseTag）。 */
	UFUNCTION(Exec)
	void AddTag(const FString& TagName);

	/** 移除由 GasAddTag 添加的 GameplayTag。 */
	UFUNCTION(Exec)
	void RemoveTag(const FString& TagName);

	/** 切换 showdebug AbilitySystem 显示并翻到下一分类。 */
	UFUNCTION(Exec)
	void CycleDebug();

private:
	// 解析当前控制 Pawn（或 PlayerState）上的 ASC；找不到时输出提示并返回 nullptr。
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;

	// 在 ASC 全部 Attribute 中按名查找：先精确匹配，再大小写不敏感子串匹配。
	static bool FindAttribute(UAbilitySystemComponent* AbilitySystemComponent, const FString& AttributeName, FGameplayAttribute& OutAttribute);

	// 向控制台与日志同时输出一行调试信息。
	void Report(const FString& Message) const;
};
