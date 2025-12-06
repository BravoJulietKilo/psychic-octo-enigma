// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PraxisShopFloorSystem.generated.h"

/**
 * 
 */
UCLASS()
class PRAXISSIMULATIONKERNEL_API UPraxisShopFloorSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Override the pure virtual GetStatId function
	virtual TStatId GetStatId() const override; 

};
