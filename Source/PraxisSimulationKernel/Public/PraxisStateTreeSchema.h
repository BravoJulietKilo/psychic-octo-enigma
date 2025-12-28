// Copyright 2025 Celsian Pty Ltd
#pragma once
#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "PraxisStateTreeSchema.generated.h"

/**
 * Custom State Tree schema for Praxis machine simulation
 * Allows Praxis-specific tasks, conditions, and evaluators
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Praxis Machine Schema"))
class PRAXISSIMULATIONKERNEL_API UPraxisStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()
public:
	UPraxisStateTreeSchema();
	
	/**
	 * Override to specify which structs (tasks/conditions/evaluators) are allowed in this schema
	 * This is what makes your custom tasks visible in the State Tree editor
	 */
	virtual bool IsStructAllowed(const UScriptStruct* InStruct) const override;
};
