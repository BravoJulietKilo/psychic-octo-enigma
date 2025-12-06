#include "PraxisCore.h"
//#include "PraxisSimulationKernel.h"

#define LOCTEXT_NAMESPACE "FPraxisCoreModule"

void FPraxisCoreModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("PraxisCore: StartupModule"));
}

void FPraxisCoreModule::ShutdownModule()
{
	UE_LOG(LogTemp, Warning, TEXT("PraxisCore: ShutdownModule"));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FPraxisCoreModule, PraxisCore)