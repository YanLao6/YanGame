/**
 * UAS_RestoreBurnout — 修复熔断的术式，一次性的"修复机会"凭证。
 *
 * 本技能不被直接激活：它是否存在于 ASC 中，就代表玩家当前有没有修复机会。
 * 术式被熔断时授予给自己，修复成功后授予给对手，用掉即由服务器收回。
 * 借助 ActivatableAbilities 的复制，客户端轮盘无需额外同步即可知道机会有无。
 *
 * 修复动作本身在轮盘上以"修复 XX"的形式逐项展开，
 * 由 UAS_TechniqueWheel 选定后交给 UYanTechniqueComponent::ServerCommitRestore 落实。
 *
 * 须在 UYanTechniqueComponent 的术式清单中列为一条，
 * 其 AbilityTag 即组件上的 RestoreAbilityTag。
 */
class UAS_RestoreBurnout : UYanGameplayAbility
{
}
