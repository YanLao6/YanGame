class UMoverGrapplingCanclelAbility : UMoverInputAbility
{
	default AbilityTags.AddTag(GameplayTags::Ability_Mover_GrapplingCancel);

	UFUNCTION(BlueprintOverride)
	void InputReleased()
	{
		EndAbility();
	}
}
