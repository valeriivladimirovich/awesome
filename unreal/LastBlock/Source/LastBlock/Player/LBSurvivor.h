#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/LBTypes.h"
#include "InputActionValue.h"
#include "LBSurvivor.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class UAnimMontage;
class UStaticMesh;
class ALBZombie;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLBOnToast, const FText&, Text, bool, bWarning);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLBOnStatsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLBOnDied);

/**
 * The survivor. Ported from the browser version:
 *  - walk / run on the stick, isometric camera, roof hides when you step inside;
 *  - hunger, thirst, health regeneration, flashlight battery at night;
 *  - searching containers takes time and makes noise, loot is scarce;
 *  - melee steps in so the fist or blade sinks into the body; damage lands mid-swing, no knockback;
 *  - guns: aim assist, spread, magazines, reload, loud enough to call the neighbourhood.
 */
UCLASS()
class LASTBLOCK_API ALBSurvivor : public ACharacter
{
	GENERATED_BODY()

public:
	ALBSurvivor();

	// ---- state read by HUD and Animation Blueprint ----
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float Health = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float Food = 85.f;
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float Water = 80.f;
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float Battery = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") int32 Kills = 0;
	/** 1 at the start of a swing, 0 when it is over; drive the swing pose with it */
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float SwingAlpha = 0.f;
	/** Seconds left of the current search, 0 when not searching */
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float SearchLeft = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float SearchTotal = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Survivor") float ReloadLeft = 0.f;

	UPROPERTY(BlueprintAssignable) FLBOnToast OnToast;
	UPROPERTY(BlueprintAssignable) FLBOnStatsChanged OnStatsChanged;
	UPROPERTY(BlueprintAssignable) FLBOnDied OnDied;

	/** Optional montages per weapon (Fists, Bat, Axe, Pistol, Rifle); procedural pose is used otherwise */
	UPROPERTY(EditDefaultsOnly, Category = "Survivor|Animation") TMap<ELBWeapon, TObjectPtr<UAnimMontage>> AttackMontages;
	UPROPERTY(EditDefaultsOnly, Category = "Survivor|Animation") TObjectPtr<UAnimMontage> SearchMontage;
	/** Weapon meshes attached to the right hand socket */
	UPROPERTY(EditDefaultsOnly, Category = "Survivor|Weapons") TMap<ELBWeapon, TObjectPtr<UStaticMesh>> WeaponMeshes;
	UPROPERTY(EditDefaultsOnly, Category = "Survivor|Weapons") FName HandSocket = "hand_r";

	// ---- actions (touch buttons and keys call these) ----
	UFUNCTION(BlueprintCallable, Category = "Survivor") void Attack();
	UFUNCTION(BlueprintCallable, Category = "Survivor") void SetAttackHeld(bool bHeld) { bAttackHeld = bHeld; if (bHeld) Attack(); }
	UFUNCTION(BlueprintCallable, Category = "Survivor") void Interact();
	UFUNCTION(BlueprintCallable, Category = "Survivor") void Reload();
	UFUNCTION(BlueprintCallable, Category = "Survivor") void CycleWeapon();
	UFUNCTION(BlueprintCallable, Category = "Survivor") void UseItem(ELBItem Item);
	/** 0 near, 1 mid, 2 far */
	UFUNCTION(BlueprintCallable, Category = "Survivor") void SetZoom(int32 Level);

	UFUNCTION(BlueprintPure, Category = "Survivor") ELBWeapon GetWeapon() const { return Weapon; }
	UFUNCTION(BlueprintPure, Category = "Survivor") int32 GetItemCount(ELBItem Item) const { return Items.FindRef(Item); }
	UFUNCTION(BlueprintPure, Category = "Survivor") int32 GetMag(ELBWeapon W) const { return Mags.FindRef(W); }
	UFUNCTION(BlueprintPure, Category = "Survivor") bool IsFlashlightOn() const;
	UFUNCTION(BlueprintPure, Category = "Survivor") bool IsDead() const { return bDead; }
	/** Name of a container in reach, empty if none (for the "search" button) */
	UFUNCTION(BlueprintPure, Category = "Survivor") FText GetNearbyContainerName() const;

	void AddKill() { ++Kills; OnStatsChanged.Broadcast(); }

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Flashlight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WeaponMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Placeholder;

	// input assets are built in code, so no .uasset is needed to play
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AttackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleAction;

private:
	ELBWeapon Weapon = ELBWeapon::Fists;
	TSet<ELBWeapon> Owned = { ELBWeapon::Fists };
	TMap<ELBItem, int32> Items;
	TMap<ELBWeapon, int32> Mags;

	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bAttackHeld = false;
	bool bDead = false;
	float Cooldown = 0.f;
	float StrikeIn = 0.f;
	float NoiseTimer = 0.f;
	float StepPhase = 0.f;
	float StarveWarn = 0.f;
	int32 SearchIndex = INDEX_NONE;
	TWeakObjectPtr<ALBZombie> LungeTarget;
	float LungeSink = 0.f;

	void BuildInput();
	void OnMove(const FInputActionValue& Value);
	void OnMoveEnd(const FInputActionValue& Value) { MoveInput = FVector2D::ZeroVector; }
	void OnAttackStart(const FInputActionValue&) { SetAttackHeld(true); }
	void OnAttackEnd(const FInputActionValue&) { SetAttackHeld(false); }
	void OnInteract(const FInputActionValue&) { Interact(); }
	void OnReload(const FInputActionValue&) { Reload(); }
	void OnCycle(const FInputActionValue&) { CycleWeapon(); }

	void TickMovement(float Dt);
	void TickStats(float Dt);
	void TickMelee(float Dt);
	void StrikeMelee();
	void FireGun();
	void FinishSearch();
	void EmitNoise(float RadiusTiles);
	void Toast(const FText& Text, bool bWarn = false) { OnToast.Broadcast(Text, bWarn); }
	void RefreshWeaponMesh();
	void Die();
	TArray<ALBZombie*> ZombiesNear(float RadiusTiles) const;
};
