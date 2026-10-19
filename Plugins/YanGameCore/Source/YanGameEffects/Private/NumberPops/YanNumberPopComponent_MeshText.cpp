#include "NumberPops/YanNumberPopComponent_MeshText.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NumberPops/YanDamagePopStyle.h"
#include "TimerManager.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanNumberPopComponent_MeshText)

UYanNumberPopComponent_MeshText::UYanNumberPopComponent_MeshText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ComponentLifespan = 1.0f;

	SignDigitParameterName           = FName(TEXT("+Or-"));
	ColorParameterName               = FName(TEXT("Color"));
	AnimationLifespanParameterName   = FName(TEXT("Animation Lifespan"));
	IsCriticalHitParameterName       = FName(TEXT("isCriticalHit?"));
	MoveToCameraParameterName        = FName(TEXT("MoveToCamera"));
	PositionParameterNames           = {TEXT("0a"), TEXT("1a"), TEXT("2a"), TEXT("3a"), TEXT("4a"), TEXT("5a"), TEXT("6a"), TEXT("7a"), TEXT("8a")};
	ScaleRotationAngleParameterNames = {TEXT("0b"), TEXT("1b"), TEXT("2b"), TEXT("3b"), TEXT("4b"), TEXT("5b"), TEXT("6b"), TEXT("7b"), TEXT("8b")};
	DurationParameterNames           = {TEXT("0c"), TEXT("1c"), TEXT("2c"), TEXT("3c"), TEXT("4c"), TEXT("5c"), TEXT("6c"), TEXT("7c"), TEXT("8c")};

	SpacingPercentageForOnes = 0.8f;

	DistanceFromCameraBeforeDoublingSize = 1024.0f;
	CriticalHitSizeMultiplier            = 1.7f;

	FontXSize = 10.920001f;
	FontYSize = 21.0f;

	NumberOfNumberRotations = 1.0f;
}

void UYanNumberPopComponent_MeshText::AddNumberPop(const FYanNumberPopRequest& NewRequest)
{
	// 非本地控制器直接丢弃，避免监听服务器主机上出现重复飘字
	if (const APlayerController* PC = GetController<APlayerController>())
	{
		if (!PC->IsLocalController())
		{
			return;
		}
	}

	FTempNumberPopInfo PreparedNumberInfo;

	// 拆出十进制各位
	{
		int32 LocalDamage = NewRequest.NumberToDisplay;
		PreparedNumberInfo.DamageNumberArray.Empty();

		if (LocalDamage == 0)
		{
			PreparedNumberInfo.DamageNumberArray.Insert(0, 0);
		}
		else
		{
			while (LocalDamage > 0)
			{
				PreparedNumberInfo.DamageNumberArray.Insert(LocalDamage % 10, 0);
				LocalDamage /= 10;
			}
		}

		// 首位留给正负号，实际取值由材质决定
		PreparedNumberInfo.DamageNumberArray.Insert(0, 0);
	}

	// 取池中组件或新建
	{
		UStaticMesh* MeshToUse = DetermineStaticMesh(NewRequest);
		if (MeshToUse == nullptr)
		{
			return;
		}

		FPooledNumberPopComponentList& ComponentPool = PooledComponentMap.FindOrAdd(MeshToUse);

		UStaticMeshComponent* ComponentToUse = nullptr;
		if (ComponentPool.Components.Num() > 0)
		{
			ComponentToUse = ComponentPool.Components.Pop();
		}
		else
		{
			ComponentToUse = NewObject<UStaticMeshComponent>(GetOwner());
			ComponentToUse->SetupAttachment(nullptr);
			ComponentToUse->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			ComponentToUse->SetStaticMesh(MeshToUse);

			// 供后处理按模板值排除飘字
			ComponentToUse->SetRenderCustomDepth(true);
			ComponentToUse->SetCustomDepthStencilValue(123);

			// 材质用世界位置偏移做动画，数字会远离原始包围盒，需放大包围盒避免被剔除
			ComponentToUse->SetBoundsScale(2000.0f);

			// 颜色与数字逐实例覆盖，必须用动态材质实例
			for (int32 MatIdx = 0; MatIdx < ComponentToUse->GetNumMaterials(); ++MatIdx)
			{
				ComponentToUse->CreateDynamicMaterialInstance(MatIdx);
			}
		}

		check(ComponentToUse);
		ComponentToUse->RegisterComponent();

		UWorld* LocalWorld = GetWorld();
		check(LocalWorld);
		LiveComponents.Emplace(ComponentToUse, &ComponentPool, LocalWorld->GetTimeSeconds() + ComponentLifespan);

		PreparedNumberInfo.StaticMeshComponent = ComponentToUse;
		for (int32 MatIdx = 0; MatIdx < ComponentToUse->GetNumMaterials(); ++MatIdx)
		{
			PreparedNumberInfo.MeshMIDs.Add(Cast<UMaterialInstanceDynamic>(ComponentToUse->GetMaterial(MatIdx)));
		}

		if (!LocalWorld->GetTimerManager().IsTimerActive(ReleaseTimerHandle))
		{
			LocalWorld->GetTimerManager().SetTimer(ReleaseTimerHandle, this, &ThisClass::ReleaseNextComponents, ComponentLifespan);
		}
	}

	// 朝向相机，并加一点随机偏移使同时出现的飘字不完全重叠
	FTransform CameraTransform;
	FVector    NumberLocation(NewRequest.WorldLocation);
	if (const APlayerController* PC = GetController<APlayerController>())
	{
		if (const APlayerCameraManager* PlayerCameraManager = PC->PlayerCameraManager)
		{
			CameraTransform = FTransform(PlayerCameraManager->GetCameraRotation(), PlayerCameraManager->GetCameraLocation());

			const float RandomMagnitude = 5.0f;
			NumberLocation += FMath::RandPointInBox(FBox(FVector(-RandomMagnitude), FVector(RandomMagnitude)));
		}
	}

	PreparedNumberInfo.StaticMeshComponent->SetWorldTransform(FTransform(CameraTransform.GetRotation(), NumberLocation));

	SetMaterialParameters(NewRequest, PreparedNumberInfo, CameraTransform, NumberLocation);
}

void UYanNumberPopComponent_MeshText::ReleaseNextComponents()
{
	UWorld* LocalWorld = GetWorld();
	check(LocalWorld);

	const float CurrentTime = LocalWorld->GetTimeSeconds();

	int32 NumReleased = 0;
	for (const FLiveNumberPopEntry& LiveComp : LiveComponents)
	{
		// 数组按到期时间升序，遇到未到期项即可停止
		if (CurrentTime < LiveComp.ReleaseTime)
		{
			break;
		}

		++NumReleased;
		if (ensure(LiveComp.Component))
		{
			LiveComp.Component->UnregisterComponent();

			if (ensure(LiveComp.Pool))
			{
				LiveComp.Pool->Components.Push(LiveComp.Component);
			}
			else
			{
				LiveComp.Component->SetFlags(RF_Transient);
				LiveComp.Component->Rename(nullptr, GetTransientPackage(), RF_NoFlags);
			}
		}
	}

	LiveComponents.RemoveAt(0, NumReleased);

	if (LiveComponents.Num() > 0)
	{
		const float TimeUntilNextRelease = LiveComponents[0].ReleaseTime - CurrentTime;
		LocalWorld->GetTimerManager().SetTimer(ReleaseTimerHandle, this, &ThisClass::ReleaseNextComponents, TimeUntilNextRelease);
	}
}

FLinearColor UYanNumberPopComponent_MeshText::DetermineColor(const FYanNumberPopRequest& Request) const
{
	for (const UYanDamagePopStyle* Style : Styles)
	{
		if (Style && Style->bOverrideColor && Style->MatchPattern.Matches(Request.TargetTags))
		{
			return Request.bIsCriticalDamage ? Style->CriticalColor : Style->Color;
		}
	}

	return FLinearColor::White;
}

UStaticMesh* UYanNumberPopComponent_MeshText::DetermineStaticMesh(const FYanNumberPopRequest& Request) const
{
	for (const UYanDamagePopStyle* Style : Styles)
	{
		if (Style && Style->bOverrideMesh && Style->MatchPattern.Matches(Request.TargetTags))
		{
			return Style->TextMesh;
		}
	}

	return nullptr;
}

void UYanNumberPopComponent_MeshText::SetMaterialParameters(const FYanNumberPopRequest& Request, FTempNumberPopInfo& NewDamageNumberInfo, const FTransform& CameraTransform, const FVector& NumberLocation)
{
	const UWorld* World = GetWorld();
	if (!World || !GEngine)
	{
		return;
	}

	const float RealGameTime = World->GetRealTimeSeconds();

	// 正负号目前不显示，保留位置供后续治疗数字等场景使用
	const bool bShouldShowSign = false;
	const bool bIsSignNegative = true;

	for (UMaterialInstanceDynamic* MeshMID : NewDamageNumberInfo.MeshMIDs)
	{
		MeshMID->SetScalarParameterValue(SignDigitParameterName, bIsSignNegative ? 0.5f : 0.0f);
		MeshMID->SetVectorParameterValue(ColorParameterName, DetermineColor(Request));

		// 位数超出材质可承载的上限时，退化为全 9 显示
		const int32 MaxSupportedDigits = FMath::Min(FMath::Min(PositionParameterNames.Num(), ScaleRotationAngleParameterNames.Num()), DurationParameterNames.Num());
		if (!ensure(NewDamageNumberInfo.DamageNumberArray.Num() <= MaxSupportedDigits))
		{
			NewDamageNumberInfo.DamageNumberArray.SetNum(MaxSupportedDigits);

			// 跳过索引 0，那一位是正负号
			for (int32 DigitIndex = 1; DigitIndex < NewDamageNumberInfo.DamageNumberArray.Num(); ++DigitIndex)
			{
				NewDamageNumberInfo.DamageNumberArray[DigitIndex] = 9;
			}
		}

		MeshMID->SetScalarParameterValue(AnimationLifespanParameterName, ComponentLifespan);
		MeshMID->SetScalarParameterValue(IsCriticalHitParameterName, Request.bIsCriticalDamage ? 1.0f : 0.0f);

		const int32 DamageNumberArrayLength   = NewDamageNumberInfo.DamageNumberArray.Num();
		float       OffsetAccumulatedValue    = (DamageNumberArrayLength * -1.0f) + (bShouldShowSign ? 0.0f : -1.0f);
		const int32 LastIndex                 = (DamageNumberArrayLength >= 4) ? DamageNumberArrayLength : 4;

		for (int32 NumberIndex = 0; NumberIndex < LastIndex; ++NumberIndex)
		{
			const float   NumberYOffset        = ((NumberIndex / FMath::Max(1, DamageNumberArrayLength - 1)) - 0.5f) * 2.0f;
			const FVector CameraSpaceDirection = CameraTransform.TransformVectorNoScale(FVector(0.0f, NumberYOffset, 0.0f));

			const bool bTightSpacing = (NumberIndex < DamageNumberArrayLength)
				&& ((NewDamageNumberInfo.DamageNumberArray[NumberIndex] == 1)
					|| ((NumberIndex > 0) && (NewDamageNumberInfo.DamageNumberArray[NumberIndex - 1] == 1)));
			OffsetAccumulatedValue += bTightSpacing ? SpacingPercentageForOnes : 1.0f;

			FLinearColor RGBAPositionParameter(CameraSpaceDirection);
			RGBAPositionParameter.A = OffsetAccumulatedValue;
			MeshMID->SetVectorParameterValue(PositionParameterNames[NumberIndex], RGBAPositionParameter);

			const float DistanceFromCameraToNumber = (CameraTransform.GetLocation() - NumberLocation).Size();
			const float DistanceSpriteScale        = (DistanceFromCameraBeforeDoublingSize == 0.0f)
				                                         ? 1.0f
				                                         : FMath::Clamp(DistanceFromCameraToNumber / DistanceFromCameraBeforeDoublingSize, 1.0f, 1000000000.0f);

			// 超出实际位数的槽位缩放为 0，等效于不显示
			const float ScaleToZeroMultiplier = ((NumberIndex < DamageNumberArrayLength) && (((NumberIndex == 0) && bShouldShowSign) || (NumberIndex != 0))) ? 1.0f : 0.0f;

			const float HitSizeMultiplier  = Request.bIsCriticalDamage ? CriticalHitSizeMultiplier : 1.0f;
			const float FontSizeMultiplier = HitSizeMultiplier * DistanceSpriteScale * ScaleToZeroMultiplier;

			FLinearColor RGBAScaleRotationParameter;
			RGBAScaleRotationParameter.R = FontXSize * FontSizeMultiplier;
			RGBAScaleRotationParameter.G = FontYSize * FontSizeMultiplier;
			RGBAScaleRotationParameter.B = NewDamageNumberInfo.DamageNumberArray[FMath::Min(DamageNumberArrayLength - 1, NumberIndex)];
			RGBAScaleRotationParameter.A = FMath::Sign(CameraSpaceDirection.X) * NumberOfNumberRotations;
			MeshMID->SetVectorParameterValue(ScaleRotationAngleParameterNames[NumberIndex], RGBAScaleRotationParameter);

			FLinearColor RGBADurationParameter;
			RGBADurationParameter.R = RealGameTime + ComponentLifespan;
			RGBADurationParameter.G = FMath::FRand();
			MeshMID->SetVectorParameterValue(DurationParameterNames[NumberIndex], RGBADurationParameter);
		}

		// 观战用的过场相机光圈更大，拉近的飘字会失焦，此时不做拉近
		const bool bIsSpectating = false;
		MeshMID->SetScalarParameterValue(MoveToCameraParameterName, bIsSpectating ? 0.0f : 1.0f);
	}
}
