// Copyright 2025 Celsian Pty Ltd

#include "PraxisScheduleService.h"
#include "PraxisCore.h"
#include "EngineUtils.h"
#include "Misc/DateTime.h"
#include "Containers/Queue.h"

void UPraxisScheduleService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogPraxisSim, Log, TEXT("Schedule service initialized"));
}

void UPraxisScheduleService::Deinitialize()
{
	// Clear all data
	MachineQueues.Empty();
	Orders.Empty();
	Operators.Empty();
	UnassignedWorkOrders.Empty();
	RegisteredMachines.Empty();
	
	UE_LOG(LogPraxisSim, Log, TEXT("Schedule service deinitialized"));
	
	Super::Deinitialize();
}

// ════════════════════════════════════════════════════════════════════════════════
// Work Order Management
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisScheduleService::LoadSchedule(const TArray<FPraxisWorkOrder>& WorkOrders)
{
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Loading schedule with %d work orders"), 
		WorkOrders.Num());
	
	for (const FPraxisWorkOrder& WO : WorkOrders)
	{
		AddWorkOrder(WO);
	}
	
	// Try to assign any waiting work orders to registered machines
	TryAssignPendingWorkOrders();
}

void UPraxisScheduleService::AddWorkOrder(const FPraxisWorkOrder& NewWO)
{
	const int64 Id = NewWO.WorkOrderID;
	
	// Create order state
	FPraxisOrderState& S = Orders.FindOrAdd(Id);
	S.WorkOrder = NewWO;
	S.Status = 0; // Queued
	S.MachineId = NAME_None; // Not assigned yet
	
	// Add to unassigned queue
	UnassignedWorkOrders.Add(Id);
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("Work order %lld added to queue (SKU: %s, Qty: %d)"),
		Id, *NewWO.SKU, NewWO.Quantity);
	
	// Try to assign immediately if machines are available
	TryAssignPendingWorkOrders();
}

bool UPraxisScheduleService::RemoveWorkOrder(int64 WorkOrderID)
{
	if (!Orders.Remove(WorkOrderID)) 
	{
		return false;
	}
	
	// Remove from all queues
	UnassignedWorkOrders.Remove(WorkOrderID);
	for (auto& KVP : MachineQueues)
	{
		KVP.Value.Remove(WorkOrderID);
	}
	
	UE_LOG(LogPraxisSim, Verbose, TEXT("Work order %lld removed"), WorkOrderID);
	return true;
}

// ════════════════════════════════════════════════════════════════════════════════
// Query Methods
// ════════════════════════════════════════════════════════════════════════════════

bool UPraxisScheduleService::GetNextForMachine(FName MachineId, FPraxisWorkOrder& OutWO) const
{
	if (const TArray<int64>* Q = MachineQueues.Find(MachineId))
	{
		for (int64 Id : *Q)
		{
			if (const FPraxisOrderState* S = Orders.Find(Id))
			{
				if (S->Status == 0) // Queued
				{ 
					OutWO = S->WorkOrder; 
					return true; 
				}
			}
		}
	}
	return false;
}

void UPraxisScheduleService::GetActiveForMachine(FName MachineId, TArray<FPraxisWorkOrder>& Out) const
{
	Out.Reset();
	if (const TArray<int64>* Q = MachineQueues.Find(MachineId))
	{
		for (int64 Id : *Q)
		{
			if (const FPraxisOrderState* S = Orders.Find(Id))
			{
				if (S->Status == 0 || S->Status == 1) // Queued or Running
				{
					Out.Add(S->WorkOrder);
				}
			}
		}
	}
}

void UPraxisScheduleService::GetSchedule(TArray<FPraxisWorkOrder>& OutAll) const
{
	OutAll.Reset();
	for (const auto& KVP : Orders) 
	{
		OutAll.Add(KVP.Value.WorkOrder);
	}
}

int32 UPraxisScheduleService::GetPendingWorkOrderCount() const
{
	return UnassignedWorkOrders.Num();
}

// ════════════════════════════════════════════════════════════════════════════════
// State Transitions
// ════════════════════════════════════════════════════════════════════════════════

bool UPraxisScheduleService::StartWorkOrder(int64 WorkOrderID, FName MachineId)
{
	if (FPraxisOrderState* S = Orders.Find(WorkOrderID))
	{
		S->Status = 1; // Running
		S->MachineId = MachineId; 
		S->StartTs = NowUnixSeconds();
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("Work order %lld started on machine %s"), 
			WorkOrderID, *MachineId.ToString());
		
		OnWorkOrderStarted.Broadcast(WorkOrderID);
		return true;
	}
	return false;
}

bool UPraxisScheduleService::CompleteWorkOrder(int64 WorkOrderID)
{
	if (FPraxisOrderState* S = Orders.Find(WorkOrderID))
	{
		S->Status = 2; // Done
		S->EndTs = NowUnixSeconds();
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("Work order %lld completed on machine %s"), 
			WorkOrderID, *S->MachineId.ToString());
		
		OnWorkOrderCompleted.Broadcast(WorkOrderID);
		
		// Try to assign next work order to the now-idle machine
		TryAssignToMachine(S->MachineId);
		
		return true;
	}
	return false;
}

// ════════════════════════════════════════════════════════════════════════════════
// Machine Registration & Assignment
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisScheduleService::RegisterMachine(FName MachineId)
{
	if (!RegisteredMachines.Contains(MachineId))
	{
		RegisteredMachines.Add(MachineId);
		MachineQueues.FindOrAdd(MachineId);
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("Machine %s registered with schedule service"), 
			*MachineId.ToString());
		
		// Try to assign a work order immediately
		TryAssignToMachine(MachineId);
	}
}

void UPraxisScheduleService::NotifyMachineIdle(FName MachineId)
{
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("Machine %s is now idle - checking for work orders"), 
		*MachineId.ToString());
	
	TryAssignToMachine(MachineId);
}

void UPraxisScheduleService::TryAssignToMachine(FName MachineId)
{
	// Get next unassigned work order
	if (UnassignedWorkOrders.Num() == 0)
	{
		UE_LOG(LogPraxisSim, Verbose, 
			TEXT("No pending work orders to assign to %s"), 
			*MachineId.ToString());
		return;
	}
	
	// Get first unassigned work order (FIFO)
	const int64 WorkOrderID = UnassignedWorkOrders[0];
	UnassignedWorkOrders.RemoveAt(0);
	
	if (FPraxisOrderState* S = Orders.Find(WorkOrderID))
	{
		// Assign to machine
		S->MachineId = MachineId;
		MachineQueues.FindOrAdd(MachineId).Add(WorkOrderID);
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("Assigned work order %lld (SKU: %s, Qty: %d) to machine %s"),
			WorkOrderID, 
			*S->WorkOrder.SKU, 
			S->WorkOrder.Quantity,
			*MachineId.ToString());
		
		OnWorkOrderAssigned.Broadcast(WorkOrderID, MachineId);
		
		// Notify the machine via MachineLogicComponent
		NotifyMachineOfAssignment(MachineId, S->WorkOrder);
	}
}

void UPraxisScheduleService::TryAssignPendingWorkOrders()
{
	// Try to assign work orders to all registered idle machines
	for (const FName& MachineId : RegisteredMachines)
	{
		if (UnassignedWorkOrders.Num() == 0)
		{
			break; // No more work orders to assign
		}
		
		// Check if machine already has queued work
		const TArray<int64>* MachineQueue = MachineQueues.Find(MachineId);
		if (!MachineQueue || MachineQueue->Num() == 0)
		{
			// Machine is idle, assign work
			TryAssignToMachine(MachineId);
		}
	}
}

void UPraxisScheduleService::NotifyMachineOfAssignment(FName MachineId, const FPraxisWorkOrder& WorkOrder)
{
	// Find the machine actor and call AssignWorkOrder using reflection
	// This avoids circular dependency by not including MachineLogicComponent.h
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			// Find MachineLogicComponent by class name (reflection)
			UActorComponent* Component = It->FindComponentByClass(
				FindObject<UClass>(nullptr, TEXT("/Script/PraxisSimulationKernel.MachineLogicComponent"))
			);
			
			if (Component)
			{
				// Check if MachineId matches using reflection
				FName* CompMachineId = Component->GetClass()->FindPropertyByName(TEXT("MachineId"))->ContainerPtrToValuePtr<FName>(Component);
				
				if (CompMachineId && *CompMachineId == MachineId)
				{
					// Call AssignWorkOrder using reflection
					UFunction* Func = Component->FindFunction(TEXT("AssignWorkOrder"));
					if (Func)
					{
						struct
						{
							int64 WorkOrderId;
							FString SKU;
							int32 Quantity;
						} Params;
						
						Params.WorkOrderId = WorkOrder.WorkOrderID;
						Params.SKU = WorkOrder.SKU;
						Params.Quantity = WorkOrder.Quantity;
						
						Component->ProcessEvent(Func, &Params);
						return;
					}
				}
			}
		}
		
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Could not find machine actor for MachineId: %s"), 
			*MachineId.ToString());
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Operator Management
// ════════════════════════════════════════════════════════════════════════════════

bool UPraxisScheduleService::AssignOperatorToMachine(FName OperatorId, FName MachineId)
{
	FPraxisOperatorState& Op = Operators.FindOrAdd(OperatorId);
	if (Op.bBusy && Op.MachineId == MachineId) 
	{
		return true; // Already assigned
	}
	
	Op.OperatorId = OperatorId; 
	Op.MachineId = MachineId; 
	Op.bBusy = true;
	
	OnOperatorAssigned.Broadcast(OperatorId, MachineId);
	return true;
}

bool UPraxisScheduleService::ReleaseOperator(FName OperatorId)
{
	if (FPraxisOperatorState* Op = Operators.Find(OperatorId))
	{
		Op->bBusy = false; 
		Op->MachineId = NAME_None;
		OnOperatorReleased.Broadcast(OperatorId);
		return true;
	}
	return false;
}

void UPraxisScheduleService::RegisterOperator(FName OperatorId)
{
	Operators.FindOrAdd(OperatorId).OperatorId = OperatorId;
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Operator %s registered"), 
		*OperatorId.ToString());
}

// ════════════════════════════════════════════════════════════════════════════════
// Utility
// ════════════════════════════════════════════════════════════════════════════════

int64 UPraxisScheduleService::NowUnixSeconds() const
{
	// Placeholder - Orchestrator should provide sim time; swap in later
	return FDateTime::UtcNow().ToUnixTimestamp();
}
