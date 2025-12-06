#pragma once

UENUM(BlueprintType)
enum class EPraxisUnitOfMeasure : uint8
{
	Each     UMETA(DisplayName="Each"),
	Kilogram UMETA(DisplayName="Kilogram"),
	Liter    UMETA(DisplayName="Liter"),
	Meter    UMETA(DisplayName="Meter"),
	Second   UMETA(DisplayName="Second"),
	Pack     UMETA(DisplayName="Pack"),
	Carton   UMETA(DisplayName="Carton"),
	Case     UMETA(DisplayName="Case")

	/* Each: Each unit of measure is equal to one unit of measure.
		Kilogram: Kilogram is the unit of measure for weight.
		Liter: Liter is the unit of measure for volume.
		Meter: Meter is the unit of measure for length.
		Second: Second is the unit of measure for time. */
};
