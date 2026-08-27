#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PortalProtectPlayerController.generated.h"

class ADefenderPlacementSpot;
class UUserWidget;

/** FPS look + aim-at-pad placement. Paths remain blocked for builds. Esc toggles pause menu. */
UCLASS()
class FORT_FUMBLE_API APortalProtectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APortalProtectPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Placement")
	float PlacementTraceDistance = 4000.f;

	/** Soft path to the pause UMG widget (default: /Game/UI/Widgets/WBP_PauseMenu). */
	UPROPERTY(EditAnywhere, Category = "Pause")
	TSoftClassPtr<UUserWidget> PauseMenuClass;

	/** Toggle pause menu (Escape). Safe to call from Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void TogglePauseMenu();

	/** Close pause menu and resume gameplay. Bind WBP_PauseMenu Resume button to this. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void ResumeGame();

	/** Alias for ResumeGame — matches UnpauseGame naming used in WBP_PauseMenu. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void UnpauseGame();

	/** Leave the match and open the main menu map. Bind Quit button if not already wired in BP. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void QuitToMainMenu();

	UFUNCTION(BlueprintPure, Category = "Pause")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

protected:
	void OnLeftClick();
	void OnRestart();
	ADefenderPlacementSpot* TracePlacementSpot() const;

	void ShowPauseMenu();
	void HidePauseMenu();
	UClass* ResolvePauseMenuClass();

	UPROPERTY()
	TObjectPtr<ADefenderPlacementSpot> HoveredSpot;

	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget;

	UPROPERTY()
	TSubclassOf<UUserWidget> PauseMenuClassHard;

	bool bPauseMenuOpen = false;
};
