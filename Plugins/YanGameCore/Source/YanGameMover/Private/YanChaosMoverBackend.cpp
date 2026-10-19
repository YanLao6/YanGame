#include "YanChaosMoverBackend.h"

#include "YanChaosMoverSimulation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanChaosMoverBackend)

UYanChaosMoverBackendComponent::UYanChaosMoverBackendComponent()
{
	SimulationClass = UYanChaosMoverSimulation::StaticClass();
}
