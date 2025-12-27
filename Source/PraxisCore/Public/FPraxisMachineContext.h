// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "FPraxisMachineContext.generated.h"

/**
 * FPraxisMachineContext
 * 
 * Shared context data for machine StateTree execution.
 * This struct acts as the "memory" that all states, tasks, and evaluators share.
 * Each machine instance has its own independent context.
 * 
 * DESIGN NOTES:
 * - Located in PraxisCore to avoid cross-module header dependency issues
 * - Kept simple - only POD types and FString/FName to ensure UHT compatibility
 * - No complex includes - StateTree tasks will include this, so keep it lightweight
 */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FPraxisMachineContext
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION (Set at initialization)
	// ═══════════════════════════════════════════════════════════════════════════
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config")
	FName MachineId = TEXT("Machine_01");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config", meta=(ClampMin="0.0"))
	float ProductionRate = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config", meta=(ClampMin="0.0"))
	float ChangeoverDuration = 30.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config", meta=(ClampMin="0.0", ClampMax="1.0"))
	float JamProbabilityPerTick = 0.05f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config", meta=(ClampMin="0.0"))
	float MeanJamDuration = 60.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ScrapRate = 0.05f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SlowSpeedFactor = 0.7f;

	// ═══════════════════════════════════════════════════════════════════════════
	// RUNTIME STATE (Modified by tasks during execution)
	// ═══════════════════════════════════════════════════════════════════════════
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|Production")
	float ProductionAccumulator = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|Production")
	int32 OutputCounter = 0;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|Production")
	int32 ScrapCounter = 0;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|State")
	float TimeInState = 0.0f;
	
	// ═══════════════════════════════════════════════════════════════════════════
	// WORK ORDER DATA (simplified - no cross-module FPraxisWorkOrder dependency)
	// ═══════════════════════════════════════════════════════════════════════════
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|WorkOrder")
	FString CurrentSKU;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|WorkOrder")
	int32 TargetQuantity = 0;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|WorkOrder")
	bool bHasActiveWorkOrder = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// TASK-SPECIFIC STATE
	// ═══════════════════════════════════════════════════════════════════════════
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|Jam")
	float JamDurationRemaining = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category="Runtime|Changeover")
	float ChangeoverTimeRemaining = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// UTILITIES
	// ═══════════════════════════════════════════════════════════════════════════
	
	void ResetProductionCounters()
	{
		ProductionAccumulator = 0.0f;
		OutputCounter = 0;
		ScrapCounter = 0;
	}
	
	int32 GetTotalUnitsProduced() const
	{
		return OutputCounter + ScrapCounter;
	}
	
	bool IsWorkOrderComplete() const
	{
		return bHasActiveWorkOrder && OutputCounter >= TargetQuantity;
	}
};
