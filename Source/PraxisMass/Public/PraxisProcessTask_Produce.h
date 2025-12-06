// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PraxisProcessTask_Produce.generated.h"

/**
 * 
 */

USTRUCT()
struct PRAXISMASS_API FPraxisProcessTask_Produce : public FStateTreeTaskBase
{
	GENERATED_BODY()

	// virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	// 									   const EStateTreeStateChangeType ChangeType,
	// 									   const FStateTreeTransitionResult& Transition) const override
	// {
	// 	return EStateTreeRunStatus::Running;
	// }

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override
	{
		// For now, always succeed after one tick
		return EStateTreeRunStatus::Succeeded;
	}
};
