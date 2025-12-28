// Copyright 2025 Celsian Pty Ltd
#include "PraxisStateTreeSchema.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "GameFramework/Actor.h"

UPraxisStateTreeSchema::UPraxisStateTreeSchema()
{
	// UStateTreeSchema doesn't have a ContextActorClass property
	// The schema compatibility is determined by the StateTree asset itself
}

bool UPraxisStateTreeSchema::IsStructAllowed(const UScriptStruct* InStruct) const
{
	if (!InStruct)
	{
		return false;
	}
	
	// Allow all State Tree tasks
	if (InStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
	{
		return true;
	}
	
	// Allow all State Tree conditions
	if (InStruct->IsChildOf(FStateTreeConditionBase::StaticStruct()))
	{
		return true;
	}
	
	// Allow all State Tree evaluators
	if (InStruct->IsChildOf(FStateTreeEvaluatorBase::StaticStruct()))
	{
		return true;
	}
	
	// Call parent implementation for other struct types
	return Super::IsStructAllowed(InStruct);
}
