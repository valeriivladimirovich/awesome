#include "Player/LBSurvivor.h"
#include "Zombies/LBZombie.h"
#include "World/LBCity.h"
#include "World/LBDayNight.h"
#include "Core/LBGameData.h"
#include "Core/LBSettings.h"
#include "FX/LBFX.h"
#include "Game/LBGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Perception/AISense_Hearing.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LastBlock"

namespace
{
	constexpr float U = LB_TILE;
	constexpr float WalkSpeed = 1.6f * U;   // tiles per second from the browser version
	constexpr float RunSpeed = 2.5f * U;
	constexpr float SwingTime = .3f;
	constexpr float StrikeDelay = .13f;     // the blow lands mid-swing, when the arm is extended
}

ALBSurvivor::ALBSurvivor()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(30.f, 90.f);
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->MaxWalkSpeed = RunSpeed;
	Move->MaxAcceleration = 2400.f;
	Move->BrakingDecelerationWalking = 2400.f;
	// slide along walls and corners instead of sticking (the old corner-snag bug)
	Move->bUseFlatBaseForFloorChecks = true;

	GetMesh()->SetRelativeLocation(FVector(0, 0, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0, -90.f, 0));

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->SetUsingAbsoluteRotation(true);
	Boom->SetWorldRotation(FRotator(-52.f, 45.f, 0.f));   // isometric three-quarter view
	Boom->TargetArmLength = 1700.f;
	Boom->bDoCollisionTest = false;                        // roofs and walls are cut away instead
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom);
	Camera->SetFieldOfView(34.f);

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(GetCapsuleComponent());
	Flashlight->SetRelativeLocation(FVector(30.f, 0.f, 40.f));
	Flashlight->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(0.f);
	Flashlight->SetAttenuationRadius(2600.f);
	Flashlight->SetInnerConeAngle(14.f);
	Flashlight->SetOuterConeAngle(26.f);
	Flashlight->SetLightColor(FLinearColor(1.f, .94f, .82f));
	Flashlight->SetCastShadows(true);
	Flashlight->SetVolumetricScatteringIntensity(2.f);    // visible beam in fog and rain

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	WeaponMesh->SetupAttachment(GetMesh(), HandSocket);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(GetCapsuleComponent());
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) Placeholder->SetStaticMesh(Cyl.Object);
	Placeholder->SetRelativeScale3D(FVector(.5f, .4f, 1.75f));
}

void ALBSurvivor::BeginPlay()
{
	Super::BeginPlay();
	Placeholder->SetVisibility(GetMesh()->GetSkinnedAsset() == nullptr);
	Items.Add(ELBItem::Food, 1);
	Items.Add(ELBItem::Water, 1);
	RefreshWeaponMesh();
}

void ALBSurvivor::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			BuildInput();
			Sub->AddMappingContext(Mapping, 0);
		}
}

// ---------------------------------------------------------------- input

void ALBSurvivor::BuildInput()
{
	if (Mapping) return;
	Mapping = NewObject<UInputMappingContext>(this);
	auto MakeAction = [this](EInputActionValueType Type) { UInputAction* A = NewObject<UInputAction>(this); A->ValueType = Type; return A; };
	MoveAction = MakeAction(EInputActionValueType::Axis2D);
	AttackAction = MakeAction(EInputActionValueType::Boolean);
	InteractAction = MakeAction(EInputActionValueType::Boolean);
	ReloadAction = MakeAction(EInputActionValueType::Boolean);
	CycleAction = MakeAction(EInputActionValueType::Boolean);

	// WASD into one 2D axis: W/S on Y (swizzled), A/S negated
	auto MapMoveKey = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(MoveAction, Key);
		if (bSwizzle) M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Mapping));
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
	};
	MapMoveKey(EKeys::W, true, false);
	MapMoveKey(EKeys::S, true, true);
	MapMoveKey(EKeys::D, false, false);
	MapMoveKey(EKeys::A, false, true);
	// on iPhone the default virtual joystick feeds the left gamepad stick
	Mapping->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	Mapping->MapKey(AttackAction, EKeys::SpaceBar);
	Mapping->MapKey(AttackAction, EKeys::LeftMouseButton);
	Mapping->MapKey(AttackAction, EKeys::Gamepad_RightTrigger);
	Mapping->MapKey(InteractAction, EKeys::E);
	Mapping->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(ReloadAction, EKeys::R);
	Mapping->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);
	Mapping->MapKey(CycleAction, EKeys::Q);
	Mapping->MapKey(CycleAction, EKeys::Gamepad_FaceButton_Top);
}

void ALBSurvivor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;
	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALBSurvivor::OnMove);
	EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ALBSurvivor::OnMoveEnd);
	EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ALBSurvivor::OnAttackStart);
	EIC->BindAction(AttackAction, ETriggerEvent::Completed, this, &ALBSurvivor::OnAttackEnd);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ALBSurvivor::OnInteract);
	EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &ALBSurvivor::OnReload);
	EIC->BindAction(CycleAction, ETriggerEvent::Started, this, &ALBSurvivor::OnCycle);
}

void ALBSurvivor::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
}

// ---------------------------------------------------------------- tick

void ALBSurvivor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) return;
	TickMovement(DeltaSeconds);
	TickMelee(DeltaSeconds);
	TickStats(DeltaSeconds);

	if (ALBCity* City = ALBCity::Get(this)) City->SetFocus(GetActorLocation());
	// walls between the camera and the player fade out in their material (see README: M_Wall)
	if (UMaterialParameterCollection* MPC = ULBSettings::Get()->WorldParams.LoadSynchronous())
		UKismetMaterialLibrary::SetVectorParameterValue(this, MPC, TEXT("PlayerPos"), FLinearColor(GetActorLocation()));
}

void ALBSurvivor::TickMovement(float Dt)
{
	const float M = MoveInput.Size();
	if (SearchLeft > 0.f && M > .4f) { SearchLeft = 0.f; SearchIndex = INDEX_NONE; Toast(LOCTEXT("SearchCut", "Обыск прерван")); }
	if (SearchLeft > 0.f || M < .05f) return;

	// stick is camera-relative; the camera looks along +45 degrees
	const FRotator CamYaw(0.f, Boom->GetComponentRotation().Yaw, 0.f);
	const FVector Fwd = CamYaw.RotateVector(FVector::ForwardVector), Right = CamYaw.RotateVector(FVector::RightVector);
	const FVector Dir = (Fwd * MoveInput.Y + Right * MoveInput.X).GetSafeNormal2D();
	// full stick runs, partial stick walks (same thresholds as the browser version)
	const float Speed = M > .92f ? RunSpeed : WalkSpeed * FMath::Min(1.f, M / .85f + .1f);
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	AddMovementInput(Dir, 1.f);
	if (SwingAlpha <= 0.f && ReloadLeft <= 0.f) SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), Dt, 14.f));

	const float Vel = GetVelocity().Size2D();
	// footsteps: noise for zombies, dust when running
	NoiseTimer -= Dt;
	if (Vel > .3f * U && NoiseTimer <= 0.f) { NoiseTimer = .5f; EmitNoise(Vel > 2.2f * U ? 4.5f : 1.6f); }
	const float PrevPhase = StepPhase;
	StepPhase += Dt * Vel / U * 7.5f;
	if (FMath::FloorToInt(PrevPhase / PI) != FMath::FloorToInt(StepPhase / PI) && Vel > 2.2f * U)
		if (ALBCity* City = ALBCity::Get(this); City && City->BuildingAtWorld(GetActorLocation()) < 0)
			ULBFX::Dust(this, GetActorLocation() - FVector(0, 0, 88.f));
}

void ALBSurvivor::TickStats(float Dt)
{
	const ULBSettings* S = ULBSettings::Get();
	const float K = 6.f / S->DayLengthMinutes;     // drain scales with day length, as in the browser version
	const bool bRun = GetVelocity().Size2D() > 2.2f * U;
	Food = FMath::Max(0.f, Food - Dt * 100.f / 420.f * (bRun ? 1.5f : 1.f) * K);
	Water = FMath::Max(0.f, Water - Dt * 100.f / 330.f * (bRun ? 1.6f : 1.f) * K);
	if (Food <= 0.f || Water <= 0.f)
	{
		Health -= Dt * 1.4f;
		StarveWarn -= Dt;
		if (StarveWarn <= 0.f) { StarveWarn = 20.f; Toast(Food <= 0.f ? LOCTEXT("Hungry", "Голод отнимает силы. Поешь") : LOCTEXT("Thirsty", "Жажда отнимает силы. Попей"), true); }
	}
	else if (Food > 45.f && Water > 45.f && Health < 100.f) Health = FMath::Min(100.f, Health + Dt * .45f);

	const ALBDayNight* DN = ALBDayNight::Get(this);
	const float Night = DN ? DN->GetNight() : 0.f;
	if (Night > .5f && Battery > 0.f)
	{
		Battery = FMath::Max(0.f, Battery - Dt * .42f * K);
		if (Battery <= 0.f) Toast(LOCTEXT("BatteryDead", "Фонарик сел. Батарейка в рюкзаке?"), true);
	}
	Flashlight->SetIntensity(IsFlashlightOn() ? 2500.f * Night : 0.f);

	Cooldown -= Dt;
	if (ReloadLeft > 0.f)
	{
		ReloadLeft -= Dt;
		if (ReloadLeft <= 0.f)
		{
			const FLBWeaponDef& W = ULBGameData::GetWeapon(Weapon);
			int32& Mag = Mags.FindOrAdd(Weapon);
			int32& Ammo = Items.FindOrAdd(W.Ammo);
			const int32 Take = FMath::Min(W.MagSize - Mag, Ammo);
			Mag += Take; Ammo -= Take;
		}
	}
	if (SearchLeft > 0.f) { SearchLeft -= Dt; if (SearchLeft <= 0.f) FinishSearch(); }
	if (bAttackHeld && ULBGameData::GetWeapon(Weapon).bAuto && Cooldown <= 0.f) Attack();
	if (Health <= 0.f) Die();
	OnStatsChanged.Broadcast();
}

bool ALBSurvivor::IsFlashlightOn() const
{
	const ALBDayNight* DN = ALBDayNight::Get(this);
	return !bDead && Battery > 0.f && DN && DN->GetNight() > .5f;
}

// ---------------------------------------------------------------- combat

TArray<ALBZombie*> ALBSurvivor::ZombiesNear(float RadiusTiles) const
{
	TArray<ALBZombie*> Out;
	const float R2 = FMath::Square(RadiusTiles * U);
	for (TActorIterator<ALBZombie> It(GetWorld()); It; ++It)
		if (!It->bDead && FVector::DistSquared2D(It->GetActorLocation(), GetActorLocation()) < R2) Out.Add(*It);
	return Out;
}

static bool ClearLine(const UWorld* World, const FVector& A, const FVector& B, const AActor* IgnoreA, const AActor* IgnoreB)
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LBLine), false, IgnoreA);
	Q.AddIgnoredActor(IgnoreB);
	return !World->LineTraceSingleByChannel(Hit, A + FVector(0, 0, 40), B + FVector(0, 0, 40), ECC_Visibility, Q);
}

void ALBSurvivor::Attack()
{
	if (bDead || Cooldown > 0.f) return;
	const FLBWeaponDef& W = ULBGameData::GetWeapon(Weapon);
	if (W.bGun) { FireGun(); return; }
	Cooldown = W.Cooldown;
	SwingAlpha = 1.f;
	SearchLeft = 0.f; SearchIndex = INDEX_NONE;

	// turn to the nearest zombie in reach and step in so the blow sinks into the body
	ALBZombie* Near = nullptr; float BestD = (W.Range + .7f) * U;
	for (ALBZombie* Z : ZombiesNear(W.Range + .7f))
	{
		const float D = FVector::Dist2D(Z->GetActorLocation(), GetActorLocation());
		if (D < BestD && ClearLine(GetWorld(), GetActorLocation(), Z->GetActorLocation(), this, Z)) { BestD = D; Near = Z; }
	}
	if (Near)
	{
		SetActorRotation(FRotator(0, (Near->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));
		LungeTarget = Near; LungeSink = W.SinkDistance * U;
	}
	StrikeIn = StrikeDelay;
	if (TObjectPtr<UAnimMontage>* Montage = AttackMontages.Find(Weapon)) PlayAnimMontage(*Montage);
	ULBFX::Sound(this, ULBSettings::Get()->SwingSound.LoadSynchronous(), GetActorLocation(), .6f, FMath::FRandRange(.9f, 1.1f));
}

void ALBSurvivor::TickMelee(float Dt)
{
	SwingAlpha = FMath::Max(0.f, SwingAlpha - Dt / SwingTime);
	// the step-in lasts for the first half of the swing
	if (ALBZombie* Z = LungeTarget.Get(); Z && !Z->bDead && SwingAlpha > .47f)
	{
		const FVector To = Z->GetActorLocation() - GetActorLocation();
		const float Over = To.Size2D() - LungeSink;
		if (Over > 2.f)
		{
			// sweep so the step never pushes through a wall; zombie capsules are ignored so the hand can pass into the body
			FCollisionQueryParams Q(SCENE_QUERY_STAT(LBLunge), false, this);
			Q.AddIgnoredActor(Z);
			FHitResult Hit;
			const FVector Step = To.GetSafeNormal2D() * FMath::Min(Over, Dt * 5.5f * U);
			const FVector From = GetActorLocation();
			if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Step, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(28.f), Q))
				SetActorLocation(From + Step, false, nullptr, ETeleportType::None);
		}
	}
	if (StrikeIn > 0.f) { StrikeIn -= Dt; if (StrikeIn <= 0.f) StrikeMelee(); }
}

void ALBSurvivor::StrikeMelee()
{
	LungeTarget.Reset();
	const FLBWeaponDef& W = ULBGameData::GetWeapon(Weapon);
	bool bHitAny = false;
	const FVector Fwd = GetActorForwardVector();
	for (ALBZombie* Z : ZombiesNear(W.Range + .3f))
	{
		const FVector To = Z->GetActorLocation() - GetActorLocation();
		const float D = To.Size2D();
		const float Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(Fwd.GetSafeNormal2D(), To.GetSafeNormal2D()), -1.f, 1.f));
		if (Ang > 1.15f && D > .4f * U) continue;
		if (!ClearLine(GetWorld(), GetActorLocation(), Z->GetActorLocation(), this, Z)) continue;
		bHitAny = true;
		FHitResult HitInfo; HitInfo.ImpactPoint = Z->GetActorLocation(); HitInfo.Location = HitInfo.ImpactPoint;
		// no knockback: damage only; the zombie flinches in place
		UGameplayStatics::ApplyPointDamage(Z, W.Damage, To.GetSafeNormal2D(), HitInfo, GetController(), this, nullptr);
	}
	EmitNoise(bHitAny ? 6.f : 3.5f);
	if (bHitAny) ULBFX::Sound(this, ULBSettings::Get()->HitSound.LoadSynchronous(), GetActorLocation(), .9f, FMath::FRandRange(.85f, 1.05f));
}

void ALBSurvivor::FireGun()
{
	const FLBWeaponDef& W = ULBGameData::GetWeapon(Weapon);
	if (ReloadLeft > 0.f) return;
	int32& Mag = Mags.FindOrAdd(Weapon);
	if (Mag <= 0)
	{
		Reload();
		if (ReloadLeft <= 0.f) { Cooldown = .35f; Toast(LOCTEXT("NoAmmo", "Нет патронов. Смени оружие"), true); }
		return;
	}
	Cooldown = W.Cooldown; --Mag;
	SearchLeft = 0.f; SearchIndex = INDEX_NONE;

	// aim assist: favour zombies close to where the player faces
	const FVector Me = GetActorLocation();
	ALBZombie* Best = nullptr; float BestScore = BIG_NUMBER;
	for (ALBZombie* Z : ZombiesNear(W.Range))
	{
		const FVector To = Z->GetActorLocation() - Me;
		const float D = To.Size2D() / U;
		const float Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(GetActorForwardVector(), To.GetSafeNormal2D()), -1.f, 1.f));
		if (Ang > .9f && D > 2.2f) continue;
		const float Score = D + Ang * 4.f;
		if (Score < BestScore && ClearLine(GetWorld(), Me, Z->GetActorLocation(), this, Z)) { BestScore = Score; Best = Z; }
	}
	if (Best) SetActorRotation(FRotator(0, (Best->GetActorLocation() - Me).Rotation().Yaw, 0));

	const float Yaw = GetActorRotation().Yaw + FMath::RadiansToDegrees(FMath::FRandRange(-W.Spread, W.Spread));
	const FVector Dir = FRotator(0, Yaw, 0).Vector();
	const FVector Muzzle = Me + Dir * 45.f + FVector(0, 0, 40.f);
	FVector End = Muzzle + Dir * W.Range * U;

	// walls stop the bullet
	FHitResult WallHit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LBShot), false, this);
	for (ALBZombie* Z : ZombiesNear(W.Range + 1.f)) Q.AddIgnoredActor(Z);
	const bool bWall = GetWorld()->LineTraceSingleByChannel(WallHit, Muzzle, End, ECC_Visibility, Q);
	if (bWall) End = WallHit.ImpactPoint;

	// first zombie along the ray (capsule radius ~35 cm)
	ALBZombie* Hit = nullptr; float HitT = BIG_NUMBER;
	for (ALBZombie* Z : ZombiesNear(W.Range + 1.f))
	{
		const FVector P = FMath::ClosestPointOnSegment(Z->GetActorLocation(), Muzzle, End);
		if (FVector::Dist2D(P, Z->GetActorLocation()) > 40.f) continue;
		const float T = FVector::Dist2D(P, Muzzle);
		if (T < HitT) { HitT = T; Hit = Z; }
	}
	if (Hit) End = Muzzle + Dir * HitT;

	ULBFX::Muzzle(this, Muzzle, Dir.Rotation());
	ULBFX::Tracer(this, Muzzle, End);
	ULBFX::Sound(this, (Weapon == ELBWeapon::Rifle ? ULBSettings::Get()->RifleSound : ULBSettings::Get()->PistolSound).LoadSynchronous(), Muzzle);
	if (Hit)
	{
		const bool bHead = FMath::FRand() < (Hit == Best ? .22f : .12f);
		FHitResult Info; Info.ImpactPoint = End; Info.Location = End; Info.BoneName = bHead ? Hit->HeadBone : NAME_None;
		UGameplayStatics::ApplyPointDamage(Hit, W.Damage * (bHead ? 2.5f : 1.f), Dir, Info, GetController(), this, nullptr);
	}
	else if (bWall) ULBFX::Impact(this, WallHit.ImpactPoint, WallHit.ImpactNormal);

	EmitNoise(W.Noise);
	if (TObjectPtr<UAnimMontage>* Montage = AttackMontages.Find(Weapon)) PlayAnimMontage(*Montage);
	if (Mag <= 0) Reload();
}

void ALBSurvivor::Reload()
{
	const FLBWeaponDef& W = ULBGameData::GetWeapon(Weapon);
	if (!W.bGun || ReloadLeft > 0.f || Mags.FindRef(Weapon) >= W.MagSize || Items.FindRef(W.Ammo) <= 0) return;
	ReloadLeft = W.ReloadTime;
}

void ALBSurvivor::CycleWeapon()
{
	// best melee weapon, then the guns
	TArray<ELBWeapon> List;
	for (ELBWeapon M : { ELBWeapon::Axe, ELBWeapon::Bat, ELBWeapon::Fists }) if (Owned.Contains(M)) { List.Add(M); break; }
	for (ELBWeapon G : { ELBWeapon::Pistol, ELBWeapon::Rifle }) if (Owned.Contains(G)) List.Add(G);
	if (List.Num() < 2) { Toast(LOCTEXT("NoOther", "Другого оружия пока нет")); return; }
	const int32 I = List.IndexOfByKey(Weapon);
	Weapon = List[(I + 1) % List.Num()];
	ReloadLeft = 0.f;
	RefreshWeaponMesh();
	Toast(FText::Format(LOCTEXT("InHands", "В руках: {0}"), ULBGameData::GetWeapon(Weapon).Name));
}

void ALBSurvivor::RefreshWeaponMesh()
{
	TObjectPtr<UStaticMesh>* M = WeaponMeshes.Find(Weapon);
	WeaponMesh->SetStaticMesh(M ? M->Get() : nullptr);
	WeaponMesh->SetVisibility(M != nullptr);
}

void ALBSurvivor::EmitNoise(float RadiusTiles)
{
	UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), 1.f, this, RadiusTiles * U);
}

float ALBSurvivor::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead) return 0.f;
	Damage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	Health -= Damage;
	SearchLeft = 0.f; SearchIndex = INDEX_NONE;
	ULBFX::Blood(this, GetActorLocation() + FVector(0, 0, 30), DamageCauser ? GetActorLocation() - DamageCauser->GetActorLocation() : FVector::UpVector, 5);
	if (Health <= 0.f) Die();
	return Damage;
}

void ALBSurvivor::Die()
{
	if (bDead) return;
	bDead = true;
	Health = 0.f;
	Flashlight->SetIntensity(0.f);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) DisableInput(PC);
	GetCharacterMovement()->DisableMovement();
	if (GetMesh()->GetSkinnedAsset() && GetMesh()->GetPhysicsAsset())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
	}
	OnDied.Broadcast();
	if (ALBGameMode* GM = GetWorld()->GetAuthGameMode<ALBGameMode>()) GM->OnPlayerDied(this);
}

// ---------------------------------------------------------------- loot

FText ALBSurvivor::GetNearbyContainerName() const
{
	const ALBCity* City = ALBCity::Get(this);
	if (!City) return FText::GetEmpty();
	const int32 K = City->FindContainer(GetActorLocation(), .6f);
	if (K == INDEX_NONE) return FText::GetEmpty();
	const FLBContainer* C = const_cast<ALBCity*>(City)->GetContainer(K);
	return C && !C->bSearched ? C->Name : FText::GetEmpty();
}

void ALBSurvivor::Interact()
{
	if (bDead || SearchLeft > 0.f) return;
	ALBCity* City = ALBCity::Get(this);
	if (!City) return;
	const int32 K = City->FindContainer(GetActorLocation(), .6f);
	const FLBContainer* C = City->GetContainer(K);
	if (!C || C->bSearched) return;
	const FLBFurniturePiece& Fp = City->Data().Furniture[C->Furniture];
	SearchIndex = K;
	SearchTotal = SearchLeft = Fp.Kind == ELBFurniture::Shelf ? 1.2f : 1.5f;
	SetActorRotation(FRotator(0, (City->ContainerLocation(K) - GetActorLocation()).Rotation().Yaw, 0));
	EmitNoise(3.5f);
	if (SearchMontage) PlayAnimMontage(SearchMontage);
}

void ALBSurvivor::FinishSearch()
{
	ALBCity* City = ALBCity::Get(this);
	FLBContainer* C = City ? City->GetContainer(SearchIndex) : nullptr;
	SearchIndex = INDEX_NONE;
	if (!C) return;
	C->bSearched = true;
	if (C->Loot.Num() == 0) { Toast(FText::Format(LOCTEXT("Empty", "{0}: пусто"), C->Name)); return; }

	TArray<FText> Got;
	for (ELBItem It : C->Loot)
	{
		ELBWeapon NewW;
		if (ULBGameData::ItemToWeapon(It, NewW))
		{
			const FLBWeaponDef& W = ULBGameData::GetWeapon(NewW);
			if (!Owned.Contains(NewW))
			{
				Owned.Add(NewW);
				if (W.bGun)
				{
					Mags.Add(NewW, FMath::RandRange(2, W.MagSize / 2));
					Weapon = NewW;
					Toast(FText::Format(LOCTEXT("FoundGun", "Нашёл: {0}! В магазине {1}. Выстрелы созывают зомби"), W.Name, Mags[NewW]), true);
				}
				else
				{
					if (!ULBGameData::GetWeapon(Weapon).bGun && W.Rank > ULBGameData::GetWeapon(Weapon).Rank) Weapon = NewW;
					Toast(FText::Format(LOCTEXT("FoundMelee", "Нашёл: {0}"), W.Name));
				}
				RefreshWeaponMesh();
			}
			else if (W.bGun)
			{
				const int32 N = FMath::RandRange(4, 10);
				Items.FindOrAdd(W.Ammo) += N;
				Got.Add(FText::Format(LOCTEXT("ItemN", "{0} ×{1}"), ULBGameData::GetItemName(W.Ammo), N));
			}
			continue;
		}
		const int32 N = It == ELBItem::Ammo9 ? FMath::RandRange(5, 12) : It == ELBItem::Ammo545 ? FMath::RandRange(8, 20) : 1;
		Items.FindOrAdd(It) += N;
		Got.Add(N > 1 ? FText::Format(LOCTEXT("ItemN2", "{0} ×{1}"), ULBGameData::GetItemName(It), N) : ULBGameData::GetItemName(It));
	}
	C->Loot.Reset();
	if (Got.Num()) Toast(FText::Format(LOCTEXT("Loot", "{0}: {1}"), C->Name, FText::Join(FText::FromString(TEXT(", ")), Got)));
}

void ALBSurvivor::UseItem(ELBItem Item)
{
	int32* Count = Items.Find(Item);
	if (!Count || *Count <= 0) return;
	switch (Item)
	{
	case ELBItem::Food: Food = FMath::Min(100.f, Food + 35.f); break;
	case ELBItem::Water: Water = FMath::Min(100.f, Water + 40.f); break;
	case ELBItem::Bandage: Health = FMath::Min(100.f, Health + 30.f); break;
	case ELBItem::Medkit: Health = FMath::Min(100.f, Health + 60.f); break;
	case ELBItem::Battery: Battery = 100.f; break;
	default: return;
	}
	--*Count;
	OnStatsChanged.Broadcast();
}

void ALBSurvivor::SetZoom(int32 Level)
{
	static const float Lengths[] = { 1700.f, 2200.f, 2900.f };
	Boom->TargetArmLength = Lengths[FMath::Clamp(Level, 0, 2)];
}

#undef LOCTEXT_NAMESPACE
