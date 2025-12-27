// Copyright 2025 Celsian Pty Ltd

#include "Components/MachineContextComponent.h"

UMachineContextComponent::UMachineContextComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMachineContextComponent::InitializeContext(
	FName InMachineId,
	float InProductionRate,
	float InChangeoverDuration,
	float InScrapRate,
	float InJamProbability,
	float InMeanJamDuration,
	float InSlowSpeedFactor)
{
	Context.MachineId = InMachineId;
	Context.ProductionRate = InProductionRate;
	Context.ChangeoverDuration = InChangeoverDuration;
	Context.ScrapRate = InScrapRate;
	Context.JamProbabilityPerTick = InJamProbability;
	Context.MeanJamDuration = InMeanJamDuration;
	Context.SlowSpeedFactor = InSlowSpeedFactor;
	
	// Reset runtime state
	Context.ResetProductionCounters();
	Context.TimeInState = 0.0f;
	Context.bHasActiveWorkOrder = false;
	Context.CurrentSKU.Empty();
	Context.TargetQuantity = 0;
	Context.JamDurationRemaining = 0.0f;
	Context.ChangeoverTimeRemaining = 0.0f;
}
