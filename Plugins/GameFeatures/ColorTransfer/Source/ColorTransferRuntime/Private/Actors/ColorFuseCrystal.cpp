#include "Actors/ColorFuseCrystal.h"

#include "Actors/ColorWindZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorFuseCrystal)

AColorFuseCrystal::AColorFuseCrystal()
{
	PoolCapacity = 50.0f;
}

void AColorFuseCrystal::AuthOnBroken()
{
	for (AColorWindZone* Wind : WindsToDeactivate)
	{
		if (Wind)
		{
			Wind->AuthDeactivate();
		}
	}

	Destroy();
}
