// canvas HUD - seed, tower HP, coins, controls, crosshair

#include "Game/PortalProtectHUD.h"
#include "Game/PortalProtectGameMode.h"
#include "Tower/CentralTower.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

// pull stats from game mode / tower each frame
void APortalProtectHUD::DrawHUD()
{
	Super::DrawHUD();

	APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	const ACentralTower* Tower = GM->GetTower();
	const float TowerHP = Tower ? Tower->GetHealth() : 0.f;
	const float TowerMax = Tower ? Tower->GetMaxHealth() : 1.f;

	const FString Line1 = FString::Printf(TEXT("PORTAL PROTECT  |  Seed: %d"), GM->GetTerrainSeed());
	const FString Line2 = FString::Printf(TEXT("Tower HP: %.0f / %.0f"), TowerHP, TowerMax);
	const FString Line3 = FString::Printf(TEXT("Coins: %d   |   Defender cost: %d   |   Places left: %d"),
		GM->GetCoinBalance(), GM->GetDefenderCost(), GM->GetDefendersRemaining());
	const FString Line4 = TEXT("WASD move  |  Mouse look  |  Space jump  |  LMB place  |  Esc/P pause  |  R restart");

	DrawText(Line1, FLinearColor::White, 40.f, 40.f, GEngine->GetMediumFont(), 1.2f);
	DrawText(Line2, FLinearColor(0.6f, 0.85f, 1.f), 40.f, 70.f, GEngine->GetMediumFont(), 1.2f);
	DrawText(Line3, FLinearColor(1.f, 0.9f, 0.3f), 40.f, 100.f, GEngine->GetMediumFont(), 1.2f);
	DrawText(Line4, FLinearColor(0.9f, 0.9f, 0.7f), 40.f, 130.f, GEngine->GetMediumFont(), 1.05f);

	// crosshair for aiming at pads
	if (Canvas)
	{
		const float CX = Canvas->SizeX * 0.5f;
		const float CY = Canvas->SizeY * 0.5f;
		DrawLine(CX - 10.f, CY, CX + 10.f, CY, FLinearColor(1.f, 1.f, 1.f, 0.7f));
		DrawLine(CX, CY - 10.f, CX, CY + 10.f, FLinearColor(1.f, 1.f, 1.f, 0.7f));
	}

	const FString Status = GM->GetStatusMessage();
	if (!Status.IsEmpty())
	{
		DrawText(Status, FLinearColor(1.f, 0.75f, 0.35f), 40.f, 165.f, GEngine->GetMediumFont(), 1.15f);
	}
}
