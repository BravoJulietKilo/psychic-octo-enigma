// Copyright 2025 Celsian Pty Ltd
// UPraxisMetricsSubsystem - central aggregator for simulation metrics
// Receives structured metric events from MachineLogicComponent and other systems
// Persists, logs, or streams them to UI or external analytics sinks

#include "PraxisMetricsSubsystem.h"
#include "PraxisCore.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ════════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogPraxisSim, Log, TEXT("Metrics subsystem initialized."));
    MetricBuffer.Reserve(1024); // Reserve for efficiency
}

void UPraxisMetricsSubsystem::Deinitialize()
{
    UE_LOG(LogPraxisSim, Log, TEXT("Metrics subsystem deinitializing. Flushing %d events."), MetricBuffer.Num());
    FlushMetrics();
    MetricBuffer.Empty();
    MachineStats.Empty();
    Super::Deinitialize();
}

// ════════════════════════════════════════════════════════════════════════════════
// Event Recording Interface
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisMetricsSubsystem::RecordStateChange(
    FName MachineId, 
    const FString& FromState, 
    const FString& ToState, 
    const FDateTime& Timestamp)
{
    FString Context = FString::Printf(TEXT("%s → %s"), *FromState, *ToState);
    AddEvent(MachineId, TEXT("StateChange"), 0.0, Context);
    
    // Update machine stats
    FPraxisMachineStats& Stats = MachineStats.FindOrAdd(MachineId);
    Stats.MachineId = MachineId;
    
    // Calculate time in previous state
    if (!Stats.StateStartTime.GetTicks() == 0) // Has previous state
    {
        const double Duration = (Timestamp - Stats.StateStartTime).GetTotalSeconds();
        
        if (FromState == TEXT("Production"))
        {
            Stats.ProductionTime += Duration;
        }
        else if (FromState == TEXT("Idle"))
        {
            Stats.IdleTime += Duration;
        }
        else if (FromState == TEXT("Changeover"))
        {
            Stats.ChangeoverTime += Duration;
        }
        else if (FromState == TEXT("Jammed"))
        {
            Stats.JammedTime += Duration;
        }
    }
    
    // Update current state
    Stats.CurrentState = ToState;
    Stats.StateStartTime = Timestamp;
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s state change: %s"), 
        *MachineId.ToString(), *Context);
}

void UPraxisMetricsSubsystem::RecordGoodProduction(
    FName MachineId, 
    int32 Units, 
    const FString& SKU, 
    const FDateTime& Timestamp)
{
    AddEvent(MachineId, TEXT("Production"), static_cast<double>(Units), SKU);
    
    FPraxisMachineStats& Stats = MachineStats.FindOrAdd(MachineId);
    Stats.TotalGoodUnits += Units;
    
    // Update quality rate
    const int32 TotalUnits = Stats.TotalGoodUnits + Stats.TotalScrapUnits;
    if (TotalUnits > 0)
    {
        Stats.QualityRate = static_cast<double>(Stats.TotalGoodUnits) / TotalUnits;
    }
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s produced %d good units of %s"), 
        *MachineId.ToString(), Units, *SKU);
}

void UPraxisMetricsSubsystem::RecordScrap(
    FName MachineId, 
    int32 Units, 
    const FString& SKU, 
    const FDateTime& Timestamp)
{
    AddEvent(MachineId, TEXT("Scrap"), static_cast<double>(Units), SKU);
    
    FPraxisMachineStats& Stats = MachineStats.FindOrAdd(MachineId);
    Stats.TotalScrapUnits += Units;
    
    // Update quality rate
    const int32 TotalUnits = Stats.TotalGoodUnits + Stats.TotalScrapUnits;
    if (TotalUnits > 0)
    {
        Stats.QualityRate = static_cast<double>(Stats.TotalGoodUnits) / TotalUnits;
    }
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s scrapped %d units of %s"), 
        *MachineId.ToString(), Units, *SKU);
}

void UPraxisMetricsSubsystem::RecordWorkOrderEvent(
    FName MachineId, 
    int64 WorkOrderID, 
    const FString& EventType, 
    const FDateTime& Timestamp)
{
    FString Context = FString::Printf(TEXT("WO_%lld"), WorkOrderID);
    AddEvent(MachineId, EventType, static_cast<double>(WorkOrderID), Context);
    
    // Count completed work orders
    if (EventType == TEXT("WorkOrderCompleted"))
    {
        FPraxisMachineStats& Stats = MachineStats.FindOrAdd(MachineId);
        Stats.CompletedWorkOrders++;
    }
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s work order event: %s (WO %lld)"), 
        *MachineId.ToString(), *EventType, WorkOrderID);
}

void UPraxisMetricsSubsystem::RecordChangeover(
    FName MachineId, 
    const FString& FromSKU, 
    const FString& ToSKU, 
    double Duration, 
    const FDateTime& Timestamp)
{
    FString Context = FString::Printf(TEXT("%s → %s"), *FromSKU, *ToSKU);
    AddEvent(MachineId, TEXT("Changeover"), Duration, Context);
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s changeover: %s (%.1fs)"), 
        *MachineId.ToString(), *Context, Duration);
}

void UPraxisMetricsSubsystem::RecordJam(
    FName MachineId, 
    double Duration, 
    const FDateTime& Timestamp)
{
    AddEvent(MachineId, TEXT("Jam"), Duration, TEXT(""));
    
    FPraxisMachineStats& Stats = MachineStats.FindOrAdd(MachineId);
    Stats.JamCount++;
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s jammed for %.1f seconds"), 
        *MachineId.ToString(), Duration);
}

// Legacy methods
void UPraxisMetricsSubsystem::RecordMachineEvent(
    FName MachineId, 
    const FString& EventType, 
    const FDateTime& Timestamp)
{
    AddEvent(MachineId, EventType, 0.0, TEXT(""));
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s event '%s' at %s"), 
        *MachineId.ToString(), *EventType, *Timestamp.ToString());
}

void UPraxisMetricsSubsystem::RecordProduction(
    FName MachineId, 
    double Units, 
    int32 TickCount)
{
    FString EventType = FString::Printf(TEXT("ProductionTick_%d"), TickCount);
    AddEvent(MachineId, EventType, Units);
    
    UE_LOG(LogPraxisSim, Verbose, 
        TEXT("[Metrics] %s produced %.2f units (tick %d)"), 
        *MachineId.ToString(), Units, TickCount);
}

// ════════════════════════════════════════════════════════════════════════════════
// Query Interface
// ════════════════════════════════════════════════════════════════════════════════

FPraxisMachineStats UPraxisMetricsSubsystem::GetMachineStats(FName MachineId) const
{
    if (const FPraxisMachineStats* Stats = MachineStats.Find(MachineId))
    {
        // Calculate derived metrics
        FPraxisMachineStats Result = *Stats;
        
        const double TotalTime = Result.ProductionTime + Result.IdleTime + 
                                 Result.ChangeoverTime + Result.JammedTime;
        
        if (TotalTime > 0.0)
        {
            // Utilization = productive time / total time
            Result.Utilization = Result.ProductionTime / TotalTime;
            
            // Simple OEE approximation = utilization × quality
            // (In real OEE, you'd also factor in performance/speed)
            Result.OEE = Result.Utilization * Result.QualityRate;
        }
        
        return Result;
    }
    
    // Return empty stats if machine not found
    FPraxisMachineStats Empty;
    Empty.MachineId = MachineId;
    return Empty;
}

TArray<FPraxisMachineStats> UPraxisMetricsSubsystem::GetAllMachineStats() const
{
    TArray<FPraxisMachineStats> Result;
    
    for (const auto& Pair : MachineStats)
    {
        Result.Add(GetMachineStats(Pair.Key));
    }
    
    return Result;
}

TArray<FPraxisMetricEvent> UPraxisMetricsSubsystem::GetMachineEvents(FName MachineId) const
{
    TArray<FPraxisMetricEvent> Result;
    
    for (const FPraxisMetricEvent& Event : MetricBuffer)
    {
        if (Event.SourceId == MachineId)
        {
            Result.Add(Event);
        }
    }
    
    return Result;
}

// ════════════════════════════════════════════════════════════════════════════════
// Persistence & Export
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisMetricsSubsystem::FlushMetrics()
{
    if (MetricBuffer.IsEmpty())
    {
        UE_LOG(LogPraxisSim, Verbose, TEXT("Metrics flush skipped (buffer empty)."));
        return;
    }

    // Log all events
    for (const FPraxisMetricEvent& E : MetricBuffer)
    {
        UE_LOG(LogPraxisSim, Log, 
            TEXT("[Metrics] %s | %s | %.2f | %s | %s"),
            *E.SourceId.ToString(), 
            *E.EventType, 
            E.Value, 
            *E.Context,
            *E.TimestampUTC.ToString());
    }

    // Don't clear buffer - keep for queries
    // MetricBuffer.Empty();
}

bool UPraxisMetricsSubsystem::ExportToCSV(const FString& FilePath)
{
    if (MetricBuffer.IsEmpty())
    {
        UE_LOG(LogPraxisSim, Warning, TEXT("Cannot export metrics - buffer is empty"));
        return false;
    }
    
    // Build CSV content
    FString CSV = TEXT("SourceId,EventType,Value,Context,Timestamp\n");
    
    for (const FPraxisMetricEvent& E : MetricBuffer)
    {
        CSV += FString::Printf(TEXT("%s,%s,%.2f,%s,%s\n"),
            *E.SourceId.ToString(),
            *E.EventType,
            E.Value,
            *E.Context,
            *E.TimestampUTC.ToString());
    }
    
    // Write to file
    const FString FullPath = FPaths::ProjectSavedDir() / FilePath;
    
    if (FFileHelper::SaveStringToFile(CSV, *FullPath))
    {
        UE_LOG(LogPraxisSim, Log, 
            TEXT("Exported %d metrics to: %s"), 
            MetricBuffer.Num(), *FullPath);
        return true;
    }
    
    UE_LOG(LogPraxisSim, Error, 
        TEXT("Failed to export metrics to: %s"), 
        *FullPath);
    return false;
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisMetricsSubsystem::AddEvent(
    FName SourceId, 
    const FString& Type, 
    double Value, 
    const FString& Context)
{
    FPraxisMetricEvent Event;
    Event.SourceId = SourceId;
    Event.EventType = Type;
    Event.Value = Value;
    Event.Context = Context;
    Event.TimestampUTC = FDateTime::UtcNow();
    
    MetricBuffer.Add(Event);
    UpdateAggregates(Event);
}

void UPraxisMetricsSubsystem::UpdateAggregates(const FPraxisMetricEvent& Event)
{
    // Most aggregation is done in the specific Record* methods
    // This is a hook for any cross-cutting aggregate updates
}
