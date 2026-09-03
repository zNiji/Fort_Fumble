// match bootstrap, economy, placement rules

#include "Game/PortalProtectGameMode.h"
#include "Game/PortalProtectPlayerController.h"
#include "Game/PortalProtectPawn.h"
#include "Game/PortalProtectHUD.h"
#include "Terrain/ProceduralTerrainActor.h"
#include "Tower/CentralTower.h"
#include "Enemy/EnemySpawner.h"
#include "Economy/CoinSpawner.h"
#include "Defender/DefenderPlacementSpot.h"
#include "Defender/DefenderUnit.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"

APortalProtectGameMode::APortalProtectGameMode()
{
	DefaultPawnClass = APortalProtectPawn::StaticClass();
	PlayerControllerClass = APortalProtectPlayerController::StaticClass();
	HUDClass = APortalProtectHUD::StaticClass();
	DefendersRemaining = StartingDefenders;
	CoinBalance = StartingCoins;
}

void APortalProtectGameMode::BeginPlay()
{
	Super::BeginPlay();
	DefendersRemaining = StartingDefenders;
	CoinBalance = StartingCoins;
	Score = 0;
	bGameOver = false;
	StatusMessage.Empty();
	PlayerPlaceAttempts = 0;
	SpawnWorld();

	// pawn might not exist yet at game mode BeginPlay - retry until possessed
	GetWorldTimerManager().SetTimer(PlayerPlaceTimer, this, &APortalProtectGameMode::PlacePlayerOnTerrain, 0.05f, true);
	// survival points while the portal is still standing
	GetWorldTimerManager().SetTimer(SurvivalScoreTimer, this, &APortalProtectGameMode::TickSurvivalScore, 1.0f, true);
}

// spawn terrain, tower, spawners, pads - wire everything to generated terrain data
void APortalProtectGameMode::SpawnWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Terrain = World->SpawnActor<AProceduralTerrainActor>(AProceduralTerrainActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!Terrain)
	{
		return;
	}

	// BeginPlay on terrain usually already ran this, but just in case
	if (Terrain->GetPaths().Num() == 0)
	{
		Terrain->GenerateTerrain();
	}

	Tower = World->SpawnActor<ACentralTower>(ACentralTower::StaticClass(), Terrain->GetTowerLocation(), FRotator::ZeroRotator, Params);
	if (Tower)
	{
		Tower->OnTowerDestroyed.AddDynamic(this, &APortalProtectGameMode::NotifyTowerDestroyed);
	}

	Spawner = World->SpawnActor<AEnemySpawner>(AEnemySpawner::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Spawner)
	{
		Spawner->Configure(Terrain);
	}

	CoinSpawner = World->SpawnActor<ACoinSpawner>(ACoinSpawner::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (CoinSpawner)
	{
		CoinSpawner->Configure(Terrain);
	}

	PlacementSpots.Reset();
	for (const FDefenderSlotData& Slot : Terrain->GetDefenderSlots())
	{
		ADefenderPlacementSpot* Spot = World->SpawnActor<ADefenderPlacementSpot>(
			ADefenderPlacementSpot::StaticClass(), Slot.Location, FRotator::ZeroRotator, Params);
		if (Spot)
		{
			PlacementSpots.Add(Spot);
		}
	}

	// make sure budget covers all the pads (at least 3 per path)
	DefendersRemaining = FMath::Max(StartingDefenders, PlacementSpots.Num());
}

// drop FPS pawn on random off-path cell, face the tower - retries until pawn exists
void APortalProtectGameMode::PlacePlayerOnTerrain()
{
	UWorld* World = GetWorld();
	if (!World || !Terrain)
	{
		return;
	}

	++PlayerPlaceAttempts;
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pawn)
	{
		if (PlayerPlaceAttempts >= 40)
		{
			GetWorldTimerManager().ClearTimer(PlayerPlaceTimer);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(PlayerPlaceTimer);

	FVector SpawnLoc;
	if (!Terrain->TryGetRandomOffPathLocation(SpawnLoc, 20.f))
	{
		SpawnLoc = Terrain->GetTowerLocation() + FVector(400.f, 0.f, 100.f);
	}

	if (const ACharacter* AsChar = Cast<ACharacter>(Pawn))
	{
		SpawnLoc.Z += AsChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	else
	{
		SpawnLoc.Z += 96.f;
	}

	Pawn->SetActorLocation(SpawnLoc, false, nullptr, ETeleportType::ResetPhysics);

	const FVector ToTower = (Terrain->GetTowerLocation() - SpawnLoc).GetSafeNormal2D();
	const FRotator Facing = ToTower.IsNearlyZero() ? FRotator::ZeroRotator : ToTower.Rotation();
	Pawn->SetActorRotation(FRotator(0.f, Facing.Yaw, 0.f));
	PC->SetControlRotation(FRotator(-10.f, Facing.Yaw, 0.f));
}

// check coins + budget, spawn cannon on pad top, deduct cost
bool APortalProtectGameMode::TryPlaceDefenderAtSpot(ADefenderPlacementSpot* Spot)
{
	if (bGameOver || !Spot || Spot->IsOccupied())
	{
		return false;
	}

	if (DefendersRemaining <= 0)
	{
		SetStatusMessage(TEXT("No defender placements remaining."));
		return false;
	}

	if (CoinBalance < DefenderCost)
	{
		SetStatusMessage(FString::Printf(TEXT("Need %d coins (have %d)."), DefenderCost, CoinBalance));
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// spawn on pad surface then lift by pivot-to-ground so base sits on the disc
	const FVector Surface = Spot->GetPadSurfaceLocation();
	ADefenderUnit* Defender = GetWorld()->SpawnActor<ADefenderUnit>(
		ADefenderUnit::StaticClass(), Surface, FRotator::ZeroRotator, Params);
	if (!Defender)
	{
		return false;
	}
	Defender->SetActorLocation(Surface + FVector(0.f, 0.f, Defender->GetPivotToGroundOffset()));

	Spot->SetOccupied(true);
	--DefendersRemaining;
	CoinBalance -= DefenderCost;
	SetStatusMessage(FString::Printf(TEXT("Defender placed (−%d coins)."), DefenderCost), 1.5f);
	return true;
}

void APortalProtectGameMode::AddCoins(int32 Amount)
{
	if (Amount <= 0 || bGameOver)
	{
		return;
	}
	CoinBalance += Amount;
	SetStatusMessage(FString::Printf(TEXT("+%d coins"), Amount), 1.2f);
}

void APortalProtectGameMode::AddScore(int32 Amount)
{
	if (Amount <= 0 || bGameOver)
	{
		return;
	}
	Score += Amount;
}

void APortalProtectGameMode::TickSurvivalScore()
{
	if (bGameOver)
	{
		return;
	}
	AddScore(SurvivalPointsPerSecond);
}

void APortalProtectGameMode::SetStatusMessage(const FString& Message, float Duration)
{
	StatusMessage = Message;
	GetWorldTimerManager().ClearTimer(StatusMessageTimer);
	GetWorldTimerManager().SetTimer(StatusMessageTimer, this, &APortalProtectGameMode::ClearStatusMessage, Duration, false);
}

void APortalProtectGameMode::ClearStatusMessage()
{
	StatusMessage.Empty();
}

int32 APortalProtectGameMode::GetTerrainSeed() const
{
	return Terrain ? Terrain->GetSeed() : 0;
}

// lose condition - stop spawns, show game over UI
void APortalProtectGameMode::NotifyTowerDestroyed()
{
	bGameOver = true;
	GetWorldTimerManager().ClearTimer(SurvivalScoreTimer);
	if (Spawner)
	{
		Spawner->SetSpawningEnabled(false);
	}

	if (UWorld* World = GetWorld())
	{
		APlayerController* FirstPC = World->GetFirstPlayerController();
		if (APortalProtectPlayerController* PC = Cast<APortalProtectPlayerController>(FirstPC))
		{
			PC->ShowGameOverMenu();
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("NotifyTowerDestroyed: expected PortalProtectPlayerController but got %s. Check GameMode/PlayerController project settings."),
				FirstPC ? *FirstPC->GetClass()->GetName() : TEXT("null"));
		}
	}
}
