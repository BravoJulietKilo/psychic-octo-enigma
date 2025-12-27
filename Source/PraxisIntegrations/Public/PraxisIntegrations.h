#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Declare the logging category
DECLARE_LOG_CATEGORY_EXTERN(LogPraxisSim, Log, All); 

class PRAXISINTEGRATIONS_API FPraxisIntegrationsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
