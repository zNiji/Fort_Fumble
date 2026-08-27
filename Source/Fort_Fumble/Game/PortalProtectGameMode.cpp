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
	bGameOver = false;
	StatusMessage.Empty();
	PlayerPlaceAttempts = 0;
	SpawnWorld();

	// Pawn may not exist yet during GameMode BeginPlay — retry until FPS character is possessed.
	GetWorldTimerManager().SetTimer(PlayerPlaceTimer, this, &APortalProtectGameMode::PlacePlayerOnTerrain, 0.05f, true);
}

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

	// Ensure generation completed (BeginPlay normally does this during spawn).
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

	// Placement budget must cover the pads (≥ 3 per pathway).
	DefendersRemaining = FMath::Max(StartingDefenders, PlacementSpots.Num());
}

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
	// Spawn on pad top; then raise by the cannon's pivot→ground so the base sits on the disc.
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

void APortalProtectGameMode::NotifyTowerDestroyed()
{
	bGameOver = true;
	if (Spawner)
	{
		Spawner->SetSpawningEnabled(false);
	}
	SetStatusMessage(TEXT("GAME OVER — the portal fell."), 30.f);
}
