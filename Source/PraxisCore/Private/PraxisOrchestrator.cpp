// Copyright 2025 Celsian Pty Ltd

#include "PraxisOrchestrator.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "PraxisSimulationKernel/Public/PraxisSimulationKernel.h"
#include "PraxisScheduleService.h" 
#include "PraxisInventoryService.h" 
#include "PraxisMetricsSubsystem.h"
#include "PraxisRandomService.h"

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
	
	// Initialization
	UE_LOG(LogPraxisSim, Log, TEXT("Orchestrator subsystem initialized"));
}

/**
 *
 */
void UPraxisOrchestrator::Pause()
{
	if (bPaused || Phase != TEXT("Run"))
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator Pause() ignored (already paused or not running)."));
		return;
	}

	bPaused = true;
	Phase = TEXT("Pause");
	OnPhaseChanged.Broadcast(Phase);
    OnPaused.Broadcast();
	
	// Stop the tick timer, but keep state intact
	FixedStep_StopTimer();

	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator paused at %s (tick %d)."), *SimClockUTC.ToString(), TickCount);
}

/**
 *
 */
void UPraxisOrchestrator::Resume()
{
	if (!bPaused || Phase != TEXT("Pause"))
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator Resume() ignored (not paused)."));
		return;
	}

	bPaused = false;
	Phase = TEXT("Run");
	OnPhaseChanged.Broadcast(Phase);
	OnResumed.Broadcast();
	
	// Restart timer at same cadence; tick count continues
	FixedStep_StartTimer();

	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator resumed at %s (tick %d)."), *SimClockUTC.ToString(), TickCount);
}

/**
 *
 */
void UPraxisOrchestrator::Stop()
{
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator Stop() invoked."));
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
	// Use GameInstance TimerManager for persistence across level transitions.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (GI->GetTimerManager().IsTimerActive(FixedStepTimer))
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
 * This function initializes and sets up a repeating timer using the GameInstance's timer system.
 * The timer is configured based on the simulation's tick interval (`TickIntervalSeconds`)
 * and an optional simulation speed multiplier (`SimSpeedMultiplier`).
 * It ensures that the simulation loop advances in fixed steps, independent of frame rate or other dynamics.
 *
 * Using GameInstance's TimerManager (rather than World's) ensures the timer persists across
 * level transitions and PIE restarts, which is appropriate for a GameInstanceSubsystem.
 *
 * If the simulation is paused or not properly initialized (i.e., no valid GameInstance is found),
 * the timer will not be started.
 *
 * The calculated cadence of the timer is determined by:
 * TickIntervalSeconds / FMath::Max(SimSpeedMultiplier, KINDA_SMALL_NUMBER)
 * This ensures that the simulation functions smoothly even if SimSpeedMultiplier is extremely small.
 *
 * The timer executes the FixedStep_OnTick() method at regular intervals.
 *
 * Preconditions:
 * - The simulation must have a valid GameInstance context.
 *
 * Side Effects:
 * - A timer is created and assigned to the FixedStepTimer handle.
 * - FixedStep_OnTick() will be called repeatedly at the calculated cadence.
 */
void UPraxisOrchestrator::FixedStep_StartTimer()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("FixedStep_StartTimer: No valid GameInstance! Timer not started."));
		return;
	}

	const double Cadence = TickIntervalSeconds / FMath::Max(SimSpeedMultiplier, KINDA_SMALL_NUMBER);
	UE_LOG(LogPraxisSim, Warning, TEXT("FixedStep_StartTimer: cadence=%.3f seconds, timer bound to GameInstance"),
		   Cadence);

	GI->GetTimerManager().SetTimer(
		FixedStepTimer,
		this,
		&UPraxisOrchestrator::FixedStep_OnTick,
		Cadence,
		true  // looping
	);
}

/**
 * Stops the fixed-step timer in the orchestrator.
 *
 * Clears the timer from the GameInstance's timer manager. Using GameInstance's TimerManager
 * ensures proper cleanup that persists across level transitions.
 */
void UPraxisOrchestrator::FixedStep_StopTimer()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(FixedStepTimer);
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

	UE_LOG(LogPraxisSim, Warning, TEXT("OnSimTick.Broadcast() with %d listeners"), OnSimTick.GetAllObjects().Num());

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
 * Resolves and validates all subsystem dependencies required by the Orchestrator.
 *
 * Service criticality:
 * - REQUIRED: Random (deterministic RNG for simulation)
 * - OPTIONAL: Schedule, Metrics (production/logistics features)
 * - OPTIONAL: Inventory (world-scoped, may not exist during initialization)
 *
 * If critical services are missing, logs an error with remediation steps.
 * Optional services log warnings when unavailable but don't block startup.
 */
void UPraxisOrchestrator::ResolveServices()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Orchestrator ResolveServices: No GameInstance! Cannot resolve any services."));
		return;
	}

	// ── Critical Services (GameInstance-scoped) ──────────────────────────────────
	Random = GI->GetSubsystem<UPraxisRandomService>();
	if (!Random)
	{
		UE_LOG(LogPraxisSim, Error, 
			TEXT("Orchestrator ResolveServices: CRITICAL - UPraxisRandomService not found!\n")
			TEXT("  → Check that PraxisCore module is loaded\n")
			TEXT("  → Verify UPraxisRandomService is registered as a GameInstanceSubsystem\n")
			TEXT("  → Simulation will fail without deterministic RNG"));
	}
	else
	{
		UE_LOG(LogPraxisSim, Log, TEXT("Orchestrator ResolveServices: Random service resolved ✓"));
	}

	// ── Optional Services (GameInstance-scoped) ──────────────────────────────────
	Schedule = GI->GetSubsystem<UPraxisScheduleService>();
	if (!Schedule)
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Orchestrator ResolveServices: UPraxisScheduleService not found (optional).\n")
			TEXT("  → Work order scheduling will be unavailable\n")
			TEXT("  → Check PraxisCore module if this is unexpected"));
	}
	else
	{
		UE_LOG(LogPraxisSim, Log, TEXT("Orchestrator ResolveServices: Schedule service resolved ✓"));
	}

	Metrics = GI->GetSubsystem<UPraxisMetricsSubsystem>();
	if (!Metrics)
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Orchestrator ResolveServices: UPraxisMetricsSubsystem not found (optional).\n")
			TEXT("  → Performance metrics and analytics will be unavailable\n")
			TEXT("  → Check PraxisCore module if this is unexpected"));
	}
	else
	{
		UE_LOG(LogPraxisSim, Log, TEXT("Orchestrator ResolveServices: Metrics subsystem resolved ✓"));
	}

	// ── World-scoped Services ────────────────────────────────────────────────────
	// Inventory is world-scoped and may not exist during early initialization.
	// We'll try to resolve it but won't error if unavailable yet.
	if (UWorld* World = GI->GetWorld())
	{
		Inventory = World->GetSubsystem<UPraxisInventoryService>();
		if (!Inventory)
		{
			UE_LOG(LogPraxisSim, Warning, 
				TEXT("Orchestrator ResolveServices: UPraxisInventoryService not found (optional).\n")
				TEXT("  → Inventory tracking will be unavailable\n")
				TEXT("  → This is normal if called before world is fully initialized\n")
				TEXT("  → Check PraxisCore module if this persists after BeginPlay"));
		}
		else
		{
			UE_LOG(LogPraxisSim, Log, TEXT("Orchestrator ResolveServices: Inventory service resolved ✓"));
		}
	}
	else
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Orchestrator ResolveServices: No World available yet.\n")
			TEXT("  → World-scoped subsystems (Inventory) will be unavailable\n")
			TEXT("  → This is normal during GameInstance initialization"));
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
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator subsystem initialized (boot phase)."));

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
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator ApplyManifestDefaults"));
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
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator BeginSession: Starting new session."));

	// ── Guard: Avoid double start ────────────────────────────────────────────────
	if (Phase == TEXT("Run"))
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator BeginSession called while already running."));
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
		UE_LOG(LogPraxisSim, Error, TEXT("Orchestrator BeginSession: CRITICAL - Random service not available! Simulation cannot proceed deterministically."));
	}

	if (Schedule)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator BeginSession: Schedule service ready."));
		// later: Schedule->ResetActiveOrders();
	}

	if (Metrics)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator BeginSession: Metrics subsystem ready."));
		// later: Metrics->Reset();
	}

	// ── Broadcast lifecycle events ───────────────────────────────────────────────
	OnBeginSession.Broadcast();

	// ── Start fixed-step simulation loop ─────────────────────────────────────────
	FixedStep_StartTimer();

	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator BeginSession complete. Tick timer started."));
}


/**
 *
 */
void UPraxisOrchestrator::EndSession()
{
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession: terminating current simulation session."));

	// ── Guard: Only end if actually running ──────────────────────────────────────
	if (Phase != TEXT("Run") && Phase != TEXT("Pause"))
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession called outside of active session (phase: %s)."), *Phase.ToString());
		return;
	}

	// ── Stop fixed-step timer ────────────────────────────────────────────────────
	FixedStep_StopTimer();

	// ── Freeze RNG state (optional) ──────────────────────────────────────────────
	if (Random)
	{
		// Random->EndTick();      // optional — define if you need tick finalization
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession: Random service tick finalized."));
	}

	// ── Notify dependent systems ────────────────────────────────────────────────
	if (Schedule)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession: Notifying Schedule service."));
		// later: Schedule->FinalizeSession();
	}

	if (Metrics)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession: Flushing metrics."));
		// later: Metrics->FlushSessionData(SimClockUTC);
	}

	if (Inventory)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession: Finalizing inventory state."));
		// later: Inventory->CommitFinalState();
	}

	// ── Broadcast lifecycle events ──────────────────────────────────────────────
	OnEndSession.Broadcast();

	// ── Update orchestrator state ────────────────────────────────────────────────
	Phase   = TEXT("End");
	bPaused = true;

	OnPhaseChanged.Broadcast(Phase);

	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator EndSession complete. Simulation halted at %s after %d ticks."),
		   *SimClockUTC.ToString(), TickCount);
}


void UPraxisOrchestrator::Deinitialize()
{
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator Deinitialize: shutting down subsystem."));

	// Ensure we end cleanly (stops timer, broadcasts end)
	EndSession();

	Super::Deinitialize();
	UE_LOG(LogPraxisSim, Warning, TEXT("Orchestrator deinitialized successfully."));
}
