// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "PraxisItemDefFragment.generated.h"

// Item definition (SKU, family, UOM)
USTRUCT()
struct PRAXISMASS_API FPraxisItemDefFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY() FName SKU;
	UPROPERTY() FName Family;
	UPROPERTY() FName UOM;
};

// Inventory location (bin/area)
USTRUCT()
struct FPraxisInventoryLocFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY() FName LocationId;
	UPROPERTY() int32 QtyUnits = 1;
};

// Processing state (setup/run timers, state enum)
USTRUCT()
struct FPraxisProcessingStateFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY() uint8 State = 0;      // 0=Idle, 1=Setup, 2=Run, 3=Done
	UPROPERTY() float SetupRemaining = 0.f;
	UPROPERTY() float RunRemaining = 0.f;
};

// Tags
USTRUCT()
struct FPraxisReadyForDispatchTag : public FMassTag

{
	GENERATED_BODY()
};
