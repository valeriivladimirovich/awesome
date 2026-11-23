#pragma once

#include "CoreMinimal.h"
#include "LBTypes.generated.h"

// One map tile is 1.8 m, the same scale as the browser version.
static constexpr float LB_TILE = 180.f;
static constexpr int32 LB_MAP_N = 150;

UENUM(BlueprintType)
enum class ELBGround : uint8
{
	Grass, Road, Walk, Dirt, Wood, Tile, Concrete, Lot, Water, Forest, Sand
};

// Wall edge codes, as in the browser map: 1 solid, 2 window, 3 fence, 4 door
UENUM(BlueprintType)
enum class ELBEdge : uint8
{
	None = 0, Solid = 1, Window = 2, Fence = 3, Door = 4
};

UENUM(BlueprintType)
enum class ELBItem : uint8
{
	None, Food, Water, Bandage, Medkit, Battery, Ammo9, Ammo545,
	Bat, Axe, Pistol, Rifle
};

UENUM(BlueprintType)
enum class ELBWeapon : uint8
{
	Fists, Bat, Axe, Pistol, Rifle
};

UENUM(BlueprintType)
enum class ELBBuildingType : uint8
{
	House, Shed, Shop, Pharmacy, Hardware, Cafe, GasShop, Warehouse, Police, Clinic, Tent
};

UENUM(BlueprintType)
enum class ELBFurniture : uint8
{
	Bed, HospitalBed, Sofa, TV, Table, CoffeeTable, Wardrobe, Fridge, Counter, Stove, Bath, Toilet,
	Cabinet, Desk, Bookshelf, Plant, Shelf, Checkout, Crate, Workbench, Locker, MilitaryCrate
};

/** Which wall a piece of furniture backs onto; C = free-standing */
UENUM(BlueprintType)
enum class ELBSide : uint8
{
	N, S, W, E, C
};

UENUM(BlueprintType)
enum class ELBCarKind : uint8
{
	Sedan, Hatch, SUV, Pickup, Police, Truck, Ambulance, MilitaryTruck
};

USTRUCT(BlueprintType)
struct FLBWeaponDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 1.f;
	/** Reach in tiles for melee, max range in tiles for guns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Range = .8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooldown = .42f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bGun = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAuto = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MagSize = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ELBItem Ammo = ELBItem::None;
	/** Hearing radius in tiles that a shot reaches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Noise = 0.f;
	/** Aim spread in radians */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Spread = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReloadTime = 0.f;
	/** Melee: centre distance (tiles) the player steps in to, so the fist or blade sinks into the body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SinkDistance = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct FLBFurnitureDef
{
	GENERATED_BODY()

	/** Footprint in tiles (width along the wall, depth away from it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Size = FVector2D(1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeightCm = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCentered = false;
	/** Empty text means it cannot be searched */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText ContainerName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName LootTable;
};

/** A parked or wrecked car produced by the city generator */
USTRUCT(BlueprintType)
struct FLBCarSpot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVector2D Pos = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bAlongY = false;
	UPROPERTY(BlueprintReadOnly) ELBCarKind Kind = ELBCarKind::Sedan;
	UPROPERTY(BlueprintReadOnly) FLinearColor Paint = FLinearColor::Gray;
	UPROPERTY(BlueprintReadOnly) bool bWreck = false;
	UPROPERTY(BlueprintReadOnly) bool bBurnt = false;
};

USTRUCT(BlueprintType)
struct FLBFurniturePiece
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ELBFurniture Kind = ELBFurniture::Crate;
	/** Tile-space rectangle */
	UPROPERTY(BlueprintReadOnly) FBox2D Rect = FBox2D(ForceInit);
	/** Wall it stands against */
	UPROPERTY(BlueprintReadOnly) ELBSide Side = ELBSide::N;
	UPROPERTY(BlueprintReadOnly) int32 Building = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName LootTable;
};

USTRUCT(BlueprintType)
struct FLBBuilding
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FIntRect Rect;
	UPROPERTY(BlueprintReadOnly) ELBBuildingType Type = ELBBuildingType::House;
	UPROPERTY(BlueprintReadOnly) FText SignName;
	UPROPERTY(BlueprintReadOnly) FLinearColor SignColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) FLinearColor WallColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) FLinearColor RoofColor = FLinearColor::Gray;
	UPROPERTY(BlueprintReadOnly) bool bFlatRoof = false;
	UPROPERTY(BlueprintReadOnly) bool bLit = false;
};
