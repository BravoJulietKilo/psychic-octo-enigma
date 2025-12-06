// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/FPraxisInventoryLocation.h"
#include "PraxisInventoryService.generated.h"

/**
 * 
 */
UCLASS()
class PRAXISCORE_API UPraxisInventoryService : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Praxis|Inventory")
	bool AddStock(FName LocationName, FName SKU, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Praxis|Inventory")
	bool RemoveStock(FName LocationName, FName SKU, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Praxis|Inventory")
	int32 GetStock(FName LocationName, FName SKU);

	UFUNCTION(BlueprintCallable, Category="Praxis|Inventory")
	bool MoveStock(FName Location, FName SKU, int32 MoveQuantity);
};