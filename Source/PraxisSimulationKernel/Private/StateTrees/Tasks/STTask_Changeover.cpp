// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Tasks/STTask_Changeover.h"
#include "Components/MachineContextComponent.h"
#include "Components/MachineLogicComponent.h"
#include "PraxisMetricsSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

EStateTreeRunStatus FSTTask_Changeover::EnterState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_ChangeoverInstanceData& InstanceData = Context.GetInstanceData(*this);
	
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
		UE_LOG(LogPraxisSim, Error, TEXT("[STTask_Changeover] MachineContext not found!"));
		return EStateTreeRunStatus::Failed;
	}
	
	// Get the context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Initialize changeover timer
	MachineCtx.ChangeoverTimeRemaining = MachineCtx.ChangeoverDuration;
	MachineCtx.TimeInState = 0.0f;
	
	// Store previous SKU for metrics
	InstanceData.PreviousSKU = MachineCtx.CurrentSKU;
	
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
			TEXT("Changeover"),
			FDateTime::UtcNow()
		);
	}
	InstanceData.PreviousState = TEXT("Changeover");
	
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
	
	// Report changeover completion to metrics
	if (InstanceData.Metrics)
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
		
		InstanceData.Metrics->RecordChangeover(
			ReportMachineId,
			InstanceData.PreviousSKU,
			MachineCtx.CurrentSKU,
			MachineCtx.TimeInState,
			FDateTime::UtcNow()
		);
	}
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("[%s] Exiting Changeover state - Time spent: %.1f seconds"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.TimeInState);
}
