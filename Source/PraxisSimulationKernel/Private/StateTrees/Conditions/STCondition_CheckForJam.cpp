// Copyright 2025 Celsian Pty Ltd

#include "StateTrees/Conditions/STCondition_CheckForJam.h"
#include "Components/MachineContextComponent.h"
#include "PraxisRandomService.h"
#include "StateTreeExecutionContext.h"
#include "PraxisSimulationKernel.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

bool FSTCondition_CheckForJam::TestCondition(FStateTreeExecutionContext& Context) const
{
	// Get instance data
	FSTCondition_CheckForJamInstanceData& InstanceData = Context.GetInstanceData(*this);
	
	// Auto-discover MachineContext if not bound
	if (!InstanceData.MachineContext)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			InstanceData.MachineContext = Owner->FindComponentByClass<UMachineContextComponent>();
		}
	}
	
	// Auto-discover RandomService if not bound
	if (!InstanceData.RandomService)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			if (UWorld* World = Owner->GetWorld())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					InstanceData.RandomService = GI->GetSubsystem<UPraxisRandomService>();
				}
			}
		}
	}
	
	// Verify components
	if (!InstanceData.MachineContext)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("[STCondition_CheckForJam] MachineContext not found!"));
		return false;
	}
	
	// Get context
	const FPraxisMachineContext& MachineCtx = InstanceData.MachineContext->GetContext();
	
	// If no jam probability set, never jam
	if (MachineCtx.JamProbabilityPerTick <= 0.0f)
	{
		return false;
	}
	
	// Use RandomService for deterministic jam check
	if (InstanceData.RandomService)
	{
		// Roll a random value and compare to jam probability
		const float Roll = InstanceData.RandomService->Uniform_Key(
			MachineCtx.MachineId,
			0, // Channel 0 = Machine breakdowns/failures
			0.0f,
			1.0f
		);
		
		const bool bJamOccurred = Roll < MachineCtx.JamProbabilityPerTick;
		
		if (bJamOccurred)
		{
			UE_LOG(LogPraxisSim, Warning, 
				TEXT("[%s] Jam condition triggered! (Roll: %.4f < Probability: %.4f)"), 
				*MachineCtx.MachineId.ToString(),
				Roll,
				MachineCtx.JamProbabilityPerTick);
		}
		
		return bJamOccurred;
	}
	else
	{
		// Fallback: simple deterministic check based on tick count
		// This is not ideal but provides some variation without RandomService
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("[STCondition_CheckForJam] RandomService not available - using fallback"));
		
		// Use a simple hash of the machine ID to get pseudo-random behavior
		const uint32 Hash = GetTypeHash(MachineCtx.MachineId);
		const float PseudoRandom = static_cast<float>(Hash % 10000) / 10000.0f;
		
		return PseudoRandom < MachineCtx.JamProbabilityPerTick;
	}
}
