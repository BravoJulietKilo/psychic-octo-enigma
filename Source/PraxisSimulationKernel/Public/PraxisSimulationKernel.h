// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "PraxisCore.h"  // for LogPraxisSim
#include "Modules/ModuleManager.h"

class  FPraxisSimulationKernelModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
