#include "YanMoverDataModelTypes.h"

FMoverDataStructBase* FYanCharacterInputs::Clone() const
{
	return new FYanCharacterInputs(*this);
}

bool FYanCharacterInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	// 每个 bool 只占 1 bit，极致节省带宽
	Ar.SerializeBits(&bWantsToSlide, 1);
	Ar.SerializeBits(&bIsSprintJustPressed, 1);
	Ar.SerializeBits(&bIsSprintPressed, 1);
	Ar.SerializeBits(&bIsGrapplingActive, 1);
	// 锚点仅在钩锁激活时有意义，始终随输入上传以支持服务器 re-simulation
	Ar << GrappleAnchorPoint;
	Ar << GrappleRopeLength;
	bOutSuccess = true;
	return true;
}

UScriptStruct* FYanCharacterInputs::GetScriptStruct() const
{
	return StaticStruct();
}

bool FYanCharacterInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FYanCharacterInputs& Auth = static_cast<const FYanCharacterInputs&>(AuthorityState);
	return Auth.bWantsToSlide != bWantsToSlide
	       || Auth.bIsSprintJustPressed != bIsSprintJustPressed
	       || Auth.bIsSprintPressed != bIsSprintPressed
	       || Auth.bIsGrapplingActive != bIsGrapplingActive
	       || !Auth.GrappleAnchorPoint.Equals(GrappleAnchorPoint)
	       || !FMath::IsNearlyEqual(Auth.GrappleRopeLength, GrappleRopeLength);
}

void FYanCharacterInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// bool 无法线性插值，直接按 Pct 取 From 或 To 的离散值
	const FYanCharacterInputs& Src = static_cast<const FYanCharacterInputs&>((Pct < 0.5f) ? From : To);
	bWantsToSlide                  = Src.bWantsToSlide;
	bIsSprintJustPressed           = Src.bIsSprintJustPressed;
	bIsSprintPressed               = Src.bIsSprintPressed;
	bIsGrapplingActive             = Src.bIsGrapplingActive;
	GrappleAnchorPoint             = Src.GrappleAnchorPoint;
	GrappleRopeLength              = Src.GrappleRopeLength;
}

void FYanCharacterInputs::Merge(const FMoverDataStructBase& From)
{
	// 多个 InputProducer 同帧写入时取并集（任意一方按下即视为按下）
	const FYanCharacterInputs& Src = static_cast<const FYanCharacterInputs&>(From);
	bWantsToSlide                  |= Src.bWantsToSlide;
	bIsSprintJustPressed           |= Src.bIsSprintJustPressed;
	bIsSprintPressed               |= Src.bIsSprintPressed;
	bIsGrapplingActive             |= Src.bIsGrapplingActive;
	// 锚点只由钩锁 Ability 写入，取来源中处于激活状态的值
	if (Src.bIsGrapplingActive)
	{
		GrappleAnchorPoint = Src.GrappleAnchorPoint;
		GrappleRopeLength  = Src.GrappleRopeLength;
	}
}

void FYanCharacterInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("bWantsToSlide: %i | bIsSprintJustPressed: %i | bIsSprintPressed: %i | bIsGrapplingActive: %i | GrappleAnchor: (%.1f,%.1f,%.1f) | GrappleRope: %.1f\n",
	            bWantsToSlide, bIsSprintJustPressed, bIsSprintPressed, bIsGrapplingActive,
	            GrappleAnchorPoint.X, GrappleAnchorPoint.Y, GrappleAnchorPoint.Z, GrappleRopeLength);
}

void FYanCharacterInputs::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
