// Copyright 2025 Celsian Pty Ltd
// UPraxisMetricsSubsystem - central aggregator for simulation metrics
// Receives structured metric events from MachineLogicComponent and other systems
// Persists, logs, or streams them to UI or external analytics sinks

#include "PraxisMetricsSubsystem.h"
//#include "PraxisSimulationKernel.h" // for LogPraxisSim
#include "Misc/DateTime.h"

void UPraxisMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("Metrics subsystem initialized."));
    MetricBuffer.Reserve(256); // reserve for efficiency
}

void UPraxisMetricsSubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("Metrics subsystem deinitializing. Flushing %d events."), MetricBuffer.Num());
    FlushMetrics();
    MetricBuffer.Empty();
    Super::Deinitialize();
}

// ────────────────────────────────────────────────────────────────
// Recording Interface
// ────────────────────────────────────────────────────────────────

void UPraxisMetricsSubsystem::RecordMachineEvent(FName MachineId, const FString& EventType, const FDateTime& Timestamp)
{
    UE_LOG(LogTemp, Verbose, TEXT("[Metrics] %s event '%s' at %s"), *MachineId.ToString(), *EventType, *Timestamp.ToString());
    FPraxisMetricEvent Event;
    Event.SourceId = MachineId;
    Event.EventType = EventType;
    Event.TimestampUTC = Timestamp;
    MetricBuffer.Add(Event);
}

void UPraxisMetricsSubsystem::RecordProduction(FName MachineId, double Units, int32 TickCount)
{
    FString EventType = FString::Printf(TEXT("ProductionTick_%d"), TickCount);
    AddEvent(MachineId, EventType, Units);
    UE_LOG(LogTemp, Verbose, TEXT("[Metrics] %s produced %.2f units (tick %d)"), *MachineId.ToString(), Units, TickCount);
}

void UPraxisMetricsSubsystem::FlushMetrics()
{
    if (MetricBuffer.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("Metrics flush skipped (buffer empty)."));
        return;
    }

    // In future: persist to CSV, DB, or analytics stream
    for (const FPraxisMetricEvent& E : MetricBuffer)
    {
        UE_LOG(LogTemp, Log, TEXT("[Metrics] %s | %s | %.2f | %s"),
            *E.SourceId.ToString(), *E.EventType, E.Value, *E.TimestampUTC.ToString());
    }

    MetricBuffer.Empty();
}

// ────────────────────────────────────────────────────────────────
// Internal Helper
// ────────────────────────────────────────────────────────────────

void UPraxisMetricsSubsystem::AddEvent(FName SourceId, const FString& Type, double Value)
{
    FPraxisMetricEvent Event;
    Event.SourceId = SourceId;
    Event.EventType = Type;
    Event.Value = Value;
    Event.TimestampUTC = FDateTime::UtcNow();
    MetricBuffer.Add(Event);
}
