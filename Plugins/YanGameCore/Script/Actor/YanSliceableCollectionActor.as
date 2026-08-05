/**
 * AYanSliceableCollectionActor —— 按指定角度沿平面断簇的可斩击破碎体。
 *
 * 与 AYanCuttableActor 的运行时布尔切割不同，这里不改动任何几何数据：斩击表现为
 * 「沿一个平面把附近的簇从父簇上释放出来」，靠 Chaos 的 ExternalClusterStrain 场完成。
 * 代价只有一次场命令入队，没有网格布尔运算、没有 PhysicsProxy 重建，因此可以高频触发。
 *
 * 相应的限制：断口只能落在资产预先 Fracture 出来的碎块边界上，切面角度越偏离预设裂纹，
 * 断口越像「掰开」而不是「切开」。适合杂兵与场景道具；需要精确切面的关键演出仍走 AYanCuttableActor。
 *
 * 蓝图需设置：
 *   - GeometryCollectionComponent.RestCollection  → 已 Fracture 的破碎资产，碎块越密切口越贴合
 *   - GeometryCollectionComponent.DamageThreshold → 必须低于 SliceStrain，否则场打上去也不会断
 */
class AYanSliceableCollectionActor : AGeometryCollectionActor
{
	// 打开碰撞事件；该标记在 PhysicsProxy 创建时被读入模拟参数，只能在此设默认值
	default GeometryCollectionComponent.bNotifyCollisions = true;

	/**
	 * 斩击角度（度）：绕撞击方向旋转刀刃。
	 * 0 = 水平斩（切面水平），90 = 竖直斩（切面竖直），45 = 斜斩。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	float SliceAngleDegrees = 0.0f;

	/** 切割带的半厚度（cm）：切面两侧各这么厚的范围内，簇会被断开 */
	UPROPERTY(EditAnywhere, Category = "Slice", meta = (ClampMin = "0.1"))
	float SliceHalfThickness = 15.0f;

	/**
	 * 注入带内的外部应变；须大于碎块的 DamageThreshold 才会断开。
	 * 不要给天文数字：超出阈值的部分会按 BreakDamagePropagationFactor 扩散到邻块。
	 */
	UPROPERTY(EditAnywhere, Category = "Slice", meta = (ClampMin = "0"))
	float SliceStrain = 100000.0f;

	/**
	 * 断裂沿连接图向邻块传播的比例，取值 0~1。
	 * 引擎默认 1.0 会把「超出阈值的应变」全额传给邻居，一次斩击连锁扩散成整体碎裂；
	 * 0 表示只断切割带内的簇。
	 */
	UPROPERTY(EditAnywhere, Category = "Slice", meta = (ClampMin = "0", ClampMax = "1"))
	float BreakDamagePropagationFactor = 0.0f;

	/**
	 * 关闭「碰撞冲量直接造成断裂」。
	 * 释放判定取的是碰撞冲量与外部应变的较大者，碰撞伤害开着时撞击本身就会断开一大片，
	 * 切割带形同虚设。关掉后唯一的断裂来源就是本类施加的平面场。
	 */
	UPROPERTY(EditAnywhere, Category = "Slice")
	bool bDisableCollisionDamage = true;

	/**
	 * 断簇前把撞击点附近的粒子转为动态的半径（cm），使碎块能掉落。
	 * 仅 Kinematic / Static 的破碎体需要；该场会一并把顶层簇转为动态，
	 * 半径过大会让整个物体一起掉下来。<= 0 表示跳过。
	 */
	UPROPERTY(EditAnywhere, Category = "Slice", meta = (ClampMin = "0"))
	float ReleaseRadius = 0.0f;

	/** 触发斩击所需的最小撞击冲量，用于滤掉轻微磕碰 */
	UPROPERTY(EditAnywhere, Category = "Slice|Impact", meta = (ClampMin = "0"))
	float ImpactImpulseThreshold = 200.0f;

	/** 最多被斩击几次；<= 0 表示不限次数 */
	UPROPERTY(EditAnywhere, Category = "Slice|Impact")
	int MaxSliceCount = 1;

	/** 画出切割带的实际范围，用于确认厚度与角度是否符合预期 */
	UPROPERTY(EditAnywhere, Category = "Slice|Debug")
	bool bDrawDebugSlice = false;

	/** 调试盒沿切面方向的半边长（cm） */
	UPROPERTY(EditAnywhere, Category = "Slice|Debug", meta = (EditCondition = "bDrawDebugSlice"))
	float DebugSliceExtent = 100.0f;

	private int SliceCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UGeometryCollectionComponent Collection = GeometryCollectionComponent;

		// 资产默认全额传播断裂，一处断开会沿连接图连锁扩散，斩击会变成整体炸开
		FGeometryCollectionDamagePropagationData PropagationData;
		PropagationData.bEnabled                     = BreakDamagePropagationFactor > 0.0f;
		PropagationData.BreakDamagePropagationFactor = BreakDamagePropagationFactor;
		PropagationData.ShockDamagePropagationFactor = 0.0f;
		Collection.SetDamagePropagationData(PropagationData);

		if (bDisableCollisionDamage)
		{
			Collection.SetEnableDamageFromCollision(false);
		}

		Collection.OnChaosPhysicsCollision.AddUFunction(this, n"HandlePhysicsCollision");
	}

	/**
	 * 以 ImpactDirection 为轴、按 AngleDegrees 决定刀刃朝向，在 WorldImpactPoint 处斩击。
	 * ImpactDirection 为撞击者的行进方向，无需归一化。
	 */
	UFUNCTION(BlueprintCallable, Category = "Slice")
	void SliceByAngle(FVector WorldImpactPoint, FVector ImpactDirection, float AngleDegrees)
	{
		SliceByPlane(WorldImpactPoint, ComputeSliceNormal(ImpactDirection, AngleDegrees));
	}

	/** 沿世界空间平面斩击；WorldNormal 为切面法线，无需归一化 */
	UFUNCTION(BlueprintCallable, Category = "Slice")
	void SliceByPlane(FVector WorldOrigin, FVector WorldNormal)
	{
		if (MaxSliceCount > 0 && SliceCount >= MaxSliceCount)
		{
			return;
		}

		const FVector PlaneNormal = WorldNormal.GetSafeNormal();
		if (PlaneNormal.IsNearlyZero())
		{
			Log("AYanSliceableCollectionActor: 切面法线为零向量，已忽略本次斩击");
			return;
		}

		UGeometryCollectionComponent Collection = GeometryCollectionComponent;

		// 静止的簇断开后仍不会掉落，须先把撞击点附近的粒子切到 Dynamic。
		// 该场是整型的 DynamicState 场，只有球形版本，与下面的平面应变场是两条独立命令
		if (ReleaseRadius > 0.0f)
		{
			Collection.ApplyKinematicField(ReleaseRadius, WorldOrigin);
		}

		// PlaneFalloff 只覆盖法线负侧的 Distance 范围，因此把场中心沿 +N 推半个厚度、
		// 距离取整个厚度，得到以切面为中心、两侧对称的切割带；
		// 用 None 衰减使带内应变均匀，避免边缘的簇只断一半而呈现撕裂感
		UPlaneFalloff SliceField = Cast<UPlaneFalloff>(NewObject(this, UPlaneFalloff));
		SliceField.SetPlaneFalloff(SliceStrain, 0.0f, 1.0f, 0.0f,
			SliceHalfThickness * 2.0f,
			WorldOrigin + PlaneNormal * SliceHalfThickness,
			PlaneNormal,
			EFieldFalloffType::Field_FallOff_None);

		Collection.ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
			nullptr, SliceField);

		if (bDrawDebugSlice)
		{
			// 盒子的 X 轴对齐切面法线，厚度即场覆盖的范围
			System::DrawDebugBox(WorldOrigin,
				FVector(SliceHalfThickness, DebugSliceExtent, DebugSliceExtent),
				FLinearColor::Red, PlaneNormal.ToOrientationRotator(), 5.0f, 1.0f);
		}

		SliceCount += 1;
		OnSliced(WorldOrigin, PlaneNormal);
	}

	/** 每次成功斩击后触发；蓝图可在此播刀光、切口粒子与音效 */
	UFUNCTION(BlueprintEvent, Category = "Slice")
	void OnSliced(FVector SliceOrigin, FVector SliceNormal)
	{
	}

	UFUNCTION()
	private void HandlePhysicsCollision(const FChaosPhysicsCollisionInfo&in CollisionInfo)
	{
		if (CollisionInfo.AccumulatedImpulse.Size() < ImpactImpulseThreshold)
		{
			return;
		}

		// 撞击者的速度即入射方向；静止物体压上来时退化为沿接触法线切入
		FVector ImpactDirection = CollisionInfo.OtherVelocity;
		if (ImpactDirection.IsNearlyZero())
		{
			ImpactDirection = -CollisionInfo.Normal;
		}

		SliceByAngle(CollisionInfo.Location, ImpactDirection, SliceAngleDegrees);
	}

	// 刀刃在垂直于入射方向的平面内按角度旋转，切面法线同时垂直于刀刃与入射方向
	private FVector ComputeSliceNormal(FVector ImpactDirection, float AngleDegrees) const
	{
		const FVector Forward = ImpactDirection.GetSafeNormal();

		// 正对上下方向撞击时 Forward 与世界 Up 共线，改用前向轴建立参考系
		FVector Right = Forward.CrossProduct(FVector::UpVector);
		if (Right.IsNearlyZero())
		{
			Right = Forward.CrossProduct(FVector::ForwardVector);
		}
		Right = Right.GetSafeNormal();

		const FVector Up      = Right.CrossProduct(Forward);
		const float   Radians = Math::DegreesToRadians(AngleDegrees);
		const FVector Blade   = Right * Math::Cos(Radians) + Up * Math::Sin(Radians);

		return Blade.CrossProduct(Forward).GetSafeNormal();
	}
}
