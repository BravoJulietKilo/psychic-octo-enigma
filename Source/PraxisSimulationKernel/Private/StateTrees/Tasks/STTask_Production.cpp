// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Tasks/STTask_Production.h"
#include "Components/MachineContextComponent.h"
#include "Components/MachineLogicComponent.h"
#include "PraxisRandomService.h"
#include "PraxisMetricsSubsystem.h"
#include "PraxisInventoryService.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

EStateTreeRunStatus FSTTask_Production::EnterState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_ProductionInstanceData& InstanceData = Context.GetInstanceData(*this);
	
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
					InstanceData.Metrics = GI->GetSubsystem<UPraxisMetricsSubsystem>();
				}
				
				// Get inventory service (WorldSubsystem)
				InstanceData.Inventory = World->GetSubsystem<UPraxisInventoryService>();
			}
		}
	}
	
	// Verify components found
	if (!InstanceData.MachineContext)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("[STTask_Production] MachineContext not found!"));
		return EStateTreeRunStatus::Failed;
	}
	
	if (!InstanceData.RandomService)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("[STTask_Production] RandomService not found - scrap will be deterministic"));
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
			TEXT("Production"),
			FDateTime::UtcNow()
		);
	}
	InstanceData.PreviousState = TEXT("Production");
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("[%s] Production started - Target: %d units of %s"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.TargetQuantity,
		*MachineCtx.CurrentSKU);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_Production::Tick(
	FStateTreeExecutionContext& Context, 
	const float DeltaTime) const
{
	// Get instance data
	FSTTask_ProductionInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// Get context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Update time in state
	MachineCtx.TimeInState += DeltaTime;
	
	// Accumulate production progress
	// Progress = ProductionRate (units/sec) × DeltaTime (sec)
	MachineCtx.ProductionAccumulator += MachineCtx.ProductionRate * DeltaTime;
	
	// Process completed units
	while (MachineCtx.ProductionAccumulator >= 1.0f)
	{
		MachineCtx.ProductionAccumulator -= 1.0f;
		
		// Get MachineId for reporting (resolve once per unit)
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
		
		// Consume raw material from reservation (creates WIP)
		if (InstanceData.Inventory)
		{
			// For now, assume input SKU is "Steel_Bar" - TODO: Get from BOM
			FName InputSKU = TEXT("Steel_Bar");
			
			if (!InstanceData.Inventory->ConsumeReservedMaterial(
				ReportMachineId,
				MachineCtx.CurrentWorkOrderId,
				InputSKU))
			{
				// No material available - skip this production cycle
				UE_LOG(LogPraxisSim, Warning, 
					TEXT("[%s] No reserved material available for production"),
					*ReportMachineId.ToString());
				continue;
			}
		}
		
		// Determine if this unit is scrap
		if (ShouldScrapUnit(InstanceData, MachineCtx))
		{
			MachineCtx.ScrapCounter++;
			
			// Convert WIP to scrap in inventory
			if (InstanceData.Inventory)
			{
				FName ScrapLocation = FName(*FString::Printf(TEXT("%s.Scrap"), *ReportMachineId.ToString()));
				InstanceData.Inventory->ProduceScrap(
					ReportMachineId,
					MachineCtx.CurrentWorkOrderId,
					FName(*MachineCtx.CurrentSKU),
					ScrapLocation
				);
			}
			
			// Report scrap to metrics
			if (InstanceData.Metrics)
			{
				InstanceData.Metrics->RecordScrap(
					ReportMachineId,
					1,
					MachineCtx.CurrentSKU,
					FDateTime::UtcNow()
				);
			}
			
			UE_LOG(LogPraxisSim, Verbose, 
				TEXT("[%s] Produced SCRAP unit (%d/%d good, %d scrap)"), 
				*ReportMachineId.ToString(),
				MachineCtx.OutputCounter,
				MachineCtx.TargetQuantity,
				MachineCtx.ScrapCounter);
		}
		else
		{
			MachineCtx.OutputCounter++;
			
			// Convert WIP to finished good in inventory
			if (InstanceData.Inventory)
			{
				FName OutputLocation = FName(*FString::Printf(TEXT("%s.Output"), *ReportMachineId.ToString()));
				InstanceData.Inventory->ProduceFinishedGood(
					ReportMachineId,
					MachineCtx.CurrentWorkOrderId,
					FName(*MachineCtx.CurrentSKU),
					OutputLocation
				);
			}
			
			// Report good production to metrics
			if (InstanceData.Metrics)
			{
				InstanceData.Metrics->RecordGoodProduction(
					ReportMachineId,
					1,
					MachineCtx.CurrentSKU,
					FDateTime::UtcNow()
				);
			}
			
			UE_LOG(LogPraxisSim, Verbose, 
				TEXT("[%s] Produced GOOD unit (%d/%d good, %d scrap)"), 
				*ReportMachineId.ToString(),
				MachineCtx.OutputCounter,
				MachineCtx.TargetQuantity,
				MachineCtx.ScrapCounter);
		}
	}
	
	// Check if work order is complete
	if (MachineCtx.OutputCounter >= MachineCtx.TargetQuantity)
	{
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] Work order complete! Produced: %d good, %d scrap"), 
			*MachineCtx.MachineId.ToString(),
			MachineCtx.OutputCounter,
			MachineCtx.ScrapCounter);
		
		// Clear work order
		MachineCtx.bHasActiveWorkOrder = false;
		MachineCtx.CurrentSKU.Empty();
		MachineCtx.TargetQuantity = 0;
		
		// Notify the MachineLogicComponent so it can tell the schedule service
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			if (UMachineLogicComponent* LogicComp = Owner->FindComponentByClass<UMachineLogicComponent>())
			{
				LogicComp->NotifyWorkOrderComplete();
			}
		}
		
		// Work order complete - transition to Idle
		return EStateTreeRunStatus::Succeeded;
	}
	
	// Still producing
	return EStateTreeRunStatus::Running;
}

void FSTTask_Production::ExitState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data
	FSTTask_ProductionInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return;
	}
	
	// Get context
	const FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetContext();
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("[%s] Exiting Production state - Final: %d good, %d scrap"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.OutputCounter,
		MachineCtx.ScrapCounter);
}

bool FSTTask_Production::ShouldScrapUnit(
	const FInstanceDataType& InstanceData, 
	const FPraxisMachineContext& MachineCtx) const
{
	if (!InstanceData.RandomService)
	{
		// Fallback: deterministic scrap based on simple modulo
		const int32 TotalProduced = MachineCtx.OutputCounter + MachineCtx.ScrapCounter;
		const int32 ScrapInterval = FMath::Max(1, FMath::RoundToInt(1.0f / MachineCtx.ScrapRate));
		return (TotalProduced % ScrapInterval) == 0;
	}
	
	// Use random service for probabilistic scrap
	// Generate a random float in [0, 1) and compare to ScrapRate
	const float Roll = InstanceData.RandomService->Uniform_Key(
		MachineCtx.MachineId,
		2, // Channel 2 = Quality defects (per convention)
		0.0f,
		1.0f
	);
	
	return Roll < MachineCtx.ScrapRate;
}
