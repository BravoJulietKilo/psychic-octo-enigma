// Fill out your copyright notice in the Description page of Project Settings.


#include "PraxisInventoryService.h"

bool UPraxisInventoryService::AddStock(FName LocationName, FName SKU, int32 Quantity)
{
	return true;
}

bool UPraxisInventoryService::RemoveStock(FName LocationName, FName SKU, int32 Quantity)
{
	return true;
}

int32 UPraxisInventoryService::GetStock(FName LocationName, FName SKU)
{
	return 0;
}

bool UPraxisInventoryService::MoveStock(FName Location, FName SKU, int32 MoveQuantity)
{
	return true;
}
