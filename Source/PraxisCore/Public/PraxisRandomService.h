// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PraxisSimulationKernel/Public/PraxisSimulationKernel.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PraxisRandomService.generated.h"

/**
 * Provides deterministic random sampling utilities for gameplay and simulation.
 * - Supports uniform and exponential random draws
 * - Blueprint-callable for lab use
 * - Backed by FRandomStream (seeded, reproducible)
 */
UCLASS(BlueprintType)
class PRAXISCORE_API UPraxisRandomService : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// Set a reproducible seed (optional, call from Orchestrator)
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	void Initialise(int32 InBaseSeed);
	// Called once per DES tick by the Orchestrator
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	void BeginTick(int32 InTickCount);

	// --- Stateless, order-independent draws (per key/channel) ---
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	int32 RandomInt_Key(const FName& Key, int32 Channel, int32 Min, int32 Max);

	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float Uniform_Key(const FName& Key, int32 Channel, float Min, float Max);

	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float ExponentialFromMean_Key(const FName& Key, int32 Channel, float Mean);

	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	bool  EventOccursInStep_Key(const FName& Key, int32 Channel, float Lambda, float DeltaT);

	// Integer [Min, Max]
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	int32 GenerateRandomInt(int32 Min, int32 Max);

	// Uniform float in [Min, Max]
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float GenerateUniformProbability(float Min, float Max);

	// Exponential random draw with mean μ (seconds, etc.)
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float GenerateExponentialFromMean(float Mean);

	// Bernoulli: returns true with probability p = 1 - e^{-λΔt}
	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	bool EventOccursInStep(float Lambda, float DeltaT);

	UFUNCTION(BlueprintCallable, Category="Praxis|Random")
	float GenerateExponentialProbability(float a, float b);

private:
	FRandomStream Stream;

	// Helpers (non-BP)
	double SampleExponential(double Lambda);

	// Derive a per-tick, per-key, per-channel stream without shared state.
	FRandomStream MakeDerivedStream(const FName& Key, int32 Channel) const;

	// Low-level helpers
	static double SampleExponential(FRandomStream& Rng, double Lambda);

private:
	int32 BaseSeed = 12345;
	int32 TickCount = 0;

	// Optional: a stateful stream for legacy/simple use (not order-independent).
	FRandomStream Stateful;
	
};