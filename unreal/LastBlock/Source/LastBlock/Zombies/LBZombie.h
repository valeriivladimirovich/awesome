#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/LBTypes.h"
#include "LBZombie.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ELBZombieState : uint8
{
	Wander, Chase, Investigate
};

/**
 * A zombie. Melee and bullets never push it back: a hit only makes it flinch in place
 * (HitReact drives the upper body in the Animation Blueprint) and stalls its step.
 * Death turns the skeletal mesh into a physics ragdoll; the axe can cut limbs off.
 */
UCLASS()
class LASTBLOCK_API ALBZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ALBZombie();

	// ---- read by the Animation Blueprint ----
	/** 1 right after a hit, fades to 0 in .3 s; blend a flinch pose / additive by this */
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") float HitReact = 0.f;
	/** Hit direction relative to the zombie's facing, degrees (0 = from the front) */
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") float HitAngle = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") bool bRunner = false;
	/** 0..1, drags the right leg */
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") float Limp = 0.f;
	/** 0 none, 1 left arm gone, -1 right arm gone */
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") int32 LostArm = 0;
	/** Seconds left of the attack wind-up, 0 when not attacking */
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") float AttackWindup = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") ELBZombieState State = ELBZombieState::Wander;
	UPROPERTY(BlueprintReadOnly, Category = "Zombie") bool bDead = false;

	UPROPERTY(EditDefaultsOnly, Category = "Zombie") float Health = 3.5f;
	/** Shamble speed in tiles per second, as in the browser version */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FVector2D WalkSpeedTiles = FVector2D(.48f, .7f);
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FVector2D RunSpeedTiles = FVector2D(1.1f, 1.3f);
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FVector2D AttackDamage = FVector2D(8.f, 13.f);
	/** Bones hidden when a limb is cut off (UE5 mannequin names) */
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FName HeadBone = "head";
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FName LeftArmBone = "upperarm_l";
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FName RightArmBone = "upperarm_r";
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FName LeftLegBone = "calf_l";
	UPROPERTY(EditDefaultsOnly, Category = "Zombie") FName RightLegBone = "calf_r";

	void InitZombie(bool bInRunner);

	/** Called by the AI: begin a swing at the player; damage lands after the wind-up if still in reach */
	void StartAttack(AActor* Target);
	bool CanAttack() const { return !bDead && AttackWindup <= 0.f && AttackCooldown <= 0.f && HitReact <= 0.f; }

	/** Movement speed for the current state; zombies are faster at night */
	void ApplyStateSpeed(float Night);

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Weapon that dealt the last blow: decides ragdoll strength and dismemberment */
	ELBWeapon LastWeapon = ELBWeapon::Fists;

protected:
	virtual void BeginPlay() override;

	/** Visible stand-in when no skeletal mesh is assigned in the Blueprint */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Placeholder;

private:
	float AttackCooldown = 0.f;
	float BleedTime = 0.f;
	float GroanTimer = 0.f;
	float BaseWalk = 100.f;
	float BaseRun = 210.f;
	TWeakObjectPtr<AActor> AttackTarget;

	void Die(const FVector& Impulse, bool bHeadshot);
	void CutLimb(FName Bone, const FVector& Dir);
	void FinishAttack();
};
