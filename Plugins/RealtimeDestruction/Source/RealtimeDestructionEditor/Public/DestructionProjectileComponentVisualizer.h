// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * DestructProjectileComponent 的可视化工具
 * 在编辑器视口中将 ToolShape 绘制为线框。
 */

class FDestructionProjectileComponentVisualizer : public FComponentVisualizer
{
public:
	/**
	 * 组件可视化
	 */
	virtual void DrawVisualization(
		const UActorComponent* Component,
		const FSceneView* View,
		FPrimitiveDrawInterface* PDI
	) override; 


private:
	/** 球体可视化 */
	void DrawSphere(
		const class UDestructionProjectileComponent* Component,
		FPrimitiveDrawInterface* PDI,
		const FLinearColor& Color
		);

	/** 圆柱体可视化 */
	void DrawCylinder(
		const class UDestructionProjectileComponent* Component,
		FPrimitiveDrawInterface* PDI,
		const FLinearColor& Color
	);

	void DrawDecalPreview(
		const class UDestructionProjectileComponent* Component,
		FPrimitiveDrawInterface* PDI,
		const FLinearColor& Color
	);
	
	
};
