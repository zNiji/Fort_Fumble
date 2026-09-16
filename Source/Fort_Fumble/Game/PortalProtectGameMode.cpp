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
	SelectedDefenderType = EDefenderType::Cannon;
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

	// make sure budget covers all the pads (at least 4 per path)
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

int32 APortalProtectGameMode::GetDefenderCostForType(EDefenderType Type) const
{
	switch (Type)
	{
	case EDefenderType::Marksman:
		return MarksmanCost;
	case EDefenderType::Mortar:
		return MortarCost;
	case EDefenderType::Cannon:
	default:
		return CannonCost;
	}
}

FString APortalProtectGameMode::GetDefenderDisplayName(EDefenderType Type)
{
	switch (Type)
	{
	case EDefenderType::Marksman:
		return TEXT("Marksman");
	case EDefenderType::Mortar:
		return TEXT("Mortar");
	case EDefenderType::Cannon:
	default:
		return TEXT("Cannon");
	}
}

void APortalProtectGameMode::SetSelectedDefenderType(EDefenderType Type)
{
	if (bGameOver)
	{
		return;
	}
	SelectedDefenderType = Type;
	const int32 Cost = GetDefenderCostForType(SelectedDefenderType);
	UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Selected defender type -> %s (cost %d)"),
		*GetDefenderDisplayName(SelectedDefenderType), Cost);
	SetStatusMessage(
		FString::Printf(TEXT("Selected: %s (%d coins)."), *GetDefenderDisplayName(SelectedDefenderType), Cost),
		1.8f);
}

void APortalProtectGameMode::CycleSelectedDefenderType(int32 Delta)
{
	if (bGameOver || Delta == 0)
	{
		return;
	}
	const int32 Count = 3;
	int32 Index = static_cast<int32>(SelectedDefenderType);
	Index = (Index + Delta) % Count;
	if (Index < 0)
	{
		Index += Count;
	}
	SetSelectedDefenderType(static_cast<EDefenderType>(Index));
}

// check coins + budget, spawn defender on pad top, deduct cost
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

	const EDefenderType PlaceType = SelectedDefenderType;
	const int32 Cost = GetDefenderCostForType(PlaceType);
	if (CoinBalance < Cost)
	{
		SetStatusMessage(FString::Printf(TEXT("Need %d coins for %s (have %d)."),
			Cost, *GetDefenderDisplayName(PlaceType), CoinBalance));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Surface = Spot->GetPadSurfaceLocation();
	const FTransform SpawnXform(FRotator::ZeroRotator, Surface);

	// defer BeginPlay so InitializeAsType runs before default Cannon setup
	ADefenderUnit* Defender = World->SpawnActorDeferred<ADefenderUnit>(
		ADefenderUnit::StaticClass(),
		SpawnXform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Defender)
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[PortalProtect] placing %s (cost %d, coins %d -> %d)"),
		*GetDefenderDisplayName(PlaceType), Cost, CoinBalance, CoinBalance - Cost);

	Defender->InitializeAsType(PlaceType);
	const FVector FinalLoc = Surface + FVector(0.f, 0.f, Defender->GetPivotToGroundOffset());
	Defender->FinishSpawning(FTransform(FRotator::ZeroRotator, FinalLoc));

	Defender->SetOwningSpot(Spot); // so death frees this pad (does not destroy it)
	Spot->SetOccupied(true);
	--DefendersRemaining;
	CoinBalance -= Cost;
	SetStatusMessage(FString::Printf(TEXT("%s placed (−%d coins)."),
		*GetDefenderDisplayName(PlaceType), Cost), 1.5f);
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

int32 APortalProtectGameMode::GetCurrentWave() const
{
	return Spawner ? Spawner->GetCurrentWave() : 0;
}

int32 APortalProtectGameMode::GetEnemiesRemainingInWave() const
{
	return Spawner ? Spawner->GetEnemiesRemaining() : 0;
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

// pad stays; give the place slot back so you can rebuild after a wipe
void APortalProtectGameMode::NotifyDefenderDestroyed()
{
	if (bGameOver)
	{
		return;
	}
	++DefendersRemaining;
}
