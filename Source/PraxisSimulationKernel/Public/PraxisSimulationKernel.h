// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Declare the logging category
DECLARE_LOG_CATEGORY_EXTERN(LogPraxisSim, Log, All);

// If UBT didn’t define the module export macro yet (rare but happens in early boot)
#ifndef PRAXISSIMULATIONKERNEL_API
#define PRAXISSIMULATIONKERNEL_API
#pragma message("PRAXISSIMULATIONKERNEL_API was not defined by UBT — check module naming.")
#endif

class PRAXISSIMULATIONKERNEL_API FPraxisSimulationKernelModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
