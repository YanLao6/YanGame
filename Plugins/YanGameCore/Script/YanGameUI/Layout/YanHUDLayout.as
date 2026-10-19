/**
 * YanHUDLayout — 游戏内 HUD 根布局
 *
 * 职责：
 * - 由 GameFeatureAction_AddWidgets 压入 UI.Layer.Game，作为游戏内常驻 HUD 根。
 * - 继承 UModularGameplayLayout：基类在 NativeOnInitialized 注册 UI.Action.Escape，
 *   按下 ESC 时把 EscapeMenuClass 压入 UI.Layer.Menu。
 * - 展示大厅信息文本（Host/Join 成功并进入地图后有效）。
 *
 * UMG 布局需绑定（变量名需一致）：
 *   - UCommonTextBlock LobbyInfoText（可选，缺失时跳过刷新）
 *
 * Blueprint Details 设置：
 *   - EscapeMenuClass → W_YanGameMenu
 *   - InputConfig     → Game（菜单关闭后输入需交还游戏）
 */
class UYanHUDLayout : UModularGameplayLayout
{
}
