
// Copyright 2025 Celsian Pty Ltd

#include "PraxisShopFloorSystem.h"
#include "Stats/Stats.h" // Include for TStatId


TStatId UPraxisShopFloorSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMyCustomTickableSubsystem, STATGROUP_Tickables);
}