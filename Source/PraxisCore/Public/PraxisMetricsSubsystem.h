// Copyright 2025 Celsian Pty Ltd
// UPraxisMetricsSubsystem - central aggregator for simulation metrics.
// Receives structured metric events from MachineLogicComponent and other systems,
// and stores or forwards them for later visualization or analysis.

#pragma once

#include "CoreMinimal.h"
#include "PraxisCore.h"  // for LogPraxisSim
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

    /** Type of event: e.g., StateChange, Production, Scrap, Changeover, Jam */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    FString EventType;

    /** Optional numeric value (e.g., quantity produced, duration, energy consumed) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    double Value = 0.0;

    /** Timestamp (UTC) of when the event occurred */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    FDateTime TimestampUTC;
    
    /** Optional context data (e.g., SKU, Work Order ID, State Name) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metrics")
    FString Context;
};

// ────────────────────────────────────────────────────────────────
// Aggregated Machine Statistics (for dashboards)
// ────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FPraxisMachineStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    FName MachineId;
    
    /** Total good units produced */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int32 TotalGoodUnits = 0;
    
    /** Total scrap units produced */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int32 TotalScrapUnits = 0;
    
    /** Number of completed work orders */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int32 CompletedWorkOrders = 0;
    
    /** Total time spent in Production state (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double ProductionTime = 0.0;
    
    /** Total time spent in Idle state (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double IdleTime = 0.0;
    
    /** Total time spent in Changeover state (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double ChangeoverTime = 0.0;
    
    /** Total time spent in Jammed state (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double JammedTime = 0.0;
    
    /** Number of jam incidents */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int32 JamCount = 0;
    
    /** Current state */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    FString CurrentState;
    
    /** Time entered current state */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    FDateTime StateStartTime;
    
    /** Calculated OEE (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double OEE = 0.0;
    
    /** Utilization (0.0 - 1.0) - time spent producing vs total time */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double Utilization = 0.0;
    
    /** Quality rate (0.0 - 1.0) - good units / total units */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    double QualityRate = 1.0;
};

// ────────────────────────────────────────────────────────────────
// Metrics Subsystem
// ────────────────────────────────────────────────────────────────
UCLASS()
class PRAXISCORE_API UPraxisMetricsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════════════════════════════
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Event Recording Interface (called by machines/operators)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /** Record a state change (Idle → Production, Production → Jammed, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordStateChange(FName MachineId, const FString& FromState, const FString& ToState, const FDateTime& Timestamp);
    
    /** Record production output (good units) */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordGoodProduction(FName MachineId, int32 Units, const FString& SKU, const FDateTime& Timestamp);
    
    /** Record scrap production */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordScrap(FName MachineId, int32 Units, const FString& SKU, const FDateTime& Timestamp);
    
    /** Record work order lifecycle event */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordWorkOrderEvent(FName MachineId, int64 WorkOrderID, const FString& EventType, const FDateTime& Timestamp);
    
    /** Record changeover start/completion */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordChangeover(FName MachineId, const FString& FromSKU, const FString& ToSKU, double Duration, const FDateTime& Timestamp);
    
    /** Record jam incident */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordJam(FName MachineId, double Duration, const FDateTime& Timestamp);

    /** Records a general machine event (start, stop, failure, etc.) - LEGACY */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordMachineEvent(FName MachineId, const FString& EventType, const FDateTime& Timestamp);

    /** Records a production metric (e.g., units produced per tick) - LEGACY */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void RecordProduction(FName MachineId, double Units, int32 TickCount);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Query Interface (for dashboards/UI)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /** Get aggregated statistics for a specific machine */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    FPraxisMachineStats GetMachineStats(FName MachineId) const;
    
    /** Get aggregated statistics for all machines */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    TArray<FPraxisMachineStats> GetAllMachineStats() const;
    
    /** Get raw event stream for a machine (for detailed analysis) */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    TArray<FPraxisMetricEvent> GetMachineEvents(FName MachineId) const;
    
    /** Get all events in the buffer */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    const TArray<FPraxisMetricEvent>& GetAllEvents() const { return MetricBuffer; }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Persistence & Export
    // ═══════════════════════════════════════════════════════════════════════════
    
    /** Flushes all buffered metrics to persistent storage or analytics sink */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    void FlushMetrics();
    
    /** Export metrics to CSV file */
    UFUNCTION(BlueprintCallable, Category = "Praxis|Metrics")
    bool ExportToCSV(const FString& FilePath);

protected:
    /** Internal helper to standardize event creation and timestamping */
    void AddEvent(FName SourceId, const FString& Type, double Value = 0.0, const FString& Context = "");
    
    /** Update aggregated statistics when an event is recorded */
    void UpdateAggregates(const FPraxisMetricEvent& Event);

private:
    /** In-memory store of metric events */
    UPROPERTY()
    TArray<FPraxisMetricEvent> MetricBuffer;
    
    /** Aggregated statistics per machine (for fast queries) */
    UPROPERTY()
    TMap<FName, FPraxisMachineStats> MachineStats;
};
