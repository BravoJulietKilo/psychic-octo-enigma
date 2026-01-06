// Copyright 2025 Celsian Pty Ltd

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "MassArchetypeTypes.h"
#include "Types/EPraxisLocationType.h"
#include "Fragments/MaterialFragments.h"
#include "PraxisInventoryService.generated.h"

// Forward declarations
class UMassEntitySubsystem;

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
	
	/** Get total entity count (for debugging) */
	UFUNCTION(BlueprintCallable, Category = "Praxis|Inventory")
	int32 GetTotalEntityCount() const { return MaterialEntities.Num(); }

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
	
	UPROPERTY(BlueprintAssignable, Category = "Praxis|Inventory")
	FOnInventoryChanged OnInventoryChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Praxis|Inventory")
	FOnLocationCapacityWarning OnLocationCapacityWarning;
	
	UPROPERTY(BlueprintAssignable, Category = "Praxis|Inventory")
	FOnLowStock OnLowStock;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// Internal Helpers
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Spawn a material entity with all fragments */
	FMassEntityHandle SpawnMaterialEntity(
		FName SKU,
		int32 Quantity,
		FName LocationId,
		FName SubLocationId,
		float VolumePerUnit,
		EMaterialState InitialState = EMaterialState::RawMaterial);
	
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

	// ═══════════════════════════════════════════════════════════════════════════
	// Data
	// ═══════════════════════════════════════════════════════════════════════════
	
	/** Mass entity subsystem reference */
	UPROPERTY()
	TObjectPtr<UMassEntitySubsystem> MassEntitySubsystem = nullptr;
	
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
