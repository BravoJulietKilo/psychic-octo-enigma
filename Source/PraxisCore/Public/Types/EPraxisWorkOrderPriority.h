#pragma once

UENUM(BlueprintType)
enum class EPraxisWorkOrderPriority : uint8
{
	None      UMETA(DisplayName="None"),
	Low       UMETA(DisplayName="Low"),
	Medium    UMETA(DisplayName="Medium"),
	High      UMETA(DisplayName="High"),
	Expedited UMETA(DisplayName="Expedited")
	
};