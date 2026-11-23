#include "World/LBDayNight.h"
#include "World/LBCity.h"
#include "Core/LBSettings.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"

ALBDayNight::ALBDayNight()
{
	PrimaryActorTick.bCanEverTick = true;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	RootComponent = Sun;
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetCastShadows(true);
	Sun->SetAtmosphereSunLight(true);
	// mobile: a single cascade around the player is enough for a top-down camera
	Sun->SetDynamicShadowCascades(2);
	Sun->DynamicShadowDistanceMovableLight = 6000.f;

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Sun);
	Sky->SetMobility(EComponentMobility::Movable);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Sun);
	Fog->SetFogDensity(.01f);
}

ALBDayNight* ALBDayNight::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ALBDayNight> It(World); It; ++It) return *It;
	return nullptr;
}

void ALBDayNight::BeginPlay()
{
	Super::BeginPlay();
	const ULBSettings* S = ULBSettings::Get();
	Hour = S->StartHour;
	bRaining = FMath::FRand() < S->RainChance;
	Wetness = bRaining ? 1.f : 0.f;
}

void ALBDayNight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ULBSettings* S = ULBSettings::Get();

	// clock
	Hour += DeltaSeconds * 24.f / (S->DayLengthMinutes * 60.f);
	if (Hour >= 24.f) { Hour -= 24.f; ++Day; OnNewDay.Broadcast(Day); }

	// night factor: dark from ~21:00 to ~5:00, with an hour of dusk and dawn
	const float Dusk = FMath::Clamp((Hour - 19.5f) / 1.5f, 0.f, 1.f);
	const float Dawn = FMath::Clamp((6.5f - Hour) / 1.5f, 0.f, 1.f);
	Night = FMath::SmoothStep(0.f, 1.f, FMath::Max(Dusk, Dawn));

	// weather changes every few in-game hours
	WeatherTimer -= DeltaSeconds;
	if (WeatherTimer <= 0.f) { WeatherTimer = FMath::FRandRange(120.f, 300.f); bRaining = FMath::FRand() < S->RainChance; }
	Wetness = FMath::Clamp(Wetness + DeltaSeconds * (bRaining ? .2f : -.05f), 0.f, 1.f);

	// sun arc: rises in the east, sets in the west; at night a weak blue moon from the same rig
	const float DayT = FMath::Clamp((Hour - 6.f) / 13.f, 0.f, 1.f);
	const float Elev = Night < .5f ? FMath::Lerp(8.f, 62.f, FMath::Sin(DayT * PI)) : 40.f;
	Sun->SetWorldRotation(FRotator(-Elev, Night < .5f ? FMath::Lerp(-110.f, 110.f, DayT) : 150.f, 0.f));
	const float Gold = 1.f - FMath::Clamp(FMath::Sin(DayT * PI) * 2.2f, 0.f, 1.f);
	const FLinearColor SunCol = FMath::Lerp(FLinearColor(1.f, .97f, .92f), FLinearColor(1.f, .66f, .45f), Gold);
	Sun->SetLightColor(FMath::Lerp(SunCol, FLinearColor(.55f, .65f, 1.f), Night));
	Sun->SetIntensity(FMath::Lerp(6.f, .25f, Night) * (1.f - Wetness * .6f));
	Sky->SetIntensity(FMath::Lerp(1.f, .06f, Night) * (1.f - Wetness * .25f));
	Fog->SetFogDensity(.006f + Wetness * .01f + Night * .006f);

	// rain falls around the player; attached lazily because the pawn spawns after this actor
	if (!Rain)
		if (UNiagaraSystem* RainFX = S->RainFX.LoadSynchronous())
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
				Rain = UNiagaraFunctionLibrary::SpawnSystemAttached(RainFX, Pawn->GetRootComponent(), NAME_None, FVector(0, 0, 900), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false);
	if (Rain) Rain->SetVariableFloat(TEXT("User.Intensity"), bRaining ? 1.f : 0.f);

	// world material parameters: wet asphalt and puddles, lit windows
	if (UMaterialParameterCollection* MPC = S->WorldParams.LoadSynchronous())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Wetness"), Wetness);
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Night"), Night);
	}

	// street lamps follow the player a few times per second
	LightTimer -= DeltaSeconds;
	if (LightTimer <= 0.f)
	{
		LightTimer = .1f;
		if (ALBCity* City = ALBCity::Get(this))
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
				City->UpdateLights(Pawn->GetActorLocation(), Night, GetWorld()->GetTimeSeconds());
	}
}
