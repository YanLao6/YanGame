#include "MoverLibrary/MoverGameplayLibrary.h"

#include "MoverComponent.h"
#include "Engine/World.h"
#include "Actor/YanHookProjectile.h"
#include "AbilitySystemComponent.h"
#include "GE/MoverLaunchExecutionCalculation.h"
#include "Clinch/YanClinchAuthorityComponent.h"
#include "Messages/VerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverGameplayLibrary)

AYanHookProjectile* UMoverGameplayLibrary::SpawnHookProjectile(UMoverComponent*                MoverComp,
                                                                     TSubclassOf<AYanHookProjectile> ProjectileClass,
                                                                     FVector                         SpawnLocation,
                                                                     FRotator                        SpawnRotation)
{
	if (!MoverComp || !ProjectileClass)
	{
		return nullptr;
	}
	UWorld* World = MoverComp->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner                          = MoverComp->GetOwner();
	return World->SpawnActor<AYanHookProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, Params);
}

void UMoverGameplayLibrary::ReleaseGrabbedState(AActor* TargetActor)
{
	UAbilitySystemComponent* TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(TargetActor);
	if (!TargetASC)
	{
		return;
	}

	FGameplayTagContainer GrantedTags;
	GrantedTags.AddTag(Status_Grabbed);
	TargetASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

void UMoverGameplayLibrary::ApplyLaunchEffectToTarget(UAbilitySystemComponent*     InstigatorASC,
                                                            UAbilitySystemComponent*     TargetASC,
                                                            TSubclassOf<UGameplayEffect> LaunchEffectClass,
                                                            FVector                      LaunchVelocity)
{
	if (!InstigatorASC || !TargetASC || !LaunchEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
	Context.AddInstigator(InstigatorASC->GetAvatarActor(), InstigatorASC->GetAvatarActor());

	FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(LaunchEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	// 将 FVector 拆成 3 个浮点 SetByCaller，规避 GE Spec 无法直接携带向量的限制
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityX, LaunchVelocity.X);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityY, LaunchVelocity.Y);
	Spec.Data->SetSetByCallerMagnitude(NAME_Mover_Launch_VelocityZ, LaunchVelocity.Z);

	InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
}
