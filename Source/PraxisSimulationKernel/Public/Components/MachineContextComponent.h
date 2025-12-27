// Copyright 2025 Celsian Pty Ltd
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FPraxisMachineContext.h"
#include "MachineContextComponent.generated.h"

/**
 * UMachineContextComponent
 * 
 * Simple wrapper component that exposes FPraxisMachineContext to StateTree.
 * StateTree tasks can bind to this component to read/write machine state.
 * 
 * Usage in StateTree:
 * - Bind task instance data to "Actor > Machine Context Component > Context"
 * - Tasks can then read/modify any property in FPraxisMachineContext
 */
UCLASS(ClassGroup=(Praxis), meta=(BlueprintSpawnableComponent))
class PRAXISSIMULATIONKERNEL_API UMachineContextComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMachineContextComponent();

	// ═══════════════════════════════════════════════════════════════════════════
	// Public Accessors
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Get the context (const access) */
	UFUNCTION(BlueprintPure, Category = "Machine Context")
	const FPraxisMachineContext& GetContext() const { return Context; }
	
	/** Get mutable context (for tasks that need to modify state) */
	UFUNCTION(BlueprintCallable, Category = "Machine Context")
	FPraxisMachineContext& GetMutableContext() { return Context; }

	// ═══════════════════════════════════════════════════════════════════════════
	// Initialization
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Initialize the context with configuration values */
	void InitializeContext(
		FName InMachineId,
		float InProductionRate,
		float InChangeoverDuration,
		float InScrapRate,
		float InJamProbability,
		float InMeanJamDuration,
		float InSlowSpeedFactor
	);

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// Data
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** The actual machine context - exposed for StateTree binding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine Context")
	FPraxisMachineContext Context;
};
