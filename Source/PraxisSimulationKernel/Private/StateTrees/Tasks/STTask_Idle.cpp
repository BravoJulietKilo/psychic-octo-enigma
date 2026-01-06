// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Tasks/STTask_Idle.h"
#include "Components/MachineContextComponent.h"
#include "Components/MachineLogicComponent.h"
#include "PraxisMetricsSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

EStateTreeRunStatus FSTTask_Idle::EnterState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_IdleInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// Auto-discover components if not bound
	if (!InstanceData.MachineContext)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			InstanceData.MachineContext = Owner->FindComponentByClass<UMachineContextComponent>();
			
			if (UWorld* World = Owner->GetWorld())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					InstanceData.Metrics = GI->GetSubsystem<UPraxisMetricsSubsystem>();
				}
			}
		}
	}
	
	// Verify component found
	if (!InstanceData.MachineContext)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("[STTask_Idle] MachineContext not found on owner actor!"));
		return EStateTreeRunStatus::Failed;
	}
	
	// Get the context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Reset time in state
	MachineCtx.TimeInState = 0.0f;
	
	// Report state change to metrics
	if (InstanceData.Metrics && !InstanceData.PreviousState.IsEmpty())
	{
		// Get MachineId from owner's LogicComponent
		FName ReportMachineId = MachineCtx.MachineId;
		if (ReportMachineId == NAME_None)
		{
			if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
			{
				if (UMachineLogicComponent* LogicComp = Owner->FindComponentByClass<UMachineLogicComponent>())
				{
					ReportMachineId = LogicComp->MachineId;
				}
			}
		}
		
		InstanceData.Metrics->RecordStateChange(
			ReportMachineId,
			InstanceData.PreviousState,
			TEXT("Idle"),
			FDateTime::UtcNow()
		);
	}
	InstanceData.PreviousState = TEXT("Idle");
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("[%s] Entered Idle state"), 
		*MachineCtx.MachineId.ToString());
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_Idle::Tick(
	FStateTreeExecutionContext& Context, 
	const float DeltaTime) const
{
	// Get instance data
	FSTTask_IdleInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// Get context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Track time in idle
	MachineCtx.TimeInState += DeltaTime;
	
	// Check if work order has been assigned
	if (MachineCtx.bHasActiveWorkOrder)
	{
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] Work order assigned: %s (Qty: %d) - transitioning from Idle"), 
			*MachineCtx.MachineId.ToString(),
			*MachineCtx.CurrentSKU,
			MachineCtx.TargetQuantity);
		
		// Work order assigned - trigger transition (likely to Changeover state)
		return EStateTreeRunStatus::Succeeded;
	}
	
	// Still waiting for work
	return EStateTreeRunStatus::Running;
}
