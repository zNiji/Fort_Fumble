// initial scatter + respawn up to MaxActiveCoins

#include "Economy/CoinSpawner.h"
#include "Economy/CoinPickup.h"
#include "Terrain/ProceduralTerrainActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ACoinSpawner::ACoinSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	CoinClass = ACoinPickup::StaticClass();
}

void ACoinSpawner::BeginPlay()
{
	Super::BeginPlay();
	RespawnTimer = RespawnInterval;
}

void ACoinSpawner::Configure(AProceduralTerrainActor* InTerrain)
{
	Terrain = InTerrain;
	if (!Terrain || !CoinClass)
	{
		return;
	}

	const int32 ToSpawn = FMath::Min(InitialCoinCount, MaxActiveCoins);
	for (int32 I = 0; I < ToSpawn; ++I)
	{
		SpawnOneCoin();
	}
}

void ACoinSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Terrain || !CoinClass)
	{
		return;
	}

	RespawnTimer -= DeltaTime;
	if (RespawnTimer <= 0.f)
	{
		RespawnTimer = RespawnInterval;
		if (CountActiveCoins() < MaxActiveCoins)
		{
			SpawnOneCoin();
		}
	}
}

// random off-path spot from terrain helper
void ACoinSpawner::SpawnOneCoin()
{
	if (!Terrain || !GetWorld() || !CoinClass)
	{
		return;
	}

	// never go over MaxActiveCoins (initial burst and respawn share this)
	if (CountActiveCoins() >= MaxActiveCoins)
	{
		return;
	}

	FVector Loc;
	if (!Terrain->TryGetRandomOffPathLocation(Loc, 40.f))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<ACoinPickup>(CoinClass, Loc, FRotator::ZeroRotator, Params);
}

int32 ACoinSpawner::CountActiveCoins() const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACoinPickup::StaticClass(), Found);
	return Found.Num();
}
