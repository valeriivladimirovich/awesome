#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/LBTypes.h"
#include "LBVehicle.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;

/**
 * Parked car. The body and the four wheels are separate components, so the same actor
 * can later be swapped for a Chaos wheeled vehicle when driving is added.
 */
UCLASS()
class LASTBLOCK_API ALBVehicle : public AActor
{
	GENERATED_BODY()

public:
	ALBVehicle();

	void Setup(const FLBCarSpot& Spot);

	UFUNCTION(BlueprintPure, Category = "Vehicle") FLBCarSpot GetSpot() const { return Spot; }
	UFUNCTION(BlueprintPure, Category = "Vehicle") TArray<USceneComponent*> GetWheels() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Cabin;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Siren;

	UPROPERTY() FLBCarSpot Spot;

private:
	float SirenPhase = 0.f;
	bool IsBig() const { return Spot.Kind == ELBCarKind::Truck || Spot.Kind == ELBCarKind::Ambulance || Spot.Kind == ELBCarKind::MilitaryTruck; }
};
