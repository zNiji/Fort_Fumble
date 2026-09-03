// player controller - aim at pads to place, escape for pause menu
// line trace from camera each tick, left click asks game mode to spawn a cannon
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PortalProtectPlayerController.generated.h"

class ADefenderPlacementSpot;
class UUserWidget;

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

	// pause widget - defaults to WBP_PauseMenu in content
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Pause")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass;

	// game over widget - defaults to WBP_GameOverMenu
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GameOver")
	TSubclassOf<UUserWidget> GameOverMenuWidgetClass;

	// escape or P - safe to call from blueprint
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void TogglePauseMenu();

	// close pause and go back to playing - hook resume button to this
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void ResumeGame();

	// same as ResumeGame - WBP_PauseMenu uses UnpauseGame name sometimes
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void UnpauseGame();

	// back to main menu map
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void QuitToMainMenu();

	// reload this level - hook restart on game over screen
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void RestartGame();

	// game mode calls this when the tower dies
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void ShowGameOverMenu();

	UFUNCTION(BlueprintPure, Category = "Pause")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "GameOver")
	bool IsGameOverMenuOpen() const { return bGameOverMenuOpen; }

protected:
	void OnLeftClick();
	void OnRestart();
	ADefenderPlacementSpot* TracePlacementSpot() const;

	void ShowPauseMenu();
	void HidePauseMenu();
	UClass* ResolvePauseMenuClass();
	UClass* ResolveGameOverMenuClass();
	void HideGameOverMenu();
	UClass* TryLoadWidgetClass(const TCHAR* ClassObjectPath, const TCHAR* DebugName) const;

	UPROPERTY()
	TObjectPtr<ADefenderPlacementSpot> HoveredSpot;

	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> GameOverMenuWidget;

	UPROPERTY()
	TSoftClassPtr<UUserWidget> PauseMenuClassSoft;

	UPROPERTY()
	TSoftClassPtr<UUserWidget> GameOverMenuClassSoft;

	bool bPauseMenuOpen = false;
	bool bGameOverMenuOpen = false;
};
