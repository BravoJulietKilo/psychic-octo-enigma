// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Tasks/STTask_TestIncrement.h"
#include "Components/MachineContextComponent.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"

EStateTreeRunStatus FSTTask_TestIncrement::EnterState(
	FStateTreeExecutionContext& Context, 
	const FStateTreeTransitionResult& Transition) const
{
	// Get instance data (contains our bound component reference)
	FSTTask_TestIncrementInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// Verify binding
	if (!InstanceData.MachineContext)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("[TestIncrement] MachineContext not bound!"));
		return EStateTreeRunStatus::Failed;
	}
	
	// Get the context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("[%s] TestIncrement task entered - Current counter: %d"), 
		*MachineCtx.MachineId.ToString(),
		MachineCtx.OutputCounter);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_TestIncrement::Tick(
	FStateTreeExecutionContext& Context, 
	const float DeltaTime) const
{
	// Get instance data
	FSTTask_TestIncrementInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.MachineContext)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// Get mutable context
	FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetMutableContext();
	
	// Increment counter
	MachineCtx.OutputCounter++;
	
	// Log every 10 increments to avoid spam
	if (MachineCtx.OutputCounter % 10 == 0)
	{
		UE_LOG(LogPraxisSim, Log, 
			TEXT("[%s] TestIncrement tick - Counter: %d, DeltaTime: %.3f"), 
			*MachineCtx.MachineId.ToString(),
			MachineCtx.OutputCounter,
			DeltaTime);
	}
	
	// Keep running forever (this is just a test)
	return EStateTreeRunStatus::Running;
}
