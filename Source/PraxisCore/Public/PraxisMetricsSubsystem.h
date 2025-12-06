// Copyright 2025 Celsian Pty Ltd
// UPraxisMetricsSubsystem - central aggregator for simulation metrics.
// Receives structured metric events from MachineLogicComponent and other systems,
// and stores or forwards them for later visualization or analysis.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PraxisMetricsSubsystem.generated.h"

// ────────────────────────────────────────────────────────────────
// Metric Event Data
// ────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FPraxisMetricEvent
{
    GENERATED_BODY()

    /** Machine or Operator ID that generated this metric */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    FName SourceId;

    /** Type of event: e.g., Start, Stop, ProductionTick, Failure, etc. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    FString EventType;

    /** Optional numeric value (e.g., quantity produced, duration, energy consumed) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    double Value = 0.0;

    /** Timestamp (UTC) of when the event occurred */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    FDateTime TimestampUTC;
};

// ────────────────────────────────────────────────────────────────
// Metrics Subsystem
// ────────────────────────────────────────────────────────────────
UCLASS()
class PRAXISCORE_API UPraxisMetricsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ─── Lifecycle ───────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ─── Metrics Recording Interface ─────────────────────────────
    /** Records a general machine event (start, stop, failure, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordMachineEvent(FName MachineId, const FString& EventType, const FDateTime& Timestamp);

    /** Records a production metric (e.g., units produced per tick) */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordProduction(FName MachineId, double Units, int32 TickCount);

    /** Flushes all buffered metrics to persistent storage or analytics sink */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void FlushMetrics();

protected:
    /** Internal helper to standardize event creation and timestamping */
    void AddEvent(FName SourceId, const FString& Type, double Value = 0.0);

private:
    /** In-memory store of metric events (to be replaced by database or stream output) */
    UPROPERTY()
    TArray<FPraxisMetricEvent> MetricBuffer;
};