#include "FX/LBFX.h"
#include "Core/LBSettings.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	UNiagaraComponent* Spawn(const UObject* Ctx, const TSoftObjectPtr<UNiagaraSystem>& Soft, const FVector& Loc, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UNiagaraSystem* Sys = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
		if (!Sys || !Ctx) return nullptr;
		// pooled: dozens of hits per second during a fight must not allocate components
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(Ctx, Sys, Loc, Rot, FVector(1), true, true, ENCPoolMethod::AutoRelease);
	}
}

void ULBFX::Blood(const UObject* WorldContext, const FVector& Location, const FVector& Direction, int32 Amount)
{
	if (UNiagaraComponent* C = Spawn(WorldContext, ULBSettings::Get()->BloodFX, Location))
	{
		C->SetVariableVec3(TEXT("User.Direction"), Direction.GetSafeNormal());
		C->SetVariableInt(TEXT("User.Amount"), Amount);
	}
}

void ULBFX::Impact(const UObject* WorldContext, const FVector& Location, const FVector& Normal)
{
	if (UNiagaraComponent* C = Spawn(WorldContext, ULBSettings::Get()->ImpactFX, Location, Normal.Rotation()))
		C->SetVariableVec3(TEXT("User.Normal"), Normal);
}

void ULBFX::Tracer(const UObject* WorldContext, const FVector& Start, const FVector& End)
{
	if (UNiagaraComponent* C = Spawn(WorldContext, ULBSettings::Get()->TracerFX, Start))
		C->SetVariableVec3(TEXT("User.BeamEnd"), End);
}

void ULBFX::Muzzle(const UObject* WorldContext, const FVector& Location, const FRotator& Rotation)
{
	Spawn(WorldContext, ULBSettings::Get()->MuzzleFX, Location, Rotation);
}

void ULBFX::Dust(const UObject* WorldContext, const FVector& Location)
{
	Spawn(WorldContext, ULBSettings::Get()->DustFX, Location);
}

void ULBFX::Sound(const UObject* WorldContext, USoundBase* Sound, const FVector& Location, float Volume, float Pitch)
{
	if (Sound && WorldContext) UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume, Pitch);
}
