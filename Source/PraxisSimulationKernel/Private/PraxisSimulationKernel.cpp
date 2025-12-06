#include "PraxisSimulationKernel.h"

DEFINE_LOG_CATEGORY(LogPraxisSim);

#define LOCTEXT_NAMESPACE "FPraxisSimulationKernelModule"

void FPraxisSimulationKernelModule::StartupModule()
{
	UE_LOG(LogPraxisSim, Log, TEXT("Praxis Simulation Kernel module started."));
}

void FPraxisSimulationKernelModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("Praxis Simulation Kernel module shut down."));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FPraxisSimulationKernelModule, PraxisSimulationKernel)