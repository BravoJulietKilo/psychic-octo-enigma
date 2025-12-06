#pragma once

UENUM(BlueprintType)
enum class EPraxisShift : uint8
{
	Day       UMETA(DisplayName="Day"),
	Afternoon UMETA(DisplayName="Afternoon"),
	Night     UMETA(DisplayName="Night")
};
