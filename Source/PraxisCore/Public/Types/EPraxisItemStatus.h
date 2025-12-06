#pragma once

UENUM(BlueprintType)
enum class EPraxisItemStatus : uint8
{
	RawMaterial     UMETA(DisplayName="RawMaterial"),
	WorkInProcess   UMETA(DisplayName="WorkInProcess"),
	FinishedGoods   UMETA(DisplayName="FinishedGoods"),
	Scrap           UMETA(DisplayName="Scrap")

};