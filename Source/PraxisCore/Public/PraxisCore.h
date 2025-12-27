#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Declare the logging category
PRAXISCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogPraxisSim, Log, All); 

class PRAXISCORE_API FPraxisCoreModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
