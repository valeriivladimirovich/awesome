#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/LBTypes.h"
#include "Widgets/Layout/Anchors.h"
#include "LBHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UButton;
class UVerticalBox;
class UCanvasPanel;
class ALBSurvivor;

/**
 * In-game HUD built entirely in C++ (no widget asset needed): clock, health / food /
 * water / battery bars, weapon and ammo, toasts, and big touch buttons for attack,
 * search, reload, weapon swap and the backpack. A Blueprint child can restyle it.
 */
UCLASS()
class LASTBLOCK_API ULBHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeaponText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToastText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SearchText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> FoodBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> WaterBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> BatteryBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> SearchBar;
	UPROPERTY(Transient) TObjectPtr<UButton> AttackButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SearchButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ReloadButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SwapButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BagButton;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> BagPanel;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ItemButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ItemTexts;

	UFUNCTION() void OnAttackPressed();
	UFUNCTION() void OnAttackReleased();
	UFUNCTION() void OnSearch();
	UFUNCTION() void OnReload();
	UFUNCTION() void OnSwap();
	UFUNCTION() void OnBag();
	UFUNCTION() void OnUseFood();
	UFUNCTION() void OnUseWater();
	UFUNCTION() void OnUseBandage();
	UFUNCTION() void OnUseMedkit();
	UFUNCTION() void OnUseBattery();
	UFUNCTION() void OnToast(const FText& Text, bool bWarning);

private:
	float ToastTime = 0.f;
	TWeakObjectPtr<ALBSurvivor> Bound;

	ALBSurvivor* Survivor() const;
	void Use(ELBItem Item);
	UButton* MakeButton(UCanvasPanel* Root, const FText& Label, const FAnchors& Anchor, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Tint, UTextBlock** OutText = nullptr);
	UProgressBar* MakeBar(UCanvasPanel* Root, const FVector2D& Pos, const FLinearColor& Fill);
};
