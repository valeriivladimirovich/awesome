#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LBFX.generated.h"

class USoundBase;

/**
 * Fire-and-forget Niagara effects and one-shot sounds. Systems come from Project Settings ->
 * Last Block; missing systems are skipped silently so the game runs without art.
 * All systems should use Fixed Bounds and a low Max Particles budget (see README).
 */
UCLASS()
class LASTBLOCK_API ULBFX : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Blood spray; Direction is where the blow was heading */
	UFUNCTION(BlueprintCallable, Category = "LastBlock|FX", meta = (WorldContext = "WorldContext"))
	static void Blood(const UObject* WorldContext, const FVector& Location, const FVector& Direction, int32 Amount = 8);

	/** Sparks and a puff of dust where a bullet hits something hard */
	UFUNCTION(BlueprintCallable, Category = "LastBlock|FX", meta = (WorldContext = "WorldContext"))
	static void Impact(const UObject* WorldContext, const FVector& Location, const FVector& Normal);

	UFUNCTION(BlueprintCallable, Category = "LastBlock|FX", meta = (WorldContext = "WorldContext"))
	static void Tracer(const UObject* WorldContext, const FVector& Start, const FVector& End);

	UFUNCTION(BlueprintCallable, Category = "LastBlock|FX", meta = (WorldContext = "WorldContext"))
	static void Muzzle(const UObject* WorldContext, const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintCallable, Category = "LastBlock|FX", meta = (WorldContext = "WorldContext"))
	static void Dust(const UObject* WorldContext, const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "LastBlock|FX", meta = (WorldContext = "WorldContext"))
	static void Sound(const UObject* WorldContext, USoundBase* Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
};
