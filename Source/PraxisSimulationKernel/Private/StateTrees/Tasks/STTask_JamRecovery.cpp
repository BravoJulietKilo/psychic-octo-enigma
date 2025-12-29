// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Tasks/STTask_JamRecovery.h"
#include "Components/MachineContextComponent.h"
#include "PraxisRandomService.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

EStateTreeRunStatus FSTTask_JamRecovery::EnterState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_JamRecoveryInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// Auto-discover MachineContext if not bound
	if (!InstanceData.MachineContext)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			InstanceData.MachineContext = Owner->FindComponentByClass<UMachineContextComponent>();
		}
	}
	
	// Auto-discover RandomService if not bound
	if (!InstanceData.RandomService)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			if (UWorld* World = Owner->GetWorld())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					InstanceData.RandomService = GI->GetSubsystem<UPraxisRandomService>();
				}
			}
		}
	}
	
	// Verify components found
	if (!InstanceData.MachineContext)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("[STTask_JamRecovery] MachineContext not found!"));
		return EStateTreeRunStatus::Failed;
	}
	
	// Get the context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Sample jam duration from exponential distribution
	if (InstanceData.RandomService)
	{
		// Use RandomService with machine-specific channel for jam recovery
		MachineCtx.JamDurationRemaining = InstanceData.RandomService->ExponentialFromMean_Key(
			MachineCtx.MachineId,
			0, // Channel 0 = Machine breakdowns/failures (per convention)
			MachineCtx.MeanJamDuration
		);
	}
	else
	{
		// Fallback: use mean duration directly
		MachineCtx.JamDurationRemaining = MachineCtx.MeanJamDuration;
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("[STTask_JamRecovery] RandomService not found - using mean jam duration"));
	}
	
	// Reset time in state
	MachineCtx.TimeInState = 0.0f;
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("[%s] JAM OCCURRED - Recovery time: %.1f seconds (Mean: %.1f)"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.JamDurationRemaining,
		MachineCtx.MeanJamDuration);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_JamRecovery::Tick(
	FStateTreeExecutionContext& Context, 
	const float DeltaTime) const
{
	// Get instance data
	FSTTask_JamRecoveryInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// Get context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Update timers
	MachineCtx.TimeInState += DeltaTime;
	MachineCtx.JamDurationRemaining -= DeltaTime;
	
	// Check if recovery is complete
	if (MachineCtx.JamDurationRemaining <= 0.0f)
	{
		MachineCtx.JamDurationRemaining = 0.0f;
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] Jam recovery complete - Resuming production"), 
			*MachineCtx.MachineId.ToString());
		
		// Recovery complete - transition back to Production
		return EStateTreeRunStatus::Succeeded;
	}
	
	// Still recovering
	return EStateTreeRunStatus::Running;
}

void FSTTask_JamRecovery::ExitState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_JamRecoveryInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return;
	}
	
	// Get context
	const FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetContext();
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("[%s] Exiting Jam Recovery state - Downtime: %.1f seconds"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.TimeInState);
}
