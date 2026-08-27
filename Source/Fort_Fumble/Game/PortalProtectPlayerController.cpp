#include "Game/PortalProtectPlayerController.h"
#include "Game/PortalProtectGameMode.h"
#include "Defender/DefenderPlacementSpot.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

APortalProtectPlayerController::APortalProtectPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	// Default asset: Content/UI/Widgets/WBP_PauseMenu.uasset
	static ConstructorHelpers::FClassFinder<UUserWidget> PauseMenuBP(
		TEXT("/Game/UI/Widgets/WBP_PauseMenu"));
	if (PauseMenuBP.Succeeded())
	{
		PauseMenuClassHard = PauseMenuBP.Class;
		PauseMenuClass = PauseMenuBP.Class;
	}
}

void APortalProtectPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
}

void APortalProtectPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APortalProtectPlayerController::OnLeftClick);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APortalProtectPlayerController::OnRestart);

		// Esc must fire while paused so the player can unpause.
		FInputKeyBinding EscapeBinding(FInputChord(EKeys::Escape), IE_Pressed);
		EscapeBinding.bExecuteWhenPaused = true;
		EscapeBinding.KeyDelegate.GetDelegateForManualSet().BindUObject(
			this, &APortalProtectPlayerController::TogglePauseMenu);
		InputComponent->KeyBindings.Add(EscapeBinding);
	}
}

void APortalProtectPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bPauseMenuOpen || IsPaused())
	{
		return;
	}

	ADefenderPlacementSpot* Spot = TracePlacementSpot();
	if (HoveredSpot && HoveredSpot != Spot)
	{
		HoveredSpot->SetHighlighted(false);
	}
	HoveredSpot = Spot;
	if (HoveredSpot)
	{
		HoveredSpot->SetHighlighted(true);
	}
}

ADefenderPlacementSpot* APortalProtectPlayerController::TracePlacementSpot() const
{
	FVector ViewLoc;
	FRotator ViewRot;
	GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector TraceEnd = ViewLoc + ViewRot.Vector() * PlacementTraceDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlacementTrace), true, GetPawn());
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, Params))
	{
		return nullptr;
	}
	return Cast<ADefenderPlacementSpot>(Hit.GetActor());
}

void APortalProtectPlayerController::OnLeftClick()
{
	if (bPauseMenuOpen || IsPaused())
	{
		return;
	}

	APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr;
	if (!GM || GM->IsGameOver())
	{
		return;
	}

	if (ADefenderPlacementSpot* Spot = TracePlacementSpot())
	{
		GM->TryPlaceDefenderAtSpot(Spot);
	}
}

void APortalProtectPlayerController::OnRestart()
{
	if (bPauseMenuOpen || IsPaused())
	{
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
}

UClass* APortalProtectPlayerController::ResolvePauseMenuClass()
{
	if (UClass* Loaded = PauseMenuClass.LoadSynchronous())
	{
		return Loaded;
	}
	return PauseMenuClassHard.Get();
}

void APortalProtectPlayerController::TogglePauseMenu()
{
	if (bPauseMenuOpen)
	{
		ResumeGame();
	}
	else
	{
		ShowPauseMenu();
	}
}

void APortalProtectPlayerController::ResumeGame()
{
	HidePauseMenu();
}

void APortalProtectPlayerController::UnpauseGame()
{
	ResumeGame();
}

void APortalProtectPlayerController::QuitToMainMenu()
{
	HidePauseMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Scenes/MainMenu")));
}

void APortalProtectPlayerController::ShowPauseMenu()
{
	if (bPauseMenuOpen)
	{
		return;
	}

	UClass* WidgetClass = ResolvePauseMenuClass();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pause menu widget class missing (expected /Game/UI/Widgets/WBP_PauseMenu)."));
		return;
	}

	if (!PauseMenuWidget)
	{
		PauseMenuWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (!PauseMenuWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create WBP_PauseMenu."));
			return;
		}

		// Wire standard button names if present (BP graphs can also call ResumeGame / QuitToMainMenu).
		if (UButton* ResumeBtn = Cast<UButton>(PauseMenuWidget->GetWidgetFromName(TEXT("ButtonResume"))))
		{
			ResumeBtn->OnClicked.AddDynamic(this, &APortalProtectPlayerController::ResumeGame);
		}
		if (UButton* QuitBtn = Cast<UButton>(PauseMenuWidget->GetWidgetFromName(TEXT("ButtonQuit"))))
		{
			QuitBtn->OnClicked.AddDynamic(this, &APortalProtectPlayerController::QuitToMainMenu);
		}
	}

	PauseMenuWidget->AddToViewport(100);
	PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	SetPause(true);
	bPauseMenuOpen = true;
}

void APortalProtectPlayerController::HidePauseMenu()
{
	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	SetPause(false);
	bPauseMenuOpen = false;
}
