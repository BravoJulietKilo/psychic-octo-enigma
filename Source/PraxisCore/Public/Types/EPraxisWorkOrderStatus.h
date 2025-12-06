#pragma once

UENUM(BlueprintType)
enum class EPraxisWorkOrderStatus : uint8
{
    OnHold     UMETA(DisplayName="On Hold"),
    Created    UMETA(DisplayName="Created"),
	Assigned   UMETA(DisplayName="Assigned"),
	Ready      UMETA(DisplayName="Ready"),
	Queued     UMETA(DisplayName="Queued"),
	Running    UMETA(DisplayName="Running"),
	Completed  UMETA(DisplayName="Completed"),
	Cancelled  UMETA(DisplayName="Cancelled")
};