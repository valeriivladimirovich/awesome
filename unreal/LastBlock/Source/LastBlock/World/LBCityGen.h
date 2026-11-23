#pragma once

#include "CoreMinimal.h"
#include "Core/LBTypes.h"

/** Prop kinds placed on streets and yards (same set as the browser version) */
enum class ELBProp : uint8
{
	Bin, Hydrant, Bench, BusStop, NewsBox, StopSign, Barrier, Barrel, Tires, Dumpster, Bbq, Chair, Bike,
	TrashBags, Slide, Swings, Picnic, Pump, Post, PriceSign, Fountain, CrateOut, Pallet, Cone, Planter,
	Mailbox, SwingPost, Sandbag, FireBarrel, Count
};

struct FLBPropSpot
{
	ELBProp Kind = ELBProp::Bin;
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Half = FVector2D(.2, .2);   // footprint half size in tiles (already rotated)
	bool bSolid = true;
	bool bRotated = false;
};

struct FLBTreeSpot
{
	FVector2D Pos = FVector2D::ZeroVector;
	uint8 Kind = 0;          // 0 broadleaf, 1 pine, 2 bush
	float Scale = 1.f;
};

struct FLBLampSpot
{
	FVector2D Pos = FVector2D::ZeroVector;
	bool bOn = true;
	float Phase = 0.f;       // > 8 means this one flickers
};

struct FLBContainer
{
	int32 Furniture = INDEX_NONE;
	FText Name;
	FName LootTable;
	TArray<ELBItem> Loot;
	bool bSearched = false;
};

/**
 * Tile-based city: a 150x150 grid of 1.8 m tiles with walls on tile edges.
 * Pure data, no UObjects: ALBCity turns it into meshes, collision and actors.
 */
struct LASTBLOCK_API FLBCity
{
	static constexpr int32 N = LB_MAP_N;

	TArray<ELBGround> Ground;      // N*N
	TArray<int16> Bld;             // N*N, building index or -1
	TArray<uint8> HEdge;           // N*(N+1): edge along x at row j (between tiles (i,j-1) and (i,j))
	TArray<uint8> VEdge;           // (N+1)*N: edge along y at column i (between tiles (i-1,j) and (i,j))

	TArray<FLBBuilding> Buildings;
	TArray<FLBFurniturePiece> Furniture;
	TArray<FLBContainer> Containers;
	TArray<FLBCarSpot> Cars;
	TArray<FLBLampSpot> Lamps;
	TArray<FLBTreeSpot> Trees;
	TArray<FLBPropSpot> Props;
	/** Rugs and garden beds: purely visual floor decals */
	TArray<FBox2D> Rugs;

	FVector2D Start = FVector2D(70.5, 62.5);

	ELBGround GroundAt(int32 I, int32 J) const { return InBounds(I, J) ? Ground[J * N + I] : ELBGround::Forest; }
	int32 BuildingAt(int32 I, int32 J) const { return InBounds(I, J) ? Bld[J * N + I] : -1; }
	ELBEdge HAt(int32 I, int32 J) const { return (I >= 0 && I < N && J >= 0 && J <= N) ? (ELBEdge)HEdge[J * N + I] : ELBEdge::None; }
	ELBEdge VAt(int32 I, int32 J) const { return (I >= 0 && I <= N && J >= 0 && J < N) ? (ELBEdge)VEdge[J * (N + 1) + I] : ELBEdge::None; }
	static bool InBounds(int32 I, int32 J) { return I >= 0 && J >= 0 && I < N && J < N; }

	/** Tile centre in world units (cm) */
	static FVector TileToWorld(const FVector2D& T, float Z = 0.f) { return FVector(T.X * LB_TILE, T.Y * LB_TILE, Z); }
	static FVector2D WorldToTile(const FVector& W) { return FVector2D(W.X / LB_TILE, W.Y / LB_TILE); }

	/** Builds the whole city from a seed */
	void Generate(int32 Seed);
};
