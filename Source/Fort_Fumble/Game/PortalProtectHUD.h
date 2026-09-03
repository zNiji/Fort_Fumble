// simple canvas HUD for match stats - no UMG needed for the basics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PortalProtectHUD.generated.h"

class UFont;

UCLASS()
class FORT_FUMBLE_API APortalProtectHUD : public AHUD
{
	GENERATED_BODY()

public:
	APortalProtectHUD();
	virtual void DrawHUD() override;

protected:
	// Roboto SDF - stays sharp at HUD size (bitmap fonts blur when scaled)
	UPROPERTY()
	TObjectPtr<UFont> ScoreFont;

	// one-shot so we don't spam Output Log every frame
	bool bLoggedScoreDraw = false;
};
