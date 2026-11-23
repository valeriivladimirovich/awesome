#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LBGameMode.generated.h"

class ALBZombie;
class ALBSurvivor;
class ALBCity;

/**
 * Works in an empty level: spawns the city and the day/night rig if the level has none,
 * puts the survivor at the start and fills the town with zombies.
 */
UCLASS()
class LASTBLOCK_API ALBGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALBGameMode();

	/** Set to a Blueprint child with the zombie skeletal mesh and Animation Blueprint */
	UPROPERTY(EditDefaultsOnly, Category = "LastBlock") TSubclassOf<ALBZombie> ZombieClass;
	/** Seconds after death before a new run starts */
	UPROPERTY(EditDefaultsOnly, Category = "LastBlock") float RestartDelay = 6.f;

	virtual void StartPlay() override;
	void OnPlayerDied(ALBSurvivor* Survivor);

	UFUNCTION(BlueprintPure, Category = "LastBlock") int32 GetAliveZombies() const { return Alive; }

private:
	int32 Alive = 0;
	FTimerHandle PopulationTimer;

	UFUNCTION() void HandleZombieGone(AActor* Actor);
	ALBZombie* SpawnZombie(ALBCity* City, const FVector& Avoid, float MinTiles, float MaxTiles);
	void TopUpPopulation();
};
