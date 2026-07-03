/**
 * YanPrimaryLayout — 根 UI 布局（PrimaryGameLayout）
 *
 * 职责：
 * - 作为 B_YanUIPolicy 的 LayoutClass 指定的根布局。
 * - 通过 RegisterLayer 将 UMG 中的 Stack 控件与 GameplayTag 绑定，
 *   供 PushWidgetToLayerStackAsync 等接口按 Tag 查找并压入 Widget。
 *
 * UMG 布局需绑定（变量名需与下方一致）：
 *   UCommonActivatableWidgetStack  MenuLayer     —— 对应 UI.Layer.Menu（前端菜单、Press Start、主菜单）
 *   UCommonActivatableWidgetStack  GameLayer     —— 对应 UI.Layer.Game（游戏内 HUD）
 *   UCommonActivatableWidgetStack  GameMenuLayer —— 对应 UI.Layer.GameMenu（游戏内暂停菜单）
 *   UCommonActivatableWidgetStack  ModalLayer    —— 对应 UI.Layer.Modal（确认弹窗等）
 *
 * 使用方式：
 *   在编辑器中创建继承本类的 UMG 蓝图（如 W_YanPrimaryLayout），
 *   拖入四个 CommonActivatableWidgetStack，按上方名称绑定，即可自动完成注册。
 */
class UYanPrimaryLayout : UPrimaryGameLayout
{
	// ── UMG 绑定 ──────────────────────────────────────────────────────────────

	/** 游戏内 HUD 层 */
	UPROPERTY(BindWidget)
	UCommonActivatableWidgetStack GameLayer_Stack;

	/** 游戏内菜单层：暂停菜单等 */
	UPROPERTY(BindWidget)
	UCommonActivatableWidgetStack GameMenu_Stack;

	/** 前端菜单层：PressStart、主菜单等前端 UI */
	UPROPERTY(BindWidget)
	UCommonActivatableWidgetStack Menu_Stack;

	/** 模态弹窗层：确认框、错误提示等 */
	UPROPERTY(BindWidget)
	UCommonActivatableWidgetStack Modal_Stack;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		// 将每个 Stack 控件注册到对应的 GameplayTag Layer
		// 必须在非设计时执行，避免编辑器预览时出错
		if (!IsDesignTime)
		{
			RegisterLayer(FGameplayTag::RequestGameplayTag(n"UI.Layer.Game"),     GameLayer_Stack);
			RegisterLayer(FGameplayTag::RequestGameplayTag(n"UI.Layer.GameMenu"), GameMenu_Stack);
			RegisterLayer(FGameplayTag::RequestGameplayTag(n"UI.Layer.Menu"),     Menu_Stack);
			RegisterLayer(FGameplayTag::RequestGameplayTag(n"UI.Layer.Modal"),    Modal_Stack);
		}
	}
}
