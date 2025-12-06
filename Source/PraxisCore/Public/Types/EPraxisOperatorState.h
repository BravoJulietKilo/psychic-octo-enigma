#pragma once

UENUM(BlueprintType)
enum class EPraxisOperatorState : uint8
{
    Idle        UMETA(DisplayName="Idle"),
    Sitting     UMETA(DisplayName="Sitting"),
    Standing    UMETA(DisplayName="Standing"),
    Walking     UMETA(DisplayName="Walking"),
    Working     UMETA(DisplayName="Working"),
    OnBreak     UMETA(DisplayName="OnBreak"),
    Unavailable UMETA(DisplayName="Unavailable"),
    Listening   UMETA(DisplayName="Listening"),
    Talking     UMETA(DisplayName="Talking"),
    Explaining  UMETA(DisplayName="Explaining")
    	
};