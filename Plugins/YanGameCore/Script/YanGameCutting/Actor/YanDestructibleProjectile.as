/**
 * AYanDestructibleProjectile —— 以破碎体（GeometryCollection）自身作为弹体的投射物。
 *
 * 弹体飞行、撞击、推动其他刚体、按 DamageThreshold 碎裂，全部由 Chaos 求解器直接产出，
 * 因此不需要 ProjectileMovementComponent，也不需要额外的 Sweep 命中检测。
 *
 * ObjectType、初速度、CCD 只在 PhysicsProxy 初始化的那一刻被读入 SimulationParameters，
 * 之后改属性对已有 proxy 无效，因此 Launch() 的做法是：先写好这些参数，再 ResetState()
 * 重建物理状态——粒子在重建时就带着初速度出生，不需要等物理帧再补一次速度。
 *
 * 蓝图需设置：
 *   - GeometryCollectionComponent.RestCollection  → 破碎资产（决定外形与碎块层级）
 *   - GeometryCollectionComponent.DamageThreshold → 碎裂难度；过高则撞击不碎
 */
class AYanDestructibleProjectile : AGeometryCollectionActor
{
	// 未发射时保持运动学状态，停在 Spawn 位置不自由落体；
	// 打开 bNotifyCollisions 才会给碰撞形状加上 ContactNotify 标记并派发碰撞事件
	default GeometryCollectionComponent.ObjectType = EObjectStateTypeEnum::Chaos_Object_Kinematic;
	default GeometryCollectionComponent.bNotifyCollisions = true;

	/** 发射后存活时间（秒），到期自毁；<= 0 表示不自动销毁 */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float LifeSpanAfterLaunch = 8.0f;

	/** 发射后是否受重力影响；false 为直线飞行 */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	bool bUseGravity = true;

	/** 绕飞行方向的自旋角速度（度/秒），0 表示不自旋 */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float SpinDegreesPerSecond = 0.0f;

	/** 撞击冲量的模长超过此值才算有效撞击 */
	UPROPERTY(EditAnywhere, Category = "Projectile|Impact")
	float ImpactImpulseThreshold = 200.0f;

	/** 有效撞击时是否让弹体自身碎开 */
	UPROPERTY(EditAnywhere, Category = "Projectile|Impact")
	bool bShatterOnImpact = true;

	/** 撞击点注入的外部应变；超过碎块 DamageThreshold 的簇会断开 */
	UPROPERTY(EditAnywhere, Category = "Projectile|Impact", meta = (EditCondition = "bShatterOnImpact"))
	float ImpactStrain = 5000.0f;

	/** 应变的作用半径（cm）；0 表示只作用于命中的那一块 */
	UPROPERTY(EditAnywhere, Category = "Projectile|Impact", meta = (EditCondition = "bShatterOnImpact"))
	float ImpactStrainRadius = 50.0f;

	private bool bLaunched = false;
	private bool bImpacted = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		if (!GeometryCollectionComponent.bNotifyCollisions)
		{
			Log("AYanDestructibleProjectile: 蓝图覆盖了 Notify Collisions（当前为关），撞击回调不会触发。请在组件详情面板 ChaosPhysics|Events 下勾选 Notify Collisions");
		}

		GeometryCollectionComponent.OnChaosPhysicsCollision.AddUFunction(this, n"HandlePhysicsCollision");
	}

	/** 沿 Direction 以 Speed（cm/s）发射；Direction 无需预先归一化。只会生效一次 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Launch(FVector Direction, float Speed)
	{
		if (bLaunched)
		{
			return;
		}

		const FVector LaunchDirection = Direction.GetSafeNormal();
		if (LaunchDirection.IsNearlyZero())
		{
			Log("AYanDestructibleProjectile: 发射方向为零向量，已忽略本次 Launch");
			return;
		}

		bLaunched = true;

		UGeometryCollectionComponent Collection = GeometryCollectionComponent;

		// 以下参数全部在物理状态创建时才被读取，因此先写属性、最后统一重建
		Collection.ObjectType            = EObjectStateTypeEnum::Chaos_Object_Dynamic;
		Collection.InitialVelocityType   = EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined;
		Collection.InitialLinearVelocity = LaunchDirection * Speed;

		if (SpinDegreesPerSecond != 0.0f)
		{
			// Chaos 的角速度单位为 rad/s
			Collection.InitialAngularVelocity = LaunchDirection * Math::DegreesToRadians(SpinDegreesPerSecond);
		}

		// 高速弹体易穿透薄壁
		Collection.SetUseCCD(true);
		Collection.SetEnableGravity(bUseGravity);

		// 重建 PhysicsProxy：粒子在此刻按上面的参数出生，直接带着初速度飞出
		Collection.ResetState();

		if (LifeSpanAfterLaunch > 0.0f)
		{
			SetLifeSpan(LifeSpanAfterLaunch);
		}
	}

	/** 有效撞击时触发；蓝图可在此播特效、施加伤害 */
	UFUNCTION(BlueprintEvent, Category = "Projectile")
	void OnProjectileImpact(UPrimitiveComponent HitComponent, FVector ImpactLocation, FVector ImpactNormal)
	{
	}

	UFUNCTION()
	private void HandlePhysicsCollision(const FChaosPhysicsCollisionInfo&in CollisionInfo)
	{
		// 只处理发射后的首次有效撞击：碎裂与命中反馈都应当只发生一次
		if (!bLaunched || bImpacted)
		{
			return;
		}

		if (CollisionInfo.AccumulatedImpulse.Size() < ImpactImpulseThreshold)
		{
			return;
		}

		bImpacted = true;

		if (bShatterOnImpact)
		{
			UGeometryCollectionComponent Collection = GeometryCollectionComponent;
			Collection.ApplyExternalStrain(Collection.GetRootIndex(), CollisionInfo.Location,
				ImpactStrainRadius, 1, 1.0f, ImpactStrain);
		}

		OnProjectileImpact(CollisionInfo.OtherComponent, CollisionInfo.Location, CollisionInfo.Normal);
	}
}
