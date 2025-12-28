// Copyright 2025 Celsian Pty Ltd
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StateTreeReference.h"
#include "MachineLogicComponent.generated.h"

// Forward declarations
class UPraxisOrchestrator;
class UPraxisRandomService;
class UPraxisMetricsSubsystem;
class UStateTree;
class UStateTreeComponent;
class UMachineContextComponent;

/**
 * UMachineLogicComponent
 * 
 * Owns and drives a StateTree that controls machine behavior.
 * Creates a MachineContextComponent that StateTree tasks can bind to for state access.
 * Ticks the StateTree in response to simulation ticks from Orchestrator.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup=(Praxis), meta=(BlueprintSpawnableComponent))
class PRAXISSIMULATIONKERNEL_API UMachineLogicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMachineLogicComponent();

	// ═══════════════════════════════════════════════════════════════════════════
	// Public API
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Assign a work order to this machine */
	UFUNCTION(BlueprintCallable, Category = "Machine")
	void AssignWorkOrder(const FString& SKU, int32 Quantity);
	
	/** Get current machine state for debugging/monitoring */
	UFUNCTION(BlueprintCallable, Category = "Machine")
	FString GetCurrentStateName() const;
	
	/** Check if machine is currently processing */
	UFUNCTION(BlueprintPure, Category = "Machine")
	bool IsProcessing() const;
	
	/** Get the StateTree component (for advanced access) */
	UFUNCTION(BlueprintPure, Category = "Machine")
	UStateTreeComponent* GetStateTreeComponent() const { return StateTreeComponent; }
	
	/** Get the machine context component */
	UFUNCTION(BlueprintPure, Category = "Machine")
	UMachineContextComponent* GetMachineContextComponent() const { return MachineContextComponent; }

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// Lifecycle
	// ═══════════════════════════════════════════════════════════════════════════
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Orchestrator Callbacks
	// ═══════════════════════════════════════════════════════════════════════════
	
	UFUNCTION()
	void HandleSimTick(double SimDeltaSeconds, int32 TickCount);

	UFUNCTION()
	void HandleEndSession();

	// ═══════════════════════════════════════════════════════════════════════════
	// Initialization
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Initialize the machine context with configuration values */
	void InitializeMachineContext();

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// Configuration (Editable in Editor/Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Machine identifier (must be unique) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config")
	FName MachineId = TEXT("Machine_01");
	
	/** StateTree asset that defines machine behavior */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config")
	FStateTreeReference StateTreeRef;
	
	/** Base production rate (units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config", meta = (ClampMin = "0.1"))
	float ProductionRate = 1.0f;
	
	/** Changeover/setup time (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config", meta = (ClampMin = "0.0"))
	float ChangeoverDuration = 30.0f;
	
	/** Scrap rate (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScrapRate = 0.05f;
	
	/** Jam probability per tick (small value, e.g., 0.001) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JamProbabilityPerTick = 0.001f;
	
	/** Mean jam recovery duration (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config", meta = (ClampMin = "1.0"))
	float MeanJamDuration = 120.0f;
	
	/** Speed reduction when in "Slow" mode (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlowSpeedFactor = 0.5f;

	// ═══════════════════════════════════════════════════════════════════════════
	// Runtime State (Read-Only in Editor) - Mirrored from Context for debugging
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Current work order SKU */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Runtime")
	FString CurrentSKU;
	
	/** Current work order quantity */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Runtime")
	int32 CurrentQuantity = 0;
	
	/** Units produced (good) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Runtime")
	int32 OutputCounter = 0;
	
	/** Units scrapped */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Runtime")
	int32 ScrapCounter = 0;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// Service References
	// ═══════════════════════════════════════════════════════════════════════════
	
	UPROPERTY()
	TObjectPtr<UPraxisOrchestrator> Orchestrator = nullptr;

	UPROPERTY()
	TObjectPtr<UPraxisRandomService> RandomService = nullptr;

	UPROPERTY()
	TObjectPtr<UPraxisMetricsSubsystem> Metrics = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════
	// Components
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** StateTree component (created programmatically) */
	UPROPERTY()
	TObjectPtr<UStateTreeComponent> StateTreeComponent = nullptr;
	
	/** Machine context component (created programmatically) - provides state to StateTree */
	UPROPERTY()
	TObjectPtr<UMachineContextComponent> MachineContextComponent = nullptr;
};
