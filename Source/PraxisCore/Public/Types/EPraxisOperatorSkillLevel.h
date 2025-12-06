#pragma once

UENUM(BlueprintType)
enum class EPraxisOperatorSkillLevel : uint8
{
	None        UMETA(DisplayName="None"),
	EntryLevel  UMETA(DisplayName="Entry Level"),
	Developing  UMETA(DisplayName="Developing"),
	Moderate    UMETA(DisplayName="Moderate"),
	Advanced    UMETA(DisplayName="Advanced"),
	Expert      UMETA(DisplayName="Expert")
};