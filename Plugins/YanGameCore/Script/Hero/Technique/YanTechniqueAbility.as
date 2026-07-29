/**
 * UYanTechniqueAbility — 术式技能基类。
 *
 * 术式的身份由技能自己声明，不在别处另立清单：
 *   - TechniqueTag 标识该术式，按 Ability.CursedTechnique.<角色|Base>.<术式> 命名
 *   - DisplayName 供轮盘显示
 *
 * UYanTechniqueComponent 遍历 ASC 时据此识别术式、生成轮盘选项，
 * 因此凡是应当出现在术式轮盘上的技能都应继承本类。
 *
 * 之所以不复用技能资产自带的 AssetTags：引擎未给该属性标注 BlueprintReadOnly，
 * 脚本只能在 default 语句中访问，运行时读不到。两者请填一致的值。
 */
class UYanTechniqueAbility : UYanGameplayAbility
{
	/** 术式标识，熔断与修复按它寻址 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technique", Meta = (Categories = "Ability.CursedTechnique"))
	FGameplayTag TechniqueTag;

	/** 轮盘上显示的名称，如"苍""赫""领域展开" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technique")
	FText DisplayName;
}
