#include "Abilities/YanGameplayAbility_Slash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Messages/VerbMessageHelpers.h"
#include "Weapon/WeaponAbilityTargetData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameplayAbility_Slash)

namespace SlashConsoleVariables
{
	static float                   DrawFanTraceDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawFanTraceDuration(
		TEXT("Slash.DrawFanTraceDuration"),
		DrawFanTraceDuration,
		TEXT("绘制斩击扇区采样射线的持续时间，单位为秒；大于零时启用。"),
		ECVF_Default);

	static float                   DrawFieldBoxDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawFieldBoxDuration(
		TEXT("Slash.DrawFieldBoxDuration"),
		DrawFieldBoxDuration,
		TEXT("绘制斩击破坏场盒线框的持续时间，单位为秒；大于零时启用。"),
		ECVF_Default);
}

//////////////////////////////////////////////////////////////////////

// 命中目标是否为 Pawn；挂在 Pawn 身上的 Actor 同样视为命中该 Pawn
static bool IsPawnHitResult(const FHitResult& Hit)
{
	if (Hit.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
	{
		return true;
	}

	const AActor* HitActor = Hit.HitObjectHandle.FetchActor();
	return (HitActor != nullptr) && (Cast<APawn>(HitActor->GetAttachParentActor()) != nullptr);
}

// Destructible 是引擎内置的 ObjectType 通道，只能从命中组件的碰撞对象类型判断
static bool IsDestructibleHitResult(const FHitResult& Hit)
{
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	return (HitComponent != nullptr) && (HitComponent->GetCollisionObjectType() == ECC_Destructible);
}

// 按 Actor 去重后追加命中结果，避免同一目标被多条射线重复结算
static void AddUniqueHitResult(TArray<FHitResult>& OutHits, const FHitResult& Hit)
{
	auto Pred = [&Hit](const FHitResult& Other)
	{
		return Other.HitObjectHandle == Hit.HitObjectHandle;
	};

	if (!OutHits.ContainsByPredicate(Pred))
	{
		OutHits.Add(Hit);
	}
}

//////////////////////////////////////////////////////////////////////

UYanGameplayAbility_Slash::UYanGameplayAbility_Slash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UYanGameplayAbility_Slash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 能力实例会被复用，每次激活都要清空上一次蓄力的残留状态
	bSlashReleased    = false;
	ChargeElapsedTime = 0.0f;
	SlashGeometry     = FSlashAbilityTargetData_Geometry();
	RotationSamples.Reset();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StartRotationSampling();
}

void UYanGameplayAbility_Slash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRotationSampling();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UYanGameplayAbility_Slash::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	ReleaseSlash();
}

void UYanGameplayAbility_Slash::ReleaseSlash()
{
	if (bSlashReleased)
	{
		return;
	}
	bSlashReleased = true;

	StopRotationSampling();

	// 瞄准解算依赖本地视角采样，远端只能等待复制过来的 TargetData
	if ((CurrentActorInfo != nullptr) && CurrentActorInfo->IsLocallyControlled())
	{
		StartSlashTargeting();
	}
}

void UYanGameplayAbility_Slash::StartSlashTargeting()
{
	TArray<FHitResult> FoundHits;
	PerformLocalTargeting(FoundHits);

	FGameplayAbilityTargetDataHandle TargetData;

	// 几何参数整次斩击只发一份，排在命中项之前，权威端遍历时先还原再结算
	TargetData.Add(new FSlashAbilityTargetData_Geometry(SlashGeometry));

	// 一次斩击的所有命中同属一批，共享 CartridgeID 供表现层合并
	const int32 CartridgeID = FMath::Rand();
	for (const FHitResult& FoundHit : FoundHits)
	{
		FWeaponAbilityTargetData_SingleTargetHit* NewTargetData = new FWeaponAbilityTargetData_SingleTargetHit();
		NewTargetData->HitResult                                = FoundHit;
		NewTargetData->CartridgeID                              = CartridgeID;

		TargetData.Add(NewTargetData);
	}

	OnTargetDataReadyCallback(TargetData, FGameplayTag());
}

float UYanGameplayAbility_Slash::GetChargeAlpha() const
{
	return FMath::Clamp(ChargeElapsedTime / FMath::Max(MaxChargeTime, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////
// 视角采样

void UYanGameplayAbility_Slash::StartRotationSampling()
{
	StopRotationSampling();

	if (const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (const AController* Controller = AvatarPawn->GetController())
		{
			LastControlRotation = Controller->GetControlRotation();
		}
	}

	RotationSampleTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickSampleRotation));
}

void UYanGameplayAbility_Slash::StopRotationSampling()
{
	if (RotationSampleTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RotationSampleTickerHandle);
		RotationSampleTickerHandle.Reset();
	}
}

bool UYanGameplayAbility_Slash::TickSampleRotation(float DeltaTime)
{
	// 蓄力计时不依赖控制器是否可用，否则控制器短暂缺失会让蓄力时长偏小
	ChargeElapsedTime += DeltaTime;

	const APawn*       AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = (AvatarPawn != nullptr) ? AvatarPawn->GetController() : nullptr;
	if (Controller == nullptr)
	{
		return true;
	}

	const FRotator CurrentRotation = Controller->GetControlRotation();
	const FRotator DeltaRotation   = (CurrentRotation - LastControlRotation).GetNormalized();
	LastControlRotation            = CurrentRotation;

	FSlashRotationSample& Sample = RotationSamples.AddDefaulted_GetRef();
	Sample.TimeSeconds           = ChargeElapsedTime;
	// Yaw 增量对应屏幕向右，Pitch 增量对应屏幕向上
	Sample.Delta                 = FVector2D(DeltaRotation.Yaw, DeltaRotation.Pitch);

	// 窗口外的样本不再参与解算，长时间蓄力也不会让数组无限增长
	const float CutoffTime = ChargeElapsedTime - DirectionSampleWindow;
	RotationSamples.RemoveAll([CutoffTime](const FSlashRotationSample& Other)
	{
		return Other.TimeSeconds < CutoffTime;
	});

	return true;
}

FVector2D UYanGameplayAbility_Slash::ComputeSampledSlashDirection() const
{
	if (RotationSamples.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}

	const float WindowEnd  = RotationSamples.Last().TimeSeconds;
	const float SafeWindow = FMath::Max(DirectionSampleWindow, UE_KINDA_SMALL_NUMBER);

	FVector2D Accumulated = FVector2D::ZeroVector;
	for (const FSlashRotationSample& Sample : RotationSamples)
	{
		// 越接近松手的样本权重越高，蓄力初期的瞄准微调不应影响挥砍朝向
		const float Weight = FMath::Max(1.0f - (WindowEnd - Sample.TimeSeconds) / SafeWindow, 0.0f);
		Accumulated        += Sample.Delta * Weight;
	}

	if (Accumulated.Size() < MinSlashInputMagnitude)
	{
		return FVector2D::ZeroVector;
	}

	return Accumulated.GetSafeNormal();
}

//////////////////////////////////////////////////////////////////////
// 瞄准解算

void UYanGameplayAbility_Slash::PerformLocalTargeting(TArray<FHitResult>& OutHits)
{
	APawn* const AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if ((AvatarPawn == nullptr) || !AvatarPawn->IsLocallyControlled())
	{
		return;
	}

	const FTransform TargetTransform = GetTargetingTransform(AvatarPawn, EWeaponTargetingSource::CameraTowardsFocus);
	const FVector    AimDir          = TargetTransform.GetUnitAxis(EAxis::X);
	const FVector    CameraRight     = TargetTransform.GetUnitAxis(EAxis::Y);
	const FVector    CameraUp        = TargetTransform.GetUnitAxis(EAxis::Z);

	// 屏幕空间的挥砍向量投影回世界空间；两个基向量都垂直于视线，因此结果与视线严格正交
	const FVector2D ScreenSlashDir = ComputeSampledSlashDirection();
	FVector         SlashDir       = (CameraRight * ScreenSlashDir.X + CameraUp * ScreenSlashDir.Y).GetSafeNormal();
	if (SlashDir.IsNearlyZero())
	{
		// 玩家没有主动挥动视角时退化为水平横斩
		SlashDir = CameraRight;
	}

	SlashGeometry.SlashOrigin      = TargetTransform.GetTranslation();
	SlashGeometry.AimDirection     = AimDir;
	// 该法线同时是破坏场盒子的厚度轴；叉乘顺序保证盒子的宽度轴与 SlashDir 同向
	SlashGeometry.SlashPlaneNormal = FVector::CrossProduct(AimDir, SlashDir).GetSafeNormal();
	// 采样已在松手时停止，此处读到的就是本次蓄力的最终进度
	SlashGeometry.SetChargeAlpha(GetChargeAlpha());

	TraceSlashFan(SlashGeometry.SlashOrigin, AimDir, SlashDir, OutHits);
}

void UYanGameplayAbility_Slash::TraceSlashFan(const FVector& Origin, const FVector& AimDir, const FVector& SlashDir, TArray<FHitResult>& OutHits)
{
	const float NearHalfWidth = NearSlashWidth * 0.5f;
	const float FarHalfWidth  = FarSlashWidth * 0.5f;
	const int32 RayCount      = FMath::Max(FanTraceCount, 1);

	for (int32 RayIndex = 0; RayIndex < RayCount; ++RayIndex)
	{
		// 单条射线时取扇区中轴，多条时在 [-1, 1] 上均匀铺开
		const float Alpha = (RayCount > 1) ? ((static_cast<float>(RayIndex) / static_cast<float>(RayCount - 1)) * 2.0f - 1.0f) : 0.0f;

		// 近端与远端各自按宽度铺开，扇区因此是梯形而非从一点发散的三角形
		const FVector RayStart = Origin + SlashDir * (NearHalfWidth * Alpha);
		const FVector RayEnd   = Origin + AimDir * MaxSlashRange + SlashDir * (FarHalfWidth * Alpha);

#if ENABLE_DRAW_DEBUG
		if (SlashConsoleVariables::DrawFanTraceDuration > 0.0f)
		{
			DrawDebugLine(GetWorld(), RayStart, RayEnd, FColor::Cyan, false, SlashConsoleVariables::DrawFanTraceDuration, 0, 1.0f);
		}
#endif

		TraceSingleSlashRay(RayStart, RayEnd, OutHits);
	}
}

void UYanGameplayAbility_Slash::TraceSingleSlashRay(const FVector& RayStart, const FVector& RayEnd, TArray<FHitResult>& OutHits)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TArray<AActor*> PenetratedPawns;

	for (int32 Step = 0; Step <= MaxPawnPenetrations; ++Step)
	{
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SlashTrace), /*bTraceComplex=*/ true, /*IgnoreActor=*/ GetAvatarActorFromActorInfo());
		TraceParams.bReturnPhysicalMaterial = true;
		AddAdditionalTraceIgnoreActors(TraceParams);
		// 已结算过的 Pawn 不再阻挡，斩击得以继续向后延伸
		TraceParams.AddIgnoredActors(PenetratedPawns);

		const ECollisionChannel TraceChannel = DetermineTraceChannel(TraceParams, /*bIsSimulated=*/ false);

		TArray<FHitResult> StepHits;
		World->LineTraceMultiByChannel(StepHits, RayStart, RayEnd, TraceChannel, TraceParams);

		AActor* PawnToPenetrate = nullptr;
		for (const FHitResult& Hit : StepHits)
		{
			AddUniqueHitResult(OutHits, Hit);

			if (IsDestructibleHitResult(Hit))
			{
				// 斩击止于第一个可破坏物
				return;
			}

			if (Hit.bBlockingHit)
			{
				if (IsPawnHitResult(Hit))
				{
					PawnToPenetrate = Hit.GetActor();
				}
				// 多重检测的结果以阻挡命中收尾，其后不会再有目标
				break;
			}
		}

		if (PawnToPenetrate == nullptr)
		{
			return;
		}
		PenetratedPawns.Add(PawnToPenetrate);
	}
}

//////////////////////////////////////////////////////////////////////
// 结算

void UYanGameplayAbility_Slash::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	// 父类会取走 InData 的所有权，命中结果必须在此之前拷贝出来
	TArray<FHitResult> SlashHits;
	SlashHits.Reserve(InData.Num());
	for (int32 Index = 0; Index < InData.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* TargetData = InData.Get(Index);
		if (TargetData == nullptr)
		{
			continue;
		}

		// 权威端没有本地采样，几何基准只能取自复制过来的这一项
		if (TargetData->GetScriptStruct() == FSlashAbilityTargetData_Geometry::StaticStruct())
		{
			SlashGeometry = *static_cast<const FSlashAbilityTargetData_Geometry*>(TargetData);
			continue;
		}

		if (const FHitResult* Hit = TargetData->GetHitResult())
		{
			SlashHits.Add(*Hit);
		}
	}

	Super::OnTargetDataReadyCallback(InData, ApplicationTag);

	// 父类提交失败时能力已经结束，不应再产生任何斩击效果
	if (!IsActive())
	{
		return;
	}

	// 伤害与破坏场同归权威端；客户端的碎裂由 GeometryCollection 自身的复制驱动
	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyDamageToPawnHits(SlashHits);
		SpawnSlashFields(SlashHits);
	}

	K2_EndAbility();
}

void UYanGameplayAbility_Slash::ApplyDamageToPawnHits(const TArray<FHitResult>& Hits)
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if ((SlashDamageEffect == nullptr) || (SourceASC == nullptr))
	{
		return;
	}

	TSet<AActor*> DamagedActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if ((HitActor == nullptr) || !IsPawnHitResult(Hit))
		{
			continue;
		}

		bool bAlreadyDamaged = false;
		DamagedActors.Add(HitActor, &bAlreadyDamaged);
		if (bAlreadyDamaged)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(HitActor);
		if (TargetASC == nullptr)
		{
			continue;
		}

		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(SlashDamageEffect, GetAbilityLevel());
		if (!SpecHandle.IsValid())
		{
			continue;
		}

		// 命中信息写入 Context，供伤害 Execution 读取距离衰减与物理材质
		SpecHandle.Data->GetContext().AddHitResult(Hit);
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
}

void UYanGameplayAbility_Slash::SpawnSlashFields(const TArray<FHitResult>& Hits)
{
	if (SlashFieldActorClass == nullptr)
	{
		return;
	}

	const float SlashDepth = FMath::Lerp(MinSlashDepth, MaxSlashDepth, SlashGeometry.GetChargeAlpha());

	TSet<AActor*> FracturedActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if ((HitActor == nullptr) || !IsDestructibleHitResult(Hit))
		{
			continue;
		}

		bool bAlreadyFractured = false;
		FracturedActors.Add(HitActor, &bAlreadyFractured);
		if (bAlreadyFractured)
		{
			continue;
		}

		SpawnSlashFieldAt(Hit.ImpactPoint, SlashDepth);
	}
}

void UYanGameplayAbility_Slash::SpawnSlashFieldAt(const FVector& ImpactPoint, float SlashDepth)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 斩击越远越宽，宽度按命中点到起点的实际距离在近端与远端之间插值
	const float DistanceAlpha = FMath::Clamp(FVector::Dist(SlashGeometry.SlashOrigin, ImpactPoint) / FMath::Max(MaxSlashRange, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float SlashWidth    = FMath::Lerp(NearSlashWidth, FarSlashWidth, DistanceAlpha);

	// 盒子沿视线从命中面向内延伸，因此中心要推进半个深度
	const FVector  FieldLocation = ImpactPoint + SlashGeometry.AimDirection * (SlashDepth * 0.5f);
	// X 轴为深度、Z 轴为厚度，Y 轴由叉乘确定并与挥砍方向同向
	const FRotator FieldRotation = FRotationMatrix::MakeFromXZ(SlashGeometry.AimDirection, SlashGeometry.SlashPlaneNormal).Rotator();

	// 厚度轴保持原始尺寸，只缩放深度与宽度
	const FVector FieldScale(SlashDepth / FMath::Max(FieldBoxBaseSize.X, UE_KINDA_SMALL_NUMBER), SlashWidth / FMath::Max(FieldBoxBaseSize.Y, UE_KINDA_SMALL_NUMBER), 1.0f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner                          = GetAvatarActorFromActorInfo();
	SpawnParams.Instigator                     = Cast<APawn>(GetAvatarActorFromActorInfo());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform FieldTransform(FieldRotation, FieldLocation, FieldScale);
	AActor* const    SlashField = World->SpawnActor<AActor>(SlashFieldActorClass, FieldTransform, SpawnParams);

	// 场只需在断裂发生的瞬间生效，留存过久会持续扰动已经落地的碎块
	if ((SlashField != nullptr) && (SlashFieldLifeSpan > 0.0f))
	{
		SlashField->SetLifeSpan(SlashFieldLifeSpan);
	}

#if ENABLE_DRAW_DEBUG
	if (SlashConsoleVariables::DrawFieldBoxDuration > 0.0f)
	{
		const FVector DebugExtent = FieldBoxBaseSize * FieldScale * 0.5f;
		DrawDebugBox(World, FieldLocation, DebugExtent, FieldRotation.Quaternion(), FColor::Orange, false, SlashConsoleVariables::DrawFieldBoxDuration, 0, 2.0f);
	}
#endif
}
