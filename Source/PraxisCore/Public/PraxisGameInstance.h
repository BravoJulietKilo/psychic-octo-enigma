// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PraxisGameInstance.generated.h"

UCLASS()

class PRAXISCORE_API UPraxisGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
};

