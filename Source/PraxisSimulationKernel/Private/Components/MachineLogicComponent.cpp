// Copyright 2025 Celsian Pty Ltd

#include "Components/MachineLogicComponent.h"
#include "Components/MachineContextComponent.h"
#include "PraxisSimulationKernel.h"
#include "PraxisOrchestrator.h"
#include "PraxisRandomService.h"
#include "PraxisMetricsSubsystem.h"
#include "StateTree.h"
#include "Components/StateTreeComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

UMachineLogicComponent::UMachineLogicComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Driven by Orchestrator, not per-frame
}

// ════════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════════

void UMachineLogicComponent::OnRegister()
{
	Super::OnRegister();
	
	if (!GetOwner())
	{
		return;
	}
	
	// Create MachineContext component first (StateTree tasks will bind to this)
	if (!MachineContextComponent)
	{
		MachineContextComponent = NewObject<UMachineContextComponent>(
			GetOwner(), 
			UMachineContextComponent::StaticClass(), 
			TEXT("MachineContext")
		);
		
		if (MachineContextComponent)
		{
			MachineContextComponent->RegisterComponent();
			
			UE_LOG(LogPraxisSim, Verbose, 
				TEXT("[%s] MachineContext component created"), 
				*MachineId.ToString());
		}
	}
	
	// Create StateTree component
	if (!StateTreeComponent)
	{
		StateTreeComponent = NewObject<UStateTreeComponent>(
			GetOwner(), 
			UStateTreeComponent::StaticClass(), 
			TEXT("MachineStateTree")
		);
		
		if (StateTreeComponent)
		{
			// Pass the asset pointer directly to the component
			StateTreeComponent->SetStateTree(const_cast<UStateTree*>(StateTreeRef.GetStateTree()));
			
			// Disable automatic start - we'll control it manually
			StateTreeComponent->SetStartLogicAutomatically(false);
			
			// Register the component
			StateTreeComponent->RegisterComponent();
			
			// Disable automatic ticking - we'll tick it manually from orchestrator
			StateTreeComponent->SetComponentTickEnabled(false);
			
			UE_LOG(LogPraxisSim, Verbose, 
				TEXT("[%s] StateTree component created"), 
				*MachineId.ToString());
		}
	}
}

void UMachineLogicComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve core services from GameInstance
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			Orchestrator = GI->GetSubsystem<UPraxisOrchestrator>();
			RandomService = GI->GetSubsystem<UPraxisRandomService>();
			Metrics = GI->GetSubsystem<UPraxisMetricsSubsystem>();
		}
	}

	// Validate services
	if (!Orchestrator)
	{
		UE_LOG(LogPraxisSim, Error, 
			TEXT("[%s] MachineLogicComponent: Orchestrator not found!"), 
			*MachineId.ToString());
		return;
	}

	// Subscribe to orchestrator events
	Orchestrator->OnSimTick.AddDynamic(this, &UMachineLogicComponent::HandleSimTick);
	Orchestrator->OnEndSession.AddDynamic(this, &UMachineLogicComponent::HandleEndSession);

	// Initialize machine context
	InitializeMachineContext();
	
	// Start the StateTree
	if (StateTreeComponent && StateTreeComponent->IsRegistered())
	{
		StateTreeComponent->StartLogic();
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] MachineLogicComponent initialized and StateTree started"), 
			*MachineId.ToString());
	}
	else
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("[%s] MachineLogicComponent initialized WITHOUT StateTree"), 
			*MachineId.ToString());
	}
}

void UMachineLogicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop StateTree
	if (StateTreeComponent && StateTreeComponent->IsRegistered())
	{
		StateTreeComponent->StopLogic(TEXT("Component EndPlay"));
	}

	// Unsubscribe from orchestrator
	if (Orchestrator)
	{
		Orchestrator->OnSimTick.RemoveDynamic(this, &UMachineLogicComponent::HandleSimTick);
		Orchestrator->OnEndSession.RemoveDynamic(this, &UMachineLogicComponent::HandleEndSession);
	}

	Super::EndPlay(EndPlayReason);
}

// ════════════════════════════════════════════════════════════════════════════════
// Initialization
// ════════════════════════════════════════════════════════════════════════════════

void UMachineLogicComponent::InitializeMachineContext()
{
	if (!MachineContextComponent)
	{
		UE_LOG(LogPraxisSim, Error, 
			TEXT("[%s] Cannot initialize context - MachineContextComponent is null!"), 
			*MachineId.ToString());
		return;
	}
	
	// Initialize the context component with our configuration
	MachineContextComponent->InitializeContext(
		MachineId,
		ProductionRate,
		ChangeoverDuration,
		ScrapRate,
		JamProbabilityPerTick,
		MeanJamDuration,
		SlowSpeedFactor
	);
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("[%s] Machine context initialized"), 
		*MachineId.ToString());
}

// ════════════════════════════════════════════════════════════════════════════════
// Orchestrator Callbacks
// ════════════════════════════════════════════════════════════════════════════════

void UMachineLogicComponent::HandleSimTick(double SimDeltaSeconds, int32 TickCount)
{
	// Manually tick the StateTree component
	if (StateTreeComponent && StateTreeComponent->IsRegistered())
	{
		StateTreeComponent->TickComponent(
			static_cast<float>(SimDeltaSeconds), 
			LEVELTICK_All, 
			nullptr
		);
	}

	// Update runtime display properties from context component
	if (MachineContextComponent)
	{
		const auto& Context = MachineContextComponent->GetContext();
		OutputCounter = Context.OutputCounter;
		ScrapCounter = Context.ScrapCounter;
		CurrentSKU = Context.CurrentSKU;
		CurrentQuantity = Context.TargetQuantity;
	}
}

void UMachineLogicComponent::HandleEndSession()
{
	// Flush any pending metrics
	if (Metrics)
	{
		Metrics->FlushMetrics();
	}
	
	if (MachineContextComponent)
	{
		const auto& Context = MachineContextComponent->GetContext();
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] Session ended - Final stats: %d good, %d scrap"), 
			*MachineId.ToString(),
			Context.OutputCounter,
			Context.ScrapCounter);
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════════════

void UMachineLogicComponent::AssignWorkOrder(const FString& SKU, int32 Quantity)
{
	if (!MachineContextComponent)
	{
		UE_LOG(LogPraxisSim, Error, 
			TEXT("[%s] Cannot assign work order - MachineContextComponent is null!"), 
			*MachineId.ToString());
		return;
	}
	
	// Update machine context
	auto& Context = MachineContextComponent->GetMutableContext();
	Context.CurrentSKU = SKU;
	Context.TargetQuantity = Quantity;
	Context.bHasActiveWorkOrder = true;
	
	// Reset counters for new work order
	Context.OutputCounter = 0;
	Context.ScrapCounter = 0;
	Context.ProductionAccumulator = 0.0f;
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("[%s] Work order assigned: %s (Qty: %d)"), 
		*MachineId.ToString(),
		*SKU,
		Quantity);
	
	// Record metric
	if (Metrics && Orchestrator)
	{
		Metrics->RecordMachineEvent(
			MachineId, 
			TEXT("WorkOrderAssigned"), 
			Orchestrator->GetSimDateTimeUTC());
	}
	
	// TODO: Optionally send StateTree event to trigger immediate transition
	// StateTreeComponent->SendEvent(...);
}

FString UMachineLogicComponent::GetCurrentStateName() const
{
	if (StateTreeComponent)
	{
		const EStateTreeRunStatus Status = StateTreeComponent->GetStateTreeRunStatus();
		return UEnum::GetValueAsString(Status);
	}
	
	return TEXT("No StateTree");
}

bool UMachineLogicComponent::IsProcessing() const
{
	if (MachineContextComponent)
	{
		return MachineContextComponent->GetContext().bHasActiveWorkOrder;
	}
	
	return false;
}
