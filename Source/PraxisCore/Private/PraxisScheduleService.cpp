// Copyright 2025 Celsian Pty Ltd

#include "PraxisScheduleService.h"
#include "Misc/DateTime.h"
#include "Containers/Queue.h"


void UPraxisScheduleService::AddWorkOrder(const FPraxisWorkOrder& NewWO)
{
    const int64 Id = NewWO.WorkOrderID; // adjust to your field name
    FPraxisOrderState& S = Orders.FindOrAdd(Id);
    S.WorkOrder = NewWO;
    S.Status = 0;
    MachineQueues.FindOrAdd(NewWO.MachineId).Add(Id);   // requires MachineId on WO
    OnWorkOrderAssigned.Broadcast(Id, NewWO.MachineId);
}

bool UPraxisScheduleService::RemoveWorkOrder(int64 WorkOrderID)
{
    if (!Orders.Remove(WorkOrderID)) return false;
    for (auto& KVP : MachineQueues)
    {
        KVP.Value.Remove(WorkOrderID);
    }
    return true;
}

bool UPraxisScheduleService::GetNextForMachine(FName MachineId, FPraxisWorkOrder& OutWO) const
{
    if (const TArray<int64>* Q = MachineQueues.Find(MachineId))
    {
        for (int64 Id : *Q)
        {
            if (const FPraxisOrderState* S = Orders.Find(Id))
            {
                if (S->Status == 0) { OutWO = S->WorkOrder; return true; }
            }
        }
    }
    return false;
}

void UPraxisScheduleService::GetActiveForMachine(FName MachineId, TArray<FPraxisWorkOrder>& Out) const
{
    Out.Reset();
    if (const TArray<int64>* Q = MachineQueues.Find(MachineId))
    {
        for (int64 Id : *Q)
            if (const FPraxisOrderState* S = Orders.Find(Id))
                if (S->Status == 0 || S->Status == 1) Out.Add(S->WorkOrder);
    }
}

void UPraxisScheduleService::GetSchedule(TArray<FPraxisWorkOrder>& OutAll) const
{
    OutAll.Reset();
    for (const auto& KVP : Orders) OutAll.Add(KVP.Value.WorkOrder);
}

bool UPraxisScheduleService::AssignOperatorToMachine(FName OperatorId, FName MachineId)
{
    FPraxisOperatorState& Op = Operators.FindOrAdd(OperatorId);
    if (Op.bBusy && Op.MachineId == MachineId) return true;
    Op.OperatorId = OperatorId; Op.MachineId = MachineId; Op.bBusy = true;
    OnOperatorAssigned.Broadcast(OperatorId, MachineId);
    return true;
}

bool UPraxisScheduleService::ReleaseOperator(FName OperatorId)
{
    if (FPraxisOperatorState* Op = Operators.Find(OperatorId))
    {
        Op->bBusy = false; Op->MachineId = NAME_None;
        OnOperatorReleased.Broadcast(OperatorId);
        return true;
    }
    return false;
}

bool UPraxisScheduleService::StartWorkOrder(int64 WorkOrderID, FName MachineId)
{
    if (FPraxisOrderState* S = Orders.Find(WorkOrderID))
    {
        S->Status = 1; S->MachineId = MachineId; S->StartTs = NowUnixSeconds();
        OnWorkOrderStarted.Broadcast(WorkOrderID);
        return true;
    }
    return false;
}

bool UPraxisScheduleService::CompleteWorkOrder(int64 WorkOrderID)
{
    if (FPraxisOrderState* S = Orders.Find(WorkOrderID))
    {
        S->Status = 2; S->EndTs = NowUnixSeconds();
        OnWorkOrderCompleted.Broadcast(WorkOrderID);
        return true;
    }
    return false;
}

int64 UPraxisScheduleService::NowUnixSeconds() const
{
    // placeholder: Orchestrator should provide sim time; swap in later
    return FDateTime::UtcNow().ToUnixTimestamp();
}

void UPraxisScheduleService::RegisterMachine(FName MachineId)
{
    MachineQueues.FindOrAdd(MachineId);
}

void UPraxisScheduleService::RegisterOperator(FName OperatorId)
{
    Operators.FindOrAdd(OperatorId).OperatorId = OperatorId;
}
