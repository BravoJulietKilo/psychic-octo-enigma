// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "STCondition_CheckForJam.generated.h"

// Forward declarations
class UMachineContextComponent;
class UPraxisRandomService;
struct FPraxisMachineContext;

/**
 * Instance data for the Check for Jam condition
 */
USTRUCT()
struct FSTCondition_CheckForJamInstanceData
{
	GENERATED_BODY()

	/** Reference to the machine context component (auto-discovered at runtime) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UMachineContextComponent> MachineContext = nullptr;
	
	/** Reference to the random service (for probabilistic jam check) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UPraxisRandomService> RandomService = nullptr;
};

/**
 * STCondition_CheckForJam
 * 
 * Probabilistically checks if a jam should occur based on JamProbabilityPerTick.
 * Used as a transition condition from Production → Jam Recovery state.
 * 
 * Each machine has its own jam probability via MachineContext.JamProbabilityPerTick.
 * Uses RandomService to make the check deterministic/reproducible.
 */
USTRUCT(meta = (Category = "Praxis", DisplayName = "Check for Jam"))
struct PRAXISSIMULATIONKERNEL_API FSTCondition_CheckForJam : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTCondition_CheckForJamInstanceData;

	FSTCondition_CheckForJam() = default;

	// ═══════════════════════════════════════════════════════════════════════════
	// FStateTreeConditionBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
