// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "StateTreeEvaluatorBase.h"
// #include "PraxisProcessFragments.h"
#include "PraxisProcessEvaluator.generated.h"

USTRUCT()
struct PRAXISMASS_API FPraxisProcessEvaluator : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()

	// Blackboard output
	UPROPERTY(EditAnywhere, Category="Output") float RunRemaining = 0.f;

	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override
	{
		// This is where you’d read FPraxisProcessingStateFragment in a real impl.
		// For now, just leaves RunRemaining untouched.
	}
};
