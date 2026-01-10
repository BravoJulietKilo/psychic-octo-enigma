// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "MassArchetypeTypes.h"
#include "Types/EPraxisLocationType.h"
#include "Types/FPraxisMaterialFlowEvent.h"
#include "PraxisInventoryService.generated.h"

// Forward declarations
class UPraxisMassSubsystem;
class UPraxisLocationRegistry;

/**
 * Location Capacity Definition
 * Defines storage limits for a location
 */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FLocationCapacity
{
	GENERATED_BODY()

	/** Location identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LocationId;
	
	/** Sub-location identifier (Zone, Rack, Bin, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SubLocationId;
	
	/** Location type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPraxisLocationType LocationType = EPraxisLocationType::Warehouse_RM;
	
	/** Maximum volume capacity (cubic meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxVolume = 100.0f;
	
	/** Maximum item count (batches, not individual units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxItems = 1000;
	
	/** Current volume used */
	UPROPERTY(BlueprintReadOnly)
	float CurrentVolume = 0.0f;
	
	/** Current item count */
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentItems = 0;
	
	/** Is this location at capacity? */
	bool IsAtCapacity() const
	{
		return CurrentVolume >= MaxVolume || CurrentItems >= MaxItems;
	}
	
	/** Get remaining volume */
	float GetRemainingVolume() const
	{
		return FMath::Max(0.0f, MaxVolume - CurrentVolume);
	}
	
	/** Get remaining item slots */
	int32 GetRemainingItems() const
	{
		return FMath::Max(0, MaxItems - CurrentItems);
	}
	
	/** Get volume usage percentage */
	float GetVolumeUsagePercent() const
	{
		return MaxVolume > 0.0f ? (CurrentVolume / MaxVolume) * 100.0f : 0.0f;
	}
};

/**
 * Bill of Materials Entry
 * Defines material transformation rules
 */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FBOMEntry
{
	GENERATED_BODY()

	/** BOM identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BOMId;
	
	/** Output SKU */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName OutputSKU;
	
	/** Output quantity per batch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputQuantity = 1;
	
	/** Output volume per unit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OutputVolumePerUnit = 0.01f;
	
	/** Input requirements (SKU → Quantity needed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, int32> InputRequirements;
};

/**
 * Inventory Transaction Record
 */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FInventoryTransaction
{
	GENERATED_BODY()

	/** Transaction type */
	UPROPERTY(BlueprintReadOnly)
	FString TransactionType; // Purchase, Production, Sale, Transfer, Adjustment, Scrap
	
	/** SKU involved */
	UPROPERTY(BlueprintReadOnly)
	FName SKU;
	
	/** Quantity change (positive or negative) */
	UPROPERTY(BlueprintReadOnly)
	int32 QuantityDelta = 0;
	
	/** Location */
	UPROPERTY(BlueprintReadOnly)
	FName LocationId;
	
	/** Sub-location */
	UPROPERTY(BlueprintReadOnly)
	FName SubLocationId;
	
	/** Batch ID */
	UPROPERTY(BlueprintReadOnly)
	FGuid BatchId;
	
	/** Timestamp */
	UPROPERTY(BlueprintReadOnly)
	FDateTime Timestamp;
	
	/** Reference (Work Order ID, PO Number, etc.) */
	UPROPERTY(BlueprintReadOnly)
	FString Reference;
};

/**
 * Aggregate Inventory Summary (for fast queries)
 */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FInventorySummary
{
	GENERATED_BODY()

	/** SKU */
	UPROPERTY(BlueprintReadOnly)
	FName SKU;
	
	/** Total quantity across all locations */
	UPROPERTY(BlueprintReadOnly)
	int32 TotalQuantity = 0;
	
	/** Quantity by location */
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, int32> QuantityByLocation;
	
	/** Quantity by state (RM/WIP/FG) - keyed by EMaterialState uint8 */
	UPROPERTY(BlueprintReadOnly)
	TMap<uint8, int32> QuantityByState;
	
	/** Reserved quantity */
	UPROPERTY(BlueprintReadOnly)
	int32 ReservedQuantity = 0;
	
	/** Total volume occupied */
	UPROPERTY(BlueprintReadOnly)
	float TotalVolume = 0.0f;
	
	/** Available quantity (Total - Reserved) */
	int32 GetAvailableQuantity() const
	{
		return TotalQuantity - ReservedQuantity;
	}
};

/**
 * Inventory at a specific location (for visualization)
 */
USTRUCT(BlueprintType)
struct PRAXISCORE_API FLocationInventoryItem
{
	GENERATED_BODY()

	/** SKU */
	UPROPERTY(BlueprintReadOnly)
	FName SKU;
	
	/** Quantity at this location */
	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = 0;
	
	/** Material state (0=RM, 1=WIP, 2=FG, 3=Scrap, 4=InTransit) */
	UPROPERTY(BlueprintReadOnly)
	uint8 MaterialState = 0;
	
	/** Volume occupied */
	UPROPERTY(BlueprintReadOnly)
	float Volume = 0.0f;
	
	/** Is this material reserved? */
	UPROPERTY(BlueprintReadOnly)
	bool bReserved = false;
};

/**
 * UPraxisInventoryService
 * 
 * Manages material inventory using Mass entities with:
 * - BOM-based transformations
 * - Location capacity management (two-level: Location.SubLocation)
 * - Batch genealogy tracking
 * - Transaction history
 * - Aggregate caching for fast queries
 * 
 * Material Flow:
 *   RM (Warehouse) → Reserved → WIP (Machine) → FG/Scrap (Output Buffer)
 */
UCLASS()
class PRAXISCORE_API UPraxisInventoryService : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// Lifecycle
	// ═══════════════════════════════════════════════════════════════════════════
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Material Transactions
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Add raw material (purchase/receive) */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool AddRawMaterial(
		FName SKU, 
		int32 Quantity, 
		FName LocationId,
		FName SubLocationId = NAME_None,
		float VolumePerUnit = 0.01f);
	
	/** Reserve material for a work order */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool ReserveMaterial(
		FName SKU,
		int32 Quantity,
		FName LocationId,
		int64 WorkOrderId,
		FName MachineId);
	
	/** Transform material (production) using BOM */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool TransformMaterial(
		FName BOMId,
		FName SourceMachineId,
		int64 WorkOrderId,
		FName OutputLocationId);
	
	/** Ship finished goods (sales/consumption) */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool ShipFinishedGoods(
		FName SKU,
		int32 Quantity,
		FName LocationId);
	
	/** Transfer material between locations */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool TransferMaterial(
		FName SKU,
		int32 Quantity,
		FName FromLocation,
		FName ToLocation);

	// ═══════════════════════════════════════════════════════════════════════════
	// Production Operations (for StateTree integration)
	// ═══════════════════════════════════════════════════════════════════════════
	
	/**
	 * Consume one unit of reserved raw material and create WIP
	 * Called by STTask_Production at the start of each production cycle
	 * @param MachineId Machine consuming the material
	 * @param WorkOrderId Work order being processed
	 * @param SKU Material SKU to consume
	 * @return true if consumption successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool ConsumeReservedMaterial(
		FName MachineId,
		int64 WorkOrderId,
		FName SKU);
	
	/**
	 * Convert WIP to finished good
	 * Called by STTask_Production when unit passes quality check
	 * @param MachineId Machine that produced the item
	 * @param WorkOrderId Work order being processed
	 * @param OutputSKU Output product SKU
	 * @param OutputLocationId Where to place the finished good
	 * @return true if production recorded
	 */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool ProduceFinishedGood(
		FName MachineId,
		int64 WorkOrderId,
		FName OutputSKU,
		FName OutputLocationId);
	
	/**
	 * Convert WIP to scrap
	 * Called by STTask_Production when unit fails quality check
	 * @param MachineId Machine that produced the scrap
	 * @param WorkOrderId Work order being processed
	 * @param SKU Material SKU being scrapped
	 * @param ScrapLocationId Where to place the scrap
	 * @return true if scrap recorded
	 */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool ProduceScrap(
		FName MachineId,
		int64 WorkOrderId,
		FName SKU,
		FName ScrapLocationId);
	
	/**
	 * Release reservation without consuming (e.g., work order cancelled)
	 * @param MachineId Machine that had the reservation
	 * @param WorkOrderId Work order being cancelled
	 * @return true if reservation released
	 */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool ReleaseReservation(
		FName MachineId,
		int64 WorkOrderId);

	// ═══════════════════════════════════════════════════════════════════════════
	// Queries
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Get inventory summary for a SKU */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	FInventorySummary GetInventorySummary(FName SKU) const;
	
	/** Get available quantity at a location */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	int32 GetAvailableQuantity(FName SKU, FName LocationId) const;
	
	/** Check if location has capacity for volume */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	bool HasCapacity(FName LocationId, float RequiredVolume) const;
	
	/** Get location capacity info */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	FLocationCapacity GetLocationCapacity(FName LocationId) const;
	
	/** Get transaction history */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	TArray<FInventoryTransaction> GetTransactionHistory(int32 MaxRecords = 100) const;
	
	/** Get all inventory items at a specific location (for visualization) */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	TArray<FLocationInventoryItem> GetInventoryAtLocation(FName LocationId) const;
	
	/** Get total entity count (for debugging) */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	int32 GetTotalEntityCount() const { return MaterialEntities.Num(); }
	
	/** Debug print inventory summary to log */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	void DebugPrintInventory(FName SKU) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Configuration
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Register a BOM */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	void RegisterBOM(const FBOMEntry& BOM);
	
	/** Register a location with capacity */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	void RegisterLocation(
		FName LocationId, 
		EPraxisLocationType LocationType,
		float MaxVolume, 
		int32 MaxItems,
		FName SubLocationId = NAME_None);
	
	/** Force aggregate cache refresh */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	void RefreshAggregates() { UpdateAggregates(); }

	// ═══════════════════════════════════════════════════════════════════════════
	// Events
	// ═══════════════════════════════════════════════════════════════════════════
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInventoryChanged, FName, SKU, FName, LocationId, int32, NewQuantity);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLocationCapacityWarning, FName, LocationId, float, UsedPercentage);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLowStock, FName, SKU, int32, RemainingQuantity);
	
	// Flow event delegate for visualization (non-dynamic for struct support)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMaterialFlowEvent, const FPraxisMaterialFlowEvent&);
	
	UPROPERTY(BlueprintAssignable, Category = "Praxis|Inventory")
	FOnInventoryChanged OnInventoryChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Praxis|Inventory")
	FOnLocationCapacityWarning OnLocationCapacityWarning;
	
	UPROPERTY(BlueprintAssignable, Category = "Praxis|Inventory")
	FOnLowStock OnLowStock;
	
	/** Flow event for visualization animations (transfers, production, consumption) */
	FOnMaterialFlowEvent OnMaterialFlowEvent;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// Internal Helpers
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Spawn a material entity with all fragments
	 * @param InitialState Material state (0=RawMaterial, 1=WIP, 2=FG, 3=Scrap, 4=InTransit)
	 */
	FMassEntityHandle SpawnMaterialEntity(
		FName SKU,
		int32 Quantity,
		FName LocationId,
		FName SubLocationId,
		float VolumePerUnit,
		uint8 InitialState = 0);
	
	/** Despawn material entities */
	void DespawnMaterialEntities(const TArray<FMassEntityHandle>& Entities);
	
	/** Update aggregate cache from Mass entities */
	void UpdateAggregates();
	
	/** Log transaction */
	void LogTransaction(const FInventoryTransaction& Transaction);
	
	/** Check and update location capacity */
	bool UpdateLocationCapacity(FName LocationId, float VolumeDelta, int32 ItemDelta);
	
	/** Build the entity template for material batches */
	void BuildMaterialEntityTemplate();
	
	/** Broadcast a flow event to visualizers */
	void BroadcastFlowEvent(const FPraxisMaterialFlowEvent& Event);
	
	/** Get world position for a location (queries LocationRegistry) */
	FVector GetLocationWorldPosition(FName LocationId) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Data
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Praxis Mass subsystem reference */
	UPROPERTY()
	TObjectPtr<UPraxisMassSubsystem> MassSubsystem = nullptr;
	
	/** Location registry for world positions (for flow animations) */
	UPROPERTY()
	TObjectPtr<UPraxisLocationRegistry> LocationRegistry = nullptr;
	
	/** Archetype handle for material entities */
	FMassArchetypeHandle MaterialArchetype;
	
	/** BOM registry */
	UPROPERTY()
	TMap<FName, FBOMEntry> BOMs;
	
	/** Location capacity tracking */
	UPROPERTY()
	TMap<FName, FLocationCapacity> Locations;
	
	/** Aggregate inventory cache (for fast queries) */
	UPROPERTY()
	TMap<FName, FInventorySummary> InventoryCache;
	
	/** Transaction history */
	UPROPERTY()
	TArray<FInventoryTransaction> TransactionHistory;
	
	/** Entity handle tracking (for cleanup and queries) */
	TArray<FMassEntityHandle> MaterialEntities;
	
	/** Flag to track if archetype is initialized */
	bool bArchetypeInitialized = false;
};
