#pragma once

#include "CoreMinimal.h"
//#include "PraxisSimulationKernel.h"
#include "Modules/ModuleManager.h"

class FPraxisCoreModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
