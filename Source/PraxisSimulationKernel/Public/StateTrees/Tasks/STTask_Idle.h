// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "STTask_Idle.generated.h"

// Forward declarations
class UMachineContextComponent;

/**
 * Instance data for Idle task
 */
USTRUCT()
struct FSTTask_IdleInstanceData
{
	GENERATED_BODY()

	/** Reference to the machine context component (auto-discovered at runtime) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UMachineContextComponent> MachineContext = nullptr;
	
	/** Reference to metrics subsystem */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<class UPraxisMetricsSubsystem> Metrics = nullptr;
	
	/** Track previous state for reporting state changes */
	FString PreviousState;
};

/**
 * STTask_Idle
 * 
 * Machine is idle, waiting for a work order to be assigned.
 * Transitions to success when bHasActiveWorkOrder becomes true.
 * 
 * This is the default/rest state for a machine.
 */
USTRUCT(BlueprintType, meta = (Category = "Praxis", DisplayName = "Idle"))
struct PRAXISSIMULATIONKERNEL_API FSTTask_Idle : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTask_IdleInstanceData;

	FSTTask_Idle() = default;

	// ═══════════════════════════════════════════════════════════════════════════
	// FStateTreeTaskBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
