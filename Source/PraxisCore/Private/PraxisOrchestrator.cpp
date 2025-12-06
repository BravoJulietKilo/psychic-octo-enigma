// Copyright 2025 Celsian Pty Ltd

#include "PraxisOrchestrator.h"

#include "Engine/World.h"
#include "TimerManager.h"

// If you have these headers in different modules, adjust include paths accordingly.
#include "PraxisSimulationKernel/Public/PraxisSimulationKernel.h"
#include "PraxisScheduleService.h"   // UPraxisScheduleService
#include "PraxisInventoryService.h"        // UInventoryService
#include "PraxisMetricsSubsystem.h"  // UPraxisMetricsSubsystem
#include "PraxisRandomService.h"          // URandomService

// ───────────────────────────────────────────────────────────────────────────────
// Public API
// ───────────────────────────────────────────────────────────────────────────────

/**
 * Initiates the orchestration process by setting the initial phase, resolving required services,
 * and applying default configurations from the manifest. Once the setup is complete, signals are
 * broadcast to indicate readiness and begins the orchestration session.
 *
 * Execution steps include:
 * - Transitioning to the "Init" phase and notifying listeners via the OnPhaseChanged event.
 * - Resolving dependencies or services required for orchestration.
 * - Applying default configurations from the manifest for simulation consistency.
 * - Broadcasting the OnOrchestrationReady event to signal readiness.
 * - Starting the orchestration session by invoking the BeginSession method.
 */
void UPraxisOrchestrator::Start()
{

	// Phase: Init
	Phase = TEXT("Init");
	OnPhaseChanged.Broadcast(Phase);

	ResolveServices();
	ApplyManifestDefaults();

	// Signal readiness (subsystems located, manifest defaults applied)
	OnOrchestrationReady.Broadcast();

	BeginSession();
}

/**
 *
 */
void UPraxisOrchestrator::Pause()
{
	if (bPaused || Phase != TEXT("Run"))
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator Pause() ignored (already paused or not running)."));
		return;
	}

	bPaused = true;
	Phase = TEXT("Pause");
	OnPhaseChanged.Broadcast(Phase);
    OnPaused.Broadcast();
	
	// Stop the tick timer, but keep state intact
	FixedStep_StopTimer();

	UE_LOG(LogTemp, Warning, TEXT("Orchestrator paused at %s (tick %d)."), *SimClockUTC.ToString(), TickCount);
}

/**
 *
 */
void UPraxisOrchestrator::Resume()
{
	if (!bPaused || Phase != TEXT("Pause"))
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator Resume() ignored (not paused)."));
		return;
	}

	bPaused = false;
	Phase = TEXT("Run");
	OnPhaseChanged.Broadcast(Phase);
	OnResumed.Broadcast();
	
	// Restart timer at same cadence; tick count continues
	FixedStep_StartTimer();

	UE_LOG(LogTemp, Warning, TEXT("Orchestrator resumed at %s (tick %d)."), *SimClockUTC.ToString(), TickCount);
}

/**
 *
 */
void UPraxisOrchestrator::Stop()
{
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator Stop() invoked."));
	EndSession();
}

/**
 * Sets the simulation speed multiplier for instructor controls, allowing simulation time
 * progression to be adjusted without altering the fixed step interval.
 *
 * If the multiplier is different from the current value, it will be clamped to a minimum
 * value of 0.25 to prevent excessively small intervals. The timer is restarted with the
 * new simulation speed if it is already active.
 *
 * @param InMultiplier The desired simulation speed multiplier (e.g., 1×, 2×, 4×).
 */
// The step size (TickIntervalSeconds) itself remains fixed for determinism.
void UPraxisOrchestrator::SetInstructorSimSpeedMultiplier(float InMultiplier)
{
	const float Clamped = FMath::Max(0.25f, InMultiplier); // avoid excessively small intervals
	if (FMath::IsNearlyEqual(SimSpeedMultiplier, Clamped))
	{
		return;
	}
	SimSpeedMultiplier = Clamped;

	// If timer is running, restart it with new cadence.
	if (const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		if (World->GetTimerManager().IsTimerActive(FixedStepTimer))
		{
			FixedStep_StopTimer();
			FixedStep_StartTimer();
		}
	}
}

// ───────────────────────────────────────────────────────────────────────────────
// Private: Fixed-step loop
// ───────────────────────────────────────────────────────────────────────────────

/**
 * Starts a fixed-step timer for the simulation loop.
 *
 * This function initializes and sets up a repeating timer using the Unreal Engine timer system.
 * The timer is configured based on the simulation's tick interval (`TickIntervalSeconds`)
 * and an optional simulation speed multiplier (`SimSpeedMultiplier`).
 * It ensures that the simulation loop advances in fixed steps, independent of frame rate or other dynamics.
 *
 * If the simulation is paused or not properly initialized (i.e., no valid world context is found),
 * the timer will not be started.
 *
 * The calculated cadence of the timer is determined by:
 * TickIntervalSeconds / FMath::Max(SimSpeedMultiplier, KINDA_SMALL_NUMBER)
 * This ensures that the simulation functions smoothly even if SimSpeedMultiplier is extremely small.
 *
 * The timer executes the FixedStep_OnTick() method at regular intervals.
 *
 * Preconditions:
 * - The simulation must have a valid world context (retrieved via GetGameInstance()->GetWorld()).
 *
 * Side Effects:
 * - A timer is created and assigned to the FixedStepTimer handle.
 * - FixedStep_OnTick() will be called repeatedly at the calculated cadence.
 */
void UPraxisOrchestrator::FixedStep_StartTimer()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("FixedStep_StartTimer: No valid world! Timer not started."));
		return;
	}

	const double Cadence = TickIntervalSeconds / FMath::Max(SimSpeedMultiplier, KINDA_SMALL_NUMBER);
	UE_LOG(LogTemp, Warning, TEXT("FixedStep_StartTimer: cadence=%.3f seconds, looping timer bound to world: %s"),
		   Cadence, *World->GetName());

	World->GetTimerManager().SetTimer(
		FixedStepTimer,
		this,
		&UPraxisOrchestrator::FixedStep_OnTick,
		Cadence,
		true
	);

	if (UWorld* W = GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s World: %s"), *GetName(), *W->GetName());
	}
	
}

/**
 * Stops the fixed-step timer in the orchestrator.
 *
 * If a valid game instance and world are available, this method clears the timer
 * referenced by FixedStepTimer from the game's timer manager. Typically used to halt
 * the fixed-step simulation loop when it is no longer needed or is being paused/stopped.
 */
void UPraxisOrchestrator::FixedStep_StopTimer()
{
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(FixedStepTimer);
	}
}

/**
 *
 */
void UPraxisOrchestrator::FixedStep_OnTick()
{
	// Respect pause without destroying cadence
	if (bPaused)
	{
		return;
	}

	// Advance deterministic DES step
	++TickCount;
	AdvanceSimClock(TickIntervalSeconds);

	// Notify RNG of tick boundary (order-independent per-key streams)
	if (Random)
	{
		Random->BeginTick(TickCount);
	}

	// Broadcast the fixed-step tick to listeners (Schedule, Inventory, Metrics, UI, etc.)
	OnSimTick.Broadcast(TickIntervalSeconds, TickCount);

	UE_LOG(LogTemp, Warning, TEXT("OnSimTick.Broadcast() with %d listeners"), OnSimTick.GetAllObjects().Num());

}

/**
 *
 */
void UPraxisOrchestrator::AdvanceSimClock(double StepSeconds)
{
	SimClockUTC += FTimespan::FromSeconds(StepSeconds);
}

// ───────────────────────────────────────────────────────────────────────────────
// Private: Boot/session wiring
// ───────────────────────────────────────────────────────────────────────────────

/**
 *
 */
void UPraxisOrchestrator::ResolveServices()
{
	// Locate subsystems. (They may be null if not part of the build yet.)
	if (UGameInstance* GI = GetGameInstance())
	{
		Schedule = GI->GetSubsystem<UPraxisScheduleService>();
		Metrics  = GI->GetSubsystem<UPraxisMetricsSubsystem>();
		Random   = GI->GetSubsystem<UPraxisRandomService>();
	}

	// Inventory is world-scoped
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		Inventory = World->GetSubsystem<UPraxisInventoryService>();
	}
}

/**
 * Applies default values to certain member variables of UPraxisOrchestrator
 * based on safe assumptions or configurations if they are not already initialized.
 * This function is expected to be called during the initialization phase.
 *
 * The method ensures:
 * - CourseStartUTC is set to the current UTC time if it is uninitialized.
 * - SimClockUTC is synced to CourseStartUTC.
 * - TickIntervalSeconds is clamped to a minimum sane value.
 * - If a randomization service exists and a seed is not already set, it initializes the RNG with a derived seed.
 */

void UPraxisOrchestrator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Safe to call here:
	// Create internal state, schedule delayed startup, bind events
	// DO NOT call GetWorld(), GetGameInstance(), or other subsystems yet.
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator subsystem initialized (boot phase)."));

	// Optionally queue Start() after world creation
	if (UGameInstance* GI = GetGameInstance())
	{
		// Defer Start until first world tick
		GI->GetTimerManager().SetTimerForNextTick([this]()
		{
			Start(); // safe: world + all subsystems are now valid
		});
	}
}

void UPraxisOrchestrator::ApplyManifestDefaults()
{
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator ApplyManifestDefaults"));
	// In a later pass, pull these from your Scenario Manifest.
	// For now, keep safe defaults if not set externally.

	// If CourseStartUTC not initialized, set a reasonable default (e.g., "now")
	if (!CourseStartUTC.GetTicks())
	{
		CourseStartUTC = FDateTime::UtcNow();
	}
	SimClockUTC = CourseStartUTC;

	// Tick interval: keep whatever was configured on the instance; clamp to sane minimum
	TickIntervalSeconds = FMath::Max(0.01f, TickIntervalSeconds);

	// Seed RNG if available (optional; set a deterministic base seed here if desired)
	if (Random)
	{
		// Example: derive from date for dev; replace with manifest seed later
		const int32 Seed = static_cast<int32>(CourseStartUTC.ToUnixTimestamp() & 0x7FFFFFFF);
		Random->Initialise(Seed);
	}
}

/**
 *
 */
void UPraxisOrchestrator::BeginSession()
{
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator BeginSession: Starting new session."));

	// ── Guard: Avoid double start ────────────────────────────────────────────────
	if (Phase == TEXT("Run"))
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator BeginSession called while already running."));
		return;
	}

	// ── Phase & runtime reset ────────────────────────────────────────────────────
	Phase     = TEXT("Run");
	bPaused   = false;
	TickCount = 0;
	SimClockUTC = CourseStartUTC;     // reset to manifest start
	OnPhaseChanged.Broadcast(Phase);

	// ── Random & dependent services ──────────────────────────────────────────────
	if (Random)
	{
		// Use course start timestamp for deterministic seed derivation
		const int32 Seed = static_cast<int32>(CourseStartUTC.ToUnixTimestamp() & 0x7FFFFFFF);
		Random->Initialise(Seed);
		Random->BeginTick(0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator BeginSession: Random service not found."));
	}

	if (Schedule)
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator BeginSession: Schedule service ready."));
		// later: Schedule->ResetActiveOrders();
	}

	if (Metrics)
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator BeginSession: Metrics subsystem ready."));
		// later: Metrics->Reset();
	}

	// ── Broadcast lifecycle events ───────────────────────────────────────────────
	OnBeginSession.Broadcast();

	// ── Start fixed-step simulation loop ─────────────────────────────────────────
	FixedStep_StartTimer();

	UE_LOG(LogTemp, Warning, TEXT("Orchestrator BeginSession complete. Tick timer started."));
}


/**
 *
 */
void UPraxisOrchestrator::EndSession()
{
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession: terminating current simulation session."));

	// ── Guard: Only end if actually running ──────────────────────────────────────
	if (Phase != TEXT("Run") && Phase != TEXT("Pause"))
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession called outside of active session (phase: %s)."), *Phase.ToString());
		return;
	}

	// ── Stop fixed-step timer ────────────────────────────────────────────────────
	FixedStep_StopTimer();

	// ── Freeze RNG state (optional) ──────────────────────────────────────────────
	if (Random)
	{
		// Random->EndTick();      // optional — define if you need tick finalization
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession: Random service tick finalized."));
	}

	// ── Notify dependent systems ────────────────────────────────────────────────
	if (Schedule)
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession: Notifying Schedule service."));
		// later: Schedule->FinalizeSession();
	}

	if (Metrics)
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession: Flushing metrics."));
		// later: Metrics->FlushSessionData(SimClockUTC);
	}

	if (Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession: Finalizing inventory state."));
		// later: Inventory->CommitFinalState();
	}

	// ── Broadcast lifecycle events ──────────────────────────────────────────────
	OnEndSession.Broadcast();

	// ── Update orchestrator state ────────────────────────────────────────────────
	Phase   = TEXT("End");
	bPaused = true;

	OnPhaseChanged.Broadcast(Phase);

	UE_LOG(LogTemp, Warning, TEXT("Orchestrator EndSession complete. Simulation halted at %s after %d ticks."),
		   *SimClockUTC.ToString(), TickCount);
}


void UPraxisOrchestrator::Deinitialize()
{
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator Deinitialize: shutting down subsystem."));

	// Ensure we end cleanly (stops timer, broadcasts end)
	EndSession();

	Super::Deinitialize();
	UE_LOG(LogTemp, Warning, TEXT("Orchestrator deinitialized successfully."));
}