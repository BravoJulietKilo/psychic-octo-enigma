// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "STTask_JamRecovery.generated.h"

// Forward declarations
class UMachineContextComponent;
class UPraxisRandomService;
struct FPraxisMachineContext;

/**
 * Instance data for JamRecovery task
 */
USTRUCT()
struct FSTTask_JamRecoveryInstanceData
{
	GENERATED_BODY()

	/** Reference to the machine context component (auto-discovered at runtime) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UMachineContextComponent> MachineContext = nullptr;
	
	/** Reference to the random service (for jam duration) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UPraxisRandomService> RandomService = nullptr;
};

/**
 * STTask_JamRecovery
 * 
 * Handles machine jam/stoppage recovery.
 * Duration is sampled from exponential distribution with machine-specific MeanJamDuration.
 * Each machine can have different jam characteristics via FPraxisMachineContext parameters.
 * 
 * Machine-specific parameters (set in MachineLogicComponent):
 * - JamProbabilityPerTick: How often jams occur
 * - MeanJamDuration: Average recovery time (exponential distribution)
 */
USTRUCT(BlueprintType, meta = (Category = "Praxis", DisplayName = "Jam Recovery"))
struct PRAXISSIMULATIONKERNEL_API FSTTask_JamRecovery : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTask_JamRecoveryInstanceData;

	FSTTask_JamRecovery() = default;

	// ═══════════════════════════════════════════════════════════════════════════
	// FStateTreeTaskBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
