#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LBPlayerController.generated.h"

class ULBHUDWidget;
class UTouchInterface;

/** Adds the HUD and a single left-hand movement stick for touch screens */
UCLASS()
class LASTBLOCK_API ALBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Optional Blueprint HUD; the C++ HUD is used when empty */
	UPROPERTY(EditDefaultsOnly, Category = "UI") TSubclassOf<ULBHUDWidget> HUDClass;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient) TObjectPtr<ULBHUDWidget> HUD;
	UPROPERTY(Transient) TObjectPtr<UTouchInterface> Stick;
};
