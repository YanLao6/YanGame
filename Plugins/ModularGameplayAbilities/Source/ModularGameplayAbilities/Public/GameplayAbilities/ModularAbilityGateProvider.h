// 技能门禁条件提供方接口声明。

#pragma once

#include "UObject/Interface.h"

#include "ModularAbilityGateProvider.generated.h"

struct FGameplayTag;

/** 门禁查询的结果；提供方不认识该 Tag 时返回 NotHandled，交由后续提供方回答。 */
enum class EModularAbilityGateResult : uint8
{
	NotHandled,
	Satisfied,
	Blocked
};

/** 声明本接口的 UObject 壳。 */
UINTERFACE(MinimalAPI)
class UModularAbilityGateProvider : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * 解释某类门禁 Tag 当前是否满足的提供方。
 *
 * 门禁 Tag 写在 AbilitySpec 上随其复制，而「此刻是否满足」由本接口在激活时按需求值，
 * 而非预先写进 ASC 再逐事件维护——查询式因此没有需要刷新的中间状态，
 * 也就不存在漏掉某个生命周期入口导致结论停在旧值的失效模式。
 *
 * 服务器与客户端各自向本地提供方求值：客户端那次用于本地预测，
 * 服务器那次是真正的闸门，其数据必须取自权威状态而非客户端上报。
 *
 * @see UModularAbilitySystemComponent::RegisterAbilityGateProvider
 * @see UModularGameplayAbility::DoesAbilitySatisfySpecGate
 */
class IModularAbilityGateProvider
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * 查询某个门禁 Tag 当前是否满足。
	 *
	 * 实现须对不属于自己命名空间的 Tag 返回 NotHandled，使多个提供方可以共存于同一个 ASC。
	 * 本函数在每次技能激活尝试时被调用，实现应保持为纯查询且无副作用。
	 */
	virtual EModularAbilityGateResult QueryAbilityGate(FGameplayTag GateTag) const = 0;
};
