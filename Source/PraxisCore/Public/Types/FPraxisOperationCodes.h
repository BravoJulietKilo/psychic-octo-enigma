#pragma once

#include "CoreMinimal.h"
#include "FPraxisOperationCodes.generated.h"

/** Please add a struct description */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FPraxisOperationCodes
{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SequenceNumber = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName OperationCode = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString OperationDescription = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName WorkCenter = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double StandardTime = 0.000000;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double SetupTime = 0.000000;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double TeardownTime = 0.000000;

	FPraxisOperationCodes() = default;
	FPraxisOperationCodes(int32 SequenceNumber, const FName& OperationCode, const FString& OperationDescription,
		const FName& WorkCenter, double StandardTime, double SetupTime, double TeardownTime)
		: SequenceNumber(SequenceNumber),
		  OperationCode(OperationCode),
		  OperationDescription(OperationDescription),
		  WorkCenter(WorkCenter),
		  StandardTime(StandardTime),
		  SetupTime(SetupTime),
		  TeardownTime(TeardownTime)
	{
	}
	~FPraxisOperationCodes() = default;
};
