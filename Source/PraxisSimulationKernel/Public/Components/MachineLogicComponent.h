// Copyright 2025 Celsian Pty Ltd
#pragma once

#include "CoreMinimal.h"
#include "PraxisOrchestrator.h"
#include "PraxisRandomService.h"
#include "PraxisMetricsSubsystem.h"
#include "Components/ActorComponent.h"
#include "MachineLogicComponent.generated.h"

// ────────────────────────────────────────────────────────────────────────────────
// Temporary metric struct (will be moved to PraxisMetricsSubsystem)
// ────────────────────────────────────────────────────────────────────────────────
// USTRUCT(BlueprintType)
// struct FPraxisMetricEvent
// {
// 	GENERATED_BODY()
//
// 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
// 	FName SourceId;
//
// 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
// 	FString Type;
//
// 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
// 	double Value = 0.0;
//
// 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
// 	FDateTime Timestamp = FDateTime::UtcNow();
// };

// ────────────────────────────────────────────────────────────────────────────────
// Machine Logic Component
// ────────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup=(Praxis), meta=(BlueprintSpawnableComponent))
class PRAXISSIMULATIONKERNEL_API UMachineLogicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMachineLogicComponent();

protected:
	// Lifecycle
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	// Tick handlers
	UFUNCTION()
	void HandleSimTick(double SimDeltaSeconds, int32 TickCount);

	UFUNCTION()
	void HandleEndSession();

	// Core simulation logic
	void SimulateProduction(double SimDeltaSeconds, int32 TickCount);
	void HandleFailure();

	// Metric recording
	void PublishMetrics(double SimDeltaSeconds, int32 TickCount);
	void RecordMachineStart();
	void RecordMachineStop();
	void RecordProductionTick(float UnitsThisStep, int32 TickCount);

protected:
	// ─── Configuration ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category="Machine|Config")
	FName MachineId = TEXT("UnnamedMachine");

	UPROPERTY(EditAnywhere, Category="Machine|Config", meta=(ClampMin="0.0"))
	float BaseProcessingRate = 1.0f;   // Units per second

	UPROPERTY(EditAnywhere, Category="Machine|Config", meta=(ClampMin="0.0"))
	float FailureProbability = 0.01f;  // Probability per tick

	UPROPERTY(EditAnywhere, Category="Machine|Config", meta=(ClampMin="0.0"))
	float YieldVariation = 0.05f;      // Random yield multiplier variation

	UPROPERTY(EditAnywhere, Category="Machine|Config")
	bool bActive = true;

	UPROPERTY(VisibleAnywhere, Category="Machine|Runtime")
	float WorkInProgress = 0.0f;

	UPROPERTY(VisibleAnywhere, Category="Machine|Runtime")
	float CurrentRate = 0.0f;

private:
	// ─── Service references ────────────────────────────────────────────────────
	UPROPERTY()
	TObjectPtr<UPraxisOrchestrator> Orchestrator = nullptr;

	UPROPERTY()
	TObjectPtr<UPraxisRandomService> RandomService = nullptr;

	UPROPERTY()
	TObjectPtr<UPraxisMetricsSubsystem> Metrics = nullptr;

	// ─── Runtime ───────────────────────────────────────────────────────────────
	UPROPERTY()
	TArray<FPraxisMetricEvent> MetricBuffer;
};
