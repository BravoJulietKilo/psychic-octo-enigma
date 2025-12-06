#pragma once

#include "CoreMinimal.h"
#include "FPraxisRouting.h"
#include "FPraxisBillOfMaterials.generated.h"

/** Please add a struct description */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FPraxisBillOfMaterials

{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName BoMID = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Product = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Name = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SKU = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FPraxisRouting Routing = FPraxisRouting();

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Description = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisUnitOfMeasure UnitOfMeasure = EPraxisUnitOfMeasure::Each;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 Quantity = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double AverageCost = 0.000000;

	/** Please add a variable description */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(DisplayName="RequiredMaterials"))
	//TArray<FST_Material> RequiredMaterials;

	FPraxisBillOfMaterials() = default;
	FPraxisBillOfMaterials(const FName& BoMid, const FName& Product, const FName& Name, int32 Sku,
		const FPraxisRouting& Routing, const FString& Description, EPraxisUnitOfMeasure UnitOfMeasure, int64 Quantity,
		double AverageCost)
		: BoMID(BoMid),
		  Product(Product),
		  Name(Name),
		  SKU(Sku),
		  Routing(Routing),
		  Description(Description),
		  UnitOfMeasure(UnitOfMeasure),
		  Quantity(Quantity),
		  AverageCost(AverageCost)
	{
	}
	~FPraxisBillOfMaterials() = default;
};