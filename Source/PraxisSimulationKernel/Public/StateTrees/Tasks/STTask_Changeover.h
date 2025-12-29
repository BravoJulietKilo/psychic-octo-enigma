// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "STTask_Changeover.generated.h"

// Forward declarations
class UMachineContextComponent;
struct FPraxisMachineContext;

/**
 * Instance data for Changeover task
 */
USTRUCT()
struct FSTTask_ChangeoverInstanceData
{
	GENERATED_BODY()

	/** Reference to the machine context component (auto-discovered at runtime) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UMachineContextComponent> MachineContext = nullptr;
};

/**
 * STTask_Changeover
 * 
 * Handles machine changeover/setup when switching between products or starting production.
 * Counts down the ChangeoverDuration and transitions when complete.
 * 
 * Typical flow: Idle → Changeover → Production
 */
USTRUCT(BlueprintType, meta = (Category = "Praxis", DisplayName = "Changeover"))
struct PRAXISSIMULATIONKERNEL_API FSTTask_Changeover : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTask_ChangeoverInstanceData;

	FSTTask_Changeover() = default;

	// ═══════════════════════════════════════════════════════════════════════════
	// FStateTreeTaskBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
