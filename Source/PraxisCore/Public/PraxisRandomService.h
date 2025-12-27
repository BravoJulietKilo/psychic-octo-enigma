// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PraxisSimulationKernel/Public/PraxisSimulationKernel.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PraxisRandomService.generated.h"

/**
 * UPraxisRandomService
 * 
 * Provides deterministic random sampling utilities for gameplay and simulation.
 * - Supports uniform and exponential random draws
 * - Blueprint-callable for lab use
 * - Backed by FRandomStream (seeded, reproducible)
 * 
 * ═══════════════════════════════════════════════════════════════════════════════
 * TWO MODES OF OPERATION
 * ═══════════════════════════════════════════════════════════════════════════════
 * 
 * 1. STATEFUL METHODS (GenerateRandomInt, etc.)
 *    - Draw sequentially from a shared Stateful stream
 *    - Call order matters for determinism
 *    - Simpler API, but requires strict execution order
 *    - Use when: Single-threaded, predictable call sequence
 * 
 * 2. STATELESS METHODS (*_Key)
 *    - Order-independent draws using Key/Channel/TickCount derivation
 *    - Each (Key, Channel, Tick) tuple generates an independent stream
 *    - Call order doesn't matter — parallel-safe
 *    - Use when: Multiple entities, parallel execution, or distributed systems
 * 
 * ═══════════════════════════════════════════════════════════════════════════════
 * CHANNEL USAGE GUIDELINES
 * ═══════════════════════════════════════════════════════════════════════════════
 * 
 * Channels separate independent random processes for the same entity (Key).
 * This prevents accidental correlation between logically independent events.
 * 
 * RECOMMENDED CHANNEL ASSIGNMENTS (customize per project needs):
 * 
 *   Channel 0:  Machine breakdowns / failures
 *   Channel 1:  Operator behavior / decisions
 *   Channel 2:  Quality defects / inspection results
 *   Channel 3:  Material arrival / supply chain
 *   Channel 4:  Processing time variations
 *   Channel 5:  Demand / customer orders
 *   Channel 6-9: Reserved for future expansion
 *   Channel 10+: Application-specific use
 * 
 * EXAMPLE USAGE:
 * 
 *   // Machine "Lathe_01" uses Channel 0 for breakdowns
 *   bool bBroken = Random->EventOccursInStep_Key(
 *       TEXT("Lathe_01"), 
 *       0,              // Channel 0 = breakdowns
 *       0.05f,          // λ = 0.05 failures/hour
 *       DeltaT
 *   );
 * 
 *   // Same machine uses Channel 4 for processing time variation
 *   float ProcessTime = Random->ExponentialFromMean_Key(
 *       TEXT("Lathe_01"),
 *       4,              // Channel 4 = processing time
 *       120.0f          // mean = 120 seconds
 *   );
 * 
 *   // These draws are statistically independent despite sharing a Key!
 * 
 * WHY CHANNELS MATTER:
 *   Without channels, all random events for an entity would share one stream.
 *   This can cause spurious correlations (e.g., "machines that break down more
 *   also happen to have faster processing times" purely due to RNG coupling).
 *   Channels eliminate this artifact.
 */
UCLASS(BlueprintType)
class PRAXISCORE_API UPraxisRandomService : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// ─── Initialization ──────────────────────────────────────────────────────────
	
	/** 
	 * Set a reproducible base seed (called automatically by Orchestrator).
	 * All derived streams are deterministic functions of this seed.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	void Initialise(int32 InBaseSeed);
	
	/** 
	 * Called once per DES tick by the Orchestrator.
	 * Updates TickCount used for stateless stream derivation.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	void BeginTick(int32 InTickCount);

	// ─── Stateless, order-independent draws (per key/channel) ───────────────────
	
	/**
	 * Generate random integer in [Min, Max] using stateless derivation.
	 * 
	 * @param Key     Entity identifier (e.g., "Machine_A", "Operator_5")
	 * @param Channel Random process type (0=breakdowns, 1=behavior, etc.)
	 * @param Min     Inclusive lower bound
	 * @param Max     Inclusive upper bound
	 * @return        Random integer in [Min, Max]
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	int32 RandomInt_Key(const FName& Key, int32 Channel, int32 Min, int32 Max);

	/**
	 * Generate uniform random float in [Min, Max] using stateless derivation.
	 * 
	 * @param Key     Entity identifier
	 * @param Channel Random process type (see class documentation)
	 * @param Min     Lower bound
	 * @param Max     Upper bound
	 * @return        Random float in [Min, Max]
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float Uniform_Key(const FName& Key, int32 Channel, float Min, float Max);

	/**
	 * Generate exponentially distributed random value with given mean.
	 * Common for inter-arrival times, service times, time-to-failure.
	 * 
	 * @param Key     Entity identifier
	 * @param Channel Random process type (e.g., 4 for processing time)
	 * @param Mean    Mean value (e.g., 120.0 seconds)
	 * @return        Random exponential sample (always > 0)
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float ExponentialFromMean_Key(const FName& Key, int32 Channel, float Mean);

	/**
	 * Determine if a Poisson event occurs in a discrete time step.
	 * Returns true with probability p = 1 - exp(-λΔt).
	 * 
	 * Used for modeling random events like failures, arrivals, defects.
	 * 
	 * @param Key     Entity identifier
	 * @param Channel Random process type (e.g., 0 for breakdowns)
	 * @param Lambda  Event rate (events per unit time)
	 * @param DeltaT  Time step duration
	 * @return        True if event occurs this step
	 * 
	 * Example: For λ=0.1 failures/hour and ΔT=5 seconds (0.00139 hours):
	 *          p ≈ 0.000139 (0.0139% chance per 5-second tick)
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	bool EventOccursInStep_Key(const FName& Key, int32 Channel, float Lambda, float DeltaT);

	// ─── Stateful sequential draws (order-dependent) ────────────────────────────
	
	/**
	 * Generate random integer in [Min, Max] from stateful stream.
	 * WARNING: Call order affects results. Use *_Key methods for parallel systems.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	int32 GenerateRandomInt(int32 Min, int32 Max);

	/**
	 * Generate uniform float in [Min, Max] from stateful stream.
	 * WARNING: Call order affects results. Use Uniform_Key for parallel systems.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float GenerateUniformProbability(float Min, float Max);

	/**
	 * Generate exponential random draw with mean μ from stateful stream.
	 * WARNING: Call order affects results. Use ExponentialFromMean_Key for parallel systems.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float GenerateExponentialFromMean(float Mean);

	/**
	 * Bernoulli event: returns true with probability p = 1 - e^{-λΔt}.
	 * WARNING: Call order affects results. Use EventOccursInStep_Key for parallel systems.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	bool EventOccursInStep(float Lambda, float DeltaT);

	/**
	 * DEPRECATED: Placeholder function. Use ExponentialFromMean_Key instead.
	 */
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float GenerateExponentialProbability(float a, float b);
	float GetUniformFloat(float X, float X1);
	float GetExponential(float MeanJamDuration);

private:
	// Helpers (non-BP)
	double SampleExponential(double Lambda);

	/**
	 * Derive an independent random stream from (BaseSeed, TickCount, Key, Channel).
	 * Uses FNV-1a hash mixing to ensure stream independence.
	 */
	FRandomStream MakeDerivedStream(const FName& Key, int32 Channel) const;

	// Low-level exponential sampling
	static double SampleExponential(FRandomStream& Rng, double Lambda);

private:
	int32 BaseSeed = 12345;
	int32 TickCount = 0;

	// Stateful stream for sequential/order-dependent draws
	FRandomStream Stateful;
	
};
