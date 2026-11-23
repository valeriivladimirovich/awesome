#include "Zombies/LBZombieAIController.h"
#include "Zombies/LBZombie.h"
#include "Player/LBSurvivor.h"
#include "World/LBCity.h"
#include "World/LBDayNight.h"
#include "Core/LBSettings.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"

namespace { constexpr float U = LB_TILE; }

const FName ALBZombieAIController::KeyState = TEXT("State");
const FName ALBZombieAIController::KeyTarget = TEXT("Target");
const FName ALBZombieAIController::KeyMoveGoal = TEXT("MoveGoal");

ALBZombieAIController::ALBZombieAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	Hearing = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	// the loudest noise (a rifle burst) carries 20 tiles; each noise event limits itself further
	Hearing->HearingRange = 21.f * U;
	Hearing->DetectionByAffiliation.bDetectEnemies = true;
	Hearing->DetectionByAffiliation.bDetectNeutrals = true;
	Hearing->DetectionByAffiliation.bDetectFriendlies = true;
	Hearing->SetMaxAge(2.f);
	Perception->ConfigureSense(*Hearing);
	Perception->SetDominantSense(Hearing->GetSenseImplementation());
	SetPerceptionComponent(*Perception);
}

void ALBZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ALBZombieAIController::OnPerception);
	ThinkTimer = FMath::FRand() * .4f;
	LastPos = InPawn->GetActorLocation();
	if (UBehaviorTree* Tree = ULBSettings::Get()->ZombieTree.LoadSynchronous())
		bUsingTree = RunBehaviorTree(Tree);
	SetState((uint8)ELBZombieState::Wander);
}

ALBZombie* ALBZombieAIController::GetZombie() const { return Cast<ALBZombie>(GetPawn()); }

float ALBZombieAIController::GetNight() const
{
	const ALBDayNight* DN = ALBDayNight::Get(this);
	return DN ? DN->GetNight() : 0.f;
}

uint8 ALBZombieAIController::GetState() const
{
	const ALBZombie* Z = GetZombie();
	return Z ? (uint8)Z->State : 0;
}

void ALBZombieAIController::SetState(uint8 NewState)
{
	ALBZombie* Z = GetZombie();
	if (!Z) return;
	Z->State = (ELBZombieState)NewState;
	Z->ApplyStateSpeed(GetNight());
	if (UBlackboardComponent* BB = GetBlackboardComponent()) BB->SetValueAsEnum(KeyState, NewState);
}

void ALBZombieAIController::OnPerception(AActor* Actor, FAIStimulus Stimulus)
{
	// a noise pulls idle zombies over to have a look; chasing ones ignore it
	if (!Stimulus.WasSuccessfullySensed() || GetState() == (uint8)ELBZombieState::Chase) return;
	const FVector Goal = Stimulus.StimulusLocation + FVector(FMath::FRandRange(-.6f, .6f) * U, FMath::FRandRange(-.6f, .6f) * U, 0.f);
	if (UBlackboardComponent* BB = GetBlackboardComponent()) BB->SetValueAsVector(KeyMoveGoal, Goal);
	LastSeen = Goal;
	SetState((uint8)ELBZombieState::Investigate);
	if (!bUsingTree) MoveToLocation(Goal, 40.f);
}

void ALBZombieAIController::UpdateSenses()
{
	ALBZombie* Z = GetZombie();
	ALBSurvivor* Player = Cast<ALBSurvivor>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Z || Z->bDead || !Player || Player->IsDead()) return;

	const float Night = GetNight();
	const ALBDayNight* DN = ALBDayNight::Get(this);
	const ALBCity* City = ALBCity::Get(this);
	const bool bLit = Player->IsFlashlightOn() || (City && City->IsNearLitLamp(Player->GetActorLocation(), 3.2f));
	const float SightTiles = (Night > .5f ? (bLit ? 6.5f : 3.f) : 7.5f) * (DN && DN->IsRaining() ? .8f : 1.f);
	const float Dist = FVector::Dist2D(Player->GetActorLocation(), Z->GetActorLocation());

	bool bSees = false;
	if (Dist < SightTiles * U)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LBZombieSight), false, Z);
		Q.AddIgnoredActor(Player);
		const FVector Eye = Z->GetActorLocation() + FVector(0, 0, 60.f);
		bSees = !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Player->GetActorLocation() + FVector(0, 0, 40.f), ECC_Visibility, Q);
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (bSees)
	{
		LastSeen = Player->GetActorLocation();
		LostTime = 0.f;
		if (GetState() != (uint8)ELBZombieState::Chase) SetState((uint8)ELBZombieState::Chase);
		if (BB) BB->SetValueAsObject(KeyTarget, Player);
	}
	else if (GetState() == (uint8)ELBZombieState::Chase)
	{
		// keeps going for a few seconds after losing sight, then searches the last known spot
		LostTime += .3f;
		if (LostTime > 4.f)
		{
			if (BB) { BB->ClearValue(KeyTarget); BB->SetValueAsVector(KeyMoveGoal, LastSeen); }
			SetState((uint8)ELBZombieState::Investigate);
		}
	}
	else if (GetState() == (uint8)ELBZombieState::Investigate && FVector::Dist2D(Z->GetActorLocation(), LastSeen) < .5f * U)
		SetState((uint8)ELBZombieState::Wander);

	// a zombie that keeps pushing against something gets a fresh path
	const float Moved = FVector::Dist2D(Z->GetActorLocation(), LastPos);
	LastPos = Z->GetActorLocation();
	StuckTime = (GetState() != (uint8)ELBZombieState::Wander && Moved < .03f * U) ? StuckTime + .3f : 0.f;
	if (StuckTime > 1.2f) { StuckTime = 0.f; StopMovement(); }
	Z->ApplyStateSpeed(Night);
}

FVector ALBZombieAIController::PickWanderPoint() const
{
	const APawn* P = GetPawn();
	if (!P) return FVector::ZeroVector;
	FNavLocation Out;
	if (const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld()))
		if (Nav->GetRandomReachablePointInRadius(P->GetActorLocation(), 3.f * U, Out)) return Out.Location;
	return P->GetActorLocation();
}

void ALBZombieAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ALBZombie* Z = GetZombie();
	if (!Z || Z->bDead) return;

	// zombies far from the player think rarely: the city holds hundreds of them
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bFar = !Player || FVector::Dist2D(Player->GetActorLocation(), Z->GetActorLocation()) > 30.f * U;
	SetActorTickInterval(bFar ? 1.f : 0.f);
	if (bFar) { if (GetMoveStatus() != EPathFollowingStatus::Idle) StopMovement(); return; }

	ThinkTimer -= DeltaSeconds;
	if (!bUsingTree && ThinkTimer <= 0.f) { ThinkTimer = .25f + FMath::FRand() * .15f; UpdateSenses(); }
	if (!bUsingTree) RunFallback(DeltaSeconds);
}

void ALBZombieAIController::RunFallback(float DeltaSeconds)
{
	ALBZombie* Z = GetZombie();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Z || !Player || Z->AttackWindup > 0.f || Z->HitReact > 0.f) return;

	// walk straight at the goal until the navmesh has been built around this zombie
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bPath = Nav && Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
	switch (Z->State)
	{
	case ELBZombieState::Chase:
	{
		const float Dist = FVector::Dist2D(Player->GetActorLocation(), Z->GetActorLocation());
		if (Dist < .6f * U) { StopMovement(); Z->StartAttack(Player); }
		else if (GetMoveStatus() == EPathFollowingStatus::Idle) MoveToActor(Player, 50.f, true, bPath, true);
		break;
	}
	case ELBZombieState::Investigate:
		if (GetMoveStatus() == EPathFollowingStatus::Idle) MoveToLocation(LastSeen, 40.f, true, bPath);
		break;
	default:
		WanderTimer -= DeltaSeconds;
		if (WanderTimer <= 0.f)
		{
			WanderTimer = FMath::FRandRange(2.f, 6.f);
			const FVector Goal = bPath ? PickWanderPoint() : Z->GetActorLocation() + FVector(FMath::FRandRange(-3.f, 3.f) * U, FMath::FRandRange(-3.f, 3.f) * U, 0.f);
			MoveToLocation(Goal, 30.f, true, bPath);
		}
		break;
	}
}
