#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LBSettings.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UNiagaraSystem;
class UBehaviorTree;
class USoundBase;
class UMaterialParameterCollection;

/**
 * Project Settings -> Game -> Last Block.
 * Every art slot is optional: empty slots fall back to engine basic shapes, so the
 * game runs straight after the first build and real art can be dropped in later.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Last Block"))
class LASTBLOCK_API ULBSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULBSettings();

	static const ULBSettings* Get() { return GetDefault<ULBSettings>(); }

	// ---------- city ----------
	/** Seed for the city layout; 0 picks a new city every run */
	UPROPERTY(Config, EditAnywhere, Category = "City") int32 CitySeed = 0;
	UPROPERTY(Config, EditAnywhere, Category = "City") int32 ZombieCount = 260;
	/** Share of zombies that sprint instead of shamble */
	UPROPERTY(Config, EditAnywhere, Category = "City", meta = (ClampMin = 0, ClampMax = 1)) float RunnerShare = .08f;

	/** Ground: one mesh per tile, coloured per ground type through a per-instance custom float */
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> TileMesh;
	/** Wall pieces are 1 tile wide, 300 cm high, pivot at bottom centre, facing +X */
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> WallSolidMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> WallWindowMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> WallDoorMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> FenceMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> TreeMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> PineMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> BushMesh;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> LampPostMesh;
	/** Furniture meshes by FName of ELBFurniture (Sofa, Bed, Fridge...); missing ones become coloured boxes */
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TMap<FName, TSoftObjectPtr<UStaticMesh>> FurnitureMeshes;
	/** Car body meshes by FName of ELBCarKind; wheels are separate components */
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TMap<FName, TSoftObjectPtr<UStaticMesh>> CarMeshes;
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UStaticMesh> WheelMesh;
	/** Material with a "Color" vector parameter and PerInstanceCustomData[0..2] as tint */
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UMaterialInterface> TintMaterial;
	/** Holds "Wetness" (0..1) and "Night" (0..1) for puddles, wet asphalt and window glow */
	UPROPERTY(Config, EditAnywhere, Category = "City|Art") TSoftObjectPtr<UMaterialParameterCollection> WorldParams;

	// ---------- AI ----------
	/** Zombie behaviour tree. Without one the controller runs the same logic in C++ */
	UPROPERTY(Config, EditAnywhere, Category = "AI") TSoftObjectPtr<UBehaviorTree> ZombieTree;

	// ---------- effects (Niagara) ----------
	/** Burst; User.Direction (vector) sets the spray direction */
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> BloodFX;
	/** Sparks plus dust at a bullet impact; User.Normal */
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> ImpactFX;
	/** Ribbon or beam from muzzle to hit; User.BeamEnd */
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> TracerFX;
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> MuzzleFX;
	/** Looping smoke column for fire barrels and burnt cars */
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> SmokeFX;
	/** Footstep dust when running */
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> DustFX;
	/** Camera-attached rain; User.Intensity 0..1 */
	UPROPERTY(Config, EditAnywhere, Category = "FX") TSoftObjectPtr<UNiagaraSystem> RainFX;

	// ---------- sound ----------
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> HitSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> SwingSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> PistolSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> RifleSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> GroanSound;

	// ---------- day ----------
	/** Real minutes for one full in-game day */
	UPROPERTY(Config, EditAnywhere, Category = "Time", meta = (ClampMin = 2)) float DayLengthMinutes = 24.f;
	UPROPERTY(Config, EditAnywhere, Category = "Time") float StartHour = 8.f;
	UPROPERTY(Config, EditAnywhere, Category = "Time", meta = (ClampMin = 0, ClampMax = 1)) float RainChance = .3f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
