#include "World/LBVehicle.h"
#include "World/LBDayNight.h"
#include "Core/LBSettings.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	UStaticMesh* Basic(const TCHAR* Name) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name)); }

	void Tint(UStaticMeshComponent* C, const FLinearColor& Col)
	{
		if (UMaterialInterface* Base = ULBSettings::Get()->TintMaterial.LoadSynchronous())
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, C);
			M->SetVectorParameterValue(TEXT("Color"), Col);
			C->SetMaterial(0, M);
		}
	}
}

ALBVehicle::ALBVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = .05f;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Collision->SetCanEverAffectNavigation(true);
	RootComponent = Collision;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);

	Cabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cabin"));
	Cabin->SetupAttachment(Collision);
	Cabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cabin->SetCanEverAffectNavigation(false);

	for (int32 K = 0; K < 4; ++K)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"), K));
		W->SetupAttachment(Collision);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetCanEverAffectNavigation(false);
		Wheels.Add(W);
	}

	Siren = CreateDefaultSubobject<UPointLightComponent>(TEXT("Siren"));
	Siren->SetupAttachment(Collision);
	Siren->SetIntensityUnits(ELightUnits::Candelas);
	Siren->SetIntensity(0.f);
	Siren->SetCastShadows(false);
	Siren->SetAttenuationRadius(1200.f);
}

TArray<USceneComponent*> ALBVehicle::GetWheels() const
{
	TArray<USceneComponent*> Out;
	for (UStaticMeshComponent* W : Wheels) Out.Add(W);
	return Out;
}

void ALBVehicle::Setup(const FLBCarSpot& InSpot)
{
	Spot = InSpot;
	const ULBSettings* S = ULBSettings::Get();
	const bool bBig = IsBig();
	// length along local X, width along Y (cm)
	const float Len = bBig ? 680.f : (Spot.Kind == ELBCarKind::Hatch ? 400.f : 450.f);
	const float Wid = bBig ? 220.f : (Spot.Kind == ELBCarKind::SUV ? 190.f : 175.f);
	const float Hgt = bBig ? 290.f : (Spot.Kind == ELBCarKind::SUV || Spot.Kind == ELBCarKind::Pickup ? 175.f : 145.f);

	// orientation: along the road, random direction, wrecks slightly skewed
	const float Seed = FMath::Frac(Spot.Pos.X * 3.71f + Spot.Pos.Y * 1.37f);
	float Yaw = (Spot.bAlongY ? 90.f : 0.f) + (Seed > .5f ? 180.f : 0.f);
	if (Spot.bWreck) Yaw += (Seed - .5f) * 28.f;
	SetActorRotation(FRotator(0, Yaw, Spot.bWreck && !Spot.bBurnt ? (Seed - .5f) * 6.f : 0.f));
	Collision->SetBoxExtent(FVector(Len / 2, Wid / 2, Hgt / 2));
	AddActorWorldOffset(FVector(0, 0, Hgt / 2));

	const FLinearColor Paint = Spot.bBurnt ? FLinearColor(.02f, .018f, .016f) : Spot.bWreck ? FLinearColor(.12f, .1f, .085f)
		: Spot.Kind == ELBCarKind::Police || Spot.Kind == ELBCarKind::Ambulance ? FLinearColor(.8f, .8f, .78f)
		: Spot.Kind == ELBCarKind::MilitaryTruck ? FLinearColor(.08f, .09f, .04f)
		: Spot.Kind == ELBCarKind::Truck ? FLinearColor(.66f, .68f, .7f) : Spot.Paint;

	UStaticMesh* Custom = nullptr;
	if (const TSoftObjectPtr<UStaticMesh>* Soft = S->CarMeshes.Find(FName(StaticEnum<ELBCarKind>()->GetNameStringByValue((int64)Spot.Kind))))
		Custom = Soft->LoadSynchronous();
	if (Custom)
	{
		// authored car: pivot on the ground, front along +X; wheels still come from WheelMesh below
		Body->SetStaticMesh(Custom);
		Body->SetRelativeLocation(FVector(0, 0, -Hgt / 2));
		Cabin->SetVisibility(false);
	}
	else
	{
		// placeholder: lower body plus a narrower cabin
		UStaticMesh* Cube = Basic(TEXT("Cube"));
		const float Lower = bBig ? 120.f : Hgt * .5f;
		Body->SetStaticMesh(Cube);
		Body->SetRelativeLocation(FVector(0, 0, -Hgt / 2 + 30.f + Lower / 2));
		Body->SetRelativeScale3D(FVector(Len, Wid, Lower) / 100.f);
		Cabin->SetStaticMesh(Cube);
		const float CabLen = bBig ? 180.f : (Spot.Kind == ELBCarKind::Pickup ? 170.f : Len * .5f);
		const float CabX = bBig ? Len / 2 - 100.f : (Spot.Kind == ELBCarKind::Pickup ? 40.f : -Len * .05f);
		Cabin->SetRelativeScale3D(FVector(CabLen, Wid - 10.f, Hgt - Lower - 30.f) / 100.f);
		Cabin->SetRelativeLocation(FVector(CabX, 0, -Hgt / 2 + 30.f + Lower + (Hgt - Lower - 30.f) / 2));
		Tint(Cabin, Spot.bBurnt ? Paint : FLinearColor(.03f, .04f, .05f));
	}
	Tint(Body, Paint);

	// wheels: separate nodes, ready for a driving model
	UStaticMesh* WheelM = S->WheelMesh.LoadSynchronous();
	const float R = bBig ? 46.f : 34.f;
	const float WX = Len / 2 - (bBig ? 110.f : 80.f);
	const FVector WheelPos[4] = { {WX, Wid / 2 - 8.f, 0}, {WX, -Wid / 2 + 8.f, 0}, {-WX, Wid / 2 - 8.f, 0}, {-WX, -Wid / 2 + 8.f, 0} };
	for (int32 K = 0; K < 4; ++K)
	{
		UStaticMeshComponent* W = Wheels[K];
		W->SetRelativeLocation(WheelPos[K] + FVector(0, 0, -Hgt / 2 + R));
		if (WheelM) { W->SetStaticMesh(WheelM); W->SetRelativeRotation(FRotator(0, K % 2 ? 180.f : 0.f, 0)); }
		else
		{
			W->SetStaticMesh(Basic(TEXT("Cylinder")));
			W->SetRelativeRotation(FRotator(0, 0, 90.f));
			W->SetRelativeScale3D(FVector(R * 2, R * 2, 24.f) / 100.f);
			Tint(W, FLinearColor(.015f, .015f, .016f));
		}
		if (Spot.bBurnt && K == 1) W->SetVisibility(false);
	}

	if ((Spot.Kind == ELBCarKind::Police || Spot.Kind == ELBCarKind::Ambulance) && !Spot.bWreck)
	{
		Siren->SetRelativeLocation(FVector(0, 0, Hgt / 2 + 20.f));
		SetActorTickEnabled(true);
		SirenPhase = Seed * 6.f;
	}
	if (Spot.bBurnt)
		if (UNiagaraSystem* Smoke = S->SmokeFX.LoadSynchronous())
			UNiagaraFunctionLibrary::SpawnSystemAttached(Smoke, Collision, NAME_None, FVector(0, 0, Hgt / 2), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false);
}

void ALBVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// emergency lights keep flashing in the dark, long after everyone has gone
	const ALBDayNight* DN = ALBDayNight::Get(this);
	const float Night = DN ? DN->GetNight() : 0.f;
	if (Night < .1f) { Siren->SetIntensity(0.f); return; }
	SirenPhase += DeltaSeconds;
	const bool bRed = FMath::Sin(SirenPhase * 7.f) > 0.f;
	Siren->SetLightColor(bRed || Spot.Kind == ELBCarKind::Ambulance ? FLinearColor(1, .1f, .1f) : FLinearColor(.15f, .25f, 1));
	Siren->SetIntensity((bRed || Spot.Kind != ELBCarKind::Ambulance ? 400.f : 0.f) * Night);
}
