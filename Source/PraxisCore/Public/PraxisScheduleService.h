// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "Types/FPraxisWorkOrder.h"
#include "UObject/NoExportTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PraxisScheduleService.generated.h"

// Forward decls
struct FPraxisWorkOrder;

USTRUCT()
struct FPraxisOrderState
{
	GENERATED_BODY()
	UPROPERTY() FPraxisWorkOrder WorkOrder;
	UPROPERTY() FName MachineId;     // bound machine (if any)
	UPROPERTY() uint8 Status = 0;    // 0=Queued,1=Running,2=Done
	UPROPERTY() int64 StartTs = 0;   // unix seconds (sim time)
	UPROPERTY() int64 EndTs   = 0;
};

USTRUCT()
struct FPraxisOperatorState
{
	GENERATED_BODY()
	UPROPERTY() FName OperatorId;
	UPROPERTY() FName MachineId;   // assigned machine or "None"
	UPROPERTY() bool  bBusy = false;
};

// ── Events ──────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorkOrderAssigned, int64, WorkOrderID, FName, MachineId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam  (FOnWorkOrderStarted,  int64, WorkOrderID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam  (FOnWorkOrderCompleted,int64, WorkOrderID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOperatorAssigned,   FName, OperatorId, FName, MachineId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam  (FOnOperatorReleased,  FName, OperatorId);

/**
 * UPraxisScheduleService
 * 
 * Manages work order scheduling and assignment to machines.
 * 
 * Features:
 * - Load schedules from external sources (CSV, Blueprint, algorithms)
 * - Auto-assign work orders to idle machines (FIFO for MVP)
 * - Track work order state (Queued → Running → Complete)
 * - Support for future scheduling algorithms
 */
UCLASS()
class PRAXISCORE_API UPraxisScheduleService : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// Subsystem Lifecycle
	// ═══════════════════════════════════════════════════════════════════════════
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Schedule Loading (Student/Algorithm Interface)
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Load a batch of work orders (from CSV, algorithm, etc.) */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	void LoadSchedule(const TArray<FPraxisWorkOrder>& WorkOrders);
	
	/** Add a single work order to the queue */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	void AddWorkOrder(const FPraxisWorkOrder& NewWO);

	/** Remove a work order */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	bool RemoveWorkOrder(int64 WorkOrderID);

	// ═══════════════════════════════════════════════════════════════════════════
	// Query Methods
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Get the next queued work order for a specific machine */
	UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
	bool GetNextForMachine(FName MachineId, FPraxisWorkOrder& OutWO) const;

	/** Get all active (queued or running) work orders for a machine */
	UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
	void GetActiveForMachine(FName MachineId, TArray<FPraxisWorkOrder>& Out) const;

	/** Get the complete schedule (all work orders) */
	UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
	void GetSchedule(TArray<FPraxisWorkOrder>& OutAll) const;
	
	/** Get number of pending (unassigned) work orders */
	UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
	int32 GetPendingWorkOrderCount() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// State Transitions
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Mark a work order as started */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	bool StartWorkOrder(int64 WorkOrderID, FName MachineId);

	/** Mark a work order as completed */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	bool CompleteWorkOrder(int64 WorkOrderID);

	// ═══════════════════════════════════════════════════════════════════════════
	// Machine Registration & Assignment
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Register a machine (called by MachineLogicComponent on BeginPlay) */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	void RegisterMachine(FName MachineId);
	
	/** Notify that a machine is now idle and ready for work */
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	void NotifyMachineIdle(FName MachineId);

	// ═══════════════════════════════════════════════════════════════════════════
	// Operator Management (Future Use)
	// ═══════════════════════════════════════════════════════════════════════════
	
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	bool AssignOperatorToMachine(FName OperatorId, FName MachineId);

	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	bool ReleaseOperator(FName OperatorId);
	
	UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
	void RegisterOperator(FName OperatorId);

	// ═══════════════════════════════════════════════════════════════════════════
	// Events (Blueprint/UI Integration)
	// ═══════════════════════════════════════════════════════════════════════════
	
	UPROPERTY(BlueprintAssignable, Category="Praxis|Schedule")
	FOnWorkOrderAssigned OnWorkOrderAssigned;
	
	UPROPERTY(BlueprintAssignable, Category="Praxis|Schedule")
	FOnWorkOrderStarted OnWorkOrderStarted;
	
	UPROPERTY(BlueprintAssignable, Category="Praxis|Schedule")
	FOnWorkOrderCompleted OnWorkOrderCompleted;
	
	UPROPERTY(BlueprintAssignable, Category="Praxis|Schedule")
	FOnOperatorAssigned OnOperatorAssigned;
	
	UPROPERTY(BlueprintAssignable, Category="Praxis|Schedule")
	FOnOperatorReleased OnOperatorReleased;

	// ═══════════════════════════════════════════════════════════════════════════
	// Utility
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Get current simulation time as Unix timestamp */
	UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
	int64 NowUnixSeconds() const;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// Internal Assignment Logic
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Try to assign a work order to a specific machine */
	void TryAssignToMachine(FName MachineId);
	
	/** Try to assign all pending work orders to available machines */
	void TryAssignPendingWorkOrders();
	
	/** Notify a machine of work order assignment */
	void NotifyMachineOfAssignment(FName MachineId, const FPraxisWorkOrder& WorkOrder);

	// ═══════════════════════════════════════════════════════════════════════════
	// Data Storage
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Per-machine queues of work order IDs */
	TMap<FName, TArray<int64>> MachineQueues;
	
	/** Global work order state table */
	TMap<int64, FPraxisOrderState> Orders;
	
	/** Unassigned work orders (FIFO queue) */
	TArray<int64> UnassignedWorkOrders;
	
	/** Registered machines */
	TSet<FName> RegisteredMachines;
	
	/** Operator state */
	TMap<FName, FPraxisOperatorState> Operators;

	/** Boot time for simulation clock (placeholder) */
	int64 BootUnixSeconds = 0;
};
