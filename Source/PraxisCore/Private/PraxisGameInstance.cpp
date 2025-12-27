// Fill out your copyright notice in the Description page of Project Settings.

#include "PraxisGameInstance.h"
#include "PraxisCore.h"  // for LogPraxisSim

void UPraxisGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogPraxisSim, Log, TEXT("Praxis GameInstance initialized"));
}
