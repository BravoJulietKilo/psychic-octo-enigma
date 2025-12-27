// Copyright 2025 Celsian Pty Ltd
// APraxisMachine - Lightweight visual actor representing a machine in the simulation.
// This actor delegates all simulation logic to UMachineLogicComponent.

#pragma once

// Forward declarations:
class UStaticMeshComponent;
class UMachineLogicComponent;

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PraxisMachine.generated.h"

class UMachineLogicComponent;

/**
 * APraxisMachine
 * -------------------------------------------------------------------
 * A minimal actor that provides the physical/visual representation
 * of a machine in the simulation. It owns:
 *   - A static mesh component for visuals.
 *   - A UMachineLogicComponent for simulation logic.
 *
 * Simulation ticking, randomization, and metrics are handled entirely
 * by UMachineLogicComponent. This actor only handles visuals and identity.
 */
UCLASS(Blueprintable)
class PRAXISSIMULATIONKERNEL_API APraxisMachine : public AActor
{
	GENERATED_BODY()

public:
	/** Constructor: sets default components. */
	APraxisMachine();

protected:
	/** Called when the game starts or the actor is spawned. */
	virtual void BeginPlay() override;

	/** Called when the actor is destroyed or the level ends. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ────────────────────────────────────────────────
	// Components
	// ────────────────────────────────────────────────

	/** The visible static mesh for this machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Visual")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Simulation logic brain (tick handling, random events, metrics). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Logic")
	TObjectPtr<UMachineLogicComponent> LogicComponent;

	// ────────────────────────────────────────────────
	// Configuration
	// ────────────────────────────────────────────────

	/** Unique name or ID of this machine (used in logs and metrics). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Config")
	FName MachineId = TEXT("Machine");

	// ────────────────────────────────────────────────
	// Runtime Control
	// ────────────────────────────────────────────────

	/** Enables or disables this machine’s simulation logic. */
	UFUNCTION(BlueprintCallable, Category="Machine|Control")
	void SetActive(bool bInActive);
};
