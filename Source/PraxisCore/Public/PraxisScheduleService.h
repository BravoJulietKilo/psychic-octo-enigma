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
 * 
 */
UCLASS()
class PRAXISCORE_API UPraxisScheduleService : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	public:
    // Add/remove
    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    void AddWorkOrder(const FPraxisWorkOrder& NewWO);

    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    bool RemoveWorkOrder(int64 WorkOrderID);

    // Query
    UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
    bool GetNextForMachine(FName MachineId, FPraxisWorkOrder& OutWO) const;

    UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
    void GetActiveForMachine(FName MachineId, TArray<FPraxisWorkOrder>& Out) const;

    UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
    void GetSchedule(TArray<FPraxisWorkOrder>& OutAll) const;

    // State transitions
    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    bool AssignOperatorToMachine(FName OperatorId, FName MachineId);

    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    bool ReleaseOperator(FName OperatorId);

    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    bool StartWorkOrder(int64 WorkOrderID, FName MachineId);

    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    bool CompleteWorkOrder(int64 WorkOrderID);

    // Time (Sim clock to unix seconds; Orchestrator will inject a getter)
    UFUNCTION(BlueprintPure, Category="Praxis|Schedule")
    int64 NowUnixSeconds() const;

    // Events for BP/UI
    UPROPERTY(BlueprintAssignable) FOnWorkOrderAssigned   OnWorkOrderAssigned;
    UPROPERTY(BlueprintAssignable) FOnWorkOrderStarted    OnWorkOrderStarted;
    UPROPERTY(BlueprintAssignable) FOnWorkOrderCompleted  OnWorkOrderCompleted;
    UPROPERTY(BlueprintAssignable) FOnOperatorAssigned    OnOperatorAssigned;
    UPROPERTY(BlueprintAssignable) FOnOperatorReleased    OnOperatorReleased;

    // Registration helpers (actors call these on BeginPlay)
    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    void RegisterMachine(FName MachineId);

    UFUNCTION(BlueprintCallable, Category="Praxis|Schedule")
    void RegisterOperator(FName OperatorId);

private:
    // Simple FIFO per machine for MVP
    TMap<FName, TArray<int64>> MachineQueues;          // WorkOrder IDs
    TMap<int64, FPraxisOrderState> Orders;             // global table
    TMap<FName, FPraxisOperatorState> Operators;

    // For NowUnixSeconds(); swap for Orchestrator hook later
    int64 BootUnixSeconds = 0;
	
};