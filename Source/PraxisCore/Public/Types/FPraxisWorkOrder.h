#pragma once

#include "CoreMinimal.h" 
#include "EPraxisUnitOfMeasure.h"
#include "EPraxisWorkOrderPriority.h"
#include "EPraxisWorkOrderStatus.h"
#include "FPraxisBillOfMaterials.h"
#include "FPraxisWorkOrder.generated.h"

enum class EPraxisWorkOrderStatus : uint8;

USTRUCT(BlueprintType)
struct PRAXISCORE_API FPraxisWorkOrder
{
	GENERATED_BODY()

public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 WorkOrderID = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SKU = "None";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Quantity = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisUnitOfMeasure UnitOfMeasure = EPraxisUnitOfMeasure::Each;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FDateTime DueDate = FDateTime::UtcNow();;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FDateTime StartDate = FDateTime::UtcNow();;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FDateTime FinishDate = FDateTime::UtcNow();;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisWorkOrderStatus WorkOrderStatus = EPraxisWorkOrderStatus::OnHold;

	/** Please add a variable description */
	// UPROPERTY(BlueprintReadWrite, EditInstanceOnly, meta=(DisplayName="AssignedMachine", MakeStructureDefaultValue="None"))
	// TObjectPtr<ABP_ProcessZ_C> AssignedMachine;

	/** Please add a variable description */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(DisplayName="BillOfMaterials"))
	//FPraxisBillOfMaterials BillOfMaterials;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double Cost = 0.000000;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EPraxisWorkOrderPriority Priority = EPraxisWorkOrderPriority::None;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName MachineId = "None";

	FPraxisWorkOrder() = default;
	FPraxisWorkOrder(int64 WorkOrderID, const FString& Sku, int32 Quantity, EPraxisUnitOfMeasure UnitOfMeasure,
		const FDateTime& DueDate, const FDateTime& StartDate, const FDateTime& FinishDate,
		EPraxisWorkOrderStatus WorkOrderStatus, double Cost, EPraxisWorkOrderPriority Priority, const FName& MachineId)
		: WorkOrderID(WorkOrderID),
		  SKU(Sku),
		  Quantity(Quantity),
		  UnitOfMeasure(UnitOfMeasure),
		  DueDate(DueDate),
		  StartDate(StartDate),
		  FinishDate(FinishDate),
		  WorkOrderStatus(WorkOrderStatus),
		  Cost(Cost),
		  Priority(Priority),
		  MachineId(MachineId)
	{
	}
	~FPraxisWorkOrder() = default;
};
