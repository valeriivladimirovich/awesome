#include "Game/LBPlayerController.h"
#include "UI/LBHUDWidget.h"
#include "GameFramework/TouchInterface.h"
#include "Engine/Texture2D.h"
#include "Blueprint/UserWidget.h"

void ALBPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;

	HUD = CreateWidget<ULBHUDWidget>(this, HUDClass ? *HUDClass : ULBHUDWidget::StaticClass());
	if (HUD) HUD->AddToViewport();

	// one movement stick in the lower left; the action buttons live in the HUD
	Stick = NewObject<UTouchInterface>(this);
	FTouchInputControl C;
	C.Image1 = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/MobileResources/HUD/VirtualJoystick_Background.VirtualJoystick_Background"));
	C.Image2 = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/MobileResources/HUD/VirtualJoystick_Thumb.VirtualJoystick_Thumb"));
	C.Center = FVector2D(.14f, .74f);      // fractions of the screen
	C.VisualSize = FVector2D(220, 220);
	C.ThumbSize = FVector2D(90, 90);
	C.InteractionSize = FVector2D(300, 300);
	C.InputScale = FVector2D(1, 1);
	C.MainInputKey = EKeys::Gamepad_LeftX;
	C.AltInputKey = EKeys::Gamepad_LeftY;
	Stick->Controls.Add(C);
	Stick->ActiveOpacity = .9f;
	Stick->InactiveOpacity = .45f;
	Stick->bPreventRecenter = false;
	ActivateTouchInterface(Stick);

	SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
	bShowMouseCursor = false;
}
