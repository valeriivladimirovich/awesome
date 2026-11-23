#include "Game/LBGameMode.h"
#include "Game/LBPlayerController.h"
#include "Player/LBSurvivor.h"
#include "Zombies/LBZombie.h"
#include "World/LBCity.h"
#include "World/LBDayNight.h"
#include "Core/LBSettings.h"
#include "LastBlock.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ALBGameMode::ALBGameMode()
{
	DefaultPawnClass = ALBSurvivor::StaticClass();
	PlayerControllerClass = ALBPlayerController::StaticClass();
	ZombieClass = ALBZombie::StaticClass();
}

void ALBGameMode::StartPlay()
{
	UWorld* World = GetWorld();
	if (!ALBCity::Get(this)) World->SpawnActor<ALBCity>();
	if (!ALBDayNight::Get(this)) World->SpawnActor<ALBDayNight>();

	Super::StartPlay();   // BeginPlay on everything: the city is generated here

	ALBCity* City = ALBCity::Get(this);
	if (!City) return;
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		Pawn->SetActorLocation(City->PlayerStart(), false, nullptr, ETeleportType::TeleportPhysics);

	// the first few blocks around the start are quiet, the rest of the town is not
	const ULBSettings* S = ULBSettings::Get();
	const FVector Start = City->PlayerStart();
	for (int32 K = 0; K < S->ZombieCount; ++K) SpawnZombie(City, Start, 9.f, 999.f);
	UE_LOG(LogLastBlock, Log, TEXT("Spawned %d zombies"), Alive);

	World->GetTimerManager().SetTimer(PopulationTimer, this, &ALBGameMode::TopUpPopulation, 20.f, true);
}

ALBZombie* ALBGameMode::SpawnZombie(ALBCity* City, const FVector& Avoid, float MinTiles, float MaxTiles)
{
	const FVector Spot = City->RandomSpot(Avoid, MinTiles, MaxTiles, true);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	ALBZombie* Z = GetWorld()->SpawnActor<ALBZombie>(ZombieClass ? *ZombieClass : ALBZombie::StaticClass(), FTransform(Spot), P);
	if (!Z) return nullptr;
	Z->InitZombie(FMath::FRand() < ULBSettings::Get()->RunnerShare);
	Z->OnDestroyed.AddUniqueDynamic(this, &ALBGameMode::HandleZombieGone);
	++Alive;
	return Z;
}

void ALBGameMode::HandleZombieGone(AActor* Actor) { Alive = FMath::Max(0, Alive - 1); }

void ALBGameMode::TopUpPopulation()
{
	// the dead are replaced from far away, slowly, and the town grows busier every day
	ALBCity* City = ALBCity::Get(this);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!City || !Pawn) return;
	int32 Living = 0;
	for (TActorIterator<ALBZombie> It(GetWorld()); It; ++It) if (!It->bDead) ++Living;
	const ALBDayNight* DN = ALBDayNight::Get(this);
	const int32 Target = ULBSettings::Get()->ZombieCount + (DN ? (DN->GetDay() - 1) * 25 : 0);
	for (int32 K = 0; K < FMath::Min(6, Target - Living); ++K) SpawnZombie(City, Pawn->GetActorLocation(), 25.f, 999.f);
}

void ALBGameMode::OnPlayerDied(ALBSurvivor* Survivor)
{
	FTimerHandle H;
	GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
	}), RestartDelay, false);
}
