// Copyright Celsian Pty Ltd 2025
// ───── Orchestrator ─────────────────────────────────────────────────────────────
/**
 * UPraxisOrchestrator
 * - Single source of truth for simulation phase & time.
 * - Fixed-step DES ticker (e.g., 5s/step) configured via Scenario Manifest.
 * - Emits Begin/EndSession and interval ticks; other systems PULL state on tick.
 * - Delegates scenario seeding to UScenarioSeeder and runtime work to services:
 *   UPraxisScheduleService, UInventoryService, UPraxisMetricsSubsystem, URandomService.
 * - Deterministic: no frame-delta coupling; tick interval cannot be changed at runtime in labs.
 */

#pragma once

#include "CoreMinimal.h"
// #include "PraxisSimulationKernel/Public/PraxisSimulationKernel.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PraxisOrchestrator.generated.h"

// ───── Events ───────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPraxisOnOrchestrationReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPraxisOnSimTick, double, SimDeltaSeconds, int32, TickCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPraxisOnPhaseChanged, FName, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPraxisOnBeginSession);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPraxisOnEndSession);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPraxisOnPaused);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPraxisOnResumed);

UCLASS(Blueprintable)
class PRAXISCORE_API UPraxisOrchestrator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "Praxis|Orchestrator")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "Praxis|Orchestrator")
	void Pause();

	UFUNCTION(BlueprintCallable, Category = "Praxis|Orchestrator")
	void Resume();

	UFUNCTION(BlueprintCallable, Category = "Praxis|Orchestrator")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Praxis|Orchestrator")
	FDateTime GetSimDateTimeUTC() const {return SimClockUTC;}

	UFUNCTION(BlueprintPure, Category = "Praxis|Orchestrator")
	float GetTickIntervalSeconds() const {return TickIntervalSeconds;}

	/** Number of DES ticks since Start(). */
	UFUNCTION(BlueprintPure, Category="Praxis|Orchestrator")
	int32 GetTickCount() const { return TickCount; }

	/** Current phase: Init | Run | Pause | End. */
	UFUNCTION(BlueprintPure, Category="Praxis|Orchestrator")
	FName GetPhase() const { return Phase; }

	/** True if the fixed-step loop is paused. */
	UFUNCTION(BlueprintPure, Category="Praxis|Orchestrator")
	bool IsPaused() const { return bPaused; }

	// ── Events (BP-bindable) ────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnOrchestrationReady OnOrchestrationReady;

	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnBeginSession OnBeginSession;

	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnEndSession OnEndSession;

	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnSimTick OnSimTick;

	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnPaused OnPaused;

	UPROPERTY(BlueprintAssignable, Category="Praxis|Orchestrator")
	FPraxisOnResumed OnResumed;

	public: // instructor controls (optional; not student-facing)
	/** Instructor-only: multiply *simulation time progression* without changing fixed step. (e.g., 1×, 2×, 4×) */
	UFUNCTION(BlueprintCallable, Category="Praxis|Orchestrator")
	void SetInstructorSimSpeedMultiplier(float InMultiplier);

private:
	// ── Fixed-step loop ─────────────────────────────────────────────────────────
	void FixedStep_StartTimer();          // schedule repeating timer at TickIntervalSeconds / SimSpeedMultiplier
	void FixedStep_StopTimer();
	void FixedStep_OnTick();              // advances sim by exactly TickIntervalSeconds
	void AdvanceSimClock(double StepSeconds);

	// ── Boot wiring ─────────────────────────────────────────────────────────────
	void ResolveServices();
	void Initialize(FSubsystemCollectionBase& Collection);
	void ApplyManifestDefaults();         // sets TickIntervalSeconds, SimClockUTC (course-time default), etc.
	void BeginSession();
	void EndSession();
	void Deinitialize();

private:
	// Services (not owned)
	UPROPERTY()
	TObjectPtr<class UPraxisScheduleService> Schedule = nullptr;
	UPROPERTY()
	TObjectPtr<class UPraxisInventoryService> Inventory = nullptr;
	UPROPERTY()
	TObjectPtr<class UPraxisMetricsSubsystem> Metrics = nullptr;
	UPROPERTY()
	TObjectPtr<class UPraxisRandomService> Random = nullptr;
	
private:
	// Config from manifest
	float     TickIntervalSeconds = 5.f;  // fixed DES step (immutable for students)
	FDateTime CourseStartUTC;             // default course start; applied to SimClockUTC on Start

	// Runtime state
	FTimerHandle FixedStepTimer;
	float     SimSpeedMultiplier = 1.f;   // instructor-only time accel (1× default)
	int32     TickCount = 0;
	bool      bPaused = false;
	FName     Phase = TEXT("Init");
	FDateTime SimClockUTC;                // authoritative sim clock (UTC)
};