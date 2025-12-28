// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "STTask_TestIncrement.generated.h"

// Forward declarations
class UMachineContextComponent;

/**
 * Instance data for TestIncrement task
 */
USTRUCT()
struct FSTTask_TestIncrementInstanceData
{
	GENERATED_BODY()

	/** Reference to the machine context component (bound in StateTree editor) */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TObjectPtr<UMachineContextComponent> MachineContext = nullptr;
};

/**
 * STTask_TestIncrement
 * 
 * SIMPLE TEST TASK - Just increments the output counter each tick and logs.
 * Used to verify StateTree → Component binding is working correctly.
 * 
 * Once this works, we know the foundation is solid for real tasks.
 */
USTRUCT(BlueprintType, meta = (Category = "Praxis", DisplayName = "Test Increment Counter"))
struct PRAXISSIMULATIONKERNEL_API FSTTask_TestIncrement : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTask_TestIncrementInstanceData;

	FSTTask_TestIncrement() = default;

	// ═══════════════════════════════════════════════════════════════════════════
	// FStateTreeTaskBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
