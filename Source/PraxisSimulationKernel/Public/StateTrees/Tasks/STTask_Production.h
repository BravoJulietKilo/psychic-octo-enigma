// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "STTask_Production.generated.h"

// Forward declarations
class UMachineContextComponent;
class UPraxisRandomService;
struct FPraxisMachineContext;

/**
 * Instance data for Production task
 */
USTRUCT()
struct FSTTask_ProductionInstanceData
{
	GENERATED_BODY()

	/** Reference to the machine context component (auto-discovered at runtime) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UMachineContextComponent> MachineContext = nullptr;
	
	/** Reference to the random service (for scrap determination) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UPraxisRandomService> RandomService = nullptr;
};

/**
 * STTask_Production
 * 
 * Handles production logic for a machine:
 * - Accumulates production progress based on ProductionRate × DeltaTime
 * - When accumulator >= 1.0, outputs a unit (good or scrap based on ScrapRate)
 * - Returns Succeeded when work order complete (OutputCounter >= TargetQuantity)
 * - Returns Running while still producing
 */
USTRUCT(BlueprintType, meta = (Category = "Praxis", DisplayName = "Production"))
struct PRAXISSIMULATIONKERNEL_API FSTTask_Production : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTask_ProductionInstanceData;

	FSTTask_Production() = default;

	// ═══════════════════════════════════════════════════════════════════════════
	// FStateTreeTaskBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
	/** Check if a produced unit should be scrapped based on scrap rate */
	bool ShouldScrapUnit(const FInstanceDataType& InstanceData, const FPraxisMachineContext& MachineCtx) const;
};
