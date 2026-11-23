#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/LBTypes.h"
#include "LBGameData.generated.h"

/**
 * Game tables ported from the browser version: weapons, loot tables, furniture.
 * Loot is scarce on purpose: about half of all containers are empty.
 */
UCLASS()
class LASTBLOCK_API ULBGameData : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "LastBlock|Data")
	static FLBWeaponDef GetWeapon(ELBWeapon Weapon);

	UFUNCTION(BlueprintPure, Category = "LastBlock|Data")
	static FLBFurnitureDef GetFurniture(ELBFurniture Kind);

	/** Rolls 0-2 items from a loot table. RichMax >= 3 makes empty rolls rarer (shop shelves, army crates) */
	static TArray<ELBItem> RollLoot(FName Table, int32 RichMax, FRandomStream& Stream);

	UFUNCTION(BlueprintPure, Category = "LastBlock|Data")
	static FText GetItemName(ELBItem Item);

	/** Weapon an item unlocks, or Fists for consumables */
	UFUNCTION(BlueprintPure, Category = "LastBlock|Data")
	static bool ItemToWeapon(ELBItem Item, ELBWeapon& OutWeapon);

	/** Furniture sets per room type, used when furnishing houses */
	static const TArray<ELBFurniture>& RoomSet(FName Room);
};
