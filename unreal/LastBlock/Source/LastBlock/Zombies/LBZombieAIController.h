#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "LBZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UBlackboardData;
class ALBZombie;

/**
 * Zombie brain. Hearing uses AI Perception (noises reported by the player: steps, shots, blows).
 * Sight is checked by hand, because how far a zombie sees depends on the night, rain,
 * the player's flashlight and nearby street lamps.
 *
 * With ZombieTree set in Project Settings the behaviour tree runs and calls UpdateSenses()
 * through UBTService_LBZombieSenses. Without it the same logic runs in Tick, so the game
 * is playable before any AI asset exists.
 */
UCLASS()
class LASTBLOCK_API ALBZombieAIController : public AAIController
{
	GENERATED_BODY()

public:
	ALBZombieAIController();

	// Blackboard keys (the BB_Zombie asset must use the same names and types)
	static const FName KeyState;       // Enum ELBZombieState
	static const FName KeyTarget;      // Object (Actor)
	static const FName KeyMoveGoal;    // Vector

	/** Sight, memory and noise handling; writes State, Target and MoveGoal */
	void UpdateSenses();
	/** Random reachable point near the zombie for idle shuffling */
	FVector PickWanderPoint() const;

	float GetNight() const;
	ALBZombie* GetZombie() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UAIPerceptionComponent> Perception;
	UPROPERTY() TObjectPtr<UAISenseConfig_Hearing> Hearing;

	UFUNCTION() void OnPerception(AActor* Actor, FAIStimulus Stimulus);

private:
	bool bUsingTree = false;
	float ThinkTimer = 0.f;
	float LostTime = 0.f;
	float WanderTimer = 0.f;
	float StuckTime = 0.f;
	FVector LastSeen = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;

	void SetState(uint8 NewState);
	uint8 GetState() const;
	void RunFallback(float DeltaSeconds);
};
