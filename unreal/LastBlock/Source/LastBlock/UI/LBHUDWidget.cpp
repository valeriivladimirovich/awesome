#include "UI/LBHUDWidget.h"
#include "Player/LBSurvivor.h"
#include "World/LBDayNight.h"
#include "Core/LBGameData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "LastBlock"

namespace
{
	const FAnchors TopLeft(0.f, 0.f), TopRight(1.f, 0.f), BottomRight(1.f, 1.f), TopCenter(.5f, 0.f);

	UCanvasPanelSlot* Place(UCanvasPanel* Root, UWidget* W, const FAnchors& A, const FVector2D& Pos, const FVector2D& Size)
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(W);
		S->SetAnchors(A);
		S->SetAlignment(FVector2D(A.Minimum.X, A.Minimum.Y));
		S->SetPosition(Pos);
		S->SetSize(Size);
		return S;
	}
}

TSharedRef<SWidget> ULBHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// top left: clock and weapon
		ClockText = WidgetTree->ConstructWidget<UTextBlock>();
		ClockText->SetShadowOffset(FVector2D(1, 1));
		Place(Root, ClockText, TopLeft, FVector2D(40, 24), FVector2D(420, 40));
		WeaponText = WidgetTree->ConstructWidget<UTextBlock>();
		WeaponText->SetShadowOffset(FVector2D(1, 1));
		Place(Root, WeaponText, TopLeft, FVector2D(40, 64), FVector2D(420, 40));

		// top right: survival bars
		HealthBar = MakeBar(Root, FVector2D(-40, 28), FLinearColor(.8f, .2f, .18f));
		FoodBar = MakeBar(Root, FVector2D(-40, 52), FLinearColor(.95f, .65f, .2f));
		WaterBar = MakeBar(Root, FVector2D(-40, 76), FLinearColor(.3f, .6f, .95f));
		BatteryBar = MakeBar(Root, FVector2D(-40, 100), FLinearColor(.92f, .9f, .45f));

		// toast in the middle of the top edge
		ToastText = WidgetTree->ConstructWidget<UTextBlock>();
		ToastText->SetJustification(ETextJustify::Center);
		ToastText->SetShadowOffset(FVector2D(1, 1));
		Place(Root, ToastText, TopCenter, FVector2D(0, 120), FVector2D(900, 60));

		// search progress above the player
		SearchBar = WidgetTree->ConstructWidget<UProgressBar>();
		SearchBar->SetFillColorAndOpacity(FLinearColor(.95f, .95f, .9f));
		Place(Root, SearchBar, FAnchors(.5f, .5f), FVector2D(0, -120), FVector2D(160, 10));

		// touch buttons, bottom right; the left side belongs to the movement stick
		AttackButton = MakeButton(Root, LOCTEXT("BtnAttack", "Удар"), BottomRight, FVector2D(-40, -40), FVector2D(170, 170), FLinearColor(.75f, .2f, .18f, .85f));
		SearchButton = MakeButton(Root, LOCTEXT("BtnSearch", "Обыскать"), BottomRight, FVector2D(-230, -40), FVector2D(150, 110), FLinearColor(.2f, .22f, .26f, .85f), &SearchText);
		ReloadButton = MakeButton(Root, LOCTEXT("BtnReload", "Перезарядка"), BottomRight, FVector2D(-40, -230), FVector2D(170, 70), FLinearColor(.2f, .22f, .26f, .8f));
		SwapButton = MakeButton(Root, LOCTEXT("BtnSwap", "Оружие"), BottomRight, FVector2D(-40, -310), FVector2D(170, 70), FLinearColor(.2f, .22f, .26f, .8f));
		BagButton = MakeButton(Root, LOCTEXT("BtnBag", "Рюкзак"), TopLeft, FVector2D(40, 110), FVector2D(150, 60), FLinearColor(.2f, .22f, .26f, .8f));

		BagPanel = WidgetTree->ConstructWidget<UVerticalBox>();
		Place(Root, BagPanel, TopLeft, FVector2D(40, 180), FVector2D(260, 360));
		BagPanel->SetVisibility(ESlateVisibility::Collapsed);
		const ELBItem Usable[] = { ELBItem::Food, ELBItem::Water, ELBItem::Bandage, ELBItem::Medkit, ELBItem::Battery };
		for (ELBItem It : Usable)
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>();
			B->SetBackgroundColor(FLinearColor(.15f, .16f, .2f, .9f));
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
			T->SetText(ULBGameData::GetItemName(It));
			B->AddChild(T);
			UVerticalBoxSlot* VS = BagPanel->AddChildToVerticalBox(B);
			VS->SetPadding(FMargin(0, 0, 0, 6));
			ItemButtons.Add(B); ItemTexts.Add(T);
		}
	}
	return Super::RebuildWidget();
}

UButton* ULBHUDWidget::MakeButton(UCanvasPanel* Root, const FText& Label, const FAnchors& Anchor, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Tint, UTextBlock** OutText)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>();
	B->SetBackgroundColor(Tint);
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
	T->SetText(Label);
	T->SetJustification(ETextJustify::Center);
	B->AddChild(T);
	Place(Root, B, Anchor, Pos, Size);
	if (OutText) *OutText = T;
	return B;
}

UProgressBar* ULBHUDWidget::MakeBar(UCanvasPanel* Root, const FVector2D& Pos, const FLinearColor& Fill)
{
	UProgressBar* P = WidgetTree->ConstructWidget<UProgressBar>();
	P->SetFillColorAndOpacity(Fill);
	Place(Root, P, TopRight, Pos, FVector2D(220, 14));
	return P;
}

void ULBHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	AttackButton->OnPressed.AddUniqueDynamic(this, &ULBHUDWidget::OnAttackPressed);
	AttackButton->OnReleased.AddUniqueDynamic(this, &ULBHUDWidget::OnAttackReleased);
	SearchButton->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnSearch);
	ReloadButton->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnReload);
	SwapButton->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnSwap);
	BagButton->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnBag);
	ItemButtons[0]->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnUseFood);
	ItemButtons[1]->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnUseWater);
	ItemButtons[2]->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnUseBandage);
	ItemButtons[3]->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnUseMedkit);
	ItemButtons[4]->OnClicked.AddUniqueDynamic(this, &ULBHUDWidget::OnUseBattery);
}

ALBSurvivor* ULBHUDWidget::Survivor() const
{
	return Cast<ALBSurvivor>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ULBHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ALBSurvivor* S = Survivor();
	if (!S) return;
	if (Bound.Get() != S) { S->OnToast.AddUniqueDynamic(this, &ULBHUDWidget::OnToast); Bound = S; }

	if (const ALBDayNight* DN = ALBDayNight::Get(this))
	{
		const int32 H = FMath::FloorToInt(DN->GetHour()), M = FMath::FloorToInt(FMath::Frac(DN->GetHour()) * 60.f);
		ClockText->SetText(FText::Format(LOCTEXT("Clock", "День {0}  {1}:{2}{3}  •  упокоено: {4}"), DN->GetDay(), H, M < 10 ? FText::FromString(TEXT("0")) : FText::GetEmpty(), M, S->Kills));
	}
	const FLBWeaponDef W = ULBGameData::GetWeapon(S->GetWeapon());
	WeaponText->SetText(W.bGun
		? FText::Format(LOCTEXT("WeaponAmmo", "{0}: {1} / {2}"), W.Name, S->GetMag(S->GetWeapon()), S->GetItemCount(W.Ammo))
		: W.Name);
	HealthBar->SetPercent(S->Health / 100.f);
	FoodBar->SetPercent(S->Food / 100.f);
	WaterBar->SetPercent(S->Water / 100.f);
	BatteryBar->SetPercent(S->Battery / 100.f);

	const FText Near = S->GetNearbyContainerName();
	SearchButton->SetVisibility(Near.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (!Near.IsEmpty()) SearchText->SetText(FText::Format(LOCTEXT("SearchName", "Обыскать\n{0}"), Near));
	SearchBar->SetVisibility(S->SearchLeft > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (S->SearchTotal > 0.f) SearchBar->SetPercent(1.f - S->SearchLeft / S->SearchTotal);
	ReloadButton->SetVisibility(W.bGun ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	const ELBItem Usable[] = { ELBItem::Food, ELBItem::Water, ELBItem::Bandage, ELBItem::Medkit, ELBItem::Battery };
	for (int32 K = 0; K < ItemTexts.Num(); ++K)
	{
		const int32 N = S->GetItemCount(Usable[K]);
		ItemTexts[K]->SetText(FText::Format(LOCTEXT("ItemCount", "{0} ×{1}"), ULBGameData::GetItemName(Usable[K]), N));
		ItemButtons[K]->SetIsEnabled(N > 0);
	}

	if (ToastTime > 0.f)
	{
		ToastTime -= InDeltaTime;
		ToastText->SetRenderOpacity(FMath::Clamp(ToastTime, 0.f, 1.f));
	}
}

void ULBHUDWidget::OnToast(const FText& Text, bool bWarning)
{
	ToastText->SetText(Text);
	ToastText->SetColorAndOpacity(bWarning ? FSlateColor(FLinearColor(1.f, .55f, .45f)) : FSlateColor(FLinearColor(.95f, .95f, .9f)));
	ToastTime = 3.5f;
}

void ULBHUDWidget::OnAttackPressed() { if (ALBSurvivor* S = Survivor()) S->SetAttackHeld(true); }
void ULBHUDWidget::OnAttackReleased() { if (ALBSurvivor* S = Survivor()) S->SetAttackHeld(false); }
void ULBHUDWidget::OnSearch() { if (ALBSurvivor* S = Survivor()) S->Interact(); }
void ULBHUDWidget::OnReload() { if (ALBSurvivor* S = Survivor()) S->Reload(); }
void ULBHUDWidget::OnSwap() { if (ALBSurvivor* S = Survivor()) S->CycleWeapon(); }
void ULBHUDWidget::OnBag() { BagPanel->SetVisibility(BagPanel->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
void ULBHUDWidget::Use(ELBItem Item) { if (ALBSurvivor* S = Survivor()) S->UseItem(Item); }
void ULBHUDWidget::OnUseFood() { Use(ELBItem::Food); }
void ULBHUDWidget::OnUseWater() { Use(ELBItem::Water); }
void ULBHUDWidget::OnUseBandage() { Use(ELBItem::Bandage); }
void ULBHUDWidget::OnUseMedkit() { Use(ELBItem::Medkit); }
void ULBHUDWidget::OnUseBattery() { Use(ELBItem::Battery); }

#undef LOCTEXT_NAMESPACE
