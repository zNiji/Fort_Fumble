#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PortalProtectHUD.generated.h"

/** On-screen HUD: tower HP, coins, placement hints, seed, and game-over. */
UCLASS()
class FORT_FUMBLE_API APortalProtectHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
