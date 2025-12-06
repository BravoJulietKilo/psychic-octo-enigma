#pragma once

#include "CoreMinimal.h"
#include "EPraxisItemStatus.h"
#include "EPraxisLocationCapTypes.h"
#include "EPraxisUnitOfMeasure.h"
#include "FPraxisInventoryLocation.generated.h"

USTRUCT(BlueprintType)
struct PRAXISCORE_API FPraxisInventoryLocation

{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName LocationName = "None";
	
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName SKU = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisItemStatus Status = EPraxisItemStatus::RawMaterial;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite)
	int32 Quantity = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisUnitOfMeasure UnitOfMeasure = EPraxisUnitOfMeasure::Each;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisLocationCapTypes CapType = EPraxisLocationCapTypes::Hard;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Cap = 0;

	FPraxisInventoryLocation() = default;
	FPraxisInventoryLocation(FName LocationName, FName SKU, EPraxisItemStatus Status, int32 Quantity, EPraxisUnitOfMeasure UnitOfMeasure, EPraxisLocationCapTypes CapType, int32 Cap)
		: LocationName(LocationName),
		  SKU(SKU),
		  Status(Status),
		  Quantity(Quantity),
		  UnitOfMeasure(UnitOfMeasure),
		  CapType(CapType),
		  Cap(Cap)
	{}
	~FPraxisInventoryLocation() = default;
};