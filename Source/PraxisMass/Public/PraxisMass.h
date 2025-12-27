#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Declare the logging category
DECLARE_LOG_CATEGORY_EXTERN(LogPraxisSim, Log, All); 

class PRAXISMASS_API FPraxisMassModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
