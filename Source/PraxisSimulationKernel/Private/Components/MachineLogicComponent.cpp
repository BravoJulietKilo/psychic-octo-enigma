// Copyright 2025 Celsian Pty Ltd
// UPraxisMetricsSubsystem – simulation metrics collection and flush subsystem.

#include "PraxisSimulationKernel/Public/Components/MachineLogicComponent.h"
#include "PraxisMetricsSubsystem.h"
#include "Misc/DateTime.h"
#include "Misc/OutputDevice.h"
#include "Engine/Engine.h"

// ────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────

void UPraxisMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    MetricBuffer.Reset();
    UE_LOG(LogTemp, Log, TEXT("MetricsSubsystem initialized."));
}

void UPraxisMetricsSubsystem::Deinitialize()
{
    FlushMetrics();
    UE_LOG(LogTemp, Log, TEXT("MetricsSubsystem deinitialized; buffer flushed."));
    Super::Deinitialize();
}

// ────────────────────────────────────────────────────────────────
// Public Recording Interface
// ────────────────────────────────────────────────────────────────

void UPraxisMetricsSubsystem::RecordMachineEvent(
    FName MachineId, const FString& EventType, const FDateTime& Timestamp)
{
    AddEvent(MachineId, EventType, /*Value*/ 0.0);
    UE_LOG(LogTemp, Verbose, TEXT("[%s] Event: %s @ %s"),
        *MachineId.ToString(),
        *EventType,
        *Timestamp.ToString());
}

void UPraxisMetricsSubsystem::RecordProduction(FName MachineId, double Units, int32 TickCount)
{
    const FString EventLabel = FString::Printf(TEXT("ProductionTick_%d"), TickCount);
    AddEvent(MachineId, EventLabel, Units);
    UE_LOG(LogTemp, Verbose, TEXT("[%s] Production tick %d: %.2f units"),
        *MachineId.ToString(), TickCount, Units);
}

void UPraxisMetricsSubsystem::FlushMetrics()
{
    if (MetricBuffer.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("MetricsSubsystem: No metrics to flush."));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Flushing %d metric events:"), MetricBuffer.Num());
    for (const FPraxisMetricEvent& Event : MetricBuffer)
    {
        UE_LOG(LogTemp, Warning, TEXT("  [%s] %s | %.2f | %s"),
            *Event.SourceId.ToString(),
            *Event.EventType,
            Event.Value,
            *Event.TimestampUTC.ToString());
    }

    MetricBuffer.Reset();
}

// ────────────────────────────────────────────────────────────────
// Internal Helper
// ────────────────────────────────────────────────────────────────

void UPraxisMetricsSubsystem::AddEvent(FName SourceId, const FString& Type, double Value)
{
    FPraxisMetricEvent E;
    E.SourceId = SourceId;
    E.EventType = Type;
    E.Value = Value;
    E.TimestampUTC = FDateTime::UtcNow();
    MetricBuffer.Add(E);
}