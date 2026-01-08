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
		SKU, Quantity, LocationId, SubLocationId, VolumePerUnit, 0);  // 0 = RawMaterial
	
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
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot reserve material - Mass subsystem not available"));
		return false;
	}
	
	if (Quantity <= 0)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Cannot reserve material - invalid quantity: %d"), Quantity);
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Find unreserved entities matching SKU and location
	int32 RemainingToReserve = Quantity;
	TArray<FMassEntityHandle> EntitiesToReserve;
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check SKU match
		const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
		if (!TypeFrag || TypeFrag->SKU != SKU)
		{
			continue;
		}
		
		// Check location match
		const FMaterialLocationFragment* LocationFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		if (!LocationFrag || LocationFrag->LocationId != LocationId)
		{
			continue;
		}
		
		// Check not already reserved
		const FMaterialReservationFragment* ReservationFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (!ReservationFrag || ReservationFrag->bReserved)
		{
			continue;
		}
		
		// Check quantity available
		const FMaterialQuantityFragment* QuantityFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		if (!QuantityFrag || QuantityFrag->Quantity <= 0)
		{
			continue;
		}
		
		// This entity can contribute to the reservation
		EntitiesToReserve.Add(Entity);
		RemainingToReserve -= QuantityFrag->Quantity;
		
		if (RemainingToReserve <= 0)
		{
			break;  // We have enough
		}
	}
	
	// Check if we found enough material
	if (RemainingToReserve > 0)
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Insufficient unreserved %s at %s. Needed: %d, Available: %d"),
			*SKU.ToString(), *LocationId.ToString(), Quantity, Quantity - RemainingToReserve);
		return false;
	}
	
	// Mark entities as reserved
	int32 TotalReserved = 0;
	for (const FMassEntityHandle& Entity : EntitiesToReserve)
	{
		FMaterialReservationFragment* ReservationFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		const FMaterialQuantityFragment* QuantityFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		
		if (ReservationFrag && QuantityFrag)
		{
			ReservationFrag->bReserved = true;
			ReservationFrag->ReservedForWorkOrder = WorkOrderId;
			ReservationFrag->ReservedForMachine = MachineId;
			ReservationFrag->ReservationTime = GetWorld()->GetTimeSeconds();
			
			TotalReserved += QuantityFrag->Quantity;
		}
	}
	
	// Log transaction
	FInventoryTransaction Transaction;
	Transaction.TransactionType = TEXT("Reservation");
	Transaction.SKU = SKU;
	Transaction.QuantityDelta = TotalReserved;
	Transaction.LocationId = LocationId;
	Transaction.Reference = FString::Printf(TEXT("WO:%lld Machine:%s"), WorkOrderId, *MachineId.ToString());
	Transaction.Timestamp = FDateTime::UtcNow();
	LogTransaction(Transaction);
	
	// Update aggregates
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Reserved %d units of %s at %s for WO:%lld (Machine: %s)"),
		TotalReserved, *SKU.ToString(), *LocationId.ToString(), WorkOrderId, *MachineId.ToString());
	
	return true;
}

// ════════════════════════════════════════════════════════════════════════════════
// Production Operations
// ════════════════════════════════════════════════════════════════════════════════

bool UPraxisInventoryService::ConsumeReservedMaterial(
	FName MachineId,
	int64 WorkOrderId,
	FName SKU)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot consume material - Mass subsystem not available"));
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Find a reserved entity matching the work order and machine
	FMassEntityHandle FoundEntity;
	float VolumePerUnit = 0.01f;
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check reservation matches
		const FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (!ResFrag || !ResFrag->bReserved)
		{
			continue;
		}
		
		if (ResFrag->ReservedForWorkOrder != WorkOrderId || ResFrag->ReservedForMachine != MachineId)
		{
			continue;
		}
		
		// Check SKU matches
		const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
		if (!TypeFrag || TypeFrag->SKU != SKU)
		{
			continue;
		}
		
		// Check has quantity
		const FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		if (!QtyFrag || QtyFrag->Quantity <= 0)
		{
			continue;
		}
		
		FoundEntity = Entity;
		VolumePerUnit = QtyFrag->VolumePerUnit;
		break;
	}
	
	if (!FoundEntity.IsSet())
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("No reserved %s found for WO:%lld on Machine:%s"),
			*SKU.ToString(), WorkOrderId, *MachineId.ToString());
		return false;
	}
	
	// Get fragment pointers
	FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(FoundEntity);
	FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(FoundEntity);
	FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(FoundEntity);
	const FMaterialGenealogyFragment* GenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(FoundEntity);
	
	if (!QtyFrag)
	{
		return false;
	}
	
	FName SourceLocation = LocFrag ? LocFrag->LocationId : NAME_None;
	FGuid ParentBatchId = GenFrag ? GenFrag->BatchId : FGuid();
	
	// Decrement quantity
	QtyFrag->Quantity -= 1;
	
	// Update source location capacity
	UpdateLocationCapacity(SourceLocation, -VolumePerUnit, 0);
	
	// Create WIP entity at machine
	FName MachineWIPLocation = FName(*FString::Printf(TEXT("%s.WIP"), *MachineId.ToString()));
	FMassEntityHandle WIPEntity = SpawnMaterialEntity(
		SKU, 1, MachineWIPLocation, NAME_None, VolumePerUnit, 1);  // 1 = WorkInProcess
	
	if (WIPEntity.IsSet())
	{
		MaterialEntities.Add(WIPEntity);
		
		// Set genealogy to link to parent batch
		FMaterialGenealogyFragment* WIPGenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(WIPEntity);
		if (WIPGenFrag && ParentBatchId.IsValid())
		{
			WIPGenFrag->ParentBatchIds.Add(ParentBatchId);
			WIPGenFrag->SourceMachineId = MachineId;
			WIPGenFrag->SourceWorkOrderId = WorkOrderId;
		}
		
		// Set reservation on WIP to track it
		FMaterialReservationFragment* WIPResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(WIPEntity);
		if (WIPResFrag)
		{
			WIPResFrag->bReserved = true;
			WIPResFrag->ReservedForWorkOrder = WorkOrderId;
			WIPResFrag->ReservedForMachine = MachineId;
			WIPResFrag->ReservationTime = GetWorld()->GetTimeSeconds();
		}
	}
	
	// If source entity is depleted, remove it
	if (QtyFrag->Quantity <= 0)
	{
		UpdateLocationCapacity(SourceLocation, 0, -1);  // Remove item count
		EntityManager.DestroyEntity(FoundEntity);
		MaterialEntities.Remove(FoundEntity);
	}
	
	// Log transaction
	FInventoryTransaction Transaction;
	Transaction.TransactionType = TEXT("Consumption");
	Transaction.SKU = SKU;
	Transaction.QuantityDelta = -1;
	Transaction.LocationId = SourceLocation;
	Transaction.Reference = FString::Printf(TEXT("WO:%lld Machine:%s"), WorkOrderId, *MachineId.ToString());
	Transaction.Timestamp = FDateTime::UtcNow();
	LogTransaction(Transaction);
	
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("Consumed 1 %s for WO:%lld (Machine: %s) -> WIP"),
		*SKU.ToString(), WorkOrderId, *MachineId.ToString());
	
	return true;
}

bool UPraxisInventoryService::ProduceFinishedGood(
	FName MachineId,
	int64 WorkOrderId,
	FName OutputSKU,
	FName OutputLocationId)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot produce FG - Mass subsystem not available"));
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Find WIP entity for this machine/work order
	FMassEntityHandle WIPEntity;
	FName MachineWIPLocation = FName(*FString::Printf(TEXT("%s.WIP"), *MachineId.ToString()));
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check state is WIP
		const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
		if (!StateFrag || StateFrag->State != EMaterialState::WorkInProcess)
		{
			continue;
		}
		
		// Check location is machine WIP
		const FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		if (!LocFrag || LocFrag->LocationId != MachineWIPLocation)
		{
			continue;
		}
		
		// Check reservation matches
		const FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (!ResFrag || ResFrag->ReservedForWorkOrder != WorkOrderId || ResFrag->ReservedForMachine != MachineId)
		{
			continue;
		}
		
		WIPEntity = Entity;
		break;
	}
	
	if (!WIPEntity.IsSet())
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("No WIP found for WO:%lld on Machine:%s"),
			WorkOrderId, *MachineId.ToString());
		return false;
	}
	
	// Get WIP data
	const FMaterialQuantityFragment* WIPQtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(WIPEntity);
	const FMaterialGenealogyFragment* WIPGenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(WIPEntity);
	
	float VolumePerUnit = WIPQtyFrag ? WIPQtyFrag->VolumePerUnit : 0.01f;
	FGuid WIPBatchId = WIPGenFrag ? WIPGenFrag->BatchId : FGuid();
	
	// Destroy WIP entity
	EntityManager.DestroyEntity(WIPEntity);
	MaterialEntities.Remove(WIPEntity);
	
	// Create FG entity at output location
	FMassEntityHandle FGEntity = SpawnMaterialEntity(
		OutputSKU, 1, OutputLocationId, NAME_None, VolumePerUnit, 2);  // 2 = FinishedGoods
	
	if (FGEntity.IsSet())
	{
		MaterialEntities.Add(FGEntity);
		
		// Set genealogy
		FMaterialGenealogyFragment* FGGenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(FGEntity);
		if (FGGenFrag)
		{
			if (WIPBatchId.IsValid())
			{
				FGGenFrag->ParentBatchIds.Add(WIPBatchId);
			}
			FGGenFrag->SourceMachineId = MachineId;
			FGGenFrag->SourceWorkOrderId = WorkOrderId;
			FGGenFrag->bPassedQuality = true;
		}
		
		// Update destination capacity
		UpdateLocationCapacity(OutputLocationId, VolumePerUnit, 1);
	}
	
	// Log transaction
	FInventoryTransaction Transaction;
	Transaction.TransactionType = TEXT("Production");
	Transaction.SKU = OutputSKU;
	Transaction.QuantityDelta = 1;
	Transaction.LocationId = OutputLocationId;
	Transaction.Reference = FString::Printf(TEXT("WO:%lld Machine:%s"), WorkOrderId, *MachineId.ToString());
	Transaction.Timestamp = FDateTime::UtcNow();
	LogTransaction(Transaction);
	
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("Produced 1 %s for WO:%lld (Machine: %s) -> %s"),
		*OutputSKU.ToString(), WorkOrderId, *MachineId.ToString(), *OutputLocationId.ToString());
	
	OnInventoryChanged.Broadcast(OutputSKU, OutputLocationId, 1);
	return true;
}

bool UPraxisInventoryService::ProduceScrap(
	FName MachineId,
	int64 WorkOrderId,
	FName SKU,
	FName ScrapLocationId)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot produce scrap - Mass subsystem not available"));
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Find WIP entity for this machine/work order
	FMassEntityHandle WIPEntity;
	FName MachineWIPLocation = FName(*FString::Printf(TEXT("%s.WIP"), *MachineId.ToString()));
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check state is WIP
		const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
		if (!StateFrag || StateFrag->State != EMaterialState::WorkInProcess)
		{
			continue;
		}
		
		// Check location is machine WIP
		const FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		if (!LocFrag || LocFrag->LocationId != MachineWIPLocation)
		{
			continue;
		}
		
		// Check reservation matches
		const FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (!ResFrag || ResFrag->ReservedForWorkOrder != WorkOrderId || ResFrag->ReservedForMachine != MachineId)
		{
			continue;
		}
		
		WIPEntity = Entity;
		break;
	}
	
	if (!WIPEntity.IsSet())
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("No WIP found for WO:%lld on Machine:%s to scrap"),
			WorkOrderId, *MachineId.ToString());
		return false;
	}
	
	// Get WIP data
	const FMaterialQuantityFragment* WIPQtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(WIPEntity);
	const FMaterialGenealogyFragment* WIPGenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(WIPEntity);
	
	float VolumePerUnit = WIPQtyFrag ? WIPQtyFrag->VolumePerUnit : 0.01f;
	FGuid WIPBatchId = WIPGenFrag ? WIPGenFrag->BatchId : FGuid();
	
	// Destroy WIP entity
	EntityManager.DestroyEntity(WIPEntity);
	MaterialEntities.Remove(WIPEntity);
	
	// Create Scrap entity at scrap location
	FMassEntityHandle ScrapEntity = SpawnMaterialEntity(
		SKU, 1, ScrapLocationId, NAME_None, VolumePerUnit, 3);  // 3 = Scrap
	
	if (ScrapEntity.IsSet())
	{
		MaterialEntities.Add(ScrapEntity);
		
		// Set genealogy with quality flag false
		FMaterialGenealogyFragment* ScrapGenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(ScrapEntity);
		if (ScrapGenFrag)
		{
			if (WIPBatchId.IsValid())
			{
				ScrapGenFrag->ParentBatchIds.Add(WIPBatchId);
			}
			ScrapGenFrag->SourceMachineId = MachineId;
			ScrapGenFrag->SourceWorkOrderId = WorkOrderId;
			ScrapGenFrag->bPassedQuality = false;  // Failed quality
		}
		
		// Update destination capacity
		UpdateLocationCapacity(ScrapLocationId, VolumePerUnit, 1);
	}
	
	// Log transaction
	FInventoryTransaction Transaction;
	Transaction.TransactionType = TEXT("Scrap");
	Transaction.SKU = SKU;
	Transaction.QuantityDelta = 1;
	Transaction.LocationId = ScrapLocationId;
	Transaction.Reference = FString::Printf(TEXT("WO:%lld Machine:%s"), WorkOrderId, *MachineId.ToString());
	Transaction.Timestamp = FDateTime::UtcNow();
	LogTransaction(Transaction);
	
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Verbose, 
		TEXT("Scrapped 1 %s for WO:%lld (Machine: %s) -> %s"),
		*SKU.ToString(), WorkOrderId, *MachineId.ToString(), *ScrapLocationId.ToString());
	
	return true;
}

bool UPraxisInventoryService::ReleaseReservation(
	FName MachineId,
	int64 WorkOrderId)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot release reservation - Mass subsystem not available"));
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	int32 ReleasedCount = 0;
	
	// Find all entities reserved for this work order/machine
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (!ResFrag || !ResFrag->bReserved)
		{
			continue;
		}
		
		if (ResFrag->ReservedForWorkOrder != WorkOrderId || ResFrag->ReservedForMachine != MachineId)
		{
			continue;
		}
		
		// Release the reservation
		ResFrag->bReserved = false;
		ResFrag->ReservedForWorkOrder = 0;
		ResFrag->ReservedForMachine = NAME_None;
		ResFrag->ReservationTime = 0.0;
		
		ReleasedCount++;
	}
	
	if (ReleasedCount > 0)
	{
		UpdateAggregates();
		
		UE_LOG(LogPraxisSim, Log, 
			TEXT("Released %d reservations for WO:%lld (Machine: %s)"),
			ReleasedCount, WorkOrderId, *MachineId.ToString());
	}
	
	return ReleasedCount > 0;
}

bool UPraxisInventoryService::TransformMaterial(
	FName BOMId,
	FName SourceMachineId,
	int64 WorkOrderId,
	FName OutputLocationId)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot transform material - Mass subsystem not available"));
		return false;
	}
	
	// Look up BOM
	const FBOMEntry* BOM = BOMs.Find(BOMId);
	if (!BOM)
	{
		UE_LOG(LogPraxisSim, Error, TEXT("BOM not found: %s"), *BOMId.ToString());
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	FName MachineWIPLocation = FName(*FString::Printf(TEXT("%s.WIP"), *SourceMachineId.ToString()));
	
	// ═══════════════════════════════════════════════════════════════════════════
	// Phase 1: Validate all inputs are available
	// ═══════════════════════════════════════════════════════════════════════════
	
	// Map of SKU -> list of (Entity, QuantityToConsume) pairs
	TMap<FName, TArray<TPair<FMassEntityHandle, int32>>> InputsToConsume;
	
	for (const auto& InputReq : BOM->InputRequirements)
	{
		const FName& InputSKU = InputReq.Key;
		const int32 RequiredQty = InputReq.Value;
		
		int32 RemainingToFind = RequiredQty;
		TArray<TPair<FMassEntityHandle, int32>>& EntitiesForSKU = InputsToConsume.FindOrAdd(InputSKU);
		
		// Find WIP entities at machine for this SKU
		for (const FMassEntityHandle& Entity : MaterialEntities)
		{
			if (RemainingToFind <= 0)
			{
				break;
			}
			
			if (!EntityManager.IsEntityValid(Entity))
			{
				continue;
			}
			
			// Check state is WIP
			const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
			if (!StateFrag || StateFrag->State != EMaterialState::WorkInProcess)
			{
				continue;
			}
			
			// Check location is machine WIP
			const FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
			if (!LocFrag || LocFrag->LocationId != MachineWIPLocation)
			{
				continue;
			}
			
			// Check SKU matches
			const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
			if (!TypeFrag || TypeFrag->SKU != InputSKU)
			{
				continue;
			}
			
			// Check reservation matches work order
			const FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
			if (!ResFrag || ResFrag->ReservedForWorkOrder != WorkOrderId || ResFrag->ReservedForMachine != SourceMachineId)
			{
				continue;
			}
			
			// Check quantity
			const FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
			if (!QtyFrag || QtyFrag->Quantity <= 0)
			{
				continue;
			}
			
			// Calculate how much to take from this entity
			const int32 TakeQty = FMath::Min(QtyFrag->Quantity, RemainingToFind);
			EntitiesForSKU.Add(TPair<FMassEntityHandle, int32>(Entity, TakeQty));
			RemainingToFind -= TakeQty;
		}
		
		if (RemainingToFind > 0)
		{
			UE_LOG(LogPraxisSim, Warning, 
				TEXT("Insufficient WIP %s at %s for BOM %s. Needed: %d, Available: %d"),
				*InputSKU.ToString(), *MachineWIPLocation.ToString(), *BOMId.ToString(),
				RequiredQty, RequiredQty - RemainingToFind);
			return false;
		}
	}
	
	// ═══════════════════════════════════════════════════════════════════════════
	// Phase 2: Check output capacity
	// ═══════════════════════════════════════════════════════════════════════════
	
	const float OutputVolume = BOM->OutputQuantity * BOM->OutputVolumePerUnit;
	if (!HasCapacity(OutputLocationId, OutputVolume))
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Insufficient capacity at %s for BOM %s output (%.2f m³ required)"),
			*OutputLocationId.ToString(), *BOMId.ToString(), OutputVolume);
		return false;
	}
	
	// ═══════════════════════════════════════════════════════════════════════════
	// Phase 3: Consume inputs and collect genealogy
	// ═══════════════════════════════════════════════════════════════════════════
	
	TArray<FGuid> ParentBatchIds;
	float TotalInputVolume = 0.0f;
	
	for (auto& SKUPair : InputsToConsume)
	{
		const FName& InputSKU = SKUPair.Key;
		TArray<TPair<FMassEntityHandle, int32>>& Entities = SKUPair.Value;
		
		for (auto& EntityPair : Entities)
		{
			FMassEntityHandle& Entity = EntityPair.Key;
			const int32 ConsumeQty = EntityPair.Value;
			
			// Get fragment pointers
			FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
			const FMaterialGenealogyFragment* GenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(Entity);
			
			if (!QtyFrag)
			{
				continue;
			}
			
			// Collect genealogy
			if (GenFrag && GenFrag->BatchId.IsValid())
			{
				ParentBatchIds.AddUnique(GenFrag->BatchId);
			}
			
			TotalInputVolume += ConsumeQty * QtyFrag->VolumePerUnit;
			
			// Decrement or destroy
			QtyFrag->Quantity -= ConsumeQty;
			
			if (QtyFrag->Quantity <= 0)
			{
				EntityManager.DestroyEntity(Entity);
				MaterialEntities.Remove(Entity);
			}
			
			// Log consumption transaction
			FInventoryTransaction ConsumeTx;
			ConsumeTx.TransactionType = TEXT("BOMConsumption");
			ConsumeTx.SKU = InputSKU;
			ConsumeTx.QuantityDelta = -ConsumeQty;
			ConsumeTx.LocationId = MachineWIPLocation;
			ConsumeTx.Reference = FString::Printf(TEXT("BOM:%s WO:%lld"), *BOMId.ToString(), WorkOrderId);
			ConsumeTx.Timestamp = FDateTime::UtcNow();
			LogTransaction(ConsumeTx);
		}
	}
	
	// ═══════════════════════════════════════════════════════════════════════════
	// Phase 4: Produce output
	// ═══════════════════════════════════════════════════════════════════════════
	
	FMassEntityHandle OutputEntity = SpawnMaterialEntity(
		BOM->OutputSKU,
		BOM->OutputQuantity,
		OutputLocationId,
		NAME_None,
		BOM->OutputVolumePerUnit,
		2);  // 2 = FinishedGoods
	
	if (OutputEntity.IsSet())
	{
		MaterialEntities.Add(OutputEntity);
		
		// Set genealogy with all parent batches
		FMaterialGenealogyFragment* OutputGenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(OutputEntity);
		if (OutputGenFrag)
		{
			OutputGenFrag->ParentBatchIds = ParentBatchIds;
			OutputGenFrag->SourceMachineId = SourceMachineId;
			OutputGenFrag->SourceWorkOrderId = WorkOrderId;
			OutputGenFrag->bPassedQuality = true;
		}
		
		// Set BOM ID on the output
		FMaterialTypeFragment* OutputTypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(OutputEntity);
		if (OutputTypeFrag)
		{
			OutputTypeFrag->BOMId = BOMId;
		}
		
		// Update destination capacity
		UpdateLocationCapacity(OutputLocationId, OutputVolume, 1);
	}
	
	// Log production transaction
	FInventoryTransaction ProduceTx;
	ProduceTx.TransactionType = TEXT("BOMProduction");
	ProduceTx.SKU = BOM->OutputSKU;
	ProduceTx.QuantityDelta = BOM->OutputQuantity;
	ProduceTx.LocationId = OutputLocationId;
	ProduceTx.Reference = FString::Printf(TEXT("BOM:%s WO:%lld Inputs:%d"), 
		*BOMId.ToString(), WorkOrderId, ParentBatchIds.Num());
	ProduceTx.Timestamp = FDateTime::UtcNow();
	if (OutputEntity.IsSet())
	{
		const FMaterialGenealogyFragment* OutGen = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(OutputEntity);
		if (OutGen)
		{
			ProduceTx.BatchId = OutGen->BatchId;
		}
	}
	LogTransaction(ProduceTx);
	
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("BOM Transform: %s -> %d x %s at %s (WO:%lld, Parents:%d)"),
		*BOMId.ToString(), BOM->OutputQuantity, *BOM->OutputSKU.ToString(),
		*OutputLocationId.ToString(), WorkOrderId, ParentBatchIds.Num());
	
	OnInventoryChanged.Broadcast(BOM->OutputSKU, OutputLocationId, BOM->OutputQuantity);
	return true;
}

bool UPraxisInventoryService::ShipFinishedGoods(
	FName SKU,
	int32 Quantity,
	FName LocationId)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot ship goods - Mass subsystem not available"));
		return false;
	}
	
	if (Quantity <= 0)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Cannot ship goods - invalid quantity: %d"), Quantity);
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Find unreserved FG entities matching SKU and location
	int32 RemainingToShip = Quantity;
	TArray<TPair<FMassEntityHandle, int32>> EntitiesToShip;  // Entity + quantity to take
	float TotalVolumeToShip = 0.0f;
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (RemainingToShip <= 0)
		{
			break;
		}
		
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check state is Finished Goods
		const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
		if (!StateFrag || StateFrag->State != EMaterialState::FinishedGoods)
		{
			continue;
		}
		
		// Check SKU match
		const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
		if (!TypeFrag || TypeFrag->SKU != SKU)
		{
			continue;
		}
		
		// Check location match
		const FMaterialLocationFragment* LocationFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		if (!LocationFrag || LocationFrag->LocationId != LocationId)
		{
			continue;
		}
		
		// Check not reserved
		const FMaterialReservationFragment* ReservationFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (ReservationFrag && ReservationFrag->bReserved)
		{
			continue;
		}
		
		// Check quantity available
		const FMaterialQuantityFragment* QuantityFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		if (!QuantityFrag || QuantityFrag->Quantity <= 0)
		{
			continue;
		}
		
		// Calculate how much to take from this entity
		const int32 TakeQuantity = FMath::Min(QuantityFrag->Quantity, RemainingToShip);
		EntitiesToShip.Add(TPair<FMassEntityHandle, int32>(Entity, TakeQuantity));
		TotalVolumeToShip += TakeQuantity * QuantityFrag->VolumePerUnit;
		RemainingToShip -= TakeQuantity;
	}
	
	// Check if we found enough material
	if (RemainingToShip > 0)
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Insufficient FG %s at %s to ship. Needed: %d, Available: %d"),
			*SKU.ToString(), *LocationId.ToString(), Quantity, Quantity - RemainingToShip);
		return false;
	}
	
	// Perform shipment - remove entities from inventory
	int32 TotalShipped = 0;
	TArray<FGuid> ShippedBatchIds;
	
	for (const auto& ShipPair : EntitiesToShip)
	{
		const FMassEntityHandle& Entity = ShipPair.Key;
		const int32 ShipQty = ShipPair.Value;
		
		FMaterialQuantityFragment* QuantityFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		const FMaterialGenealogyFragment* GenFrag = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(Entity);
		
		if (!QuantityFrag)
		{
			continue;
		}
		
		// Collect batch IDs for transaction record
		if (GenFrag && GenFrag->BatchId.IsValid())
		{
			ShippedBatchIds.AddUnique(GenFrag->BatchId);
		}
		
		const float ShipVolume = ShipQty * QuantityFrag->VolumePerUnit;
		
		if (ShipQty == QuantityFrag->Quantity)
		{
			// Ship entire entity - destroy it
			UpdateLocationCapacity(LocationId, -ShipVolume, -1);
			EntityManager.DestroyEntity(Entity);
			MaterialEntities.Remove(Entity);
		}
		else
		{
			// Partial shipment - reduce quantity
			QuantityFrag->Quantity -= ShipQty;
			UpdateLocationCapacity(LocationId, -ShipVolume, 0);  // Volume down, item count unchanged
		}
		
		TotalShipped += ShipQty;
	}
	
	// Log transaction
	FInventoryTransaction Transaction;
	Transaction.TransactionType = TEXT("Shipment");
	Transaction.SKU = SKU;
	Transaction.QuantityDelta = -TotalShipped;
	Transaction.LocationId = LocationId;
	Transaction.Reference = FString::Printf(TEXT("Batches:%d"), ShippedBatchIds.Num());
	Transaction.Timestamp = FDateTime::UtcNow();
	LogTransaction(Transaction);
	
	// Update aggregates
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Shipped %d units of %s from %s (Batches: %d)"),
		TotalShipped, *SKU.ToString(), *LocationId.ToString(), ShippedBatchIds.Num());
	
	OnInventoryChanged.Broadcast(SKU, LocationId, -TotalShipped);
	return true;
}

bool UPraxisInventoryService::TransferMaterial(
	FName SKU,
	int32 Quantity,
	FName FromLocation,
	FName ToLocation)
{
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		UE_LOG(LogPraxisSim, Error, TEXT("Cannot transfer material - Mass subsystem not available"));
		return false;
	}
	
	if (Quantity <= 0)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Cannot transfer material - invalid quantity: %d"), Quantity);
		return false;
	}
	
	if (FromLocation == ToLocation)
	{
		UE_LOG(LogPraxisSim, Warning, TEXT("Cannot transfer material - source and destination are the same"));
		return false;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Find entities to transfer (unreserved, matching SKU and source location)
	int32 RemainingToTransfer = Quantity;
	TArray<TPair<FMassEntityHandle, int32>> EntitiesToTransfer;  // Entity + quantity to take from it
	float TotalVolumeToTransfer = 0.0f;
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check SKU match
		const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
		if (!TypeFrag || TypeFrag->SKU != SKU)
		{
			continue;
		}
		
		// Check location match
		const FMaterialLocationFragment* LocationFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		if (!LocationFrag || LocationFrag->LocationId != FromLocation)
		{
			continue;
		}
		
		// Check not reserved (can't transfer reserved material)
		const FMaterialReservationFragment* ReservationFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		if (ReservationFrag && ReservationFrag->bReserved)
		{
			continue;
		}
		
		// Check quantity available
		const FMaterialQuantityFragment* QuantityFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		if (!QuantityFrag || QuantityFrag->Quantity <= 0)
		{
			continue;
		}
		
		// Calculate how much to take from this entity
		const int32 TakeQuantity = FMath::Min(QuantityFrag->Quantity, RemainingToTransfer);
		EntitiesToTransfer.Add(TPair<FMassEntityHandle, int32>(Entity, TakeQuantity));
		TotalVolumeToTransfer += TakeQuantity * QuantityFrag->VolumePerUnit;
		RemainingToTransfer -= TakeQuantity;
		
		if (RemainingToTransfer <= 0)
		{
			break;
		}
	}
	
	// Check if we found enough material
	if (RemainingToTransfer > 0)
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Insufficient unreserved %s at %s. Needed: %d, Available: %d"),
			*SKU.ToString(), *FromLocation.ToString(), Quantity, Quantity - RemainingToTransfer);
		return false;
	}
	
	// Check destination capacity
	if (!HasCapacity(ToLocation, TotalVolumeToTransfer))
	{
		UE_LOG(LogPraxisSim, Warning, 
			TEXT("Insufficient capacity at %s for %.2f m³"),
			*ToLocation.ToString(), TotalVolumeToTransfer);
		return false;
	}
	
	// Perform transfers
	int32 TotalTransferred = 0;
	for (const auto& TransferPair : EntitiesToTransfer)
	{
		const FMassEntityHandle& Entity = TransferPair.Key;
		const int32 TransferQty = TransferPair.Value;
		
		FMaterialQuantityFragment* QuantityFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		FMaterialLocationFragment* LocationFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		
		if (!QuantityFrag || !LocationFrag)
		{
			continue;
		}
		
		const float TransferVolume = TransferQty * QuantityFrag->VolumePerUnit;
		
		if (TransferQty == QuantityFrag->Quantity)
		{
			// Transfer entire entity - just update location
			LocationFrag->LocationId = ToLocation;
			LocationFrag->LocationEnterTime = GetWorld()->GetTimeSeconds();
			
			// Update capacities
			UpdateLocationCapacity(FromLocation, -TransferVolume, -1);
			UpdateLocationCapacity(ToLocation, TransferVolume, 1);
			
			TotalTransferred += TransferQty;
		}
		else
		{
			// Partial transfer - reduce source and create new entity at destination
			QuantityFrag->Quantity -= TransferQty;
			
			// Get state for new entity
			const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
			uint8 State = StateFrag ? static_cast<uint8>(StateFrag->State) : 0;
			
			// Spawn new entity at destination
			FMassEntityHandle NewEntity = SpawnMaterialEntity(
				SKU, TransferQty, ToLocation, NAME_None, QuantityFrag->VolumePerUnit, State);
			
			if (NewEntity.IsSet())
			{
				MaterialEntities.Add(NewEntity);
				
				// Copy genealogy from source
				const FMaterialGenealogyFragment* SourceGenealogy = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(Entity);
				FMaterialGenealogyFragment* NewGenealogy = EntityManager.GetFragmentDataPtr<FMaterialGenealogyFragment>(NewEntity);
				if (SourceGenealogy && NewGenealogy)
				{
					NewGenealogy->ParentBatchIds.Add(SourceGenealogy->BatchId);
				}
				
				// Update capacities (source volume reduction, destination addition)
				UpdateLocationCapacity(FromLocation, -TransferVolume, 0);  // Volume down, item count unchanged
				UpdateLocationCapacity(ToLocation, TransferVolume, 1);    // New entity at destination
				
				TotalTransferred += TransferQty;
			}
		}
	}
	
	// Log transaction
	FInventoryTransaction Transaction;
	Transaction.TransactionType = TEXT("Transfer");
	Transaction.SKU = SKU;
	Transaction.QuantityDelta = TotalTransferred;
	Transaction.LocationId = ToLocation;
	Transaction.Reference = FString::Printf(TEXT("From: %s"), *FromLocation.ToString());
	Transaction.Timestamp = FDateTime::UtcNow();
	LogTransaction(Transaction);
	
	// Update aggregates
	UpdateAggregates();
	
	UE_LOG(LogPraxisSim, Log, 
		TEXT("Transferred %d units of %s from %s to %s"),
		TotalTransferred, *SKU.ToString(), *FromLocation.ToString(), *ToLocation.ToString());
	
	OnInventoryChanged.Broadcast(SKU, FromLocation, -TotalTransferred);
	OnInventoryChanged.Broadcast(SKU, ToLocation, TotalTransferred);
	
	return true;
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

TArray<FLocationInventoryItem> UPraxisInventoryService::GetInventoryAtLocation(FName LocationId) const
{
	TArray<FLocationInventoryItem> Result;
	
	if (!MassSubsystem || !MassSubsystem->IsInitialized())
	{
		return Result;
	}
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	// Aggregate inventory by SKU and state at this location
	TMap<TPair<FName, uint8>, FLocationInventoryItem> Aggregated;
	
	for (const FMassEntityHandle& Entity : MaterialEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		
		// Check location match
		const FMaterialLocationFragment* LocFrag = EntityManager.GetFragmentDataPtr<FMaterialLocationFragment>(Entity);
		if (!LocFrag || LocFrag->LocationId != LocationId)
		{
			continue;
		}
		
		// Get other fragments
		const FMaterialTypeFragment* TypeFrag = EntityManager.GetFragmentDataPtr<FMaterialTypeFragment>(Entity);
		const FMaterialStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMaterialStateFragment>(Entity);
		const FMaterialQuantityFragment* QtyFrag = EntityManager.GetFragmentDataPtr<FMaterialQuantityFragment>(Entity);
		const FMaterialReservationFragment* ResFrag = EntityManager.GetFragmentDataPtr<FMaterialReservationFragment>(Entity);
		
		if (!TypeFrag || !QtyFrag)
		{
			continue;
		}
		
		const uint8 State = StateFrag ? static_cast<uint8>(StateFrag->State) : 0;
		const TPair<FName, uint8> Key(TypeFrag->SKU, State);
		
		FLocationInventoryItem& Item = Aggregated.FindOrAdd(Key);
		Item.SKU = TypeFrag->SKU;
		Item.MaterialState = State;
		Item.Quantity += QtyFrag->Quantity;
		Item.Volume += QtyFrag->GetTotalVolume();
		
		// Mark as reserved if any entity is reserved
		if (ResFrag && ResFrag->bReserved)
		{
			Item.bReserved = true;
		}
	}
	
	// Convert map to array
	for (const auto& Pair : Aggregated)
	{
		Result.Add(Pair.Value);
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
	uint8 InitialState)
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
		StateFrag->State = static_cast<EMaterialState>(InitialState);
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
