#pragma once

UENUM(BlueprintType)
enum class EPraxisMachineState : uint8
{
	Idle        UMETA(DisplayName="Idle"),
    Processing  UMETA(DisplayName="Processing"),
	Jammed      UMETA(DisplayName="Jammed"),
	MinorStop   UMETA(DisplayName="Minor Stop"),
	Breakdown   UMETA(DisplayName="Breakdown"),
	Completed   UMETA(DisplayName="Completed"),
	Changeover  UMETA(DisplayName="Changeover"),
	Maintenance UMETA(DisplayName="Maintenance"),
	Unavailable UMETA(DisplayName="Unavailable")
};