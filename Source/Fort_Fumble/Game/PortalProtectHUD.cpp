// canvas HUD - score up top, then seed, tower HP, coins, controls, crosshair

#include "Game/PortalProtectHUD.h"
#include "Game/PortalProtectGameMode.h"
#include "Core/PortalProtectTypes.h"
#include "Tower/CentralTower.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

APortalProtectHUD::APortalProtectHUD()
{
	// engine SDF font - crisp at ~24-32px, no stretching a tiny bitmap
	static ConstructorHelpers::FObjectFinder<UFont> FontFinder(TEXT("/Engine/EngineFonts/RobotoDistanceField"));
	if (FontFinder.Succeeded())
	{
		ScoreFont = FontFinder.Object;
	}
}

// pull stats from game mode / tower each frame
void APortalProtectHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	AGameModeBase* RawGM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	APortalProtectGameMode* GM = Cast<APortalProtectGameMode>(RawGM);
	if (!GM)
	{
		if (!bLoggedScoreDraw)
		{
			bLoggedScoreDraw = true;
			UE_LOG(LogTemp, Warning,
				TEXT("PortalProtectHUD: cannot draw Score, expected PortalProtectGameMode but got %s."),
				RawGM ? *RawGM->GetClass()->GetName() : TEXT("null"));
		}
		return;
	}

	if (!bLoggedScoreDraw)
	{
		bLoggedScoreDraw = true;
		UE_LOG(LogTemp, Log, TEXT("PortalProtectHUD: drawing Score each frame (GetScore=%d)."), GM->GetScore());
	}

	UFont* BodyFont = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* CrispScoreFont = ScoreFont;
	if (!CrispScoreFont && GEngine)
	{
		CrispScoreFont = GEngine->GetSubtitleFont() ? GEngine->GetSubtitleFont() : GEngine->GetMediumFont();
	}

	const FString ScoreLine = FString::Printf(TEXT("Score: %d"), GM->GetScore());

	// dark strip so yellow score stays readable on bright sky / grass
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), 18.f, 14.f, 420.f, 44.f);

	const FLinearColor ScoreYellow(1.f, 0.95f, 0.12f, 1.f);
	const FLinearColor ScoreShadow(0.f, 0.f, 0.f, 1.f);
	const float ScoreScale = 1.f; // native SDF size (~28px) - scale 2.6 on LargeFont was the blur
	DrawText(ScoreLine, ScoreShadow, 40.f, 24.f, CrispScoreFont, ScoreScale);
	DrawText(ScoreLine, ScoreYellow, 38.f, 22.f, CrispScoreFont, ScoreScale);

	const ACentralTower* Tower = GM->GetTower();
	const float TowerHP = Tower ? Tower->GetHealth() : 0.f;
	const float TowerMax = Tower ? Tower->GetMaxHealth() : 1.f;

	const int32 WaveNum = GM->GetCurrentWave();
	const int32 EnemiesLeft = GM->GetEnemiesRemainingInWave();

	const FString Line1 = FString::Printf(TEXT("PORTAL PROTECT  |  Seed: %d"), GM->GetTerrainSeed());
	const FString Line2 = FString::Printf(TEXT("Wave: %d   |   Enemies left: %d"),
		WaveNum > 0 ? WaveNum : 1, EnemiesLeft);
	const FString Line3 = FString::Printf(TEXT("Tower HP: %.0f / %.0f"), TowerHP, TowerMax);
	const FString SelectedName = APortalProtectGameMode::GetDefenderDisplayName(GM->GetSelectedDefenderType());
	const int32 SelectedCost = GM->GetDefenderCost();
	const FString Line4 = FString::Printf(TEXT("Coins: %d   |   Places left: %d   |   Selected: %s (%d coins)"),
		GM->GetCoinBalance(), GM->GetDefendersRemaining(), *SelectedName, SelectedCost);
	const FString Line4b = FString::Printf(TEXT("Costs — Cannon: %d   Marksman: %d   Mortar: %d"),
		GM->GetDefenderCostForType(EDefenderType::Cannon),
		GM->GetDefenderCostForType(EDefenderType::Marksman),
		GM->GetDefenderCostForType(EDefenderType::Mortar));
	const FString Line5 = TEXT("1/2/3 or Q/E or wheel — pick defender  |  LMB place  |  WASD  |  Esc pause  |  R restart");

	DrawText(Line1, FLinearColor::White, 40.f, 82.f, BodyFont, 1.2f);
	DrawText(Line2, FLinearColor(1.f, 0.55f, 0.35f), 40.f, 112.f, BodyFont, 1.2f);
	DrawText(Line3, FLinearColor(0.6f, 0.85f, 1.f), 40.f, 142.f, BodyFont, 1.2f);
	DrawText(Line4, FLinearColor(1.f, 0.9f, 0.3f), 40.f, 172.f, BodyFont, 1.2f);
	DrawText(Line4b, FLinearColor(0.75f, 0.85f, 1.f), 40.f, 202.f, BodyFont, 1.05f);
	DrawText(Line5, FLinearColor(0.9f, 0.9f, 0.7f), 40.f, 232.f, BodyFont, 1.05f);

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	DrawLine(CX - 10.f, CY, CX + 10.f, CY, FLinearColor(1.f, 1.f, 1.f, 0.7f));
	DrawLine(CX, CY - 10.f, CX, CY + 10.f, FLinearColor(1.f, 1.f, 1.f, 0.7f));

	const FString Status = GM->GetStatusMessage();
	if (!Status.IsEmpty())
	{
		DrawText(Status, FLinearColor(1.f, 0.75f, 0.35f), 40.f, 266.f, BodyFont, 1.15f);
	}
}
