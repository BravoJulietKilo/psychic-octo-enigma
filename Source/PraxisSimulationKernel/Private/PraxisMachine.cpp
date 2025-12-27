// Copyright 2025 Celsian Pty Ltd
// APraxisMachine - Lightweight visual actor representing a machine in the simulation.
// This actor delegates all simulation logic to UMachineLogicComponent.

#include "PraxisMachine.h"
#include "Components/StaticMeshComponent.h"
#include "PraxisSimulationKernel/Public/PraxisSimulationKernel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MachineLogicComponent.h"
#include "PraxisSimulationKernel/Public/Components/MachineLogicComponent.h"

// Sets default values
APraxisMachine::APraxisMachine()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create the visual mesh
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
	RootComponent = VisualMesh;

	// Create the machine logic component
	LogicComponent = CreateDefaultSubobject<UMachineLogicComponent>(TEXT("LogicComponent"));
}

void APraxisMachine::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogPraxisSim, Log, TEXT("%s: Machine BeginPlay."), *MachineId.ToString());
	if (LogicComponent)
	{
		UE_LOG(LogPraxisSim, Log, TEXT("%s: LogicComponent attached and ready."), *MachineId.ToString());
	}
}

void APraxisMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogPraxisSim, Log, TEXT("%s: Machine EndPlay."), *MachineId.ToString());
	Super::EndPlay(EndPlayReason);
}

void APraxisMachine::SetActive(bool bInActive)
{
	if (LogicComponent)
	{
		LogicComponent->SetComponentTickEnabled(bInActive);
	}
	UE_LOG(LogPraxisSim, Log, TEXT("%s: Active state set to %s."), *MachineId.ToString(), bInActive ? TEXT("TRUE") : TEXT("FALSE"));
}