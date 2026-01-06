// Copyright 2025 Celsian Pty Ltd

#include "PraxisInventoryService.h"
#include "PraxisCore.h"
#include "PraxisMassSubsystem.h"
#include "Fragments/MaterialFragments.h"

// ════════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisInventoryService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Get our custom Mass subsystem
	if (UWorld* World = GetWorld())
	{
		MassSubsystem = World->GetSubsystem<UPraxisMassSubsystem>();
		
		if (MassSubsystem && MassSubsystem->IsInitialized())
		{
			BuildMaterialEntityTemplate();
			UE_LOG(LogPraxisSim, Log, TEXT("Inventory service initialized with Mass entity support (Archetype: %s)"), 
				bArchetypeInitialized ? TEXT("Ready") : TEXT("Pending"));
		}
		else
		{
			UE_LOG(LogPraxisSim, Warning, TEXT("Inventory service: PraxisMassSubsystem not found or not initialized."));
			UE_LOG(LogPraxisSim, Warning, TEXT("  World Type: %d"), (int32)World->WorldType);
		}
	}
	else
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Inventory service: GetWorld() returned null during Initialize"));
	}
}

void UPraxisInventoryService::Deinitialize()
{
	// Capture count before cleanup
	const int32 EntityCount = MaterialEntities.Num();
	
	// Cleanup all material entities
	if (MassSubsystem && MassSubsystem->IsInitialized())
	{
		FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
		
		for (const FMassEntityHandle& Entity : MaterialEntities)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				EntityManager.DestroyEntity(Entity);
			}
		}
	}
	
	MaterialEntities.Empty();
	BOMs.Empty();
	Locations.Empty();
	InventoryCache.Empty();
	TransactionHistory.Empty();
	bArchetypeInitialized = false;
	
	UE_LOG(LogPraxisSim, Log, TEXT("Inventory service deinitialized - cleaned up %d entities"), EntityCount);
	
	Super::Deinitialize();
}

void UPraxisInventoryService::BuildMaterialEntityTemplate()
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized() || bArchetypeInitialized)
	{
		return;
	}
	
	// Build fragment requirements for material entities
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Create archetype composition descriptor with all material fragments
	FMassArchetypeCompositionDescriptor Composition;
	Composition.Fragments.Add(*FMaterialTypeFragment::StaticStruct());
	Composition.Fragments.Add(*FMaterialStateFragment::StaticStruct());
	Composition.Fragments.Add(*FMaterialQuantityFragment::StaticStruct());
	Composition.Fragments.Add(*FMaterialLocationFragment::StaticStruct());
	Composition.Fragments.Add(*FMaterialGenealogyFragment::StaticStruct());
	Composition.Fragments.Add(*FMaterialReservationFragment::StaticStruct());
	
	// Create archetype from composition
	MaterialArchetype = EntityManager.CreateArchetype(Composition);
	bArchetypeInitialized = MaterialArchetype.IsValid();
	
	if (bArchetypeInitialized)
	{
		UE_LOG(LogPraxisSim, Log, TEXT("Material entity archetype created successfully"));
	}
	else
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Failed to create material entity archetype"));
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Material Transactions
// ════════════════════════════════════════════════════════════════════════════════

bool UPraxisInventoryService::AddRawMaterial(
	FName SKU, 
	int32 Quantity, 
	FName LocationId,
	FName SubLocationId,
	float VolumePerUnit)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot add material - PraxisMassSubsystem is not available."));
		return false;
	}
	
	if (!bArchetypeInitialized)
	{
		// Try to initialize now if it wasn't ready during Initialize()
		BuildMaterialEntityTemplate();
		
		if (!bArchetypeInitialized)
		{
			UE_LOG(LogPraxisSim, Error, TEXT("Cannot add material - Material archetype failed to initialize"));
			return false;
		}
	}
	
	if (Quantity <= 0)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Cannot add material - invalid quantity: %d"), Quantity);
		return false;
	}
	
	// Check capacity
	const float RequiredVolume = Quantity * VolumePerUnit;
	if (!UpdateLocationCapacity(LocationId, RequiredVolume, 1))
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Insufficient capacity at %s for %d units of %s (%.2f m³ required)"),
			*LocationId.ToString(), Quantity, *SKU.ToString(), RequiredVolume);
		return false;
	}
	
	// Spawn material entity
	FMassEntityHandle Entity = SpawnMaterialEntity(
		SKU, Quantity, LocationId, SubLocationId, VolumePerUnit, EMaterialState::RawMaterial);
	
	if (Entity.IsSet())
	{
		MaterialEntities.Add(Entity);
		
		// Log transaction
		FInventoryTransaction Transaction;
		Transaction.TransactionType = TEXT("Purchase");
		Transaction.SKU = SKU;
		Transaction.QuantityDelta = Quantity;
		Transaction.LocationId = LocationId;
		Transaction.SubLocationId = SubLocationId;
		Transaction.Timestamp = FDateTime::UtcNow();
		
		// Get batch ID from entity for transaction record
		FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
		if (const FMaterialGenealogyFragment* Genealogy = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(Entity))
		{
			Transaction.BatchId = Genealogy->BatchId;
		}
		
		LogTransaction(Transaction);
		
		// Update aggregates
		UpdateAggregates();
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("Added %d units of %s to %s.%s (Entity: %d)"),
			Quantity, *SKU.ToString(), *LocationId.ToString(), 
			SubLocationId.IsNone() ? TEXT("") : *SubLocationId.ToString(),
			Entity.Index);
		
		OnInventoryChanged.Broadcast(SKU, LocationId, Quantity);
		return true;
	}
	
	// Rollback capacity change if entity spawn failed
	UpdateLocationCapacity(LocationId, -RequiredVolume, -1);
	return false;
}

bool UPraxisInventoryService::ReserveMaterial(
	FName SKU,
	int32 Quantity,
	FName LocationId,
	int64 WorkOrderId,
	FName MachineId)
{
	// TODO: Implement reservation logic in Phase 2
	UE_LOG(LogPraxisSim, Warning, TEXT("ReserveMaterial not yet implemented"));
	return false;
}

bool UPraxisInventoryService::TransformMaterial(
	FName BOMId,
	FName SourceMachineId,
	int64 WorkOrderId,
	FName OutputLocationId)
{
	// TODO: Implement BOM transformation in Phase 3
	UE_LOG(LogPraxisSim, Warning, TEXT("TransformMaterial not yet implemented"));
	return false;
}

bool UPraxisInventoryService::ShipFinishedGoods(
	FName SKU,
	int32 Quantity,
	FName LocationId)
{
	// TODO: Implement shipping logic in Phase 3
	UE_LOG(LogPraxisSim, Warning, TEXT("ShipFinishedGoods not yet implemented"));
	return false;
}

bool UPraxisInventoryService::TransferMaterial(
	FName SKU,
	int32 Quantity,
	FName FromLocation,
	FName ToLocation)
{
	// TODO: Implement transfer logic in Phase 2
	UE_LOG(LogPraxisSim, Warning, TEXT("TransferMaterial not yet implemented"));
	return false;
}

// ════════════════════════════════════════════════════════════════════════════════
// Queries
// ════════════════════════════════════════════════════════════════════════════════

FInventorySummary UPraxisInventoryService::GetInventorySummary(FName SKU) const
{
	if (const FInventorySummary* Summary = InventoryCache.Find(SKU))
	{
		return *Summary;
	}
	
	return FInventorySummary();
}

int32 UPraxisInventoryService::GetAvailableQuantity(FName SKU, FName LocationId) const
{
	if (const FInventorySummary* Summary = InventoryCache.Find(SKU))
	{
		if (const int32* Qty = Summary->QuantityByLocation.Find(LocationId))
		{
			// TODO: Subtract reserved quantity at this location
			return *Qty;
		}
	}
	
	return 0;
}

bool UPraxisInventoryService::HasCapacity(FName LocationId, float RequiredVolume) const
{
	if (const FLocationCapacity* Capacity = Locations.Find(LocationId))
	{
		return Capacity->GetRemainingVolume() >= RequiredVolume;
	}
	
	return true; // No capacity limit defined - allow anything
}

FLocationCapacity UPraxisInventoryService::GetLocationCapacity(FName LocationId) const
{
	if (const FLocationCapacity* Capacity = Locations.Find(LocationId))
	{
		return *Capacity;
	}
	
	return FLocationCapacity();
}

TArray<FInventoryTransaction> UPraxisInventoryService::GetTransactionHistory(int32 MaxRecords) const
{
	TArray<FInventoryTransaction> Result;
	const int32 StartIndex = FMath::Max(0, TransactionHistory.Num() - MaxRecords);
	
	for (int32 i = StartIndex; i < TransactionHistory.Num(); ++i)
	{
		Result.Add(TransactionHistory[i]);
	}
	
	return Result;
}

// ════════════════════════════════════════════════════════════════════════════════
// Configuration
// ════════════════════════════════════════════════════════════════════════════════

void UPraxisInventoryService::RegisterBOM(const FBOMEntry& BOM)
{
	BOMs.Add(BOM.BOMId, BOM);
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Registered BOM %s: %s x%d (Inputs: %d types)"),
		*BOM.BOMId.ToString(),
		*BOM.OutputSKU.ToString(),
		BOM.OutputQuantity,
		BOM.InputRequirements.Num());
}

void UPraxisInventoryService::RegisterLocation(
	FName LocationId, 
	EPraxisLocationType LocationType,
	float MaxVolume, 
	int32 MaxItems,
	FName SubLocationId)
{
	FLocationCapacity& Capacity = Locations.FindOrAdd(LocationId);
	Capacity.LocationId = LocationId;
	Capacity.SubLocationId = SubLocationId;
	Capacity.LocationType = LocationType;
	Capacity.MaxVolume = MaxVolume;
	Capacity.MaxItems = MaxItems;
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Registered location %s (Type: %d): %.1f m³, %d batches max"),
		*LocationId.ToString(), 
		static_cast<int32>(LocationType),
		MaxVolume, 
		MaxItems);
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

FMassEntityHandle UPraxisInventoryService::SpawnMaterialEntity(
	FName SKU,
	int32 Quantity,
	FName LocationId,
	FName SubLocationId,
	float VolumePerUnit,
	EMaterialState InitialState)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized() || !bArchetypeInitialized)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot spawn entity - subsystem or archetype not ready"));
		return FMassEntityHandle();
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Create entity
	FMassEntityHandle Entity = EntityManager.CreateEntity(MaterialArchetype);
	
	if (!Entity.IsSet())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Failed to create Mass entity for %s"), *SKU.ToString());
		return FMassEntityHandle();
	}
	
	const double CurrentTime = GetWorld()->GetTimeSeconds();
	
	// Set Type Fragment
	if (FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity))
	{
		TypeFrag->SKU = SKU;
		TypeFrag->BOMId = NAME_None;
		TypeFrag->UnitOfMeasure = TEXT("ea");
	}
	
	// Set State Fragment
	if (FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity))
	{
		StateFrag->State = InitialState;
		StateFrag->StateEnterTime = CurrentTime;
	}
	
	// Set Quantity Fragment
	if (FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity))
	{
		QtyFrag->Quantity = Quantity;
		QtyFrag->VolumePerUnit = VolumePerUnit;
	}
	
	// Set Location Fragment
	if (FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity))
	{
		LocFrag->LocationId = LocationId;
		LocFrag->SubLocationId = SubLocationId;
		LocFrag->LocationEnterTime = CurrentTime;
	}
	
	// Set Genealogy Fragment
	if (FMaterialGenealogyFragment* GenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(Entity))
	{
		GenFrag->BatchId = FGuid::NewGuid();
		GenFrag->ParentBatchIds.Empty();
		GenFrag->SourceMachineId = NAME_None;
		GenFrag->SourceWorkOrderId = 0;
		GenFrag->CreationTime = CurrentTime;
		GenFrag->bPassedQuality = true;
	}
	
	// Set Reservation Fragment (unreserved by default)
	if (FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity))
	{
		ResFrag->bReserved = false;
		ResFrag->ReservedForWorkOrder = 0;
		ResFrag->ReservedForMachine = NAME_None;
		ResFrag->ReservationTime = 0.0;
	}
	
	return Entity;
}

void UPraxisInventoryService::DespawnMaterialEntities(const TArray<FMassEntityHandle>& Entities)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		return;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (EntityManager.IsEntityValid(Entity))
		{
			EntityManager.DestroyEntity(Entity);
			MaterialEntities.Remove(Entity);
		}
	}
}

void UPraxisInventoryService::UpdateAggregates()
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		return;
	}
	
	// Clear existing cache
	InventoryCache.Empty();
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Iterate through all tracked material entities
	for (int32 i = 0; i < MaterialEntities.Num(); ++i)
	{
		const FMassEntityHandle& Entity = MaterialEntities[i];
		
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Get fragment data
		const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
		const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
		const FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		const FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		const FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		
		if (!TypeFrag || !QtyFrag)
		{
			continue;
		}
		
		// Get or create summary for this SKU
		FInventorySummary& Summary = InventoryCache.FindOrAdd(TypeFrag->SKU);
		Summary.SKU = TypeFrag->SKU;
		
		// Update totals
		Summary.TotalQuantity += QtyFrag->Quantity;
		Summary.TotalVolume += QtyFrag->GetTotalVolume();
		
		// Update by location
		if (LocFrag)
		{
			int32& LocationQty = Summary.QuantityByLocation.FindOrAdd(LocFrag->LocationId);
			LocationQty += QtyFrag->Quantity;
		}
		
		// Update by state
		if (StateFrag)
		{
			int32& StateQty = Summary.QuantityByState.FindOrAdd(static_cast<uint8>(StateFrag->State));
			StateQty += QtyFrag->Quantity;
		}
		
		// Update reserved quantity
		if (ResFrag && ResFrag->bReserved)
		{
			Summary.ReservedQuantity += QtyFrag->Quantity;
		}
	}
	
	UE_LOG(LogPraxisSim, Verbose, TEXT("Updated inventory aggregates: %d SKUs, %d entities"), 
		InventoryCache.Num(), MaterialEntities.Num());
}

void UPraxisInventoryService::LogTransaction(const FInventoryTransaction& Transaction)
{
	TransactionHistory.Add(Transaction);
	
	// Keep history bounded
	if (TransactionHistory.Num() > 10000)
	{
		TransactionHistory.RemoveAt(0, 1000); // Remove oldest 1000
	}
	
	UE_LOG(LogPraxisSim, Verbose, TEXT("[Transaction] %s: %s x%d @ %s"),
		*Transaction.TransactionType,
		*Transaction.SKU.ToString(),
		Transaction.QuantityDelta,
		*Transaction.LocationId.ToString());
}

bool UPraxisInventoryService::UpdateLocationCapacity(FName LocationId, float VolumeDelta, int32 ItemDelta)
{
	FLocationCapacity& Capacity = Locations.FindOrAdd(LocationId);
	
	// If location not explicitly registered, initialize with default name
	if (Capacity.LocationId.IsNone())
	{
		Capacity.LocationId = LocationId;
	}
	
	// Check if we have capacity limits defined (MaxVolume or MaxItems > 0)
	const bool bHasVolumeCap = Capacity.MaxVolume > 0.0f;
	const bool bHasItemCap = Capacity.MaxItems > 0;
	
	if (!bHasVolumeCap && !bHasItemCap)
	{
		// No limits defined - allow anything, just track usage
		Capacity.CurrentVolume += VolumeDelta;
		Capacity.CurrentItems += ItemDelta;
		return true;
	}
	
	// Check capacity
	const float NewVolume = Capacity.CurrentVolume + VolumeDelta;
	const int32 NewItems = Capacity.CurrentItems + ItemDelta;
	
	// Only check limits that are defined
	if (bHasVolumeCap && NewVolume > Capacity.MaxVolume)
	{
		const float UsedPercentage = (NewVolume / Capacity.MaxVolume) * 100.0f;
		OnLocationCapacityWarning.Broadcast(LocationId, UsedPercentage);
		return false;
	}
	
	if (bHasItemCap && NewItems > Capacity.MaxItems)
	{
		OnLocationCapacityWarning.Broadcast(LocationId, 100.0f);
		return false;
	}
	
	// Update capacity
	Capacity.CurrentVolume = FMath::Max(0.0f, NewVolume);
	Capacity.CurrentItems = FMath::Max(0, NewItems);
	
	// Warn if approaching capacity (>80%)
	if (bHasVolumeCap)
	{
		const float UsedPercentage = Capacity.GetVolumeUsagePercent();
		if (UsedPercentage > 80.0f)
		{
			OnLocationCapacityWarning.Broadcast(LocationId, UsedPercentage);
		}
	}
	
	return true;
}

void UPraxisInventoryService::DebugPrintInventory(FName SKU) const
{
	FInventorySummary Summary = GetInventorySummary(SKU);
	
	UE_LOG(LogPraxisSim, Log, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogPraxisSim, Log, TEXT("INVENTORY SUMMARY: %s"), *SKU.ToString());
	UE_LOG(LogPraxisSim, Log, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogPraxisSim, Log, TEXT("  Total Quantity: %d"), Summary.TotalQuantity);
	UE_LOG(LogPraxisSim, Log, TEXT("  Reserved: %d"), Summary.ReservedQuantity);
	UE_LOG(LogPraxisSim, Log, TEXT("  Available: %d"), Summary.GetAvailableQuantity());
	UE_LOG(LogPraxisSim, Log, TEXT("  Total Volume: %.2f m³"), Summary.TotalVolume);
	
	UE_LOG(LogPraxisSim, Log, TEXT("  By Location:"));
	for (const auto& Pair : Summary.QuantityByLocation)
	{
		UE_LOG(LogPraxisSim, Log, TEXT("    %s: %d"), *Pair.Key.ToString(), Pair.Value);
	}
	
	UE_LOG(LogPraxisSim, Log, TEXT("  By State:"));
	for (const auto& Pair : Summary.QuantityByState)
	{
		FString StateName;
		switch (static_cast<EMaterialState>(Pair.Key))
		{
			case EMaterialState::RawMaterial: StateName = TEXT("RawMaterial"); break;
			case EMaterialState::WorkInProcess: StateName = TEXT("WIP"); break;
			case EMaterialState::FinishedGoods: StateName = TEXT("FinishedGood"); break;
			case EMaterialState::Scrap: StateName = TEXT("Scrap"); break;
			default: StateName = TEXT("Unknown"); break;
		}
		UE_LOG(LogPraxisSim, Log, TEXT("    %s: %d"), *StateName, Pair.Value);
	}
	
	UE_LOG(LogPraxisSim, Log, TEXT("  Total Entities: %d"), MaterialEntities.Num());
	UE_LOG(LogPraxisSim, Log, TEXT("═══════════════════════════════════════════════════════════"));
}
