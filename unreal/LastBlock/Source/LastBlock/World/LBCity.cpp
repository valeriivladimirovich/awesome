#include "World/LBCity.h"
#include "World/LBVehicle.h"
#include "Core/LBSettings.h"
#include "Core/LBGameData.h"
#include "LastBlock.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	constexpr float U = LB_TILE;
	constexpr float WallH = 300.f, WallT = 14.f, FloorZ = 14.f, WalkZ = 12.f;

	UStaticMesh* BasicShape(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}
	template <typename T> T* LoadSoft(const TSoftObjectPtr<T>& Ptr) { return Ptr.IsNull() ? nullptr : Ptr.LoadSynchronous(); }
	FLinearColor Hex(const TCHAR* H) { return FLinearColor::FromSRGBColor(FColor::FromHex(H)); }

	/** Per-instance tint, read in the material through PerInstanceCustomData 0..2 */
	void SetTint(UInstancedStaticMeshComponent* ISM, int32 Index, const FLinearColor& C)
	{
		const float Data[3] = { C.R, C.G, C.B };
		ISM->SetCustomData(Index, MakeArrayView(Data, 3));
	}

	FLinearColor GroundColor(ELBGround G, int32 I, int32 J)
	{
		const float V = .94f + .12f * FMath::Frac(FMath::Sin(I * 12.9898f + J * 78.233f) * 43758.5453f);
		switch (G)
		{
		case ELBGround::Road: return Hex(TEXT("393c44")) * V;
		case ELBGround::Walk: return Hex(TEXT("a29c8f")) * V;
		case ELBGround::Dirt: return Hex(TEXT("8b7454")) * V;
		case ELBGround::Wood: return Hex(TEXT("9a7250")) * V;
		case ELBGround::Tile: return Hex(TEXT("d9d6cc")) * V;
		case ELBGround::Concrete: return Hex(TEXT("8f8d88")) * V;
		case ELBGround::Lot: return Hex(TEXT("45474d")) * V;
		case ELBGround::Forest: return Hex(TEXT("4f5e3c")) * V;
		case ELBGround::Sand: return Hex(TEXT("d8c79a")) * V;
		default: return Hex(TEXT("5c7044")) * V;
		}
	}
	float GroundTop(ELBGround G, bool bInBuilding) { return bInBuilding ? FloorZ : G == ELBGround::Walk ? WalkZ : 0.f; }

	/** Rough height (cm) and colour of street props for the placeholder boxes */
	void PropLook(ELBProp K, float& OutH, FLinearColor& OutC)
	{
		switch (K)
		{
		case ELBProp::Bin: OutH = 90; OutC = Hex(TEXT("3f4a3a")); break;
		case ELBProp::Hydrant: OutH = 75; OutC = Hex(TEXT("b8352c")); break;
		case ELBProp::Bench: OutH = 45; OutC = Hex(TEXT("6b4a32")); break;
		case ELBProp::BusStop: OutH = 250; OutC = Hex(TEXT("7a8a96")); break;
		case ELBProp::NewsBox: OutH = 110; OutC = Hex(TEXT("2f5f9a")); break;
		case ELBProp::StopSign: OutH = 240; OutC = Hex(TEXT("c23b3b")); break;
		case ELBProp::Barrier: OutH = 100; OutC = Hex(TEXT("d8d0c0")); break;
		case ELBProp::Barrel: case ELBProp::FireBarrel: OutH = 90; OutC = Hex(TEXT("5a4a3a")); break;
		case ELBProp::Tires: OutH = 50; OutC = Hex(TEXT("1c1c1e")); break;
		case ELBProp::Dumpster: OutH = 130; OutC = Hex(TEXT("2f5a3a")); break;
		case ELBProp::Fountain: OutH = 70; OutC = Hex(TEXT("a8a49a")); break;
		case ELBProp::Sandbag: OutH = 70; OutC = Hex(TEXT("8a7d5a")); break;
		case ELBProp::Post: case ELBProp::PriceSign: case ELBProp::SwingPost: OutH = 360; OutC = Hex(TEXT("6b6f78")); break;
		case ELBProp::Pump: OutH = 170; OutC = Hex(TEXT("c8473a")); break;
		case ELBProp::Cone: OutH = 60; OutC = Hex(TEXT("e07030")); break;
		case ELBProp::Slide: OutH = 180; OutC = Hex(TEXT("d0a030")); break;
		default: OutH = 60; OutC = Hex(TEXT("6a6a66")); break;
		}
	}

	float SideYaw(ELBSide S)
	{
		// furniture meshes face +X; the front points away from the wall they stand against
		switch (S)
		{
		case ELBSide::N: return 90.f;
		case ELBSide::S: return -90.f;
		case ELBSide::E: return 180.f;
		default: return 0.f;
		}
	}
}

ALBCity::ALBCity()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

ALBCity* ALBCity::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ALBCity> It(World); It; ++It) return *It;
	return nullptr;
}

UHierarchicalInstancedStaticMeshComponent* ALBCity::MakeISM(const TCHAR* Name, UStaticMesh* Mesh, bool bCollide, bool bShadows)
{
	UHierarchicalInstancedStaticMeshComponent* ISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
	ISM->SetupAttachment(Root);
	ISM->SetStaticMesh(Mesh);
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetNumCustomDataFloats(3);
	ISM->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollide) ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ISM->SetCanEverAffectNavigation(bCollide);
	ISM->SetCastShadow(bShadows);
	// far objects are culled; the camera never sees further than ~60 m
	ISM->SetCullDistances(0, 9000);
	if (UMaterialInterface* Tint = LoadSoft(ULBSettings::Get()->TintMaterial)) ISM->SetMaterial(0, Tint);
	ISM->RegisterComponent();
	return ISM;
}

void ALBCity::AddBox(UHierarchicalInstancedStaticMeshComponent* ISM, const FVector& Center, const FVector& SizeCm, float Yaw, const FLinearColor& Tint)
{
	// engine basic shapes are 100 cm cubes centred on the pivot
	const FTransform Xf(FRotator(0, Yaw, 0), Center, SizeCm / 100.f);
	const int32 Idx = ISM->AddInstance(Xf, /*bWorldSpace*/ true);
	SetTint(ISM, Idx, Tint);
}

void ALBCity::BeginPlay()
{
	Super::BeginPlay();
	const ULBSettings* S = ULBSettings::Get();
	const int32 Seed = S->CitySeed != 0 ? S->CitySeed : FMath::Rand();
	const double T0 = FPlatformTime::Seconds();
	City.Generate(Seed);
	BuildGround();
	BuildWalls();
	BuildRoofs();
	BuildFurniture();
	BuildNature();
	BuildProps();
	BuildVehicles();
	UE_LOG(LogLastBlock, Log, TEXT("City seed %d: %d buildings, %d furniture, %d cars, %d trees in %.0f ms"),
		Seed, City.Buildings.Num(), City.Furniture.Num(), City.Cars.Num(), City.Trees.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);

	for (int32 K = 0; K < LampLights; ++K)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetLightColor(FLinearColor(1.f, .78f, .53f));
		L->SetAttenuationRadius(2200.f);
		L->SetCastShadows(false);
		L->SetIntensity(0.f);
		L->RegisterComponent();
		LampPool.Add(L);
	}
}

void ALBCity::BuildGround()
{
	GroundISM = MakeISM(TEXT("Ground"), LoadSoft(ULBSettings::Get()->TileMesh) ? LoadSoft(ULBSettings::Get()->TileMesh) : BasicShape(TEXT("Cube")), true, false);
	GroundISM->SetCullDistances(0, 0);
	for (int32 J = 0; J < FLBCity::N; ++J)
		for (int32 I = 0; I < FLBCity::N; ++I)
		{
			const ELBGround G = City.GroundAt(I, J);
			const float Top = GroundTop(G, City.BuildingAt(I, J) >= 0);
			AddBox(GroundISM, FVector((I + .5f) * U, (J + .5f) * U, Top - 10.f), FVector(U, U, 20.f), 0.f, GroundColor(G, I, J));
		}
	// rugs and garden beds
	DecorISM = MakeISM(TEXT("Decor"), BasicShape(TEXT("Cube")), false, false);
	for (const FBox2D& Rg : City.Rugs)
	{
		const FVector2D C = Rg.GetCenter(), Sz = Rg.GetSize();
		const int32 Bi = City.BuildingAt(FMath::FloorToInt(C.X), FMath::FloorToInt(C.Y));
		AddBox(DecorISM, FVector(C.X * U, C.Y * U, (Bi >= 0 ? FloorZ : 0.f) + 1.f), FVector(Sz.X * U, Sz.Y * U, 2.f), 0.f,
			Bi >= 0 ? Hex(TEXT("8c3f3a")) : Hex(TEXT("4a3a28")));
	}
}

void ALBCity::BuildWalls()
{
	const ULBSettings* S = ULBSettings::Get();
	UStaticMesh* Cube = BasicShape(TEXT("Cube"));
	UStaticMesh* SolidM = LoadSoft(S->WallSolidMesh);
	UStaticMesh* WindowM = LoadSoft(S->WallWindowMesh);
	UStaticMesh* DoorM = LoadSoft(S->WallDoorMesh);
	UStaticMesh* FenceM = LoadSoft(S->FenceMesh);
	WallISM = MakeISM(TEXT("Walls"), SolidM ? SolidM : Cube, true);
	WindowISM = MakeISM(TEXT("WindowWalls"), WindowM ? WindowM : Cube, true);
	DoorISM = MakeISM(TEXT("DoorWalls"), DoorM ? DoorM : Cube, true);
	FenceISM = MakeISM(TEXT("Fences"), FenceM ? FenceM : Cube, true);
	GlassISM = MakeISM(TEXT("Glass"), Cube, true, false);

	auto Edge = [&](ELBEdge E, const FVector2D& Mid, float Yaw, int32 BldA, int32 BldB)
	{
		const int32 Bi = BldA >= 0 ? BldA : BldB;
		const bool bInner = BldA >= 0 && BldB >= 0;
		const FLinearColor Col = Bi < 0 ? Hex(TEXT("7a5a3c")) : bInner ? Hex(TEXT("e8dcc4")) : City.Buildings[Bi].WallColor;
		const FVector Base(Mid.X * U, Mid.Y * U, Bi >= 0 ? FloorZ : 0.f);
		const FTransform Xf(FRotator(0, Yaw, 0), Base);
		switch (E)
		{
		case ELBEdge::Solid:
			if (SolidM) SetTint(WallISM, WallISM->AddInstance(Xf, true), Col);
			else AddBox(WallISM, Base + FVector(0, 0, WallH / 2), FVector(U, WallT, WallH), Yaw, Col);
			break;
		case ELBEdge::Window:
			if (WindowM) { SetTint(WindowISM, WindowISM->AddInstance(Xf, true), Col); break; }
			AddBox(WindowISM, Base + FVector(0, 0, 45), FVector(U, WallT, 90), Yaw, Col);                    // sill
			AddBox(WindowISM, Base + FVector(0, 0, 255), FVector(U, WallT, 90), Yaw, Col);                   // lintel
			for (float Off : { -.42f, .42f })                                                                // jambs
			{
				const FVector D = FRotator(0, Yaw, 0).RotateVector(FVector(Off * U, 0, 0));
				AddBox(WindowISM, Base + D + FVector(0, 0, 150), FVector(.16f * U, WallT, 120), Yaw, Col);
			}
			AddBox(GlassISM, Base + FVector(0, 0, 150), FVector(U * .68f, 3, 120), Yaw, Hex(TEXT("2a3442")));
			break;
		case ELBEdge::Door:
			if (Bi < 0) break;
			if (DoorM) { SetTint(DoorISM, DoorISM->AddInstance(Xf, true), Col); break; }
			AddBox(DoorISM, Base + FVector(0, 0, 257), FVector(U, WallT, 86), Yaw, Col);                     // over the doorway
			for (float Off : { -.4f, .4f })
			{
				const FVector D = FRotator(0, Yaw, 0).RotateVector(FVector(Off * U, 0, 0));
				AddBox(DoorISM, Base + D + FVector(0, 0, 107), FVector(.2f * U, WallT, 214), Yaw, Col);
			}
			break;
		case ELBEdge::Fence:
			if (FenceM) SetTint(FenceISM, FenceISM->AddInstance(Xf, true), Col);
			else AddBox(FenceISM, Base + FVector(0, 0, 55), FVector(U, 6, 110), Yaw, Col);
			break;
		default: break;
		}
	};

	for (int32 J = 0; J <= FLBCity::N; ++J)
		for (int32 I = 0; I < FLBCity::N; ++I)
			if (const ELBEdge E = City.HAt(I, J); E != ELBEdge::None)
				Edge(E, FVector2D(I + .5f, J), 0.f, City.BuildingAt(I, J), City.BuildingAt(I, J - 1));
	for (int32 J = 0; J < FLBCity::N; ++J)
		for (int32 I = 0; I <= FLBCity::N; ++I)
			if (const ELBEdge E = City.VAt(I, J); E != ELBEdge::None)
				Edge(E, FVector2D(I, J + .5f), 90.f, City.BuildingAt(I, J), City.BuildingAt(I - 1, J));
}

void ALBCity::BuildRoofs()
{
	UStaticMesh* Cube = BasicShape(TEXT("Cube"));
	UMaterialInterface* Tint = LoadSoft(ULBSettings::Get()->TintMaterial);
	for (int32 Bi = 0; Bi < City.Buildings.Num(); ++Bi)
	{
		const FLBBuilding& B = City.Buildings[Bi];
		UStaticMeshComponent* Roof = NewObject<UStaticMeshComponent>(this);
		Roof->SetupAttachment(Root);
		Roof->SetStaticMesh(Cube);
		Roof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Roof->SetCanEverAffectNavigation(false);
		const FVector Center((B.Rect.Min.X + B.Rect.Width() * .5f) * U, (B.Rect.Min.Y + B.Rect.Height() * .5f) * U, FloorZ + WallH + 12.f);
		Roof->SetWorldTransform(FTransform(FRotator::ZeroRotator, Center, FVector((B.Rect.Width() * U + 60.f) / 100.f, (B.Rect.Height() * U + 60.f) / 100.f, .24f)));
		if (Tint)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Tint, Roof);
			MID->SetVectorParameterValue(TEXT("Color"), B.RoofColor);
			Roof->SetMaterial(0, MID);
		}
		Roof->RegisterComponent();
		Roofs.Add(Roof);
	}
}

void ALBCity::BuildFurniture()
{
	const ULBSettings* S = ULBSettings::Get();
	UStaticMesh* Cube = BasicShape(TEXT("Cube"));
	const UEnum* Enum = StaticEnum<ELBFurniture>();
	for (const FLBFurniturePiece& Fp : City.Furniture)
	{
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = FurnitureISM.Find((uint8)Fp.Kind);
		UStaticMesh* Custom = nullptr;
		if (const TSoftObjectPtr<UStaticMesh>* Soft = S->FurnitureMeshes.Find(FName(Enum->GetNameStringByValue((int64)Fp.Kind))))
			Custom = LoadSoft(*Soft);
		if (!Found)
		{
			const FString Name = FString::Printf(TEXT("Furn_%s"), *Enum->GetNameStringByValue((int64)Fp.Kind));
			Found = &FurnitureISM.Add((uint8)Fp.Kind, MakeISM(*Name, Custom ? Custom : Cube, true));
		}
		const FLBFurnitureDef Def = ULBGameData::GetFurniture(Fp.Kind);
		const FVector2D C = Fp.Rect.GetCenter(), Sz = Fp.Rect.GetSize();
		if (Custom)
		{
			const FTransform Xf(FRotator(0, SideYaw(Fp.Side), 0), FVector(C.X * U, C.Y * U, FloorZ));
			SetTint(*Found, (*Found)->AddInstance(Xf, true), Def.Color);
		}
		else AddBox(*Found, FVector(C.X * U, C.Y * U, FloorZ + Def.HeightCm / 2), FVector(Sz.X * U, Sz.Y * U, Def.HeightCm), 0.f, Def.Color);
	}
}

void ALBCity::BuildNature()
{
	const ULBSettings* S = ULBSettings::Get();
	UStaticMesh* TreeM = LoadSoft(S->TreeMesh);
	UStaticMesh* PineM = LoadSoft(S->PineMesh);
	UStaticMesh* BushM = LoadSoft(S->BushMesh);
	TrunkISM = MakeISM(TEXT("Trunks"), TreeM ? TreeM : BasicShape(TEXT("Cylinder")), true);
	CanopyISM = MakeISM(TEXT("Canopies"), BushM ? BushM : BasicShape(TEXT("Sphere")), false);
	PineISM = MakeISM(TEXT("Pines"), PineM ? PineM : BasicShape(TEXT("Cone")), true);
	for (const FLBTreeSpot& T : City.Trees)
	{
		const FVector P(T.Pos.X * U, T.Pos.Y * U, 0.f);
		const float Yaw = FMath::Frac(T.Pos.X * 7.31f + T.Pos.Y * 3.17f) * 360.f;
		const FLinearColor Leaf = FLinearColor::LerpUsingHSV(Hex(TEXT("3c6733")), Hex(TEXT("6a9853")), FMath::Frac(T.Pos.X * 1.7f + T.Pos.Y));
		if (T.Kind == 1)
		{
			if (PineM) SetTint(PineISM, PineISM->AddInstance(FTransform(FRotator(0, Yaw, 0), P, FVector(T.Scale)), true), Leaf);
			else AddBox(PineISM, P + FVector(0, 0, 380 * T.Scale), FVector(320, 320, 760) * T.Scale, Yaw, Hex(TEXT("2f4f35")));
		}
		else if (T.Kind == 2)
		{
			if (BushM) SetTint(CanopyISM, CanopyISM->AddInstance(FTransform(FRotator(0, Yaw, 0), P, FVector(T.Scale)), true), Leaf);
			else AddBox(CanopyISM, P + FVector(0, 0, 60 * T.Scale), FVector(170, 170, 130) * T.Scale, Yaw, Leaf);
		}
		else if (TreeM) SetTint(TrunkISM, TrunkISM->AddInstance(FTransform(FRotator(0, Yaw, 0), P, FVector(T.Scale)), true), Leaf);
		else
		{
			AddBox(TrunkISM, P + FVector(0, 0, 210 * T.Scale), FVector(32, 32, 420) * T.Scale, Yaw, Hex(TEXT("5a4332")));
			AddBox(CanopyISM, P + FVector(0, 0, 480 * T.Scale), FVector(400, 400, 330) * T.Scale, Yaw, Leaf);
		}
	}
}

void ALBCity::BuildProps()
{
	const ULBSettings* S = ULBSettings::Get();
	UStaticMesh* Cube = BasicShape(TEXT("Cube"));
	PropISM = MakeISM(TEXT("Props"), Cube, true);
	UNiagaraSystem* Smoke = LoadSoft(S->SmokeFX);
	for (const FLBPropSpot& Pr : City.Props)
	{
		float H; FLinearColor Col; PropLook(Pr.Kind, H, Col);
		const float Z = City.GroundAt(FMath::FloorToInt(Pr.Pos.X), FMath::FloorToInt(Pr.Pos.Y)) == ELBGround::Walk ? WalkZ : 0.f;
		const FVector P(Pr.Pos.X * U, Pr.Pos.Y * U, Z);
		AddBox(PropISM, P + FVector(0, 0, H / 2), FVector(Pr.Half.X * 2 * U, Pr.Half.Y * 2 * U, H), 0.f, Col);
		if (Pr.Kind == ELBProp::FireBarrel && Smoke)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Smoke, P + FVector(0, 0, H), FRotator::ZeroRotator, FVector(1), /*bAutoDestroy*/ false);
	}
	// street lamp posts
	UStaticMesh* LampM = LoadSoft(S->LampPostMesh);
	LampISM = MakeISM(TEXT("Lamps"), LampM ? LampM : Cube, true);
	for (const FLBLampSpot& L : City.Lamps)
	{
		const FVector P(L.Pos.X * U, L.Pos.Y * U, WalkZ);
		if (LampM) LampISM->AddInstance(FTransform(P), true);
		else { AddBox(LampISM, P + FVector(0, 0, 300), FVector(14, 14, 600), 0.f, Hex(TEXT("2a2c30"))); AddBox(LampISM, P + FVector(0, 0, 595), FVector(70, 26, 12), 0.f, Hex(TEXT("2a2c30"))); }
	}
}

void ALBCity::BuildVehicles()
{
	for (const FLBCarSpot& Spot : City.Cars)
	{
		const FVector P(Spot.Pos.X * U, Spot.Pos.Y * U, 0.f);
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALBVehicle* V = GetWorld()->SpawnActor<ALBVehicle>(ALBVehicle::StaticClass(), FTransform(P), Params);
		if (V) { V->Setup(Spot); Vehicles.Add(V); }
	}
}

int32 ALBCity::BuildingAtWorld(const FVector& W) const
{
	return City.BuildingAt(FMath::FloorToInt(W.X / U), FMath::FloorToInt(W.Y / U));
}

void ALBCity::SetFocus(const FVector& PlayerLocation)
{
	const int32 Bi = BuildingAtWorld(PlayerLocation);
	if (Bi == FocusBuilding) return;
	if (Roofs.IsValidIndex(FocusBuilding)) Roofs[FocusBuilding]->SetVisibility(true);
	FocusBuilding = Bi;
	if (Roofs.IsValidIndex(FocusBuilding)) Roofs[FocusBuilding]->SetVisibility(false);
}

FVector ALBCity::ContainerLocation(int32 Index) const
{
	if (!City.Containers.IsValidIndex(Index)) return FVector::ZeroVector;
	const FLBFurniturePiece& Fp = City.Furniture[City.Containers[Index].Furniture];
	const FVector2D C = Fp.Rect.GetCenter();
	return FVector(C.X * U, C.Y * U, FloorZ + 60.f);
}

int32 ALBCity::FindContainer(const FVector& From, float RadiusTiles) const
{
	int32 Best = INDEX_NONE; float BestD = RadiusTiles * U;
	const FVector2D F2(From.X, From.Y);
	for (int32 K = 0; K < City.Containers.Num(); ++K)
	{
		const FVector Loc = ContainerLocation(K);
		// distance to the edge of the furniture, not its centre: long shelves are reachable from the side
		const FLBFurniturePiece& Fp = City.Furniture[City.Containers[K].Furniture];
		const FBox2D WorldRect(Fp.Rect.Min * U, Fp.Rect.Max * U);
		const float D = FMath::Sqrt(WorldRect.ComputeSquaredDistanceToPoint(F2));
		if (D >= BestD) continue;
		// strict line of sight: no looting through walls
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LBLoot), false);
		const FVector Eye(From.X, From.Y, FloorZ + 100.f);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Loc, ECC_Visibility, Q)
			&& FVector::Dist2D(Hit.ImpactPoint, Loc) > FMath::Max(Fp.Rect.GetSize().GetMax() * U * .5f, 60.f);
		if (bBlocked) continue;
		BestD = D; Best = K;
	}
	return Best;
}

FVector ALBCity::RandomSpot(const FVector& Avoid, float MinDistTiles, float MaxDistTiles, bool bInside) const
{
	const FVector2D A = FLBCity::WorldToTile(Avoid);
	for (int32 Try = 0; Try < 400; ++Try)
	{
		const FVector2D T(FMath::FRandRange(6.f, FLBCity::N - 6.f), FMath::FRandRange(6.f, FLBCity::N - 6.f));
		const float D = FVector2D::Distance(T, A);
		if (D < MinDistTiles || D > MaxDistTiles) continue;
		const int32 I = FMath::FloorToInt(T.X), J = FMath::FloorToInt(T.Y);
		const ELBGround G = City.GroundAt(I, J);
		if (G == ELBGround::Forest) continue;
		const bool bIn = City.BuildingAt(I, J) >= 0;
		if (bIn && !bInside) continue;
		return FLBCity::TileToWorld(T, (bIn ? FloorZ : 0.f) + 100.f);
	}
	return FLBCity::TileToWorld(City.Start, 100.f);
}

void ALBCity::UpdateLights(const FVector& Focus, float Night, float Time)
{
	TArray<TPair<float, int32>> Near;
	if (Night > .1f)
		for (int32 K = 0; K < City.Lamps.Num(); ++K)
		{
			const FLBLampSpot& L = City.Lamps[K];
			if (!L.bOn) continue;
			const float D = FVector2D::Distance(FVector2D(L.Pos.X * U, L.Pos.Y * U), FVector2D(Focus.X, Focus.Y));
			if (D < 4500.f) Near.Add({ D, K });
		}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 K = 0; K < LampPool.Num(); ++K)
	{
		UPointLightComponent* Light = LampPool[K];
		if (!Near.IsValidIndex(K)) { Light->SetIntensity(0.f); continue; }
		const FLBLampSpot& L = City.Lamps[Near[K].Value];
		const bool bFlicker = L.Phase > 8.f && FMath::Sin(Time * 23.f + L.Phase) > .6f;
		Light->SetWorldLocation(FVector(L.Pos.X * U, L.Pos.Y * U, 560.f));
		Light->SetIntensity(Night * (bFlicker ? 300.f : 900.f));
	}
}

bool ALBCity::IsNearLitLamp(const FVector& W, float RadiusTiles) const
{
	const FVector2D T = FLBCity::WorldToTile(W);
	for (const FLBLampSpot& L : City.Lamps)
		if (L.bOn && FMath::Abs(L.Pos.X - T.X) < RadiusTiles && FMath::Abs(L.Pos.Y - T.Y) < RadiusTiles) return true;
	return false;
}
