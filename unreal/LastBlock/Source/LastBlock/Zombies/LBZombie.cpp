#include "Zombies/LBZombie.h"
#include "Zombies/LBZombieAIController.h"
#include "Player/LBSurvivor.h"
#include "FX/LBFX.h"
#include "Core/LBSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace { constexpr float U = LB_TILE; }

ALBZombie::ALBZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = ALBZombieAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(30.f, 90.f);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0, 240.f, 0);
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 120.f;
	// cheap movement on iPhone: no floor sweeps every frame for idle zombies
	Move->bEnablePhysicsInteraction = false;
	bUseControllerRotationYaw = false;

	GetMesh()->SetRelativeLocation(FVector(0, 0, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0, -90.f, 0));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(GetCapsuleComponent());
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Capsule(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Capsule.Succeeded()) Placeholder->SetStaticMesh(Capsule.Object);
	Placeholder->SetRelativeScale3D(FVector(.5f, .4f, 1.75f));
}

void ALBZombie::BeginPlay()
{
	Super::BeginPlay();
	// the stand-in is only needed until a real zombie mesh is set in the Blueprint
	Placeholder->SetVisibility(GetMesh()->GetSkinnedAsset() == nullptr);
	GroanTimer = FMath::FRandRange(3.f, 12.f);
}

void ALBZombie::InitZombie(bool bInRunner)
{
	bRunner = bInRunner;
	Health = bRunner ? 2.5f : FMath::FRandRange(3.f, 4.5f);
	Limp = !bRunner && FMath::FRand() < .6f ? FMath::FRandRange(.25f, .55f) : 0.f;
	BaseWalk = FMath::FRandRange(WalkSpeedTiles.X, WalkSpeedTiles.Y) * U;
	BaseRun = FMath::FRandRange(RunSpeedTiles.X, RunSpeedTiles.Y) * U;
	SetActorRotation(FRotator(0, FMath::FRandRange(0.f, 360.f), 0));
	ApplyStateSpeed(0.f);
}

void ALBZombie::ApplyStateSpeed(float Night)
{
	float Speed = bRunner ? BaseRun : BaseWalk;
	if (State == ELBZombieState::Wander) Speed *= .32f;
	else if (State == ELBZombieState::Investigate) Speed *= .8f;
	Speed *= Night > .5f ? 1.2f : 1.f;
	Speed *= 1.f - Limp * .2f;
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void ALBZombie::StartAttack(AActor* Target)
{
	if (!CanAttack()) return;
	AttackTarget = Target;
	AttackWindup = bRunner ? .3f : .4f;
	if (Target) SetActorRotation(FRotator(0, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));
	GetCharacterMovement()->StopMovementImmediately();
}

void ALBZombie::FinishAttack()
{
	AttackCooldown = bRunner ? .9f : 1.15f;
	AActor* Target = AttackTarget.Get();
	if (!Target) return;
	const float Dist = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
	if (Dist > .8f * U) return;
	// strict line of sight: no scratching through a wall
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LBZombieBite), false, this);
	Q.AddIgnoredActor(Target);
	if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Q)) return;
	const ALBZombieAIController* AI = Cast<ALBZombieAIController>(GetController());
	const float Night = AI ? AI->GetNight() : 0.f;
	const float Damage = FMath::FRandRange(AttackDamage.X, AttackDamage.Y) * (Night > .5f ? 1.15f : 1.f);
	UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, nullptr);
}

void ALBZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) return;
	HitReact = FMath::Max(0.f, HitReact - DeltaSeconds / .3f);
	AttackCooldown -= DeltaSeconds;
	if (AttackWindup > 0.f)
	{
		AttackWindup -= DeltaSeconds;
		if (AttackWindup <= 0.f) { AttackWindup = 0.f; FinishAttack(); }
	}
	if (BleedTime > 0.f)
	{
		BleedTime -= DeltaSeconds;
		if (FMath::FRand() < DeltaSeconds * 6.f) ULBFX::Blood(this, GetActorLocation() + FVector(0, 0, 40), FVector(0, 0, -1), 2);
	}
	GroanTimer -= DeltaSeconds;
	if (GroanTimer <= 0.f)
	{
		GroanTimer = FMath::FRandRange(6.f, 16.f);
		ULBFX::Sound(this, ULBSettings::Get()->GroanSound.LoadSynchronous(), GetActorLocation(), .7f, FMath::FRandRange(.85f, 1.1f));
	}
}

float ALBZombie::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead) return 0.f;
	Damage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

	FVector Dir = DamageCauser ? (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	bool bHeadshot = false;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		Dir = Point.ShotDirection.GetSafeNormal2D();
		bHeadshot = Point.HitInfo.BoneName == HeadBone;
	}
	if (const ALBSurvivor* Survivor = Cast<ALBSurvivor>(DamageCauser)) LastWeapon = Survivor->GetWeapon();

	Health -= Damage;
	// flinch in place: no knockback, the step stalls and the attack wind-up is cancelled
	HitReact = 1.f;
	HitAngle = FRotator::NormalizeAxis(Dir.Rotation().Yaw + 180.f - GetActorRotation().Yaw);
	GetCharacterMovement()->Velocity *= .2f;
	AttackWindup = 0.f;
	AttackCooldown = FMath::Max(AttackCooldown, .5f);
	State = ELBZombieState::Chase;
	ULBFX::Blood(this, GetActorLocation() - Dir * 20.f + FVector(0, 0, 40.f), Dir, 7);

	if (Health <= 0.f) Die(Dir, bHeadshot);
	else if (LastWeapon == ELBWeapon::Axe && LostArm == 0 && FMath::FRand() < .28f)
	{
		LostArm = FMath::FRand() < .5f ? 1 : -1;
		CutLimb(LostArm > 0 ? LeftArmBone : RightArmBone, Dir);
		BleedTime = 2.5f;
	}
	return Damage;
}

void ALBZombie::CutLimb(FName Bone, const FVector& Dir)
{
	USkeletalMeshComponent* M = GetMesh();
	if (M && M->GetSkinnedAsset() && M->GetBoneIndex(Bone) != INDEX_NONE)
		M->HideBoneByName(Bone, EPhysBodyOp::PBO_Term);
	ULBFX::Blood(this, M ? M->GetSocketLocation(Bone) : GetActorLocation(), Dir, 14);
}

void ALBZombie::Die(const FVector& Dir, bool bHeadshot)
{
	bDead = true;
	if (AController* C = GetController()) C->UnPossess();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetVisibility(false);

	const float Power = LastWeapon == ELBWeapon::Bat ? 2.4f : LastWeapon == ELBWeapon::Axe ? 2.f
		: LastWeapon == ELBWeapon::Rifle ? 1.9f : LastWeapon == ELBWeapon::Pistol ? 1.6f : 1.3f;

	USkeletalMeshComponent* M = GetMesh();
	if (M && M->GetSkinnedAsset() && M->GetPhysicsAsset())
	{
		// real ragdoll from the physics asset; falls the way the blow was going
		M->SetCollisionProfileName(TEXT("Ragdoll"));
		M->SetAllBodiesSimulatePhysics(true);
		M->SetSimulatePhysics(true);
		M->WakeAllRigidBodies();
		M->AddImpulse(Dir * 180.f * Power + FVector(0, 0, 60.f), NAME_None, true);
		M->bBlendPhysics = true;
	}
	if (bHeadshot) CutLimb(HeadBone, Dir);
	else if (LastWeapon == ELBWeapon::Axe && FMath::FRand() < .5f)
	{
		const FName Bones[] = { LeftArmBone, RightArmBone, LeftLegBone, RightLegBone, HeadBone, HeadBone };
		CutLimb(Bones[FMath::RandRange(0, 5)], Dir);
	}
	else if (LastWeapon == ELBWeapon::Bat && FMath::FRand() < .08f) CutLimb(HeadBone, Dir);
	ULBFX::Blood(this, GetActorLocation(), Dir, 12);

	if (ALBSurvivor* Survivor = Cast<ALBSurvivor>(UGameplayStatics::GetPlayerPawn(this, 0))) Survivor->AddKill();
	// bodies stay for a while, then clean up to keep the physics budget small
	SetLifeSpan(90.f);
}
