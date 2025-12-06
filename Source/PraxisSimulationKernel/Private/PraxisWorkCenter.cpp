// Fill out your copyright notice in the Description page of Project Settings.


#include "PraxisWorkCenter.h"


// Sets default values
APraxisWorkCenter::APraxisWorkCenter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APraxisWorkCenter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APraxisWorkCenter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

