// simple canvas HUD for match stats - no UMG needed for the basics
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PortalProtectHUD.generated.h"

UCLASS()
class FORT_FUMBLE_API APortalProtectHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
