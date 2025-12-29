// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Tasks/STTask_Changeover.h"
#include "Components/MachineContextComponent.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"
#include "GameFramework/Actor.h"

EStateTreeRunStatus FSTTask_Changeover::EnterState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_ChangeoverInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// Auto-discover component if not bound
	if (!InstanceData.MachineContext)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			InstanceData.MachineContext = Owner->FindComponentByClass<UMachineContextComponent>();
		}
	}
	
	// Verify component found
	if (!InstanceData.MachineContext)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("[STTask_Changeover] MachineContext not found!"));
		return EStateTreeRunStatus::Failed;
	}
	
	// Get the context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Initialize changeover timer
	MachineCtx.ChangeoverTimeRemaining = MachineCtx.ChangeoverDuration;
	MachineCtx.TimeInState = 0.0f;
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("[%s] Changeover started - Duration: %.1f seconds for SKU: %s"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.ChangeoverDuration,
		*MachineCtx.CurrentSKU);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_Changeover::Tick(
	FStateTreeExecutionContext& Context, 
	const float DeltaTime) const
{
	// Get instance data
	FSTTask_ChangeoverInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// Get context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Update timers
	MachineCtx.TimeInState += DeltaTime;
	MachineCtx.ChangeoverTimeRemaining -= DeltaTime;
	
	// Check if changeover is complete
	if (MachineCtx.ChangeoverTimeRemaining <= 0.0f)
	{
		MachineCtx.ChangeoverTimeRemaining = 0.0f;
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] Changeover complete - Ready to produce %s"), 
			*MachineCtx.MachineId.ToString(),
			*MachineCtx.CurrentSKU);
		
		// Changeover complete - transition to Production
		return EStateTreeRunStatus::Succeeded;
	}
	
	// Still in changeover
	return EStateTreeRunStatus::Running;
}

void FSTTask_Changeover::ExitState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_ChangeoverInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return;
	}
	
	// Get context
	const FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetContext();
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("[%s] Exiting Changeover state - Time spent: %.1f seconds"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.TimeInState);
}
