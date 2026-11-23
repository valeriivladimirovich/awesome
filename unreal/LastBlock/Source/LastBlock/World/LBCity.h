#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/LBCityGen.h"
#include "LBCity.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;
class UMaterialInterface;
class ALBVehicle;

/**
 * The whole town. Generates FLBCity on BeginPlay and turns it into instanced meshes:
 * one HISM per piece type keeps the draw call count low on iPhone.
 * Also owns the container (loot) lookup and the pool of street-lamp lights.
 */
UCLASS()
class LASTBLOCK_API ALBCity : public AActor
{
	GENERATED_BODY()

public:
	ALBCity();

	static ALBCity* Get(const UObject* WorldContext);

	const FLBCity& Data() const { return City; }

	/** Nearest unsearched-or-searched container within Radius (tiles) that the player can actually see */
	int32 FindContainer(const FVector& From, float RadiusTiles) const;
	FLBContainer* GetContainer(int32 Index) { return City.Containers.IsValidIndex(Index) ? &City.Containers[Index] : nullptr; }
	FVector ContainerLocation(int32 Index) const;

	/** Random walkable spot at least MinDistTiles away from Avoid; bInside allows building interiors */
	FVector RandomSpot(const FVector& Avoid, float MinDistTiles, float MaxDistTiles, bool bInside) const;

	/** Hides the roof of the building the player stands in */
	void SetFocus(const FVector& PlayerLocation);
	int32 BuildingAtWorld(const FVector& W) const;

	/** True when a working street lamp stands within RadiusTiles; zombies see further there at night */
	bool IsNearLitLamp(const FVector& W, float RadiusTiles) const;

	/** Street lamps: the light pool follows the camera and switches on after dusk */
	void UpdateLights(const FVector& Focus, float Night, float Time);

	FVector PlayerStart() const { return FLBCity::TileToWorld(City.Start, 100.f); }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WallISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WindowISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GlassISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DoorISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FenceISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrunkISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanopyISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PineISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PropISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LampISM;
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DecorISM;
	UPROPERTY(Transient) TMap<uint8, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FurnitureISM;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Roofs;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> LampPool;
	UPROPERTY(Transient) TArray<TObjectPtr<ALBVehicle>> Vehicles;

	/** How many street lamps may light up at once; each one costs per pixel on iPhone */
	UPROPERTY(EditAnywhere, Category = "City") int32 LampLights = 6;

private:
	FLBCity City;
	int32 FocusBuilding = INDEX_NONE;

	UHierarchicalInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, UStaticMesh* Mesh, bool bCollide, bool bShadows = true);
	void AddBox(UHierarchicalInstancedStaticMeshComponent* ISM, const FVector& Center, const FVector& SizeCm, float Yaw, const FLinearColor& Tint);

	void BuildGround();
	void BuildWalls();
	void BuildRoofs();
	void BuildFurniture();
	void BuildNature();
	void BuildProps();
	void BuildVehicles();
};
