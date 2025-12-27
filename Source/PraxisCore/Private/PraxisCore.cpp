#include "PraxisCore.h"
//#include "PraxisSimulationKernel.h"

// Define the logging category
DEFINE_LOG_CATEGORY(LogPraxisSim);

#define LOCTEXT_NAMESPACE "FPraxisCoreModule"

void FPraxisCoreModule::StartupModule()
{
	UE_LOG(LogPraxisSim, Warning, TEXT("PraxisCore: StartupModule"));
}

void FPraxisCoreModule::ShutdownModule()
{
	UE_LOG(LogPraxisSim, Warning, TEXT("PraxisCore: ShutdownModule"));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FPraxisCoreModule, PraxisCore)