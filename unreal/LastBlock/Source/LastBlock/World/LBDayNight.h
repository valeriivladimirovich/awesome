#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LBDayNight.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UNiagaraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLBOnNewDay, int32, Day);

/**
 * Clock, sun, rain and wet streets. Drives the "Wetness" and "Night" scalars in the
 * world material parameter collection, so puddles and window glow live in materials.
 */
UCLASS()
class LASTBLOCK_API ALBDayNight : public AActor
{
	GENERATED_BODY()

public:
	ALBDayNight();

	static ALBDayNight* Get(const UObject* WorldContext);

	/** 0 at noon, 1 in deep night, smooth through dusk and dawn */
	UFUNCTION(BlueprintPure, Category = "Time") float GetNight() const { return Night; }
	UFUNCTION(BlueprintPure, Category = "Time") float GetHour() const { return Hour; }
	UFUNCTION(BlueprintPure, Category = "Time") int32 GetDay() const { return Day; }
	UFUNCTION(BlueprintPure, Category = "Time") bool IsRaining() const { return bRaining; }
	UFUNCTION(BlueprintPure, Category = "Time") float GetWetness() const { return Wetness; }

	UFUNCTION(BlueprintCallable, Category = "Time") void SetHour(float NewHour) { Hour = FMath::Fmod(NewHour, 24.f); }
	UFUNCTION(BlueprintCallable, Category = "Time") void SetRaining(bool bRain) { bRaining = bRain; }

	UPROPERTY(BlueprintAssignable) FLBOnNewDay OnNewDay;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> Sky;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Rain;

private:
	float Hour = 8.f;
	int32 Day = 1;
	float Night = 0.f;
	float Wetness = 0.f;
	bool bRaining = false;
	float WeatherTimer = 0.f;
	float LightTimer = 0.f;
};
