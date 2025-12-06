#pragma once
#include "FPraxisOperationCodes.h"
#include "CoreMinimal.h"
#include "FPraxisRouting.generated.h"

USTRUCT(BlueprintType)
struct PRAXISCORE_API FPraxisRouting
{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RoutingNumber = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName RoutingDescription = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName SKU = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FPraxisOperationCodes OperationCodes = FPraxisOperationCodes();

	FPraxisRouting() = default;
	FPraxisRouting(int32 RoutingNumber, const FName& RoutingDescription, const FName& Sku,
		const FPraxisOperationCodes& OperationCodes)
		: RoutingNumber(RoutingNumber),
		  RoutingDescription(RoutingDescription),
		  SKU(Sku),
		  OperationCodes(OperationCodes)
	{
	}

	~FPraxisRouting() = default;
};
